#include "pagepool.h"

static int st_pagepool_cmp_region_addr(const void *a, const void *b)
{
    uint8_t *reg_a_addr = *(uint8_t **)a;
    uint8_t *reg_b_addr = *(uint8_t **)b;

    return st_cmp(reg_a_addr, reg_b_addr);
}

static int st_pagepool_add_region(st_pagepool_t *pool, uint8_t *region)
{
    ssize_t idx;
    int ret;

    ret = st_robustlock_lock(&pool->regions_lock);
    if (ret != ST_OK) {
        return ret;
    }

    ret = st_array_bsearch_right(&pool->regions, &region, NULL, &idx);
    if (ret == ST_OK) {
        st_bug("region should not be in array");
    } else if (ret != ST_NOT_FOUND) {
        goto quit;
    }

    ret = st_array_insert(&pool->regions, idx, &region);

quit:
    st_robustlock_unlock_err_abort(&pool->regions_lock);
    return ret;
}

static int st_pagepool_remove_region(st_pagepool_t *pool, uint8_t *region)
{
    int ret;
    ssize_t idx;

    ret = st_robustlock_lock(&pool->regions_lock);
    if (ret != ST_OK) {
        return ret;
    }

    ret = st_array_bsearch_left(&pool->regions, &region, NULL, &idx);
    if (ret != ST_OK) {
        goto quit;
    }

    ret = st_array_remove(&pool->regions, idx);

quit:
    st_robustlock_unlock_err_abort(&pool->regions_lock);
    return ret;
}

static int st_pagepool_get_region(st_pagepool_t *pool, uint8_t *addr, uint8_t **region)
{
    uint8_t *reg;
    ssize_t idx;
    int ret;

    ret = st_robustlock_lock(&pool->regions_lock);
    if (ret != ST_OK) {
        return ret;
    }

    ret = st_array_bsearch_left(&pool->regions, &addr, NULL, &idx);
    if (ret == ST_OK) {
        *region = addr;
        goto quit;
    } else if (ret != ST_NOT_FOUND) {
        goto quit;
    }

    if (idx == 0) {
        ret = ST_NOT_FOUND;
        goto quit;
    }

    reg = *(uint8_t **)st_array_get(&pool->regions, idx - 1);

    if (reg < addr && addr < reg + pool->region_size) {
        *region = reg;
        ret = ST_OK;
    }

quit:
    st_robustlock_unlock_err_abort(&pool->regions_lock);
    return ret;
}

static int st_pagepool_cmp_compound_page(st_rbtree_node_t *a, st_rbtree_node_t *b)
{
    st_pagepool_page_t *pa = st_owner(a, st_pagepool_page_t, rbnode);
    st_pagepool_page_t *pb = st_owner(b, st_pagepool_page_t, rbnode);

    return st_cmp(pa->compound_page_cnt, pb->compound_page_cnt);
}

static void st_pagepool_init_region(st_pagepool_t *pool, uint8_t *region)
{
    st_pagepool_page_t *pages = (st_pagepool_page_t *)region;

    memset(pages, 0, sizeof(*pages) * pool->pages_per_region);

    for (int i = 0; i < pool->pages_per_region; i++) {
        pages[i].region = region;
        pages[i].state = ST_PAGEPOOL_PAGE_FREE;
        pages[i].type = ST_PAGEPOOL_PAGE_SLAVE;
        pages[i].compound_page_cnt = pool->pages_per_region;
    }

    pages[0].type = ST_PAGEPOOL_PAGE_MASTER;
}

static void st_pagepool_add_pages(st_pagepool_t *pool, st_pagepool_page_t *master)
{
    st_rbtree_node_t *n;

    n = st_rbtree_search_eq(&pool->free_pages, &master->rbnode);

    if (n == NULL) {
        st_rbtree_insert(&pool->free_pages, &master->rbnode);
        st_list_init(&master->lnode);
    } else {
        st_pagepool_page_t *found = st_owner(n, st_pagepool_page_t, rbnode);
        st_list_insert_last(&found->lnode, &master->lnode);
    }
}

static void st_pagepool_remove_pages(st_pagepool_t *pool, st_pagepool_page_t *master)
{
    st_pagepool_page_t *candidate;

    if (st_rbtree_node_is_inited(&master->rbnode) == 0) {
        // removed pages in other same num pages's list
        st_list_remove(&master->lnode);

    } else if (st_list_empty(&master->lnode)) {
        // removed pages are only the num pages in tree
        st_rbtree_delete(&pool->free_pages, &master->rbnode);

    } else {
        // removed pages are in tree, and the head of same num pages list
        candidate = st_list_first_entry(&master->lnode, st_pagepool_page_t, lnode);

        st_list_remove(&master->lnode);

        int ret = st_rbtree_replace(&pool->free_pages, &master->rbnode, &candidate->rbnode);
        if (ret != ST_OK) {
            derr("st_rbtree_replace error %d\n", ret);
            st_bug("st_rbtree_replace error");
        }
    }
}

static st_pagepool_page_t *st_pagepool_prev_pages(st_pagepool_t *pool,
        st_pagepool_page_t *master)
{
    if ((uint8_t *)master == master->region) {
        return NULL;
    }

    return master - master[-1].compound_page_cnt;
}

static st_pagepool_page_t *st_pagepool_next_pages(st_pagepool_t *pool,
        st_pagepool_page_t *master)
{

    if (master + master->compound_page_cnt
            == (st_pagepool_page_t *)master->region + pool->pages_per_region) {
        return NULL;
    }

    return master + master->compound_page_cnt;
}

static st_pagepool_page_t *st_pagepool_merge_pages(st_pagepool_t *pool,
        st_pagepool_page_t *master)
{
    st_pagepool_page_t *prev, *next, *merge;
    int total = master->compound_page_cnt;

    merge = master;

    prev = st_pagepool_prev_pages(pool, master);
    if (prev != NULL && prev->state == ST_PAGEPOOL_PAGE_FREE) {
        merge = prev;
        st_pagepool_remove_pages(pool, prev);
        total += prev->compound_page_cnt;
    }

    next = st_pagepool_next_pages(pool, master);
    if (next != NULL && next->state == ST_PAGEPOOL_PAGE_FREE) {
        st_pagepool_remove_pages(pool, next);
        total += next->compound_page_cnt;
    }

    for (int i = 0; i < total; i++) {
        merge[i].state = ST_PAGEPOOL_PAGE_FREE;
        merge[i].type = ST_PAGEPOOL_PAGE_SLAVE;
        merge[i].compound_page_cnt = total;
    }

    merge[0].type = ST_PAGEPOOL_PAGE_MASTER;

    return merge;
}

static st_pagepool_page_t *st_pagepool_split_pages(st_pagepool_page_t *pages,
        int use_cnt)
{
    int i;
    st_pagepool_page_t *remain;
    int total_cnt = pages[0].compound_page_cnt;

    for (i = 0; i < use_cnt; i++) {
        pages[i].compound_page_cnt = use_cnt;
        pages[i].state = ST_PAGEPOOL_PAGE_ALLOCATED;
    }

    if (total_cnt == use_cnt) {
        return NULL;
    }

    remain = &pages[i];

    for (i = 0; i < total_cnt - use_cnt; i++) {
        remain[i].compound_page_cnt = total_cnt - use_cnt;
    }

    remain->type = ST_PAGEPOOL_PAGE_MASTER;

    return remain;
}

static int st_pagepool_get_free_pages(st_pagepool_t *pool, int cnt,
        st_pagepool_page_t **pages)
{
    st_rbtree_node_t *n;
    st_pagepool_page_t *found, *remain;
    st_pagepool_page_t tmp = {.compound_page_cnt = cnt};

    //find equal or bigger free pages to allocate
    n = st_rbtree_search_ge(&pool->free_pages, &tmp.rbnode);
    if (n == NULL) {
        return ST_NOT_FOUND;
    }

    found = st_owner(n, st_pagepool_page_t, rbnode);

    if (st_list_empty(&found->lnode)) {
        st_rbtree_delete(&pool->free_pages, &found->rbnode);
    } else {
        //use last free pages in list
        found = st_list_last_entry(&found->lnode, st_pagepool_page_t, lnode);
        st_list_remove(&found->lnode);
    }

    remain = st_pagepool_split_pages(found, cnt);
    if (remain != NULL) {
        st_pagepool_add_pages(pool, remain);
    }

    *pages = found;

    return ST_OK;
}

int st_pagepool_init(st_pagepool_t *pool)
{
    int ret;

    st_must(pool != NULL, ST_ARG_INVALID);

    pool->page_size = pool->region_cb.page_size;
    pool->region_size = pool->region_cb.reg_size;

    pool->pages_per_region = pool->region_size /
                             (sizeof(st_pagepool_page_t) + pool->page_size);

    st_must(pool->pages_per_region > 0, ST_ARG_INVALID);

    pool->space_base_offset = st_align(sizeof(st_pagepool_page_t) * pool->pages_per_region,
                                       pool->page_size);

    ret = st_rbtree_init(&pool->free_pages, st_pagepool_cmp_compound_page);
    if (ret != ST_OK) {
        return ret;
    }

    ret = st_array_init_static(&pool->regions, sizeof(uint8_t *), pool->regions_array_data,
                               ST_PAGEPOOL_MAX_REGION_CNT, st_pagepool_cmp_region_addr);
    if (ret != ST_OK) {
        return ret;
    }

    ret = st_robustlock_init(&pool->regions_lock);
    if (ret != ST_OK) {
        st_array_destroy(&pool->regions);
        return ret;
    }

    ret = st_robustlock_init(&pool->pages_lock);
    if (ret != ST_OK) {
        st_robustlock_destroy(&pool->regions_lock);
        st_array_destroy(&pool->regions);
    }

    return ST_OK;
}

int st_pagepool_destroy(st_pagepool_t *pool)
{
    int ret, err = ST_OK;

    st_must(pool != NULL, ST_ARG_INVALID);

    ret = st_array_destroy(&pool->regions);
    if (ret != ST_OK) {
        derr("st_array_destroy error %d\n", ret);
        err = ret;
    }

    ret = st_robustlock_destroy(&pool->regions_lock);
    if (ret != ST_OK) {
        derr("st_robustlock_destroy regions_lock error %d\n", ret);
        err = err == ST_OK ? ret : err;
    }

    ret = st_robustlock_destroy(&pool->pages_lock);
    if (ret != ST_OK) {
        derr("st_robustlock_destroy pages_lock error %d\n", ret);
        err = err == ST_OK ? ret : err;
    }

    return err;
}

int st_pagepool_alloc_pages(st_pagepool_t *pool, int cnt,
        st_pagepool_page_t **pages)
{
    int ret;
    uint8_t *region;

    st_must(pool != NULL, ST_ARG_INVALID);
    st_must(pages != NULL, ST_ARG_INVALID);
    st_must(0 < cnt && cnt <= pool->pages_per_region, ST_ARG_INVALID);

    ret = st_robustlock_lock(&pool->pages_lock);
    if (ret != ST_OK) {
        return ret;
    }

    ret = st_pagepool_get_free_pages(pool, cnt, pages);
    if (ret == ST_OK) {
        goto quit;
    } else if (ret != ST_NOT_FOUND) {
        goto quit;
    }

    ret = st_region_alloc_reg(&pool->region_cb, &region);
    if (ret != ST_OK) {
        goto quit;
    }

    ret = st_pagepool_add_region(pool, region);
    if (ret != ST_OK) {
        st_region_free_reg(&pool->region_cb, region);
        goto quit;
    }

    st_pagepool_init_region(pool, region);
    *pages = (st_pagepool_page_t *)region;

    st_pagepool_page_t *remain = st_pagepool_split_pages(*pages, cnt);
    if (remain != NULL) {
        st_pagepool_add_pages(pool, remain);
    }

quit:
    st_robustlock_unlock_err_abort(&pool->pages_lock);
    return ret;
}

int st_pagepool_free_pages(st_pagepool_t *pool, st_pagepool_page_t *pages)
{
    st_pagepool_page_t *merge;
    int ret;

    st_must(pool != NULL, ST_ARG_INVALID);
    st_must(pages != NULL, ST_ARG_INVALID);
    st_must(pages[0].type == ST_PAGEPOOL_PAGE_MASTER, ST_ARG_INVALID);
    st_must(pages[0].state == ST_PAGEPOOL_PAGE_ALLOCATED, ST_ARG_INVALID);

    ret = st_robustlock_lock(&pool->pages_lock);
    if (ret != ST_OK) {
        return ret;
    }

    merge = st_pagepool_merge_pages(pool, pages);

    if (merge->compound_page_cnt < pool->pages_per_region) {
        st_pagepool_add_pages(pool, merge);

    } else {
        uint8_t *region = (uint8_t *)merge;

        ret = st_pagepool_remove_region(pool, region);
        if (ret != ST_OK) {
            goto quit;
        }

        ret = st_region_free_reg(&pool->region_cb, region);
    }

quit:
    st_robustlock_unlock_err_abort(&pool->pages_lock);
    return ret;
}

int st_pagepool_page_to_addr(st_pagepool_t *pool, st_pagepool_page_t *page,
        uint8_t **addr)
{
    st_must(pool != NULL, ST_ARG_INVALID);
    st_must(page != NULL, ST_ARG_INVALID);
    st_must(addr != NULL, ST_ARG_INVALID);

    // no need to check idx range, if page invalid, the region element will cause core
    ssize_t idx = ((uint8_t *)page - page->region) / sizeof(st_pagepool_page_t);

    uint8_t *base = page->region + pool->space_base_offset;
    *addr = base + idx * pool->page_size;

    return ST_OK;
}

int st_pagepool_addr_to_page(st_pagepool_t *pool, uint8_t *addr,
        st_pagepool_page_t **page)
{
    uint8_t *region, *base;
    int ret;

    st_must(pool != NULL, ST_ARG_INVALID);
    st_must(addr != NULL, ST_ARG_INVALID);
    st_must(page != NULL, ST_ARG_INVALID);

    ret = st_pagepool_get_region(pool, addr, &region);
    if (ret != ST_OK) {
        return ret;
    }

    base = region + pool->space_base_offset;

    if (addr < base) {
        return ST_OUT_OF_RANGE;
    }

    if ((addr - base) % pool->page_size != 0) {
        return ST_ARG_INVALID;
    }

    *page = (st_pagepool_page_t *)region + (addr - base) / pool->page_size;

    return ST_OK;
}

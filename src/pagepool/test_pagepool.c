#define ST_UNIT_TEST

#include "pagepool.h"
#include "unittest/unittest.h"
#include <sys/mman.h>
#include <sys/wait.h>
#include <sched.h>

#include <sys/sem.h>
#include <sys/ipc.h>
#include <time.h>
#include <stdlib.h>

#ifdef _SEM_SEMUN_UNDEFINED
union semun
{
    int val;
    struct semid_ds *buf;
    unsigned short *array;
    struct seminfo *__buf;
};
#endif

void *alloc_buf(ssize_t size)
{
    return mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
}

void free_buf(void *addr, ssize_t size)
{
    munmap(addr, size);
}

void set_process_to_cpu(int cpu_id)
{
    int cpu_count = sysconf(_SC_NPROCESSORS_CONF);

    cpu_set_t set;

    CPU_ZERO(&set);
    CPU_SET(cpu_id % cpu_count, &set);

    sched_setaffinity(0, sizeof(set), &set);
}

int init_pagepool(st_pagepool_t *pool, uint8_t *base,
        ssize_t pool_size, ssize_t region_size)
{
    int ret;

    ret = st_region_init(&pool->region_cb, base,
                         region_size/4096, pool_size/region_size, 0);
    if (ret != ST_OK) {
        return ret;
    }

    ret = st_pagepool_init(pool);
    if (ret != ST_OK) {
        return ret;
    }

    return ST_OK;
}

int wait_children(int *pids, int pid_cnt) {
    int ret, err = ST_OK;

    for (int i = 0; i < pid_cnt; i++) {
        waitpid(pids[i], &ret, 0);
        if (ret != ST_OK) {
            err = ret;
        }
    }

    return err;
}

int check_pages(st_pagepool_page_t *pages, uint8_t *region, int cnt, int state)
{

    for (int i = 0; i < cnt; i++) {
        if (region != pages[i].region) {
            return ST_ERR;
        }

        if (state != pages[i].state) {
            return ST_ERR;
        }

        if (i == 0) {
            if (pages[i].type != ST_PAGEPOOL_PAGE_MASTER) {
                return ST_ERR;
            }
        } else {
            if (pages[i].type != ST_PAGEPOOL_PAGE_SLAVE) {
                return ST_ERR;
            }
        }

        if (pages[i].compound_page_cnt != cnt) {
            return ST_ERR;
        }
    }

    return ST_OK;
}

st_test(pagepool, init) {

    st_pagepool_t pool;
    uint8_t *buf = alloc_buf(655360);

    struct case_s {
        ssize_t region_size;
        ssize_t expect_pages_per_region;
        int expect_ret;
    } cases[] = {
        {2*4096, 1, ST_OK},
        {10*4096, 9, ST_OK},
        {20*4096, 19, ST_OK},

        {29*4096, 28, ST_OK},

        // if region size bigger than 29*4096, will cost two pages to store page_t
        {30*4096, 28, ST_OK},
        {31*4096, 29, ST_OK},

        {4096, -1, ST_ARG_INVALID},
    };

    for (int i = 0; i < st_nelts(cases); i++) {
        st_typeof(cases[0]) c = cases[i];

        st_ut_eq(ST_OK, st_region_init(&pool.region_cb, buf,
                    c.region_size/4096, 655360/c.region_size, 0), "");

        st_ut_eq(c.expect_ret, st_pagepool_init(&pool), "");
        if (c.expect_ret != ST_OK) {
            continue;
        }

        st_ut_eq(4096, pool.page_size, "");
        st_ut_eq(c.region_size, pool.region_size, "");

        st_ut_eq(c.expect_pages_per_region, pool.pages_per_region, "");
    }

    st_ut_eq(ST_ARG_INVALID, st_pagepool_init(NULL), "");

    free_buf(buf, 655360);
}

st_test(pagepool, destroy) {

    st_pagepool_t pool;
    uint8_t *buf = alloc_buf(655360);

    st_ut_eq(ST_UNINITED, st_pagepool_destroy(&pool), "");

    init_pagepool(&pool, buf, 20*4096, 16*4096);

    st_ut_eq(ST_OK, st_pagepool_destroy(&pool), "");

    st_ut_eq(ST_ARG_INVALID, st_pagepool_destroy(NULL), "");

    free_buf(buf, 655360);
}

st_test(pagepool, alloc) {

    st_pagepool_page_t *pages;
    st_pagepool_t pool;
    st_pagepool_page_t tmp;

    uint8_t *buf = alloc_buf(655360);
    st_pagepool_page_t *prev = NULL;

    struct case_s {
        int alloc_pages_cnt;
        int remain;
        int expect_ret;
    } cases[] = {
        {1, 15, ST_OK},
        {1, 14, ST_OK},
        {5, 9, ST_OK},
        {8, 1, ST_OK},
        {1, 0, ST_OK},
        {1, -1, ST_OUT_OF_MEMORY},

        {0, -1, ST_ARG_INVALID},
        {17, -1, ST_ARG_INVALID},
    };

    // 16 pages can use, another one use to store page_t
    init_pagepool(&pool, buf, 18*4096, 17*4096);

    for (int i = 0; i < st_nelts(cases); i++) {
        st_typeof(cases[0]) c = cases[i];

        st_ut_eq(c.expect_ret, st_pagepool_alloc_pages(&pool, c.alloc_pages_cnt, &pages), "");
        if (c.expect_ret != ST_OK) {
            continue;
        }

        st_ut_eq(1, st_array_current_cnt(&pool.regions), "");

        if (prev != NULL) {
            st_ut_eq(prev + prev[0].compound_page_cnt, pages, "");
        }

        st_ut_eq(ST_OK, check_pages(pages, pool.regions_array_data[0], c.alloc_pages_cnt,
                                    ST_PAGEPOOL_PAGE_ALLOCATED), "");

        tmp.compound_page_cnt = c.remain;
        st_rbtree_node_t *n = st_rbtree_search_eq(&pool.free_pages, &tmp.rbnode);
        st_pagepool_page_t *remain = st_owner(n, st_pagepool_page_t, rbnode);
        st_ut_eq(ST_OK, check_pages(remain, pool.regions_array_data[0], c.remain, ST_PAGEPOOL_PAGE_FREE), "");

        prev = pages;
    }

    st_ut_eq(ST_ARG_INVALID, st_pagepool_alloc_pages(NULL, 10, &pages), "");
    st_ut_eq(ST_ARG_INVALID, st_pagepool_alloc_pages(&pool, 10, NULL), "");

    free_buf(buf, 655360);
}

st_test(pagepool, free) {

    st_pagepool_t pool;
    st_pagepool_page_t tmp;
    uint8_t *buf = alloc_buf(655360);

    struct alloc_pages {
        int cnt;
        st_pagepool_page_t *pages;
    } allocs[] = {
        {.cnt = 1},
        {.cnt = 2},
        {.cnt = 3},
        {.cnt = 4},
        {.cnt = 5},
        {.cnt = 5},
        {.cnt = 4},
        {.cnt = 3},
        {.cnt = 2},
        {.cnt = 1},
    };

    struct case_s {
        int free_idx;
        int remain_cnt;
        int remain_pages[10];
    } cases[] = {
        {0, 1, {1}},
        {2, 2, {1, 3}},
        {4, 3, {1, 3, 5}},
        {6, 4, {1, 3, 5, 4}},
        {8, 5, {1, 3, 5, 4, 2}},
        {3, 4, {1, 12, 4, 2}},
        {1, 3, {15, 4, 2}},
        {9, 3, {15, 4, 3}},
        {5, 2, {24, 3}},
        {7, 0, {}},
    };

    // in one region 30 pages can use, another two pages use to store page_t
    init_pagepool(&pool, buf, 655360, 32*4096);

    for (int i = 0; i < st_nelts(allocs); i++) {
        st_ut_eq(ST_OK, st_pagepool_alloc_pages(&pool, allocs[i].cnt, &allocs[i].pages), "");
    }

    st_ut_eq(1, st_array_current_cnt(&pool.regions), "");

    for (int i = 0; i < st_nelts(cases); i++) {
        st_typeof(cases[0]) c = cases[i];

        st_ut_eq(ST_OK, st_pagepool_free_pages(&pool, allocs[c.free_idx].pages), "");

        for (int j = 0; j < c.remain_cnt; j++) {
            tmp.compound_page_cnt = c.remain_pages[j];
            st_rbtree_node_t *n = st_rbtree_search_eq(&pool.free_pages, &tmp.rbnode);
            st_pagepool_page_t *remain = st_owner(n, st_pagepool_page_t, rbnode);

            st_ut_ne(NULL, remain, "");
            st_ut_eq(ST_OK, check_pages(remain, pool.regions_array_data[0],
                                        c.remain_pages[j], ST_PAGEPOOL_PAGE_FREE), "");
        }
    }

    st_ut_eq(0, st_array_current_cnt(&pool.regions), "");

    st_pagepool_page_t *pages;
    st_pagepool_alloc_pages(&pool, 2, &pages);
    st_ut_eq(ST_ARG_INVALID, st_pagepool_free_pages(NULL, pages), "");
    st_ut_eq(ST_ARG_INVALID, st_pagepool_free_pages(&pool, NULL), "");
    st_ut_eq(ST_ARG_INVALID, st_pagepool_free_pages(&pool, &pages[1]), "");

    pages[0].state = ST_PAGEPOOL_PAGE_FREE;
    st_ut_eq(ST_ARG_INVALID, st_pagepool_free_pages(&pool, pages), "");

    free_buf(buf, 655360);
}

st_test(pagepool, free_and_alloc) {

    st_pagepool_t pool;
    st_pagepool_page_t tmp;
    uint8_t *buf = alloc_buf(655360);

    struct alloc_pages {
        int cnt;
        st_pagepool_page_t *pages;
    } allocs[] = {
        {.cnt = 1},
        {.cnt = 1},
        {.cnt = 2},
        {.cnt = 1},
        {.cnt = 3},
        {.cnt = 1},
        {.cnt = 4},
        {.cnt = 1},
        {.cnt = 5},
        {.cnt = 1},
        {.cnt = 6},
    };

    int free_idxes[] = {0, 2, 4, 6, 8, 10};

    struct case_s {
        int alloc_pages_cnt;
        int remain_cnt;
        int remain_pages[10];
        int not_remain_pages;
    } cases[] = {
        {1, 5, {2, 3, 4, 5, 6}, 1},
        {1, 5, {1, 3, 4, 5, 6}, 2},
        {1, 4, {3, 4, 5, 6}, 1},
        {3, 3, {4, 5, 6}, 3},
        {2, 3, {2, 5, 6}, 4},
        {4, 3, {2, 1, 6}, 5},
        {6, 2, {2, 1}, 6},
        {1, 1, {2}, 1},
        {1, 1, {1}, 2},
        {1, 0, {}, 1},
    };

    // in one region 26 pages can use, another one use to store page_t
    init_pagepool(&pool, buf, 655360, 27*4096);

    for (int i = 0; i < st_nelts(allocs); i++) {
        st_ut_eq(ST_OK, st_pagepool_alloc_pages(&pool, allocs[i].cnt, &allocs[i].pages), "");
    }

    st_ut_eq(1, st_array_current_cnt(&pool.regions), "");

    for (int i = 0; i < st_nelts(free_idxes); i++) {
        st_ut_eq(ST_OK, st_pagepool_free_pages(&pool, allocs[free_idxes[i]].pages), "");
    }

    for (int i = 0; i < st_nelts(cases); i++) {
        st_typeof(cases[0]) c = cases[i];
        st_pagepool_page_t *pages;

        st_ut_eq(ST_OK, st_pagepool_alloc_pages(&pool, c.alloc_pages_cnt, &pages), "");
        st_ut_eq(ST_OK, check_pages(pages, pool.regions_array_data[0], c.alloc_pages_cnt,
                                    ST_PAGEPOOL_PAGE_ALLOCATED), "");

        for (int j = 0; j < c.remain_cnt; j++) {
            tmp.compound_page_cnt = c.remain_pages[j];
            st_rbtree_node_t *n = st_rbtree_search_eq(&pool.free_pages, &tmp.rbnode);
            st_pagepool_page_t *remain = st_owner(n, st_pagepool_page_t, rbnode);
            st_ut_eq(ST_OK, check_pages(remain, pool.regions_array_data[0], c.remain_pages[j],
                                        ST_PAGEPOOL_PAGE_FREE), "");
        }

        tmp.compound_page_cnt = c.not_remain_pages;
        st_ut_eq(NULL, st_rbtree_search_eq(&pool.free_pages, &tmp.rbnode), "");
    }

    free_buf(buf, 655360);
}

st_test(pagepool, free_same_page_cnt) {

    st_pagepool_t pool;
    st_pagepool_page_t tmp;
    st_pagepool_page_t *page;
    st_pagepool_page_t *pages[10] = {0};

    uint8_t *buf = alloc_buf(655360);

    // in one region 29 pages can use, another two pages use to store page_t
    init_pagepool(&pool, buf, 655360, 31*4096);

    for (int i = 0; i < 19; i++) {
        if (i % 2 == 0) {
            st_ut_eq(ST_OK, st_pagepool_alloc_pages(&pool, 2, &pages[i/2]), "");
        } else {
            st_ut_eq(ST_OK, st_pagepool_alloc_pages(&pool, 1, &page), "");
        }
    }

    st_ut_eq(1, st_array_current_cnt(&pool.regions), "");

    for (int i = 0; i < 10; i++) {
        st_ut_eq(ST_OK, st_pagepool_free_pages(&pool, pages[i]), "");
    }

    tmp.compound_page_cnt = 2;
    st_rbtree_node_t *n = st_rbtree_search_eq(&pool.free_pages, &tmp.rbnode);
    st_pagepool_page_t *free_pages = st_owner(n, st_pagepool_page_t, rbnode);

    st_ut_eq(pages[0], free_pages, "");
    st_ut_eq(ST_OK, check_pages(free_pages, pool.regions_array_data[0], 2, ST_PAGEPOOL_PAGE_FREE), "");

    int i = 1;
    st_list_for_each_entry(page, &free_pages->lnode, lnode) {
        st_ut_eq(pages[i], page, "");
        st_ut_eq(ST_OK, check_pages(page, pool.regions_array_data[0], 2, ST_PAGEPOOL_PAGE_FREE), "");
        i++;
    }

    free_buf(buf, 655360);
}

st_test(pagepool, alloc_same_page_cnt) {

    st_pagepool_t pool;
    st_pagepool_page_t *page;
    uint8_t *buf = alloc_buf(655360);
    st_pagepool_page_t *pages[10] = {0};

    // in one region 29 pages can use, another two pages use to store page_t
    init_pagepool(&pool, buf, 655360, 31*4096);

    for (int i = 0; i < 19; i++) {
        if (i % 2 == 0) {
            st_ut_eq(ST_OK, st_pagepool_alloc_pages(&pool, 2, &pages[i/2]), "");
        } else {
            st_ut_eq(ST_OK, st_pagepool_alloc_pages(&pool, 1, &page), "");
        }
    }

    st_ut_eq(1, st_array_current_cnt(&pool.regions), "");

    for (int i = 0; i < 10; i++) {
        st_ut_eq(ST_OK, st_pagepool_free_pages(&pool, pages[i]), "");
    }

    for (int i = 0; i < 10; i++) {
        st_ut_eq(ST_OK, st_pagepool_alloc_pages(&pool, 2, &page), "");
        st_ut_eq(pages[9-i], page, "");
    }

    free_buf(buf, 655360);
}

st_test(pagepool, alloc_in_multi_region) {

    st_pagepool_t pool;
    st_pagepool_page_t tmp;
    st_pagepool_page_t *page;
    uint8_t *buf = alloc_buf(655360);

    // in one region 15 pages can use, another one use to store page_t
    init_pagepool(&pool, buf, 655360, 16*4096);

    for (int i = 0; i < 10; i++) {
        st_ut_eq(ST_OK, st_pagepool_alloc_pages(&pool, 10, &page), "");
        st_ut_eq(ST_OK, check_pages(page, pool.regions_array_data[i], 10, ST_PAGEPOOL_PAGE_ALLOCATED), "");
        st_ut_eq(buf + i * 16 * 4096, pool.regions_array_data[i], "");
    }

    st_ut_eq(10, st_array_current_cnt(&pool.regions), "");

    tmp.compound_page_cnt = 5;
    st_rbtree_node_t *n = st_rbtree_search_eq(&pool.free_pages, &tmp.rbnode);

    st_pagepool_page_t *free_pages = st_owner(n, st_pagepool_page_t, rbnode);
    st_ut_eq(ST_OK, check_pages(free_pages, pool.regions_array_data[0], 5, ST_PAGEPOOL_PAGE_FREE), "");

    int i = 1;
    st_list_for_each_entry(page, &free_pages->lnode, lnode) {
        st_ut_eq(ST_OK, check_pages(page, pool.regions_array_data[i], 5, ST_PAGEPOOL_PAGE_FREE), "");
        i++;
    }

    st_ut_eq(10, i, "");

    free_buf(buf, 655360);
}

st_test(pagepool, free_in_multi_region) {

    st_pagepool_t pool;
    st_pagepool_page_t tmp;
    st_pagepool_page_t *pages[10];
    uint8_t *buf = alloc_buf(655360);

    // in one region 15 pages can use, another one use to store page_t
    init_pagepool(&pool, buf, 655360, 16*4096);

    for (int i = 0; i < 10; i++) {
        st_ut_eq(ST_OK, st_pagepool_alloc_pages(&pool, 10, &pages[i]), "");
    }

    st_ut_eq(10, st_array_current_cnt(&pool.regions), "");

    for (int i = 0; i < 10; i++) {
        st_ut_eq(ST_OK, st_pagepool_free_pages(&pool, pages[i]), "");
        st_ut_eq(9 - i, st_array_current_cnt(&pool.regions), "");

        for (int j = 0; j < 9-i; j++) {
            //check remain regions order
            st_ut_eq(buf + (i+1+j)*16*4096, pool.regions_array_data[j], "");
        }

        tmp.compound_page_cnt = 5;
        st_rbtree_node_t *n = st_rbtree_search_eq(&pool.free_pages, &tmp.rbnode);
        st_pagepool_page_t *free_pages = st_owner(n, st_pagepool_page_t, rbnode);

        if (i < 9) {
            //check next region remain pages is the same pages list head
            st_ut_eq(pages[i+1]+10, free_pages, "");
        } else {
            st_ut_eq(1, st_rbtree_is_empty(&pool.free_pages), "");
        }
    }

    free_buf(buf, 655360);
}


st_test(pagepool, alloc_in_multi_process) {

    int child;
    int pids[10] = {0};
    uint8_t *buf = alloc_buf(655360);
    st_pagepool_t *pool = alloc_buf(sizeof(st_pagepool_t));

    int sem_id = semget(1234, 1, 06666|IPC_CREAT);
    st_ut_ne(-1, sem_id, "");

    union semun sem_args;
    sem_args.val = 0;
    st_ut_ne(-1, semctl(sem_id, 0, SETVAL, sem_args), "");

    // in one region 20 pages can use, another one use to store page_t
    init_pagepool(pool, buf, 655360, 21*4096);

    for (int i = 0; i < 10; i++) {

        child = fork();

        if (child == 0) {
            set_process_to_cpu(i);

            struct sembuf sem = {.sem_num = 0, .sem_op = -1, .sem_flg = SEM_UNDO};
            st_ut_ne(-1, semop(sem_id, &sem, 1), "");

            st_pagepool_page_t *page;
            int ret = st_pagepool_alloc_pages(pool, 2, &page);
            exit(ret);
        }

        pids[i] = child;
    }

    sleep(2);

    struct sembuf sem = {.sem_num = 0, .sem_op = 10, .sem_flg = SEM_UNDO};
    st_ut_ne(-1, semop(sem_id, &sem, 1), "");

    st_ut_eq(ST_OK, wait_children(pids, 10), "");

    st_ut_eq(1, st_array_current_cnt(&pool->regions), "");
    st_ut_eq(1, st_rbtree_is_empty(&pool->free_pages), "");

    semctl(sem_id, 0, IPC_RMID);

    free_buf(buf, 655360);
    free_buf(pool, sizeof(st_pagepool_t));
}

st_test(pagepool, alloc_free_in_multi_process) {

    uint8_t *buf = alloc_buf(65536000);
    st_pagepool_t *pool = alloc_buf(sizeof(st_pagepool_t));

    int pids[50] = {0};
    int child;

    // in one region 15 pages can use, another one use to store page_t
    init_pagepool(pool, buf, 65536000, 16*4096);

    for (int i = 0; i < 50; i++) {

        child = fork();

        if (child == 0) {

            set_process_to_cpu(i);

            int alloc_cnt, ret;
            st_pagepool_page_t* pages[10] = {0};

            for (int j = 0; j < 1000; j++) {
                alloc_cnt = rand() % 14 + 1;

                ret = st_pagepool_alloc_pages(pool, alloc_cnt, &pages[j % 10]);
                if (ret != ST_OK) {
                    exit(ret);
                }

                if ((j+1) % 10 == 0) {
                    for (int k = 0; k < 10; k++) {
                        ret = st_pagepool_free_pages(pool, pages[k]);
                        if (ret != ST_OK) {
                            exit(ret);
                        }
                    }
                }
            }

            exit(ST_OK);
        }
        pids[i] = child;
    }

    st_ut_eq(ST_OK, wait_children(pids, 50), "");

    st_ut_eq(0, st_array_current_cnt(&pool->regions), "");
    st_ut_eq(1, st_rbtree_is_empty(&pool->free_pages), "");

    free_buf(buf, 65536000);
    free_buf(pool, sizeof(st_pagepool_t));
}

st_test(pagepool, page_to_addr) {

    st_pagepool_page_t *pages;
    st_pagepool_t pool;

    ssize_t region_size = 32*4096;
    uint8_t *region_end, *page_addr;

    uint8_t *buf = alloc_buf(655360);

    // in one region 30 pages can use, another two pages use to store page_t
    init_pagepool(&pool, buf, 655360, region_size);

    st_pagepool_alloc_pages(&pool, 30, &pages);

    region_end = pool.regions_array_data[0] + region_size;

    for (int i = 0; i < 30; i++) {
        st_ut_eq(ST_OK, st_pagepool_page_to_addr(&pool, &pages[29-i], &page_addr), "");
        st_ut_eq(region_end - 4096 * (i + 1), page_addr, "");
    }

    st_ut_eq(ST_ARG_INVALID, st_pagepool_page_to_addr(NULL, pages, &page_addr), "");
    st_ut_eq(ST_ARG_INVALID, st_pagepool_page_to_addr(&pool, NULL, &page_addr), "");
    st_ut_eq(ST_ARG_INVALID, st_pagepool_page_to_addr(&pool, pages, NULL), "");

    free_buf(buf, 655360);
}

st_test(pagepool, addr_to_page) {

    st_pagepool_page_t *page, *pages;
    st_pagepool_t pool;

    ssize_t region_size = 32*4096;
    uint8_t *region_end, *page_addr;

    uint8_t *buf = alloc_buf(655360);

    // in one region 30 pages can use, another two pages use to store page_t
    init_pagepool(&pool, buf, 655360, region_size);

    st_pagepool_alloc_pages(&pool, 30, &pages);

    region_end = pool.regions_array_data[0] + region_size;

    for (int i = 0; i < 30; i++) {
        page_addr = region_end - 4096 * (i + 1);

        st_ut_eq(ST_OK, st_pagepool_addr_to_page(&pool, page_addr, &page), "");
        st_ut_eq(&pages[29-i], page, "");
    }

    st_ut_eq(ST_ARG_INVALID, st_pagepool_addr_to_page(NULL, page_addr, &page), "");
    st_ut_eq(ST_ARG_INVALID, st_pagepool_addr_to_page(&pool, NULL, &page), "");
    st_ut_eq(ST_ARG_INVALID, st_pagepool_addr_to_page(&pool, page_addr, NULL), "");

    st_ut_eq(ST_ARG_INVALID, st_pagepool_addr_to_page(&pool, region_end - 5000, &page), "");

    st_ut_eq(ST_NOT_FOUND, st_pagepool_addr_to_page(&pool, pool.regions_array_data[0] - 1, &page), "");
    st_ut_eq(ST_NOT_FOUND, st_pagepool_addr_to_page(&pool, region_end + 4096, &page), "");

    st_ut_eq(ST_OUT_OF_RANGE, st_pagepool_addr_to_page(&pool, region_end - 31*4096, &page), "");

    free_buf(buf, 655360);
}

st_ut_main;

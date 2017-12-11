#ifndef _PAGEPOOL_H_INCLUDED_
#define _PAGEPOOL_H_INCLUDED_

#include <stdint.h>
#include <string.h>

#include "inc/err.h"
#include "inc/log.h"
#include "inc/util.h"
#include "array/array.h"
#include "rbtree/rbtree.h"
#include "list/list.h"
#include "robustlock/robustlock.h"
#include "region/region.h"

#define ST_PAGEPOOL_PAGE_FREE 0
#define ST_PAGEPOOL_PAGE_ALLOCATED 1

#define ST_PAGEPOOL_PAGE_MASTER 0
#define ST_PAGEPOOL_PAGE_SLAVE 1

#define ST_PAGEPOOL_MAX_REGION_CNT 1024
#define ST_PAGEPOOL_MAX_SLAB_CHUNK_CNT 8

typedef struct st_pagepool_page_s st_pagepool_page_t;
typedef struct st_pagepool_s st_pagepool_t;

struct st_pagepool_page_s {

    struct {
        /* store slab chunk state */
        uint64_t bitmap[ST_PAGEPOOL_MAX_SLAB_CHUNK_CNT];
        int32_t obj_size;
        /**
            * set value when slab alloc pages from pagepool.
            * clear value when slab free pages to pagepool.
            *
            * master page: addr of slab group which it belongs to
            * slave pages: addr of master page
            */
        union {
            void *master;
            void *group;
        };
    } slab; /* for slab module use */

    st_rbtree_node_t rbnode; /* pool free_pages tree node
                              * only master use */

    st_list_t lnode; /* same compound_page_cnt pages list
                      * if page in tree, lnode is list's head
                      * else lnode is list's node
                      * only master use */

    uint8_t *region; /* region start address */

    int8_t state; /* page free or allocated */
    int8_t type; /* master or slave page */

    int16_t compound_page_cnt;
};

struct st_pagepool_s {

    /* space for regions array use */
    uint8_t *regions_array_data[ST_PAGEPOOL_MAX_REGION_CNT];
    st_array_t regions; /* array to store region start address */
    pthread_mutex_t regions_lock;

    st_rbtree_t free_pages; /* tree to store free pages */
    pthread_mutex_t pages_lock;

    ssize_t page_size;
    ssize_t region_size;
    ssize_t pages_per_region;
    ssize_t space_base_offset; /* offset from region to page space base */

    st_region_t region_cb;
};

int st_pagepool_init(st_pagepool_t *pool, ssize_t page_size);

int st_pagepool_destroy(st_pagepool_t *pool);

int st_pagepool_alloc_pages(st_pagepool_t *pool, int cnt,
                            st_pagepool_page_t **pages);

int st_pagepool_free_pages(st_pagepool_t *pool, st_pagepool_page_t *pages);

int st_pagepool_page_to_addr(st_pagepool_t *pool, st_pagepool_page_t *page,
                             uint8_t **addr);

int st_pagepool_addr_to_page(st_pagepool_t *pool, uint8_t *addr,
                             st_pagepool_page_t **page);

#endif /* _PAGEPOOL_H_INCLUDED_ */

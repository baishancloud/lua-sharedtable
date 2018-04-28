#ifndef __ST_REGION_H_INCLUDE__
#define __ST_REGION_H_INCLUDE__


#include <stdbool.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <time.h>

#include "inc/err.h"
#include "inc/log.h"
#include "inc/util.h"
#include "robustlock/robustlock.h"


#define ST_REGION_MAX_NUM 1024


typedef enum st_region_state_e {
    ST_REGION_STATE_FREE = 0x01,
    ST_REGION_STATE_BUSY = 0x02,
} st_region_state_t;


/* control block of all shm regions */
typedef struct st_region_s st_region_t;

/* here we may use bitmap to accelate */
struct st_region_s {
    uint8_t           *base_addr;
    ssize_t           page_size;
    int64_t           reg_cnt;
    ssize_t           reg_size;
    st_region_state_t states[ST_REGION_MAX_NUM];
    pthread_mutex_t   lock;
    int               use_lock;
};


/* create a posix-shared-memory based mapped area */
int st_region_shm_create(uint32_t length, void **ret_addr, int *ret_shm_fd);

/* destory posix-shared-memory area */
int st_region_shm_destroy(int shm_fd, void *addr, uint32_t length);

/* initialize shm reg control block */
int st_region_init(st_region_t *rcb,
                   uint8_t     *base_addr,
                   int64_t     pages_per_reg,
                   int64_t     reg_cnt,
                   int         use_lock);

/* destroy shm reg control block */
int st_region_destroy(st_region_t *rcb);

/* alloc a shm region */
int st_region_alloc_reg(st_region_t *rcb, uint8_t **ret_addr);

/* release rss pages of a shm region */
int st_region_free_reg(st_region_t *rcb, void *addr);

/* get region base addr by index which starts from 0 */
uint8_t *st_region_base_addr_by_idx(const st_region_t *rcb, int idx);


#endif

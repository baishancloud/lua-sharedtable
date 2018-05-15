#define _BSD_SOURCE
/**
 * region.c
 *
 * this module is used to create shared memory regions according to given
 * factor region page number and number of regions.
 */
#include "region.h"


#define ST_REGION_MMAP_PROT       (PROT_READ | PROT_WRITE)
#define ST_REGION_MMAP_FLAGS      MAP_SHARED;
#define ST_REGION_SHM_OBJ_MODE    0660

#define st_region_lock(rcb) do {                                              \
    st_typeof(rcb) _rcb = rcb;                                                \
    if (_rcb->use_lock) {                                                     \
        st_robustlock_lock(&_rcb->lock);                                      \
    }                                                                         \
} while(0)

#define st_region_unlock(rcb) do {                                            \
    st_typeof(rcb) _rcb = rcb;                                                \
    if (_rcb->use_lock) {                                                     \
        st_robustlock_unlock(&_rcb->lock);                                    \
    }                                                                         \
} while(0)


static inline int
is_reg_state_free(const st_region_state_t state)
{
    return (state == ST_REGION_STATE_FREE);
}

static inline int
is_reg_state_busy(const st_region_state_t state)
{
    return (state == ST_REGION_STATE_BUSY);
}

int
st_region_shm_memcpy(const char *shm_fn, void *dst, ssize_t length)
{
    st_assert_nonull(shm_fn);
    st_assert_nonull(dst);
    st_assert(length > 0);


    int shm_fd = shm_open(shm_fn, O_RDONLY, ST_REGION_SHM_OBJ_MODE);
    if (shm_fd == -1) {
        derrno("failed to shm_open");

        return errno;
    }

    int ret = ST_OK;

    void *addr = mmap(NULL, length, PROT_READ, MAP_SHARED, shm_fd, 0);
    if (addr == MAP_FAILED) {
        derrno("failed to mmap");

        ret = errno;
        goto quit;
    }

    memcpy(dst, addr, length);

    ret = munmap(addr, length);
    if (ret == -1) {
        derrno("failed to munmap");

        ret = errno;
        goto quit;
    }

    ret = ST_OK;

quit:
    close(shm_fd);

    return ret;
}

int
st_region_shm_mmap(int shm_fd, ssize_t length, void **ret_addr)
{
    int prot  = ST_REGION_MMAP_PROT;
    int flags = ST_REGION_MMAP_FLAGS;

    void *addr = mmap(NULL, length, prot, flags, shm_fd, 0);
    if (addr == MAP_FAILED) {
        derrno("failed to mmap");

        return errno;
    }

    *ret_addr = addr;

    return ST_OK;
}

int
st_region_shm_create(const char *shm_fn,
                     uint32_t length,
                     void **ret_addr,
                     int *ret_shm_fd)
{
    st_must(shm_fn != NULL, ST_ARG_INVALID);
    st_must(ret_addr != NULL, ST_ARG_INVALID);
    st_must(ret_shm_fd != NULL, ST_ARG_INVALID);

    int ret = -1;
    void *base_addr = MAP_FAILED;

    int shm_fd = shm_open(shm_fn,
                          O_CREAT | O_RDWR | O_TRUNC,
                          ST_REGION_SHM_OBJ_MODE);
    if (shm_fd == -1) {
        derrno("failed to shm_open");

        return errno;
    }

    ret = ftruncate(shm_fd, length);
    if (ret != 0) {
        derrno("failed to ftruncate");

        ret = errno;
        goto err_quit;
    }

    ret = st_region_shm_mmap(shm_fd, length, &base_addr);
    if (ret != ST_OK) {
        goto err_quit;
    }

    int flag = fcntl(shm_fd, F_GETFD);
    if (flag == -1) {
        derrno("failed to get shm fd flag");

        goto err_quit;
    }

    if (fcntl(shm_fd, F_SETFD, (flag &~ FD_CLOEXEC)) == -1) {
        derrno("failed to set shm fd flag");

        goto err_quit;
    }

    *ret_addr   = base_addr;
    *ret_shm_fd = shm_fd;

    return ST_OK;

err_quit:
    if (base_addr != MAP_FAILED) {
        munmap(base_addr, length);
    }

    close(shm_fd);

    if (shm_unlink(shm_fn) != 0) {
        derrno("failed to shm_unlink in st_region_shm_create");
    }

    return ret;
}

int
st_region_shm_destroy(int shm_fd,
                      const char *shm_fn,
                      void *addr,
                      uint32_t length)
{
    st_must(shm_fd > 0, ST_ARG_INVALID);
    st_must(shm_fn != NULL, ST_ARG_INVALID);
    st_must(addr != NULL, ST_ARG_INVALID);
    st_must(length > 0, ST_ARG_INVALID);

    int ret = ST_OK;

    if (shm_unlink(shm_fn) != 0) {
        derrno("failed to unlink shm: %d, %p, %d", shm_fd, addr, length);

        ret = errno;
    }

    if (munmap(addr, length) != 0) {
        derrno("failed to munmap: %d, %p, %d", shm_fd, addr, length);

        ret = (ret == ST_OK ? errno : ret);
    }

    if (close(shm_fd) != 0) {
        derrno("failed to close shm_fd: %d, %p, %d", shm_fd, addr, length);

        /** return the first errno */
        ret = (ret == ST_OK ? errno : ret);
    }

    return ret;
}

static ssize_t
get_page_size(void)
{
    return sysconf(_SC_PAGESIZE);
}

int
st_region_init(st_region_t *rcb, uint8_t *base_addr,
               int64_t pages_per_reg, int64_t reg_cnt, int use_lock)
{
    st_must(rcb != NULL, ST_ARG_INVALID);
    st_must(base_addr != NULL, ST_ARG_INVALID);
    st_must(reg_cnt > 0, ST_ARG_INVALID);
    st_must(reg_cnt <= ST_REGION_MAX_NUM, ST_ARG_INVALID);

    rcb->reg_cnt       = reg_cnt;
    rcb->base_addr     = base_addr;
    rcb->page_size     = get_page_size();
    rcb->reg_size      = rcb->page_size * pages_per_reg;
    rcb->use_lock      = !!use_lock; /** non-zero is 1 */

    memset(rcb->states, 0, sizeof(rcb->states));

    for (int idx = 0; idx < reg_cnt; idx++) {
        rcb->states[idx] = ST_REGION_STATE_FREE;
    }

    if (use_lock) {
        int ret = st_robustlock_init(&rcb->lock);
        if (ret != 0) {
            derrno("failed to init lock: %d", ret);

            return ret;
        }
    }

    return ST_OK;
}

int
st_region_destroy(st_region_t *rcb)
{
    st_must(rcb != NULL, ST_ARG_INVALID);

    if (rcb->use_lock) {
        int ret = st_robustlock_destroy(&rcb->lock);

        if (ret != 0) {
            derrno("failed to destroy lock: %d", ret);

            return ret;
        }

        rcb->use_lock = 0;
    }

    return ST_OK;
}

uint8_t *
st_region_base_addr_by_idx(const st_region_t *rcb, int idx)
{
    st_must(rcb != NULL, NULL);
    st_must(idx >= 0, NULL);
    st_must(idx < rcb->reg_cnt, NULL);

    return (uint8_t *)((uintptr_t)rcb->base_addr + idx * rcb->reg_size);
}

int
st_region_alloc_reg(st_region_t *rcb, uint8_t **ret_addr)
{
    st_must(rcb != NULL, ST_ARG_INVALID);
    st_must(ret_addr != NULL, ST_ARG_INVALID);

    int ret = -1;

    st_region_lock(rcb);

    for (int idx = 0; idx < rcb->reg_cnt; idx++) {
        if (is_reg_state_free(rcb->states[idx])) {
            *ret_addr        = st_region_base_addr_by_idx(rcb, idx);
            rcb->states[idx] = ST_REGION_STATE_BUSY;

            ret = ST_OK;
            goto quit;
        }
    }

    ret = ST_OUT_OF_MEMORY;
quit:
    st_region_unlock(rcb);

    return ret;
}

int
st_region_free_reg(st_region_t *rcb, void *addr)
{
    st_must(rcb != NULL, ST_ARG_INVALID);
    st_must(addr != NULL, ST_ARG_INVALID);

    int ret = ST_OK;

    st_region_lock(rcb);

    if ((uint8_t *)addr < rcb->base_addr) {
        derr("invalid addr: less than base_addr %p", addr);

        ret = ST_ARG_INVALID;
        goto quit;
    }

    int idx = ((uintptr_t)addr - (uintptr_t)rcb->base_addr) / rcb->reg_size;
    if (idx >= rcb->reg_cnt) {
        derr("invalid addr: too big %p", addr);

        ret = ST_OUT_OF_RANGE;
        goto quit;
    }

    uint8_t *reg_addr = st_region_base_addr_by_idx(rcb, idx);

    if (reg_addr != (uint8_t *)addr) {
        derr("invalid addr: not region start addr %p", addr);

        ret = ST_ARG_INVALID;
        goto quit;
    }

    if (is_reg_state_free(rcb->states[idx])) {
        ret = ST_OK;
        goto quit;
    }

    if (madvise(reg_addr, rcb->reg_size, MADV_REMOVE) != 0) {
        derrno("failed to release shm region: %d, %p", idx, addr);

        ret = errno;
        goto quit;
    }

    rcb->states[idx] = ST_REGION_STATE_FREE;

quit:
    st_region_unlock(rcb);

    return ret;
}

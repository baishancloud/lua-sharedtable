#define _BSD_SOURCE
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/eventfd.h>
#include <linux/limits.h>

#include "region.h"
#include "unittest/unittest.h"


#define CONFIG_REGION               10
#define PAGES_PER_REGION            1024
#define REGION_NUM                  10

static char st_test_shm_fname[NAME_MAX];

/** check if fd represent file described by fpath */
static void
check_fd_file_path(int fd)
{
    char proc_fd_path[PATH_MAX];
    memset(proc_fd_path, 0, PATH_MAX);

    snprintf(proc_fd_path, PATH_MAX, "/proc/self/fd/%d", fd);

    char *real_path = realpath(proc_fd_path, NULL);
    st_ut_ne(NULL, real_path, "real_path is NULL, %s", strerror(errno));

    strncpy(st_test_shm_fname, real_path, strlen(real_path));
    free(real_path);

    pid_t pid;
    char *suffix;
    sscanf(st_test_shm_fname, "/dev/shm/%*d_%*d_%d_%ms", &pid, &suffix);

    st_ut_eq(getpid(), pid, "pid part not right");
    st_ut_eq(0, strncmp(suffix, "st_shm_area", 11), "suffix part not right");
    free(suffix);
}

static void
test_st_region_shm_create(int *shm_fd, uint8_t **addr, int length)
{
    *shm_fd = -1;
    int ret = st_region_shm_create(length, (void **)addr, shm_fd);

    st_ut_eq(ST_OK, ret, "st_region_shm_create failed");
    st_ut_ne(MAP_FAILED, addr, "st_region_shm_create set addr wrong");

    check_fd_file_path(*shm_fd);
}

static void
test_st_region_shm_destroy(int shm_fd, uint8_t *addr, int length)
{
    int ret = st_region_shm_destroy(shm_fd, addr, length);

    st_ut_eq(ST_OK, ret, "st_region_shm_destroy failed");

    struct stat buf;
    ret = stat(st_test_shm_fname, &buf);
    st_ut_eq(-1, ret, "shm object still exist");
    st_ut_eq(ENOENT, errno, "shm object not deleted");
}

st_test(st_region, shm_create_destroy)
{
    int shm_fd     = 0;
    ssize_t length = PAGES_PER_REGION * REGION_NUM * st_page_size();
    uint8_t *addr  = NULL;

    test_st_region_shm_create(&shm_fd, &addr, length);

    test_st_region_shm_destroy(shm_fd, addr, length);
}

st_test(st_region, init_alloc_release)
{
    int shm_fd        = 0;
    uint8_t *addr     = NULL;
    ssize_t page_size = st_page_size();
    ssize_t reg_size  = PAGES_PER_REGION * page_size;
    ssize_t data_len  = reg_size * REGION_NUM;
    ssize_t cfg_len   = CONFIG_REGION * page_size;
    ssize_t length    = data_len + cfg_len;

    /** create shm area */
    test_st_region_shm_create(&shm_fd, &addr, length);

    st_region_t *rcb       = (st_region_t *)addr;
    uint8_t *data_base_ptr = (uint8_t *)((uintptr_t)addr + cfg_len);

    /** initialize shm region control block and check each fields */
    int ret = st_region_init(rcb,
                            data_base_ptr,
                            PAGES_PER_REGION,
                            REGION_NUM,
                            0);
    st_ut_eq(ST_OK, ret, "st_region_init: return value is wrong");

    st_ut_eq(REGION_NUM, rcb->reg_cnt, "reg_cnt not right");
    st_ut_eq(data_base_ptr, rcb->base_addr, "base_addr is right");
    st_ut_eq(page_size, rcb->page_size, "page_size not right");
    st_ut_eq(reg_size, rcb->reg_size, "reg_size not right");

    /** check each reg init state */
    for (int idx = 0; idx < REGION_NUM; idx++) {
        st_ut_eq(ST_REGION_STATE_FREE, rcb->states[idx], "reg state not right");
    }

    /** alloc regions by order and check region state */
    uint8_t *ret_addr = NULL;
    uint8_t *reg_addr = NULL;
    for (int idx = 0; idx < REGION_NUM; idx++) {
        reg_addr = st_region_base_addr_by_idx(rcb, idx);

        ret = st_region_alloc_reg(rcb, &ret_addr);

        st_ut_eq(ST_OK, ret, "alloc return value not right");
        st_ut_eq(reg_addr, ret_addr, "alloc addr not right");
        st_ut_eq(ST_REGION_STATE_BUSY,
                 rcb->states[idx],
                 "alloc reg state not right");
    }

    /** no more region to alloc */
    st_ut_eq(ST_OUT_OF_MEMORY,
             st_region_alloc_reg(rcb, &ret_addr),
             "alloc return value not right");

    /** free region by index and check state */
    int idx  = REGION_NUM / 2;
    reg_addr = st_region_base_addr_by_idx(rcb, idx);

    ret = st_region_free_reg(rcb, reg_addr);
    st_ut_eq(ST_OK, ret, "failed to release region");
    st_ut_eq(ST_REGION_STATE_FREE,
             rcb->states[idx],
             "release region state not right");

    ret = st_region_destroy(rcb);
    st_ut_eq(ST_OK, ret, "failed to destroy region control block");

    test_st_region_shm_destroy(shm_fd, addr, length);
}

static uint32_t
get_rss_page_num(void)
{
    /**
     * data fields in statm on proc
     *
     * VmSize    rss
     * 26978     90   72 11 0 79 0
     *
     * unit is page
     */
    const char *fpath = "/proc/self/statm";

    int fd = open(fpath, O_RDONLY);
    if (fd == -1) {
        return 0;
    }

    char line[1024];
    int ret = read(fd, line, sizeof(line));
    if (-ret == -1) {
        return 0;
    }

    char *ptr = strchr(line, ' ');
    if (ptr == NULL) {
        return 0;
    }
    ptr++;

    return (uint32_t)strtoull(ptr, NULL, 10);
}

static void
test_st_shm_alloc_all_regions(st_region_t *rcb)
{
    for (int idx = 0; idx < rcb->reg_cnt; idx++) {
        uint8_t *ret_addr = NULL;

        st_region_alloc_reg(rcb, &ret_addr);
    }
}

static uint32_t
force_load_pages(const st_region_t *rcb)
{
    ssize_t page_size = st_page_size();
    for (int idx = 0; idx < rcb->reg_cnt; idx++) {
        uint8_t *base = st_region_base_addr_by_idx(rcb, idx);

        for (int cnt = 0; cnt < PAGES_PER_REGION; cnt++) {
            uint8_t *ptr = base + cnt * page_size;

            ptr[0] = 1;
        }
    }

    return get_rss_page_num();
}

static void
notify_event(int notify_fd)
 {
    uint64_t event = 1;

    write(notify_fd, &event, sizeof(event));
 }


static void
wait_event(int wait_fd)
{
    uint64_t event = 0;

    read(wait_fd, &event, sizeof(event));
}

/** factor can be 0.2 which means [1-0.2, 1+0.2] is valid range */
static void
check_rss_page_change(uint32_t base, uint32_t value, float factor)
{
    int ceil  = base * (1 + factor);
    int floor = base * (1 - factor);

    st_ut_ge(ceil, value, "rss release too many pages");
    st_ut_le(floor, value, "rss release too few pages");
}

/** if this case failed, you may need to kill children process manually */
static void
test_children_behavior(st_region_t *rcb)
{
    int event_fd[2];

    event_fd[0] = eventfd(0, EFD_SEMAPHORE);
    event_fd[1] = eventfd(0, EFD_SEMAPHORE);

    pid_t children[2];

    pid_t pid = fork();
    st_ut_ne(-1, pid, "fork first child failed");

    if (pid == 0) {
        /** first child as notifier who release region one by one */
        int waiter   = event_fd[0]; /** used to wait for brother to notify */
        int notifier = event_fd[1]; /** used to notify brother */

        /** #1: alloc all regions */
        test_st_shm_alloc_all_regions(rcb);

        /** #2: load rss pages and sync with brother process */
        uint32_t last_pg_num = force_load_pages(rcb);
        notify_event(notifier);
        wait_event(waiter);
        /** brother finish forcing load pages too */

        /** #3: release, check rss and notify */
        for (int reg_idx = 0; reg_idx < rcb->reg_cnt; reg_idx++)  {

            /** #3.1: release a region */
            st_region_free_reg(rcb, st_region_base_addr_by_idx(rcb, reg_idx));

            /** #3.2: check rss */
            uint32_t cur_pg_num = get_rss_page_num();
            check_rss_page_change(PAGES_PER_REGION,
                                  last_pg_num - cur_pg_num,
                                  0.2);
            last_pg_num = cur_pg_num;

            /** #3.3: notify brother that release region is finished */
            notify_event(notifier);
            /** #3.4: wait for brother to finish his round */
            wait_event(waiter);
        }

        /** make sure can access the value set by brother */
        wait_event(waiter); /** wait for brother to set value  */
        uint8_t *value = st_region_base_addr_by_idx(rcb, 0);
        st_ut_eq(123, *value, "failed to pass value via shm");

    } else {
        children[0]  = pid;

        pid = fork();
        st_ut_ne(-1, pid, "fork second child failed");
        children[1] = pid;

        if (pid == 0) {
            int waiter   = event_fd[1]; /** used to wait brother to notify */
            int notifier = event_fd[0]; /** used to notify brother */

            /** #1: alloc all regions */
            test_st_shm_alloc_all_regions(rcb);

            /** #2: load pages and sync with brother */
            uint32_t last_pg_num = force_load_pages(rcb);
            wait_event(waiter);
            notify_event(notifier);
            /** brother finish forcing load pages too */

            /** #3: wait, check rss, release again, check rss again */
            for (int reg_idx = 0; reg_idx < rcb->reg_cnt; reg_idx++) {
                /** #3.1: wait for brother to finish release operation */
                wait_event(waiter);

                /** #3.2: check rss and make sure rss page decreased */
                uint32_t cur_pg_num = get_rss_page_num();
                check_rss_page_change(PAGES_PER_REGION,
                                      last_pg_num - cur_pg_num,
                                      0.2);

                /** #3.3: release the region which just released by brother */
                st_region_free_reg(rcb,
                                   st_region_base_addr_by_idx(rcb, reg_idx));

                /** #3.4: make sure rss almost the same */
                check_rss_page_change(PAGES_PER_REGION,
                                      last_pg_num - cur_pg_num,
                                      0.2);
                last_pg_num = cur_pg_num;

                /** #3.5: notify brother to go on */
                notify_event(notifier);
            }

            /** set value in a already released region */
            uint8_t *val_ptr = st_region_base_addr_by_idx(rcb, 0);
            *val_ptr = 123;
            notify_event(notifier); /** notify brother to check value */
        }
    }

    close(event_fd[1]);
    close(event_fd[0]);

    if (pid == 0) {
        /** children said: goodbye */
        exit(0);
    } else {
        waitpid(children[0], NULL, 0);
        waitpid(children[1], NULL, 0);
    }
}

static void
test_st_region_op_multi_proc(uint32_t lock)
{
    uint32_t rss_num = get_rss_page_num();
    st_ut_ne(0, rss_num, "bug in get_rss_page_num");

    /** create and initialize shm regions */
    int shm_fd        = 0;
    uint8_t *addr     = NULL;
    ssize_t page_size = st_page_size();
    ssize_t reg_size  = PAGES_PER_REGION * page_size;
    ssize_t data_len  = reg_size * REGION_NUM;
    ssize_t cfg_len   = CONFIG_REGION * page_size;
    ssize_t length    = data_len + cfg_len;

    test_st_region_shm_create(&shm_fd, &addr, length);

    st_region_t *rcb       = (st_region_t *)addr;
    uint8_t *data_base_ptr = (uint8_t *)((uintptr_t)addr + cfg_len);

    st_region_init(rcb, data_base_ptr, PAGES_PER_REGION, REGION_NUM, lock);

    test_children_behavior(rcb);

    int ret = st_region_destroy(rcb);
    st_ut_eq(ST_OK, ret, "failed to destroy region control block");

    test_st_region_shm_destroy(shm_fd, addr, length);
}

st_test(st_region, op_multiple_processes)
{
    test_st_region_op_multi_proc(0);
}

st_test(st_region, op_multiple_processes_use_lock)
{
    test_st_region_op_multi_proc(1);
}

st_ut_main;

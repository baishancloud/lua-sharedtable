#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <limits.h>
#include <linux/limits.h>

#include "capi.h"
#include "unittest/unittest.h"


#define ST_CAPI_TEST_PROCS_CNT 10


typedef int (*st_capi_do_test) (void);


st_test(st_capi, make_parse_tvalue)
{
    /** int */
    int int_val = 10;
    int tlen = sizeof(int);
    st_tvalue_t int_tvalue;

    int type = st_capi_get_type(int_val);
    st_ut_eq(ST_TYPES_INTEGER, type, "failed to get int type");

    int ret = st_capi_make_tvalue(int_tvalue, int_val);
    st_ut_eq(ST_OK, ret, "failed to init tvalue from int");

    st_ut_eq(ST_TYPES_INTEGER, int_tvalue.type, "wrong int tvalue type");
    st_ut_eq(tlen, int_tvalue.len, "wrong int tvalue len");
    st_ut_eq(&int_val, int_tvalue.bytes, "wrong int tvalue bytes");
    st_ut_eq(int_val, *((int *)int_tvalue.bytes), "wrong value in int tvalue");

    /** pid */
    pid_t pid_val = getpid();
    tlen = sizeof(pid_val);
    st_tvalue_t pid_tvalue;

    type = st_capi_get_type(pid_val);
    st_ut_eq(ST_TYPES_INTEGER, type, "failed to get pid_t type");

    ret = st_capi_make_tvalue(pid_tvalue, pid_val);
    st_ut_eq(ST_OK, ret, "failed to init tvalue from pid_t");

    st_ut_eq(ST_TYPES_INTEGER, pid_tvalue.type, "wrong pid tvalue type");
    st_ut_eq(tlen, pid_tvalue.len, "wrong pid tvalue len");
    st_ut_eq(&pid_val, pid_tvalue.bytes, "wrong pid tvalue bytes");
    st_ut_eq(pid_val, *((int *)pid_tvalue.bytes), "wrong value in pid tvalue");

    /** double */
    double double_val = 10.0;
    st_tvalue_t double_tvalue;
    tlen = sizeof(double);

    type = st_capi_get_type(double_val);
    st_ut_eq(ST_TYPES_NUMBER, type, "failed to get double type");

    ret = st_capi_make_tvalue(double_tvalue, double_val);
    st_ut_eq(ST_OK, ret, "failed to init tvalue from double");

    st_ut_eq(ST_TYPES_NUMBER, double_tvalue.type, "wrong double tvalue type");
    st_ut_eq(tlen, double_tvalue.len, "wrong double tvalue len");
    st_ut_eq(&double_val, double_tvalue.bytes, "wrong double tvalue bytes");
    st_ut_eq(double_val,
             *((double *)double_tvalue.bytes),
             "wrong value in double tvalue");

    /** st_bool */
    int bool_tlen = sizeof(st_bool);
    st_bool bool_val = 1;
    st_tvalue_t bool_tvalue;

    type = st_capi_get_type(bool_val);
    st_ut_eq(ST_TYPES_BOOLEAN, type, "failed to get bool type");

    ret = st_capi_make_tvalue(bool_tvalue, bool_val);
    st_ut_eq(ST_OK, ret, "failed to init tvalue from bool");

    st_ut_eq(ST_TYPES_BOOLEAN, bool_tvalue.type, "wrong bool tvalue type");
    st_ut_eq(bool_tlen, bool_tvalue.len, "wrong bool tvalue len");
    st_ut_eq(&bool_val, bool_tvalue.bytes, "wrong bool tvalue bytes");
    st_ut_eq(bool_val,
             *((st_bool *)bool_tvalue.bytes),
             "wrong value in bool tvalue");

    /** char* aka. string */
    char *str_val = "hello world";
    int str_tlen = strlen(str_val) + 1;
    st_tvalue_t str_tvalue;

    type = st_capi_get_type(str_val);
    st_ut_eq(ST_TYPES_STRING, type, "failed to get string type");

    ret = st_capi_make_tvalue(str_tvalue, str_val);
    st_ut_eq(ST_OK, ret, "failed to init tvalue from str");

    st_ut_eq(ST_TYPES_STRING, str_tvalue.type, "wrong string tvalue type");
    st_ut_eq(str_tlen, str_tvalue.len, "wrong string tvalue len");
    st_ut_eq(str_val, str_tvalue.bytes, "wrong string tvalue bytes");
    st_ut_eq(0,
             strcmp(str_val, (char *)str_tvalue.bytes),
             "wrong value in string tvalue");

    /** char arry */
    char *content = "carray type test";
    int carray_len = strlen(content) + 1;
    char carray[100];
    strncpy(carray, content, carray_len);

    type = st_capi_get_type((char *)carray);
    st_ut_eq(ST_TYPES_STRING, type, "failed to get char array type");

    char *carray_ptr = (char *)carray;
    st_tvalue_t carray_tvalue;
    ret = st_capi_make_tvalue(carray_tvalue, carray_ptr);
    st_ut_eq(ST_OK, ret, "failed to init tvalue from char array");

    st_ut_eq(ST_TYPES_STRING, carray_tvalue.type, "wrong char array type");
    st_ut_eq(carray_len, carray_tvalue.len, "wrong char array len");
    st_ut_eq((intptr_t)carray,
             (intptr_t)carray_tvalue.bytes,
             "wrong char array bytes");
    st_ut_eq(0,
             strcmp(carray, (char *)carray_tvalue.bytes),
             "wrong value in char array tvalue");

    /** st_table_t* */
    st_table_t table = (st_table_t) { .element_cnt = 123320, };
    st_table_t *table_val = &table;
    int table_tlen = sizeof(st_table_t *);
    st_tvalue_t table_tvalue;

    type = st_capi_get_type(table_val);
    st_ut_eq(ST_TYPES_TABLE, type, "failed to get table type");

    ret = st_capi_make_tvalue(table_tvalue, table_val);
    st_ut_eq(ST_OK, ret, "failed to init tvalue from table");

    st_ut_eq(ST_TYPES_TABLE, table_tvalue.type, "wrong table tvalue type");
    st_ut_eq(table_tlen, table_tvalue.len, "wrong table tvalue len");
    st_ut_eq(&table_val, table_tvalue.bytes, "wrong table tvalue bytes");
    st_ut_eq(table.element_cnt,
             (*((st_table_t **)table_tvalue.bytes))->element_cnt,
             "wrong value in table tvalue");

    /** uint64_t for uintptr_t */
    uintptr_t uptr_val = 0x1234;
    uint64_t u64_val = (uint64_t)uptr_val;
    st_tvalue_t u64_tvalue;
    tlen = sizeof(uint64_t);

    type = st_capi_get_type(u64_val);
    st_ut_eq(ST_TYPES_U64, type, "failed to get u64 type");

    ret = st_capi_make_tvalue(u64_tvalue, u64_val);
    st_ut_eq(ST_OK, ret, "failed to init tvalue from u64");

    st_ut_eq(ST_TYPES_U64, u64_tvalue.type, "wrong u64 tvalue type");
    st_ut_eq(tlen, u64_tvalue.len, "wrong u64 tvalue len");
    st_ut_eq(&u64_val, u64_tvalue.bytes, "wrong u64 tvalue bytes");
    st_ut_eq(u64_val,
             *((uint64_t *)u64_tvalue.bytes),
             "wrong value in u64 tvalue");
}


st_test(st_capi, module_init_destroy)
{
    st_capi_process_t *pstate = st_capi_get_process_state();
    st_ut_eq(NULL, pstate, "process state should not be inited");

    /** check st_capi_init */
    int ret = st_capi_init();
    st_ut_eq(ST_OK, ret, "failed to init module");
    pstate = st_capi_get_process_state();

    /** check process state */
    st_capi_t *lstate = pstate->lib_state;
    st_ut_ne(NULL, lstate, "lib state must not be NULL");
    st_ut_eq(1, pstate->inited, "process state inited not right");
    st_ut_eq(getpid(), pstate->pid, "pid in process state not right");
    st_ut_eq(&pstate->node, st_list_first(&lstate->p_roots), "proot not set");

    /** check library state */
    uintptr_t meta_size = (uintptr_t)lstate->data - (uintptr_t)lstate->base;
    st_ut_eq(ST_REGION_CNT * ST_REGION_SIZE,
             (uintptr_t)lstate->len - meta_size,
             "len not right");
    st_ut_eq(lstate->base, lstate, "lib state addr must be the same as base");
    st_ut_ne(NULL, lstate->g_root, "g_root must not be NULL");
    st_ut_eq(1, st_list_is_inited(&lstate->p_roots), "p_roots not inited");
    st_ut_eq(ST_CAPI_INIT_DONE, lstate->init_state, "state not right");

    /** check st_capi_destroy */
    for (int s = ST_CAPI_INIT_DONE; s >= ST_CAPI_INIT_NONE; s--) {
        pstate = st_capi_get_process_state();
        int fd = pstate->lib_state->shm_fd;

        pstate->inited = s;

        ret = st_capi_destroy();
        st_ut_eq(ST_OK, ret, "failed to destroy module");

        /** check process state */
        st_ut_eq(NULL, st_capi_get_process_state(), "process state not NULL");

        /** check share memory fd */
        struct stat buf;
        char proc_shm_fd_path[PATH_MAX];
        snprintf(proc_shm_fd_path, PATH_MAX, "/proc/self/fd/%d", fd);

        ret = stat(proc_shm_fd_path, &buf);
        st_ut_eq(-1, ret, "shm fd still exist");
        st_ut_eq(ENOENT, errno, "shm fd not closed");

        ret = st_capi_init();
        st_ut_eq(ret, ST_OK, "failed to init module");
    }

    ret = st_capi_destroy();
    st_ut_eq(ST_OK, ret, "failed to destroy module");
}


static void
st_capi_prepare_ut(void)
{
    st_assert(st_capi_init() == ST_OK);
}


static void
st_capi_tear_down_ut(void)
{
    st_assert(st_capi_destroy() == ST_OK);
}

static void
st_capi_check_proot(st_tvalue_t *value)
{
    st_tvalue_t proot_tbl_key;
    st_tvalue_t proot_tbl_value;
    st_capi_process_t *pstate = st_capi_get_process_state();

    uintptr_t key = (uintptr_t)value->bytes;
    st_capi_make_tvalue(proot_tbl_key, key);

    int ret = st_table_get_value(pstate->root, proot_tbl_key, &proot_tbl_value);
    st_ut_eq(ST_OK, ret, "failed to set table elem ref in proot: %d", ret);
    st_ut_eq(*((st_table_t **)value->bytes),
             *((st_table_t **)proot_tbl_value.bytes),
             "table addr in proot is wrong");
}


static int
st_capi_test_fork_wrapper(st_capi_do_test process_cb)
{
    pid_t workers[ST_CAPI_TEST_PROCS_CNT] = {
        [0 ... ST_CAPI_TEST_PROCS_CNT-1] = -1
    };

    st_capi_process_t *pstate = st_capi_get_process_state();

    for (int id = 0; id < ST_CAPI_TEST_PROCS_CNT; id++) {
        workers[id] = fork();

        if (workers[id] == -1) {
            derr("failed to fork child: %d, %s\n", id, strerror(errno));
            st_ut_ne(-1, workers[id], "failed to fork worker process");

        }
        else if (workers[id] == 0) {
            int ret = st_capi_worker_init();
            st_ut_eq(ST_OK, ret, "failed to init module worker: %d", ret);

            ret = process_cb();

            exit(ret);
        }
    }

    int all_passed = 1;
    for (int id = 0; id < ST_CAPI_TEST_PROCS_CNT; id++) {
        int status = ST_OK;
        waitpid(workers[id], &status, 0);
        workers[id] = -1;

        if (all_passed == 1 && status != ST_OK) {
            all_passed = 0;
        }
    }

    int cnt = 0;
    st_list_t *node;
    st_list_for_each(node, &pstate->lib_state->p_roots) {
        cnt ++;
    }
    st_ut_eq(ST_CAPI_TEST_PROCS_CNT + 1, cnt, "proots count not right");

    st_ut_eq(1, all_passed, "worker process failed");

    return ST_OK;
}


st_test(st_capi, set_add_get_remove_key)
{
    st_capi_prepare_ut();

    /**
     * test for each supported type
     * (1) set    ST_OK
     * (2) get    ST_OK
     * (3) add    ST_EXISTED
     * (4) remove ST_OK
     * (5) add    ST_OK
     * (6) remove ST_OK
     * (7) get    ST_NOT_FOUND
     */
    int
    st_capi_test_set_get_rm_key(st_capi_process_t *pstate)
    {
        st_table_t *root = NULL;
        st_table_new(&pstate->lib_state->table_pool, &root);

        int count               = 0;
        uint64_t len            = 0;
        st_tvalue_t check_value = st_str_null;

        /** int */
        int int_key = 10;
        int int_val = 100;
        int ret = st_capi_set(root, int_key, int_val);
        st_ut_eq(ST_OK, ret, "failed to set int key value");
        st_ut_eq(++count, root->element_cnt, "wrong element count");

        ret = st_capi_get(root, int_key, &check_value);
        st_ut_eq(ST_OK, ret, "failed to get int key value");

        len = sizeof(int_val);
        st_ut_eq(ST_TYPES_INTEGER, check_value.type, "wrong int value type");
        st_ut_eq(len, check_value.len, "wrong len of int value");
        st_ut_eq(int_val,
                 *((int *)check_value.bytes),
                 "wrong value of int value");

        ret = st_capi_add(root, int_key, int_val);
        st_ut_eq(ST_EXISTED, ret, "add duplicated int key");

        ret = st_capi_remove_key(root, int_key);
        st_ut_eq(ST_OK, ret, "failed to remove int key value");
        st_ut_eq(--count, root->element_cnt, "wrong element count by remove");

        ret = st_capi_add(root, int_key, int_val);
        st_ut_eq(ST_OK, ret, "failed to add key value");
        st_ut_eq(++count, root->element_cnt, "wrong element count");

        ret = st_capi_remove_key(root, int_key);
        st_ut_eq(ST_OK, ret, "failed to remove int key value");
        st_ut_eq(--count, root->element_cnt, "wrong element count by remove");

        /** suppose to fail, check_value should be untouched */
        uintptr_t bytes_addr = (uintptr_t)check_value.bytes;
        ret = st_capi_get(root, int_key, &check_value);
        st_ut_eq(ST_NOT_FOUND, ret, "failed to remove int key exists");
        st_ut_eq(bytes_addr,
                 (uintptr_t)check_value.bytes,
                 "get must not touch bytes in failure");

        st_capi_free(&check_value);

        /** char* */
        char *str_key = "how are you?";
        char *str_val = "fine, thank you and you?";
        check_value   = (st_tvalue_t)st_str_null;

        ret = st_capi_set(root, str_key, str_val);
        st_ut_eq(ST_OK, ret, "failed to set string key value");
        st_ut_eq(++count, root->element_cnt, "wrong element count");

        ret = st_capi_get(root, str_key, &check_value);
        st_ut_eq(ST_OK, ret, "failed to get str key value");

        len = strlen(str_val) + 1;
        st_ut_eq(ST_TYPES_STRING, check_value.type, "wrong str value type");
        st_ut_eq(len, check_value.len, "wrong len of str value");
        st_ut_eq(0,
                 strcmp(str_val, (char *)check_value.bytes),
                 "wrong value of str value");

        ret = st_capi_add(root, str_key, str_val);
        st_ut_eq(ST_EXISTED, ret, "add duplicated char * key");

        ret = st_capi_remove_key(root, str_key);
        st_ut_eq(ST_OK, ret, "failed to remove str key value");
        st_ut_eq(--count, root->element_cnt, "wrong element count by remove");

        ret = st_capi_add(root, str_key, str_val);
        st_ut_eq(ST_OK, ret, "failed to add str key value");
        st_ut_eq(++count, root->element_cnt, "wrong element count by remove");

        ret = st_capi_remove_key(root, str_key);
        st_ut_eq(ST_OK, ret, "failed to remove str key value");
        st_ut_eq(--count, root->element_cnt, "wrong element count by remove");

        bytes_addr = (uintptr_t)check_value.bytes;
        ret = st_capi_get(root, str_key, &check_value);
        st_ut_eq(ST_NOT_FOUND, ret, "failed to remove str key exists");
        st_ut_eq(bytes_addr,
                 (uintptr_t)check_value.bytes,
                 "get must not touch bytes in failure");

        st_capi_free(&check_value);

        /** double */
        double double_key = 10.1;
        double double_val = 100.1;
        check_value       = (st_tvalue_t)st_str_null;

        ret = st_capi_set(root, double_key, double_val);
        st_ut_eq(ST_OK, ret, "failed to set double key value");
        st_ut_eq(++count, root->element_cnt, "wrong element count");

        ret = st_capi_get(root, double_key, &check_value);
        st_ut_eq(ST_OK, ret, "failed to get double key value");

        len = sizeof(double_val);
        st_ut_eq(ST_TYPES_NUMBER, check_value.type, "wrong double value type");
        st_ut_eq(len, check_value.len, "wrong len of double value");
        st_ut_eq(double_val,
                 *((double *)check_value.bytes),
                 "wrong value of double value");

        ret = st_capi_add(root, double_key, double_val);
        st_ut_eq(ST_EXISTED, ret, "add duplicated double key");

        ret = st_capi_remove_key(root, double_key);
        st_ut_eq(ST_OK, ret, "failed to remove doule key value");
        st_ut_eq(--count, root->element_cnt, "wrong element count by remove");

        ret = st_capi_add(root, double_key, double_val);
        st_ut_eq(ST_OK, ret, "failed to add double key");
        st_ut_eq(++count, root->element_cnt, "wrong element count by remove");

        ret = st_capi_remove_key(root, double_key);
        st_ut_eq(ST_OK, ret, "failed to remove doule key value");
        st_ut_eq(--count, root->element_cnt, "wrong element count by remove");

        bytes_addr = (uintptr_t)check_value.bytes;
        ret = st_capi_get(root, double_key, &check_value);
        st_ut_eq(ST_NOT_FOUND, ret, "failed to remove double key exists");
        st_ut_eq(bytes_addr,
                 (uintptr_t)check_value.bytes,
                 "get must not touch bytes in failure");

        st_capi_free(&check_value);

        /** uint64_t */
        uint64_t u64_key = 10UL;
        uint64_t u64_val = 100UL;
        check_value      = (st_tvalue_t)st_str_null;

        ret = st_capi_set(root, u64_key, u64_val);
        st_ut_eq(ST_OK, ret, "failed to set u64 key value");
        st_ut_eq(++count, root->element_cnt, "wrong element count");

        ret = st_capi_get(root, u64_key, &check_value);

        len = sizeof(u64_val);
        st_ut_eq(ST_OK, ret, "failed to get u64 key value");
        st_ut_eq(ST_TYPES_U64, check_value.type, "wrong u64 value type");
        st_ut_eq(len, check_value.len, "wrong len of u64 value");
        st_ut_eq(u64_val,
                 *((uint64_t *)check_value.bytes),
                 "wrong value of u64 value");

        ret = st_capi_add(root, u64_key, u64_val);
        st_ut_eq(ST_EXISTED, ret, "add duplicated u64 key");

        ret = st_capi_remove_key(root, u64_key);
        st_ut_eq(ST_OK, ret, "failed to remove u64 key value");
        st_ut_eq(--count, root->element_cnt, "wrong element count by remove");

        ret = st_capi_add(root, u64_key, u64_val);
        st_ut_eq(ST_OK, ret, "failed to add u64 key");
        st_ut_eq(++count, root->element_cnt, "wrong element count by remove");

        ret = st_capi_remove_key(root, u64_key);
        st_ut_eq(ST_OK, ret, "failed to remove u64 key value");
        st_ut_eq(--count, root->element_cnt, "wrong element count by remove");

        bytes_addr = (uintptr_t)check_value.bytes;
        ret = st_capi_get(root, u64_key, &check_value);
        st_ut_eq(ST_NOT_FOUND, ret, "failed to remove u64 key exists");
        st_ut_eq(bytes_addr,
                 (uintptr_t)check_value.bytes,
                 "get must not touch bytes in failure");

        st_capi_free(&check_value);

        /** st_bool */
        st_bool bool_key = 0;
        st_bool bool_val = 1;
        check_value      = (st_tvalue_t)st_str_null;

        ret = st_capi_set(root, bool_key, bool_val);
        st_ut_eq(ST_OK, ret, "failed to set bool key value");
        st_ut_eq(++count, root->element_cnt, "wrong element count");

        ret = st_capi_get(root, bool_key, &check_value);
        st_ut_eq(ST_OK, ret, "failed to get bool key value");

        len = sizeof(st_bool);
        st_ut_eq(ST_TYPES_BOOLEAN, check_value.type, "wrong bool value type");
        st_ut_eq(len, check_value.len, "wrong len of bool value");
        st_ut_eq(bool_val,
                 *((st_bool *)check_value.bytes),
                 "wrong value of bool value");

        ret = st_capi_add(root, bool_key, bool_val);
        st_ut_eq(ST_EXISTED, ret, "add duplicated bool key");

        ret = st_capi_remove_key(root, bool_key);
        st_ut_eq(ST_OK, ret, "failed to remove bool key value");
        st_ut_eq(--count, root->element_cnt, "wrong element count by remove");

        ret = st_capi_add(root, bool_key, bool_val);
        st_ut_eq(ST_OK, ret, "failed to add bool key");
        st_ut_eq(++count, root->element_cnt, "wrong element count by remove");

        ret = st_capi_remove_key(root, bool_key);
        st_ut_eq(ST_OK, ret, "failed to remove bool key value");
        st_ut_eq(--count, root->element_cnt, "wrong element count by remove");

        bytes_addr = (uintptr_t)check_value.bytes;
        ret = st_capi_get(root, bool_key, &check_value);
        st_ut_eq(ST_NOT_FOUND, ret, "failed to remove bool key exists");
        st_ut_eq(bytes_addr,
                 (uintptr_t)check_value.bytes,
                 "get must not touch bytes in failure");

        st_capi_free(&check_value);

        /** st_table_t* */
        check_value   = (st_tvalue_t)st_str_null;
        char *tbl_key = "I am a table";

        st_table_t *tbl_val = NULL;
        ret = st_table_new(&pstate->lib_state->table_pool, &tbl_val);
        st_ut_eq(ST_OK, ret, "failed to new table element");

        ret = st_capi_set(root, tbl_key, tbl_val);
        st_ut_eq(ST_OK, ret, "failed to set table key value");
        st_ut_eq(++count, root->element_cnt, "wrong element count");

        ret = st_capi_get(root, tbl_key, &check_value);
        st_ut_eq(ST_OK, ret, "failed to get table key value");

        len = sizeof(tbl_val);
        st_ut_eq(ST_TYPES_TABLE, check_value.type, "wrong table value type");
        st_ut_eq(len, check_value.len, "wrong len of table value");
        st_ut_eq(tbl_val,
                 *((st_table_t **)check_value.bytes),
                 "wrong value of table value");

        st_capi_check_proot(&check_value);
        st_capi_free(&check_value);

        ret = st_capi_remove_key(root, tbl_key);
        st_ut_eq(ST_OK, ret, "failed to remove table key value");
        st_ut_eq(--count, root->element_cnt, "wrong element count by remove");

        bytes_addr = (uintptr_t)check_value.bytes;
        ret = st_capi_get(root, tbl_key, &check_value);
        st_ut_eq(ST_NOT_FOUND, ret, "failed to remove table key exists");
        st_ut_eq(bytes_addr,
                 (uintptr_t)check_value.bytes,
                 "get must not touch bytes in failure");

        /** pid_t aka int */
        check_value  = (st_tvalue_t)st_str_null;
        pid_t pid = getpid();
        ret = st_capi_set(root, pid, tbl_val);
        st_ut_eq(ST_OK, ret, "failed to set pid key value");
        st_ut_eq(++count, root->element_cnt, "wrong element count");

        ret = st_capi_get(root, pid, &check_value);
        st_ut_eq(ST_OK, ret, "failed to get pid key value");

        len = sizeof(tbl_val);
        st_ut_eq(ST_TYPES_TABLE, check_value.type, "wrong table value type");
        st_ut_eq(len, check_value.len, "wrong len of table value");
        st_ut_eq(tbl_val,
                 *((st_table_t **)check_value.bytes),
                 "wrong value of table value");

        st_capi_check_proot(&check_value);
        st_capi_free(&check_value);

        ret = st_capi_add(root, pid, tbl_val);
        st_ut_eq(ST_EXISTED, ret, "add duplicated pid key");

        ret = st_capi_remove_key(root, pid);
        st_ut_eq(ST_OK, ret, "failed to remove pid table key value");
        st_ut_eq(--count, root->element_cnt, "wrong element count by remove");

        ret = st_capi_add(root, pid, tbl_val);
        st_ut_eq(ST_OK, ret, "failed to add pid key");
        st_ut_eq(++count, root->element_cnt, "wrong element count by remove");

        ret = st_capi_remove_key(root, pid);
        st_ut_eq(ST_OK, ret, "failed to remove pid table key value");
        st_ut_eq(--count, root->element_cnt, "wrong element count by remove");

        bytes_addr = (uintptr_t)check_value.bytes;
        ret = st_capi_get(root, pid, &check_value);
        st_ut_eq(ST_NOT_FOUND, ret, "failed to remove table key exists");
        st_ut_eq(bytes_addr,
                 (uintptr_t)check_value.bytes,
                 "get must not touch bytes in failure");

        /** uintptr_t aka unsigned long aka uint64_t */
        check_value        = (st_tvalue_t)st_str_null;
        uintptr_t tbl_addr = (uintptr_t)tbl_val;
        ret = st_capi_set(root, tbl_addr, tbl_val);
        st_ut_eq(++count, root->element_cnt, "wrong element count");

        ret = st_capi_get(root, tbl_addr, &check_value);
        st_ut_eq(ST_OK, ret, "failed to get uintptr_t key value");

        len = sizeof(tbl_val);
        st_ut_eq(ST_TYPES_TABLE, check_value.type, "wrong table value type");
        st_ut_eq(len, check_value.len, "wrong len of table value");
        st_ut_eq(tbl_val,
                 *((st_table_t **)check_value.bytes),
                 "wrong value of table value");
        st_capi_check_proot(&check_value);
        st_capi_free(&check_value);

        ret = st_capi_add(root, tbl_addr, tbl_val);
        st_ut_eq(ST_EXISTED, ret, "add duplicated uintptr_t key");

        ret = st_capi_remove_key(root, tbl_addr);
        st_ut_eq(ST_OK, ret, "failed to remove addr table key value");
        st_ut_eq(--count, root->element_cnt, "wrong element count by remove");

        ret = st_capi_add(root, tbl_addr, tbl_val);
        st_ut_eq(ST_OK, ret, "failed to add uintptr_t key");
        st_ut_eq(++count, root->element_cnt, "wrong element count by remove");

        ret = st_capi_remove_key(root, tbl_addr);
        st_ut_eq(ST_OK, ret, "failed to remove addr table key value");
        st_ut_eq(--count, root->element_cnt, "wrong element count by remove");

        bytes_addr = (uintptr_t)check_value.bytes;
        ret = st_capi_get(root, tbl_addr, &check_value);
        st_ut_eq(ST_NOT_FOUND, ret, "failed to remove addr table key exists");
        st_ut_eq(bytes_addr,
                 (uintptr_t)check_value.bytes,
                 "get must not touch bytes in failure");

        /** char array */
        check_value = (st_tvalue_t)st_str_null;
        char carray[100];
        char *carray_tbl_key = (char *)carray;
        snprintf(carray_tbl_key, 100, "carray key table entry");

        ret = st_capi_set(root, carray_tbl_key , tbl_val);
        st_ut_eq(++count, root->element_cnt, "wrong element count");

        ret = st_capi_get(root, carray_tbl_key, &check_value);
        st_ut_eq(ST_OK, ret, "failed to get carray key value");

        len = sizeof(tbl_val);
        st_ut_eq(ST_TYPES_TABLE, check_value.type, "wrong table value type");
        st_ut_eq(len, check_value.len, "wrong len of table value");
        st_ut_eq(tbl_val,
                 *((st_table_t **)check_value.bytes),
                 "wrong value of table value");
        st_capi_check_proot(&check_value);
        st_capi_free(&check_value);

        ret = st_capi_add(root, carray_tbl_key, tbl_val);
        st_ut_eq(ST_EXISTED, ret, "add duplicated carray key");

        ret = st_capi_remove_key(root, carray_tbl_key);
        st_ut_eq(ST_OK, ret, "failed to remove addr table key value");
        st_ut_eq(--count, root->element_cnt, "wrong element count by remove");

        ret = st_capi_add(root, carray_tbl_key, tbl_val);
        st_ut_eq(ST_OK, ret, "failed to add carray key");
        st_ut_eq(++count, root->element_cnt, "wrong element count by remove");

        ret = st_capi_remove_key(root, carray_tbl_key);
        st_ut_eq(ST_OK, ret, "failed to remove addr table key value");
        st_ut_eq(--count, root->element_cnt, "wrong element count by remove");

        bytes_addr = (uintptr_t)check_value.bytes;
        ret = st_capi_get(root, carray_tbl_key, &check_value);
        st_ut_eq(ST_NOT_FOUND, ret, "failed to remove table key exists");
        st_ut_eq(bytes_addr,
                 (uintptr_t)check_value.bytes,
                 "get must not touch bytes in failure");

        st_table_remove_all(root);
        st_table_free(root);

        return ST_OK;
    }

    st_capi_test_set_get_rm_key(st_capi_get_process_state());

    st_capi_tear_down_ut();
}


st_test(st_capi, worker_init)
{
    st_capi_prepare_ut();

    int
    st_capi_test_worker_init_cb(void)
    {
        pid_t pid                 = getpid();
        st_capi_process_t *pstate = st_capi_get_process_state();
        st_capi_t *lstate         = pstate->lib_state;

        /** check my proot in root set */
        int found = 0;
        st_capi_process_t *entry;
        st_list_for_each_entry(entry, &lstate->p_roots, node) {
            if (entry == pstate) {
                found = 1;
                break;
            }
        }
        st_ut_eq(1, found, "proot not found");

        /** check process state */
        st_ut_eq(pid, pstate->pid, "wrong pid in pstate");
        st_ut_eq(1, pstate->inited, "wrong inited in pstate");
        st_ut_ne(NULL, pstate->lib_state, "lib state in pstate is NULL");

        /** check proot in gc root set */
        int ret = st_gc_add_root(&pstate->lib_state->table_pool.gc,
                                 &pstate->root->gc_head);
        st_ut_eq(ST_EXISTED, ret, "failed to put proot into gc root set");

        st_ut_bug(st_robustlock_trylock(&pstate->alive),
                  "failed to lock alive in process state");

        return ST_OK;
    }

    st_ut_eq(ST_OK,
             st_capi_test_fork_wrapper(st_capi_test_worker_init_cb),
             "callback failed");

    st_capi_tear_down_ut();
}


st_test(st_capi, new)
{
    st_capi_prepare_ut();

    st_capi_process_t *pstate = st_capi_get_process_state();
    int64_t tbl_cnt = pstate->lib_state->table_pool.table_cnt;

    int
    st_capi_test_new_cb(void)
    {
        st_capi_process_t *pstate = st_capi_get_process_state();
        int64_t root_tbl_cnt = pstate->root->element_cnt;
        st_ut_eq(0, root_tbl_cnt, "proot element_cnt is not 0");

        st_tvalue_t table_vals[10];
        for (int cnt = 0; cnt < 10; cnt++) {
            /** new table */
            int ret = st_capi_new(&table_vals[cnt]);
            st_ut_eq(ST_OK, ret, "failed to new table");

            st_table_t *table = st_table_get_table_addr_from_value(table_vals[cnt]);
            st_ut_ne(NULL, table, "failed to return table address");

            /** check table element cnt */
            st_ut_eq(++root_tbl_cnt,
                     pstate->root->element_cnt,
                     "new table is not inserted into proot");

            /** check if table is in proot */
            st_tvalue_t key;
            st_tvalue_t value;
            uintptr_t key_name = (uintptr_t)table_vals[cnt].bytes;
            st_capi_make_tvalue(key, key_name);

            ret = st_table_get_value(pstate->root, key, &value);

            st_ut_eq(ST_OK, ret, "failed to get table from proot");
            st_ut_eq(table,
                     st_table_get_table_addr_from_value(value),
                     "table address is not right");
        }

        for (int cnt = 0; cnt < 10; cnt++) {
            free(table_vals[cnt].bytes);
        }

        return ST_OK;
    }

    st_ut_eq(ST_OK,
             st_capi_test_fork_wrapper(st_capi_test_new_cb),
             "callback failed");

    /** check table cnt: proot + 10 tables for each process */
    st_ut_eq(tbl_cnt + (11 * ST_CAPI_TEST_PROCS_CNT),
             pstate->lib_state->table_pool.table_cnt,
             "total table number is wrong");

    st_capi_tear_down_ut();
}


st_test(st_capi, free)
{
    st_capi_prepare_ut();

    /**
     * case 1: remove table from new
     * case 2: remove table from get
     */
    int
    st_capi_test_free_cb(void)
    {
        st_capi_process_t *pstate = st_capi_get_process_state();
        int elem_cnt = pstate->root->element_cnt;

        /** create parent table and proot has its reference */
        st_tvalue_t ptvalue = st_str_null;
        int ret = st_capi_new(&ptvalue);
        st_ut_eq(ST_OK, ret, "failed to new parent table");
        st_ut_eq(++elem_cnt, pstate->root->element_cnt, "wrong element cnt");
        st_table_t *ptable = st_table_get_table_addr_from_value(ptvalue);

        /** parent table name in proot */
        uintptr_t ptable_name = (uintptr_t)ptvalue.bytes;

        /** create child table and proot has its reference */
        st_tvalue_t ctvalue = st_str_null;
        ret = st_capi_new(&ctvalue);
        st_ut_eq(ST_OK, ret, "failed to new child table");
        st_ut_eq(++elem_cnt, pstate->root->element_cnt, "wrong element cnt");
        st_table_t *ctable = st_table_get_table_addr_from_value(ctvalue);
        uintptr_t ctvalue_name = (uintptr_t)ctvalue.bytes;

        /** set child to parent with key name of child_key */
        char *child_key = "little child";
        ret = st_capi_set(ptable, child_key, ctable);
        st_ut_eq(ST_OK, ret, "failed to set child to parent");

        /** get child from parent, and proot has reference of get_ctvalue */
        st_tvalue_t get_ctvalue;
        ret = st_capi_get(ptable, child_key, &get_ctvalue);
        st_ut_eq(ST_OK, ret, "failed to get child");
        st_ut_eq(++elem_cnt, pstate->root->element_cnt, "wrong element cnt");
        uintptr_t cget_name = (uintptr_t)get_ctvalue.bytes;

        /** remove table from get, free get_ctvalue */
        ret = st_capi_free(&get_ctvalue);
        st_ut_eq(ST_OK, ret, "failed to free get_ctvalue");
        st_ut_eq(ST_TYPES_STRING, get_ctvalue.type, "wrong tvalue type");
        st_ut_eq(0, get_ctvalue.len, "failed to clear tvalue len");
        st_ut_eq(0, get_ctvalue.capacity, "failed to clear tvalue capacity");
        st_ut_eq(NULL, get_ctvalue.bytes, "failed to free tvalue bytes");
        st_ut_eq(--elem_cnt, pstate->root->element_cnt, "wrong element cnt");

        /** get_ctvalue reference in proot must be gone */
        ret = st_capi_get(pstate->root, cget_name, &get_ctvalue);
        st_ut_eq(ST_NOT_FOUND, ret, "failed to remove get_ctvalue in proot");

        /** remove table from new, free ctvalue */
        ret = st_capi_free(&ctvalue);
        st_ut_eq(ST_OK, ret, "failed to free ctvalue");
        st_ut_eq(ST_TYPES_STRING, ctvalue.type, "wrong ctvalue type");
        st_ut_eq(0, ctvalue.len, "failed to clear ctvalue len");
        st_ut_eq(0, ctvalue.capacity, "failed to clear ctvalue capacity");
        st_ut_eq(NULL, ctvalue.bytes, "failed to free ctvalue bytes");
        st_ut_eq(--elem_cnt, pstate->root->element_cnt, "wrong element cnt");

        ret = st_capi_get(pstate->root, ctvalue_name, &ctvalue);
        st_ut_eq(ST_NOT_FOUND, ret, "failed to remove ctvalue in proot");

        /** free parent */
        ret = st_capi_free(&ptvalue);
        st_ut_eq(ST_OK, ret, "failed to free ptvalue");
        st_ut_eq(ST_TYPES_STRING, ptvalue.type, "wrong ptvalue type");
        st_ut_eq(0, ptvalue.len, "failed to clear ptvalue len");
        st_ut_eq(0, ptvalue.capacity, "failed to clear ptvalue capacity");
        st_ut_eq(NULL, ptvalue.bytes, "failed to free ptvalue bytes");
        st_ut_eq(--elem_cnt, pstate->root->element_cnt, "wrong element cnt");

        /** parent ref must be removed from proot */
        ret = st_capi_get(pstate->root, ptable_name, &ptvalue);
        st_ut_eq(ST_NOT_FOUND, ret, "failed to remove ptvalue in proot");

        /** 0 element in proot */
        st_ut_eq(0, pstate->root->element_cnt, "proot is not clean");

        return ST_OK;
    }

    st_capi_test_fork_wrapper(st_capi_test_free_cb);

    st_capi_tear_down_ut();
}


st_test(st_capi, iterator)
{
    st_capi_prepare_ut();

    int
    st_capi_test_iterator_cb(void)
    {
        st_capi_process_t *pstate = st_capi_get_process_state();
        st_tvalue_t tbl_val = st_str_null;
        int ret = st_capi_new(&tbl_val);
        st_ut_eq(ST_OK, ret, "failed to new table: %d", ret);

        st_capi_iter_t iter;
        ret = st_capi_init_iterator(&tbl_val, &iter, NULL, 0);
        st_ut_eq(ST_OK, ret, "failed to init iterator of empty table: %d", ret);
        st_ut_eq(ST_TYPES_TABLE, iter.table.type, "iter must be table type");
        st_ut_eq(*((st_table_t **)tbl_val.bytes),
                 *((st_table_t **)(iter.table.bytes)),
                 "wrong table addr");
        st_capi_check_proot(&iter.table);

        st_tvalue_t key;
        st_tvalue_t value;
        ret = st_capi_next(&iter, &key, &value);
        st_ut_eq(ST_ITER_FINISH, ret, "failed to iterate empty table: %d", ret);

        int element_cnt = pstate->root->element_cnt;
        ret = st_capi_free_iterator(&iter);
        st_ut_eq(ST_OK, ret, "failed to free iterator of empty table: %d", ret);
        st_ut_eq(NULL, iter.table.bytes, "failed to free iterator");
        st_ut_eq(element_cnt - 1,
                 pstate->root->element_cnt,
                 "failed to remove iterator reference");

        st_tvalue_t element_tbl = st_str_null;
        ret = st_capi_new(&element_tbl);
        st_ut_eq(ST_OK, ret, "failed to new table: %d", ret);

        char *strings[] = { "a", "b", "c", "d", "e" };
        for (int cnt = 0; cnt < st_nelts(strings); cnt++) {
            st_table_t *table = st_table_get_table_addr_from_value(tbl_val);

            if (cnt != st_nelts(strings) - 1) {
                st_capi_set(table, strings[cnt], strings[cnt]);
            }
            else {
                st_table_t *t = st_table_get_table_addr_from_value(element_tbl);
                st_capi_set(table, strings[cnt], t);
            }
        }

        ret = st_capi_init_iterator(&tbl_val, &iter, NULL, 0);
        st_ut_eq(ST_OK, ret, "failed to init iterator of table: %d", ret);
        st_capi_check_proot(&iter.table);

        for (int cnt = 0; cnt < st_nelts(strings); cnt++) {
            ret = st_capi_next(&iter, &key, &value);
            st_ut_eq(ST_OK, ret, "failed to next iterator: %d", ret);

            st_ut_eq(ST_TYPES_STRING, key.type, "wrong key type");
            st_ut_eq(0,
                     strcmp(strings[cnt], (char *)key.bytes),
                     "wrong key value");
            if (cnt != st_nelts(strings) - 1) {
                st_ut_eq(ST_TYPES_STRING, value.type, "wrong value type");
                st_ut_eq(0,
                         strcmp(strings[cnt], (char *)key.bytes),
                         "wrong value");
            }
            else {
                st_table_t *t = st_table_get_table_addr_from_value(element_tbl);

                st_ut_eq(ST_TYPES_TABLE, value.type, "wrong value type");
                st_ut_eq(t, *((st_table_t **)value.bytes), "wrong value");
            }

            ret = st_capi_free(&key);
            st_ut_eq(ST_OK, ret, "failed to free key");

            ret = st_capi_free(&value);
            st_ut_eq(ST_OK, ret, "failed to free value");
        }

        ret = st_capi_next(&iter, &key, &value);
        st_ut_eq(ST_ITER_FINISH, ret, "failed to finish iterating: %d", ret);

        element_cnt = pstate->root->element_cnt;
        ret = st_capi_free_iterator(&iter);
        st_ut_eq(ST_OK, ret, "failed to free iterator: %d", ret);
        st_ut_eq(element_cnt - 1,
                 pstate->root->element_cnt,
                 "failed to remove iterator reference");

        st_capi_free(&element_tbl);
        st_capi_free(&tbl_val);
        st_ut_eq(0, pstate->root->element_cnt, "failed to remove reference");

        return ST_OK;
    }

    st_capi_test_fork_wrapper(st_capi_test_iterator_cb);

    st_capi_tear_down_ut();
}


st_ut_main;

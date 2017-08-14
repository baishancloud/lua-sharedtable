#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "robustlock.h"
#include "unittest/unittest.h"

pthread_mutex_t *alloc_lock() {
    pthread_mutex_t *lock = NULL;

    lock = mmap(NULL, sizeof(pthread_mutex_t), PROT_READ|PROT_WRITE,
                MAP_SHARED|MAP_ANONYMOUS, -1, 0);

    st_robustlock_init(lock);

    return lock;
}

void free_lock(pthread_mutex_t *lock) {
    int ret = -1;

    ret = st_robustlock_destroy(lock);
    st_ut_eq(ST_OK, ret, "lock destroy");

    munmap(lock, sizeof(pthread_mutex_t));
}

st_test(robustlock, init) {
    int ret = -1;
    pthread_mutex_t *lock = NULL;

    lock = mmap(NULL, sizeof(pthread_mutex_t), PROT_READ|PROT_WRITE,
                MAP_SHARED|MAP_ANONYMOUS, -1, 0);

    ret = st_robustlock_init(lock);
    st_ut_eq(ST_OK, ret, "lock init");

    free_lock(lock);
}

st_test(robustlock, destroy) {
    int ret = -1;
    pthread_mutex_t *lock = NULL;

    lock = alloc_lock();

    ret = st_robustlock_destroy(lock);
    st_ut_eq(ST_OK, ret, "lock destroy");

    munmap(lock, sizeof(pthread_mutex_t));
}


st_test(robustlock, lock) {
    int ret = -1;
    pthread_mutex_t *lock = NULL;

    lock = alloc_lock();

    ret = st_robustlock_lock(lock);
    st_ut_eq(ST_OK, ret, "lock");

    free_lock(lock);
}

st_test(robustlock, unlock) {
    int ret = -1;
    pthread_mutex_t *lock = NULL;

    lock = alloc_lock();

    ret = st_robustlock_lock(lock);
    st_ut_eq(ST_OK, ret, "lock");

    ret = st_robustlock_unlock(lock);
    st_ut_eq(ST_OK, ret, "unlock");

    free_lock(lock);
}

st_test(robustlock, lock_many_times) {
    int ret = -1;
    pthread_mutex_t *lock = NULL;

    lock = alloc_lock();

    for (int i = 0; i < 20; i++) {
        ret = st_robustlock_lock(lock);
        st_ut_eq(ST_OK, ret, "lock in %d times", i);

        ret = st_robustlock_unlock(lock);
        st_ut_eq(ST_OK, ret, "unlock in %d times", i);

    }

    free_lock(lock);
}

st_test(robustlock, concurrent_lock) {
    int ret = -1;
    int ch1, ch2;

    pthread_mutex_t *lock = NULL;
    lock = alloc_lock();

    ch1 = fork();
    if (ch1 == 0) {
        ret = st_robustlock_lock(lock);
        st_ut_eq(ST_OK, ret, "child1 lock");
        sleep(1);

        ret = st_robustlock_unlock(lock);
        st_ut_eq(ST_OK, ret, "child1 unlock");

        exit(0);
    }

    ch2 = fork();
    if (ch2 == 0) {
        ret = st_robustlock_lock(lock);
        st_ut_eq(ST_OK, ret, "child2 lock");
        sleep(1);

        ret = st_robustlock_unlock(lock);
        st_ut_eq(ST_OK, ret, "child2 unlock");

        exit(0);
    }

    waitpid(ch1, &ret, 0);
    waitpid(ch2, &ret, 0);

    free_lock(lock);
}

st_test(robustlock, deadlock_with_robust) {
    int ch1, ch2;
    int ret = -1;

    pthread_mutex_t *lock = NULL;
    lock = alloc_lock();

    ch1 = fork();
    if (ch1 == 0) {
        ret = st_robustlock_lock(lock);
        st_ut_eq(ST_OK, ret, "lock");

        exit(0);
    }

    waitpid(ch1, &ret, 0);

    ch2 = fork();
    if (ch2 == 0) {
        ret = st_robustlock_lock(lock);
        st_ut_eq(ST_OK, ret, "lock");

        ret = st_robustlock_unlock(lock);
        st_ut_eq(ST_OK, ret, "unlock");

        exit(0);
    }

    waitpid(ch2, &ret, 0);
    free_lock(lock);
}

st_test(robustlock, deadlock_many_times) {
    int ch1, ch2;
    int ret = -1;

    pthread_mutex_t *lock = NULL;
    lock = alloc_lock();

    for (int i = 0; i < 20; i++) {

        ch1 = fork();
        if (ch1 == 0) {
            ret = st_robustlock_lock(lock);
            st_ut_eq(ST_OK, ret, "lock");

            exit(0);
        }

        waitpid(ch1, &ret, 0);

        ch2 = fork();
        if (ch2 == 0) {
            ret = st_robustlock_lock(lock);
            st_ut_eq(ST_OK, ret, "lock");

            ret = st_robustlock_unlock(lock);
            st_ut_eq(ST_OK, ret, "unlock");

            exit(0);
        }

        waitpid(ch2, &ret, 0);
    }

    free_lock(lock);
}

st_ut_main;

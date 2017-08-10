#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "robust_lock.h"
#include "unittest/unittest.h"

pthread_mutex_t *alloc_lock() {
    pthread_mutex_t *lock = NULL;

    lock = mmap(NULL, sizeof(pthread_mutex_t), PROT_READ|PROT_WRITE,
                MAP_SHARED|MAP_ANONYMOUS, -1, 0);

    robust_lock_init(lock);

    return lock;
}

void free_lock(pthread_mutex_t *lock) {
    int ret = -1;

    ret = robust_lock_destroy(lock);
    st_ut_eq(ST_OK, ret, "lock destroy");

    munmap(lock, sizeof(pthread_mutex_t));
}

st_test(robust_lock, robust_lock_init) {
    int ret = -1;
    pthread_mutex_t *lock = NULL;

    lock = mmap(NULL, sizeof(pthread_mutex_t), PROT_READ|PROT_WRITE,
                MAP_SHARED|MAP_ANONYMOUS, -1, 0);

    ret = robust_lock_init(lock);
    st_ut_eq(ST_OK, ret, "lock init");

    free_lock(lock);
}

st_test(robust_lock, robust_lock_destroy) {
    int ret = -1;
    pthread_mutex_t *lock = NULL;

    lock = alloc_lock();

    ret = robust_lock_destroy(lock);
    st_ut_eq(ST_OK, ret, "lock destroy");

    munmap(lock, sizeof(pthread_mutex_t));
}


st_test(robust_lock, robust_lock) {
    int ret = -1;
    pthread_mutex_t *lock = NULL;

    lock = alloc_lock();

    ret = robust_lock(lock);
    st_ut_eq(ST_OK, ret, "lock");

    free_lock(lock);
}

st_test(robust_lock, robust_unlock) {
    int ret = -1;
    pthread_mutex_t *lock = NULL;

    lock = alloc_lock();

    ret = robust_lock(lock);
    st_ut_eq(ST_OK, ret, "lock");

    ret = robust_unlock(lock);
    st_ut_eq(ST_OK, ret, "unlock");

    free_lock(lock);
}

st_test(robust_lock, robust_lock_in_times) {
    int ret = -1;
    pthread_mutex_t *lock = NULL;

    lock = alloc_lock();

    for (int i = 0; i < 20; i++) {
        ret = robust_lock(lock);
        st_ut_eq(ST_OK, ret, "lock in %d times", i);

        ret = robust_unlock(lock);
        st_ut_eq(ST_OK, ret, "unlock in %d times", i);

    }

    free_lock(lock);
}

st_test(robust_lock, lock_in_children) {
    int ret = -1;
    int ch1, ch2;

    pthread_mutex_t *lock = NULL;
    lock = alloc_lock();

    ch1 = fork();
    if (ch1 == 0) {
        ret = robust_lock(lock);
        st_ut_eq(ST_OK, ret, "child1 lock");
        sleep(1);

        ret = robust_unlock(lock);
        st_ut_eq(ST_OK, ret, "child1 unlock");

        exit(0);
    }

    ch2 = fork();
    if (ch2 == 0) {
        ret = robust_lock(lock);
        st_ut_eq(ST_OK, ret, "child2 lock");
        sleep(1);

        ret = robust_unlock(lock);
        st_ut_eq(ST_OK, ret, "child2 unlock");

        exit(0);
    }

    waitpid(ch1, &ret, 0);
    waitpid(ch2, &ret, 0);

    free_lock(lock);
}

st_test(robust_lock, deadlock_with_mutex) {
    int ret = -1;
    int ch1, ch2;

    pthread_mutex_t *lock = NULL;
    lock = alloc_lock();

    ch1 = fork();
    if (ch1 == 0) {
        ret = pthread_mutex_lock(lock);
        st_ut_eq(ST_OK, ret, "lock");

        exit(0);
    }

    waitpid(ch1, &ret, 0);

    ch2 = fork();
    if (ch2 == 0) {
        ret = pthread_mutex_lock(lock);
        st_ut_ne(ST_OK, ret, "can not lock");

        exit(0);
    }

    waitpid(ch2, &ret, 0);

    free_lock(lock);
}

st_test(robust_lock, deadlock_with_robust) {
    int ch1, ch2;
    int ret = -1;

    pthread_mutex_t *lock = NULL;
    lock = alloc_lock();

    ch1 = fork();
    if (ch1 == 0) {
        ret = robust_lock(lock);
        st_ut_eq(ST_OK, ret, "lock");

        exit(0);
    }

    waitpid(ch1, &ret, 0);

    ch2 = fork();
    if (ch2 == 0) {
        ret = robust_lock(lock);
        st_ut_eq(ST_OK, ret, "lock");

        ret = robust_unlock(lock);
        st_ut_eq(ST_OK, ret, "unlock");

        exit(0);
    }

    waitpid(ch2, &ret, 0);
    free_lock(lock);
}

st_test(robust_lock, deadlock_in_times) {
    int ch1, ch2;
    int ret = -1;

    pthread_mutex_t *lock = NULL;
    lock = alloc_lock();

    for (int i = 0; i < 20; i++) {

        ch1 = fork();
        if (ch1 == 0) {
            ret = robust_lock(lock);
            st_ut_eq(ST_OK, ret, "lock");

            exit(0);
        }

        waitpid(ch1, &ret, 0);

        ch2 = fork();
        if (ch2 == 0) {
            ret = robust_lock(lock);
            st_ut_eq(ST_OK, ret, "lock");

            ret = robust_unlock(lock);
            st_ut_eq(ST_OK, ret, "unlock");

            exit(0);
        }

        waitpid(ch2, &ret, 0);
    }

    free_lock(lock);
}

st_ut_main;

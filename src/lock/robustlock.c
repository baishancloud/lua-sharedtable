#include "robustlock.h"

int st_robustlock_init(pthread_mutex_t *lock)
{
    int ret = ST_ERR;
    pthread_mutexattr_t attr;

    memset(&attr, 0, sizeof(attr));

    st_must(lock != NULL, ST_ARG_INVALID);

    ret = pthread_mutexattr_init(&attr);
    if (ret != ST_OK) {
        derr("pthread_mutexattr_init ret: %d, err: %s\n", ret, strerror(ret));
        return ret;
    }

    ret = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
    if (ret != ST_OK) {
        derr("pthread_mutexattr_settype ret: %d, err: %s\n", ret, strerror(ret));
        goto exit;
    }

    ret = pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST);
    if (ret != ST_OK) {
        derr("pthread_mutexattr_setrobust ret: %d, err: %s\n", ret, strerror(ret));
        goto exit;
    }

    ret = pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    if (ret != ST_OK) {
        derr("pthread_mutexattr_setpshared ret: %d, err: %s\n", ret, strerror(ret));
        goto exit;
    }

    ret = pthread_mutex_init(lock, &attr);
    if (ret != ST_OK) {
        derr("pthread_mutex_init ret: %d, err: %s\n", ret, strerror(ret));
        goto exit;
    }

exit:
    pthread_mutexattr_destroy(&attr);
    return ret;
}

int st_robustlock_lock(pthread_mutex_t *lock)
{
    int ret = ST_ERR;

    st_must(lock != NULL, ST_ARG_INVALID);

    while (1) {

        ret = pthread_mutex_lock(lock);
        dd("pthread_mutex_lock ret: %d, pid:%d, address: %p\n", ret, getpid(), lock);

        if (ret != EOWNERDEAD) {
            if (ret != ST_OK) {
                derr("pthread_mutex_lock ret: %d, err: %s\n", ret, strerror(ret));
            }
            return ret;
        }

        ret = pthread_mutex_consistent(lock);
        dd("pthread_mutex_consistent ret: %d, pid:%d, address: %p\n", ret, getpid(), lock);

        if (ret != ST_OK) {
            derr("pthread_mutex_consistent ret: %d, err: %s\n", ret, strerror(ret));
            return ret;
        }

        ret = pthread_mutex_unlock(lock);
        dd("pthread_mutex_unlock ret: %d, pid:%d, address: %p\n", ret, getpid(), lock);

        if (ret != ST_OK) {
            derr("pthread_mutex_unlock ret: %d, err: %s\n", ret, strerror(ret));
            return ret;
        }
    }

    return ret;
}

int st_robustlock_unlock(pthread_mutex_t *lock)
{
    st_must(lock != NULL, ST_ARG_INVALID);

    int ret = pthread_mutex_unlock(lock);
    if (ret != ST_OK) {
        derr("pthread_mutex_unlock ret: %d, err: %s\n", ret, strerror(ret));
    }

    dd("pthread_mutex_unlock ret: %d, pid:%d, address: %p\n", ret, getpid(), lock);

    return ret;
}

int st_robustlock_destroy(pthread_mutex_t *lock)
{
    st_must(lock != NULL, ST_ARG_INVALID);

    return pthread_mutex_destroy(lock);
}

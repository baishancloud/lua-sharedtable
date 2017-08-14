#include "robustlock.h"

int st_robustlock_init(pthread_mutex_t *lock)
{
    int ret = ST_ERR;
    pthread_mutexattr_t attr;

    memset(&attr, 0, sizeof(pthread_mutexattr_t));

    ret = pthread_mutexattr_init(&attr);
    if (ret != ST_OK) {
        return ret;
    }

    ret = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
    if (ret != ST_OK) {
        goto exit;
    }

    ret = pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST);
    if (ret != ST_OK) {
        goto exit;
    }

    ret = pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    if (ret != ST_OK) {
        goto exit;
    }

    ret = pthread_mutex_init(lock, &attr);
    if (ret != ST_OK) {
        goto exit;
    }

exit:
    pthread_mutexattr_destroy(&attr);
    return ret;
}

int st_robustlock_lock(pthread_mutex_t *lock)
{
    int ret = ST_ERR;

    ret = pthread_mutex_lock(lock);
    dd("pthread_mutex_lock ret: %d, pid:%d, address: %p\n", ret, getpid(), lock);
    if (ret != EOWNERDEAD) {
        return ret;
    }

    ret = pthread_mutex_consistent(lock);
    dd("pthread_mutex_consistent ret: %d, pid:%d, address: %p\n", ret, getpid(), lock);
    if (ret != ST_OK) {
        return ret;
    }

    ret = pthread_mutex_unlock(lock);
    dd("pthread_mutex_unlock ret: %d, pid:%d, address: %p\n", ret, getpid(), lock);
    if (ret != ST_OK) {
        return ret;
    }

    ret = pthread_mutex_lock(lock);
    dd("pthread_mutex_lock ret: %d, pid:%d, address: %p\n", ret, getpid(), lock);
    return ret;
}

int st_robustlock_unlock(pthread_mutex_t *lock)
{
    int ret = pthread_mutex_unlock(lock);
    dd("pthread_mutex_unlock ret: %d, pid:%d, address: %p\n", ret, getpid(), lock);

    return ret;
}

int st_robustlock_destroy(pthread_mutex_t *lock)
{
    return pthread_mutex_destroy(lock);
}

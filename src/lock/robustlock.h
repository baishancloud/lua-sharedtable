#ifndef _ROBUST_LOCK_H_INCLUDED_
#define _ROBUST_LOCK_H_INCLUDED_

#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>

#include "inc/util.h"
#include "inc/err.h"
#include "inc/log.h"

int st_robustlock_init(pthread_mutex_t *lock);
int st_robustlock_lock(pthread_mutex_t *lock);
int st_robustlock_unlock(pthread_mutex_t *lock);
int st_robustlock_destroy(pthread_mutex_t *lock);

#endif /* _ROBUST_LOCK_H_INCLUDED_ */

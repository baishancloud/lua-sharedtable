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

int robust_lock_init(pthread_mutex_t *lock);
int robust_lock(pthread_mutex_t *lock);
int robust_unlock(pthread_mutex_t *lock);
int robust_lock_destroy(pthread_mutex_t *lock);

#endif /* _ROBUST_LOCK_H_INCLUDED_ */

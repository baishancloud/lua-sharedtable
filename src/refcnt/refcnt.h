#ifndef _REFCNT_H_INCLUDED_
#define _REFCNT_H_INCLUDED_

#include <stdint.h>
#include <string.h>

#include "inc/err.h"
#include "inc/log.h"
#include "inc/util.h"
#include "inc/callback.h"
#include "rbtree/rbtree.h"
#include "robustlock/robustlock.h"

typedef struct st_refcnt_process_s st_refcnt_process_t;
typedef struct st_refcnt_s st_refcnt_t;

/**
 * each process refcnt
 */
struct st_refcnt_process_s {
    /* each process refcnt */
    int64_t cnt;

    /* process id as rbtree node key*/
    pid_t pid;

    st_rbtree_node_t rbnode;
};

/**
 * all processes refcnt
 */
struct st_refcnt_s {
    /* used to alloc or free st_refcnt_process_t struct memory space */
    st_callback_memory_t callback;

    /* all processes refcnt sum */
    int total_cnt;

    /* all processes in rbtree */
    st_rbtree_t processes;

    pthread_mutex_t lock;
};

int st_refcnt_init(st_refcnt_t *refcnt, st_callback_memory_t callback);

int st_refcnt_destroy(st_refcnt_t *refcnt);

int st_refcnt_get_process_refcnt(st_refcnt_t *refcnt, pid_t pid, int64_t *cnt);

int st_refcnt_get_total_refcnt(st_refcnt_t *refcnt, int64_t *cnt);

int st_refcnt_incr(st_refcnt_t *refcnt, pid_t pid, int64_t cnt, int64_t *process_refcnt,
                   int64_t *total_refcnt);

int st_refcnt_decr(st_refcnt_t *refcnt, pid_t pid, int64_t cnt, int64_t *process_refcnt,
                   int64_t *total_refcnt);

#endif /* _REFCNT_H_INCLUDED_ */

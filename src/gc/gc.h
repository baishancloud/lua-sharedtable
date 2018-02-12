#ifndef _GC_H_INCLUDED_
#define _GC_H_INCLUDED_

#include <stdint.h>
#include <string.h>

#include "inc/inc.h"

#include "list/list.h"
#include "time/time.h"
#include "array/array.h"
#include "atomic/atomic.h"
#include "robustlock/robustlock.h"

#define ST_GC_MAX_TIME_IN_USEC 500
#define ST_GC_MAX_ROOTS 1024

typedef struct st_gc_head_s st_gc_head_t;
typedef struct st_gc_s st_gc_t;

// each table has gc head, used for sweep unused table.
struct st_gc_head_s {
    // used by mark_queue in gc struct.
    st_list_t mark_lnode;

    // used by prev_sweep_queue, sweep_queue and garbage_queue in gc struct.
    st_list_t sweep_lnode;

    // store mark color.
    int64_t mark;
};

struct st_gc_s {
    // is a collection of items those are roots and should never be freed.
    // A root item is where the mark-process starts.
    st_gc_head_t *roots_data[ST_GC_MAX_ROOTS];
    st_array_t roots;

    // is queue of items to mark as marked.
    st_list_t mark_queue;

    // is queue of items those can not be decided whether it is a garbage in
    // previous round of gc.
    st_list_t prev_sweep_queue;

    // is queue of items whose references are deleted one or more times.
    st_list_t sweep_queue;

    // is queue of garbage item.
    st_list_t garbage_queue;

    // reamined table, after curr round of gc, the tables will move into prev_sweep_queue.
    st_list_t remained_queue;

    // a int number that indicates a complete gc.
    int64_t round;

    // gc run start time and end time.
    int64_t start_usec;
    int64_t end_usec;

    // gc has began running
    int begin;

    // in one gc step can visit table elements count.
    int max_visit_cnt;
    int curr_visit_cnt;

    // in one gc step can free table elements count.
    int max_free_cnt;
    int curr_free_cnt;

    pthread_mutex_t lock;
};

// defined to be: gc_round+0 or any int smaller than gc_round. Not yet scanned.
static inline int64_t st_gc_status_unknown(st_gc_t *gc) {
    return gc->round + 0;
}

// defined to be: gc_round+1, reachable from one of the roots.
static inline int64_t st_gc_status_reachable(st_gc_t *gc) {
    return gc->round + 1;
}

// defined to be: gc_round+2, confirmed to be unreachable from any roots.
static inline int64_t st_gc_status_garbage(st_gc_t *gc) {
    return gc->round + 2;
}

static inline int st_gc_is_status_unknown(st_gc_t *gc, int64_t mark) {
    return mark <= st_gc_status_unknown(gc);
}

static inline void st_gc_head_init(st_gc_t *gc, st_gc_head_t *gc_head) {
    gc_head->mark_lnode = (st_list_t) {NULL, NULL};
    gc_head->sweep_lnode = (st_list_t) {NULL, NULL};
    gc_head->mark = st_gc_status_unknown(gc);
}

int st_gc_init(st_gc_t *gc);

int st_gc_destroy(st_gc_t *gc);

int st_gc_run(st_gc_t *gc);

//add table and push the table to mark_queue, must be atomic
//so lock the gc lock in table module.
int st_gc_push_to_mark(st_gc_t *gc, st_gc_head_t *gc_head);

//add table and push the table to sweep_queue, must be atomic
//so lock the gc lock in table module.
int st_gc_push_to_sweep(st_gc_t *gc, st_gc_head_t *gc_head);

int st_gc_add_root(st_gc_t *gc, st_gc_head_t *gc_head);

int st_gc_remove_root(st_gc_t *gc, st_gc_head_t *gc_head);

#endif /* _GC_H_INCLUDED_ */

#ifndef _LIST_H_INCLUDED_
#define _LIST_H_INCLUDED_

#include <stddef.h>

typedef struct st_list_s st_list_t;

struct st_list_s {
    st_list_t  *prev;
    st_list_t  *next;
};

#define ST_LIST_INIT(node) { &(node), &(node) }

static inline void st_list_init(st_list_t *node) {
    node->prev = node;
    node->next = node;
}

static inline st_list_t * st_list_first(st_list_t *head) {
    return head->next;
}

static inline st_list_t * st_list_last(st_list_t *head) {
    return head->prev;
}

static inline st_list_t * st_list_next(st_list_t *node) {
    return node->next;
}

static inline st_list_t * st_list_prev(st_list_t *node) {
    return node->prev;
}

static inline int st_list_empty(st_list_t *head) {
    return head == head->prev;
}

static inline void st_list_insert_first(st_list_t *head, st_list_t *node) {
    node->next = head->next;
    node->next->prev = node;
    node->prev = head;
    head->next = node;
}

static inline void st_list_insert_last(st_list_t *head, st_list_t *node) {
    node->prev = head->prev;
    node->prev->next = node;
    node->next = head;
    head->prev = node;
}

static inline void st_list_remove(st_list_t *node) {
    node->next->prev = node->prev;
    node->prev->next = node->next;
    node->prev = NULL;
    node->next = NULL;
}

static inline void st_list_move_head(st_list_t *node, st_list_t *head) {
    st_list_remove(node);
    st_list_insert_first(head, node);
}

static inline void st_list_move_tail(st_list_t *node, st_list_t *head) {
    st_list_remove(node);
    st_list_insert_last(head, node);
}

#define st_list_for_each(node, head)                                                  \
    for (node = (head)->next; node != (head); node = node->next)

#define st_list_for_each_safe(node, next, head)                                       \
    for (node = (head)->next, next = node->next; node != (head);                      \
            node = next, next = node->next)

#define st_list_entry(node, type, member)                                             \
    (type *) ((u_char *)(node) - offsetof(type, member))

#define st_list_first_entry(head, type, member)                                       \
    st_list_empty(head) ? NULL : st_list_entry((head)->next, type, member)

#define st_list_last_entry(head, type, member)                                        \
    st_list_empty(head) ? NULL : st_list_entry((head)->prev, type, member)

#define st_list_for_each_entry(object, head, member)                                  \
    for (object = st_list_entry((head)->next, __typeof__(*object), member);           \
            &object->member != (head);                                                \
            object = st_list_entry(object->member.next, __typeof__(*object), member))

#define s3_list_for_each_entry_safe(object, next, head, member)                       \
    for (object = st_list_entry((head)->next, __typeof__(*object), member),           \
            next = st_list_entry(object->member.next, __typeof__(*object), member);   \
            &object->member != (head);                                                \
            object = next,                                                            \
            next = st_list_entry(next->member.next, __typeof__(*next), member))

#endif /* _st_list_H_INCLUDED_ */

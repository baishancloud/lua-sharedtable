#ifndef _RBTREE_H_INCLUDED_
#define _RBTREE_H_INCLUDED_

#include <stdint.h>
#include <string.h>
#include "inc/err.h"
#include "inc/log.h"
#include "inc/util.h"

typedef struct st_rbtree_node_s  st_rbtree_node_t;
typedef struct st_rbtree_s  st_rbtree_t;
typedef int (*st_rbtree_compare_pt) (st_rbtree_node_t *a, st_rbtree_node_t *b);

struct st_rbtree_node_s {
    st_rbtree_node_t *left;
    st_rbtree_node_t *right;
    st_rbtree_node_t *parent;
    uint8_t color;
};

struct st_rbtree_s {
    st_rbtree_node_t *root;
    st_rbtree_node_t sentinel;
    st_rbtree_compare_pt cmp;
};

int st_rbtree_init(st_rbtree_t *tree, st_rbtree_compare_pt cmp);
int st_rbtree_insert(st_rbtree_t *tree, st_rbtree_node_t *node);
int st_rbtree_delete(st_rbtree_t *tree, st_rbtree_node_t *node);

st_rbtree_node_t * st_rbtree_left_most(st_rbtree_t *tree);
st_rbtree_node_t * st_rbtree_right_most(st_rbtree_t *tree);

st_rbtree_node_t *s3_rbtree_search_eq(st_rbtree_t *tree, st_rbtree_node_t *node);
st_rbtree_node_t *s3_rbtree_search_le(st_rbtree_t *tree, st_rbtree_node_t *node);
st_rbtree_node_t *s3_rbtree_search_ge(st_rbtree_t *tree, st_rbtree_node_t *node);

static inline void st_rbtree_red(st_rbtree_node_t *node) {
    node->color = 1;
}

static inline void st_rbtree_black(st_rbtree_node_t *node) {
    node->color = 0;
}

static inline void st_rbtree_copy_color(st_rbtree_node_t *n1, st_rbtree_node_t *n2) {
    n1->color = n2->color;
}

static inline int st_rbtree_is_empty(st_rbtree_t *tree) {
    return tree->root == &tree->sentinel;
}

static inline int st_rbtree_is_red(st_rbtree_node_t *node) {
    return node->color;
}

static inline int st_rbtree_is_black(st_rbtree_node_t *node) {
    return !st_rbtree_is_red(node);
}

static inline int st_rbtree_is_left_child(st_rbtree_node_t *node) {
    return node == node->parent->left;
}

static inline int st_rbtree_is_right_child(st_rbtree_node_t *node) {
    return node == node->parent->right;
}

static inline int st_rbtree_node_is_inited(st_rbtree_node_t *node) {
    int ret;

    ret = node->parent == NULL && node->right == NULL && node->left == NULL;

    return !ret;
}

#endif /* _RBTREE_H_INCLUDED_ */

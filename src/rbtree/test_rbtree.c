#include <stdlib.h>
#include <string.h>

#include "unittest/unittest.h"
#include "rbtree.h"

typedef struct test_object {
    st_rbtree_node_t rb_node;
    int64_t key;
} test_object;

static int rbtree_cmp(st_rbtree_node_t *a, st_rbtree_node_t *b) {

    test_object *oa = (test_object *)a;
    test_object *ob = (test_object *)b;

    return st_cmp(oa->key, ob->key);
}

st_test(rbtree, init) {

    st_rbtree_t tree;

    st_ut_eq(ST_ARG_INVALID, st_rbtree_init(NULL, rbtree_cmp), "tree arg should not be NULL");

    st_ut_eq(ST_ARG_INVALID, st_rbtree_init(&tree, NULL), "cmp arg should not be NULL");

    st_ut_eq(ST_OK, st_rbtree_init(&tree, rbtree_cmp), "rbtree_init ok");

    st_ut_eq(&tree.sentinel, tree.root, "root should be sentinel");
}

st_test(rbtree, left_most) {

    st_rbtree_t tree;
    st_rbtree_node_t *node;

    st_rbtree_init(&tree, rbtree_cmp);

    node = st_rbtree_left_most(&tree);
    st_ut_eq(NULL, node, "no value, so left_most get NULl");

    test_object objects[] = {
        {.key = 50},
        {.key = 40},
        {.key = 30},
        {.key = 20},
        {.key = 10},
    };

    for (int i = 0; i < st_nelts(objects); i++) {
        st_rbtree_insert(&tree, &objects[i].rb_node, 1, NULL);

        test_object *obj = (test_object *)st_rbtree_left_most(&tree);

        st_ut_eq(objects[i].key, obj->key, "compare left_most value");
    }
}

st_test(rbtree, right_most) {

    st_rbtree_t tree;
    st_rbtree_node_t *node;

    st_rbtree_init(&tree, rbtree_cmp);

    node = st_rbtree_right_most(&tree);
    st_ut_eq(NULL, node, "no value, so right_most get NULl");

    test_object objects[] = {
        {.key = 10},
        {.key = 20},
        {.key = 30},
        {.key = 40},
        {.key = 50},
    };

    for (int i = 0; i < st_nelts(objects); i++) {
        st_rbtree_insert(&tree, &objects[i].rb_node, 1, NULL);

        test_object *obj = (test_object *)st_rbtree_right_most(&tree);

        st_ut_eq(objects[i].key, obj->key, "compare right_most value");
    }
}

st_test(rbtree, empty) {

    st_rbtree_t tree;

    st_rbtree_init(&tree, rbtree_cmp);

    st_ut_eq(1, st_rbtree_is_empty(&tree), "rbtree is empty");

    test_object object = {.key = 1};
    st_rbtree_insert(&tree, &object.rb_node, 1, NULL);

    st_ut_eq(0, st_rbtree_is_empty(&tree), "rbtree is not empty");
}

st_test(rbtree, search) {
    st_rbtree_t tree;
    test_object tmp;
    test_object *obj;

    test_object objects[] = {
        {.key = 2},
        {.key = 5},
        {.key = 10},
        {.key = 14},
        {.key = 19},
        {.key = 23},
        {.key = 26},
        {.key = 30},
        {.key = 40},
        {.key = 43},
    };

    struct case_s {
        int key;
        int le_key;
        int equal_key;
        int ge_key;
        int next_key;
    } cases[] = {
        {1, -1, -1, 2, 2},
        {2, 2, 2, 2, 5},
        {3, 2, -1, 5, 5},
        {5, 5, 5, 5, 10},
        {7, 5, -1, 10, 10},
        {10, 10, 10, 10, 14},
        {12, 10, -1, 14, 14},
        {14, 14, 14, 14, 19},
        {17, 14, -1, 19, 19},
        {19, 19, 19, 19, 23},
        {20, 19, -1, 23, 23},
        {23, 23, 23, 23, 26},
        {25, 23, -1, 26, 26},
        {26, 26, 26, 26, 30},
        {28, 26, -1, 30, 30},
        {30, 30, 30, 30, 40},
        {31, 30, -1, 40, 40},
        {40, 40, 40, 40, 43},
        {42, 40, -1, 43, 43},
        {43, 43, 43, 43, -1},
        {44, 43, -1, -1, -1},
    };

    st_rbtree_init(&tree, rbtree_cmp);

    for (int i = 0; i < st_nelts(objects); i++) {
        st_rbtree_insert(&tree, &objects[i].rb_node, 1, NULL);
    }

    for (int i = 0; i < st_nelts(cases); i++) {
        st_typeof(cases[0]) c = cases[i];

        tmp.key = c.key;

        obj = (test_object *)st_rbtree_search_eq(&tree,  &tmp.rb_node);
        if (c.equal_key == -1) {
            st_ut_eq(NULL, obj, "");
        } else {
            st_ut_eq(c.equal_key, obj->key, "");
        }

        obj = (test_object *)st_rbtree_search_le(&tree,  &tmp.rb_node);
        if (c.le_key == -1) {
            st_ut_eq(NULL, obj, "");
        } else {
            st_ut_eq(c.le_key, obj->key, "");
        }

        obj = (test_object *)st_rbtree_search_ge(&tree,  &tmp.rb_node);
        if (c.ge_key == -1) {
            st_ut_eq(NULL, obj, "");
        } else {
            st_ut_eq(c.ge_key, obj->key, "");
        }

        obj = (test_object *)st_rbtree_search_next(&tree,  &tmp.rb_node);
        if (c.next_key == -1) {
            st_ut_eq(NULL, obj, "");
        } else {
            st_ut_eq(c.next_key, obj->key, "");
        }
    }
}

st_test(rbtree, search_times) {

    st_rbtree_t tree;
    test_object *obj;
    test_object tmp;
    test_object objects[1000];

    st_rbtree_init(&tree, rbtree_cmp);

    tmp.key = 11;
    obj = (test_object *)st_rbtree_search_eq(&tree, &tmp.rb_node);
    st_ut_eq(NULL, obj, "tree is empty");

    for (int i = 0; i < 1000; i++) {

        // not add same key in rbtree
        while (1) {
            tmp.key = random() % 100000;
            obj = (test_object *)st_rbtree_search_eq(&tree, &tmp.rb_node);
            if (obj == NULL) {
                break;
            }
        }

        objects[i].key = tmp.key;
        st_rbtree_insert(&tree, &objects[i].rb_node, 1, NULL);
    }

    for (int i = 0; i < 1000; i++) {
        tmp.key = objects[i].key;
        obj = (test_object *)st_rbtree_search_eq(&tree, &tmp.rb_node);
        st_ut_eq(&objects[i], obj, "found object");
    }

    tmp.key = 100000 + 1;
    obj = (test_object *)st_rbtree_search_eq(&tree, &tmp.rb_node);
    st_ut_eq(NULL, obj, "not found key");
}

void test_insert_with_search(int object_num) {

    st_rbtree_t tree;
    test_object *obj;
    test_object objects[1000];
    test_object tmp;

    st_rbtree_init(&tree, rbtree_cmp);

    for (int i = 0; i < object_num; i++) {
        objects[i].key = i;

        st_rbtree_insert(&tree, &objects[i].rb_node, 1, NULL);

        st_ut_eq(1, st_rbtree_node_is_inited(&objects[i].rb_node), "");

        for (int j = 0; j <= i; j++) {
            tmp.key = objects[j].key;

            obj = (test_object *)st_rbtree_search_eq(&tree, &tmp.rb_node);

            st_ut_eq(&objects[j], obj, "search object is right");
        }
    }
}

st_test(rbtree, insert) {

    int test_object_num[] = {1, 100, 1000};

    for (int i = 0; i < st_nelts(test_object_num); i++) {
        test_insert_with_search(test_object_num[i]);
    }
}

void test_delete_with_search(int object_num) {

    st_rbtree_t tree;
    st_rbtree_node_t *node;
    test_object *obj;
    test_object objects[1000];
    test_object tmp;

    st_rbtree_init(&tree, rbtree_cmp);

    for (int i = 0; i < object_num; i++) {
        objects[i].key = i;
        st_rbtree_insert(&tree, &objects[i].rb_node, 1, NULL);
    }

    for (int i = object_num - 1; i >= 0; i--) {
        tmp.key = objects[i].key;
        obj = (test_object *)st_rbtree_search_eq(&tree, &tmp.rb_node);
        st_ut_eq(&objects[i], obj, "search object is right");

        node = &objects[i].rb_node;
        st_rbtree_delete(&tree, node);
        st_ut_eq(0, st_rbtree_node_is_inited(node), "");

        obj = (test_object *)st_rbtree_search_eq(&tree, &tmp.rb_node);
        st_ut_eq(NULL, obj, "object has been deleted");

        for (int j = 0; j <= i - 1; j++) {
            tmp.key = objects[j].key;
            obj = (test_object *)st_rbtree_search_eq(&tree, &tmp.rb_node);
            st_ut_eq(&objects[j], obj, "search object is right");
        }
    }
}

st_test(rbtree, delete) {

    int test_object_num[] = {1, 100, 1000};

    for (int i = 0; i < st_nelts(test_object_num); i++) {
        test_delete_with_search(test_object_num[i]);
    }
}

void test_add_with_delete(int object_num, int average_add, int average_delete) {

    st_rbtree_t tree;
    st_rbtree_node_t *node;
    test_object *obj;
    test_object tmp;
    int start;
    int end;
    test_object objects[1000];

    st_rbtree_init(&tree, rbtree_cmp);

    int i = 0;

    //test add some node and delete some node, than search all the node in rbtree
    while (i < object_num) {

        // add average_add num node into rbtree
        for (int j = 0; j < average_add; j++) {
            objects[i].key = i;
            st_rbtree_insert(&tree, &objects[i].rb_node, 1, NULL);
            i++;
        }

        // delete average_delete num node from rbtree
        for (int j = i - average_delete; j < i; j++) {
            node = &objects[j].rb_node;
            st_rbtree_delete(&tree, node);
        }

        for (int j = 0; j < i; j++) {
            start = (j / average_add) * average_add;
            end = start + average_add - average_delete;

            tmp.key = objects[j].key;

            // check the node wether has been deleted
            if (j >= start && j < end) {
                obj = (test_object *)st_rbtree_search_eq(&tree, &tmp.rb_node);
                st_ut_eq(&objects[j], obj, "found object");
            } else {
                obj = (test_object *)st_rbtree_search_eq(&tree, &tmp.rb_node);
                st_ut_eq(NULL, obj, "object has been deleted");
            }
        }
    }
}

st_test(rbtree, add_with_delete) {

    struct case_s {
        int obj_num;
        int average_add;
        int average_delete;
    } cases[] = {
        {100 * 10, 100, 1},
        {100 * 10, 100, 50},
        {100 * 10, 100, 99},

        {10 * 100, 10, 1},
        {10 * 100, 10, 5},
        {10 * 100, 10, 9},
    };


    for (int i = 0; i < st_nelts(cases); i++) {
        st_typeof(cases[0]) c = cases[i];
        test_add_with_delete(c.obj_num, c.average_add, c.average_delete);
    }
}

#define CHECK_SAME_NODE_VALUES(backup, original) do {                          \
    st_typeof(backup) nptr1 = backup;                                          \
    st_typeof(original) nptr2 = original;                                      \
                                                                               \
    st_ut_eq(nptr1->parent, nptr2->parent, "falsely set parent of original");  \
    st_ut_eq(nptr1->left, nptr2->left, "falsely set left of original");        \
    st_ut_eq(nptr1->right, nptr2->right, "falsely set right of original");     \
    st_ut_eq(nptr1->color, nptr2->color, "falsely set color of original");     \
} while(0)

st_test(rbtree, replace) {
    st_rbtree_t tree;
    st_rbtree_node_t *original, *replacement;

    /** test: NULL values */
    st_ut_eq(ST_ARG_INVALID,
             st_rbtree_replace(NULL, NULL, NULL),
             "failed to handle NULL tree");
    st_ut_eq(ST_ARG_INVALID,
             st_rbtree_replace(&tree, NULL, NULL),
             "failed to handle NULL original");
    st_ut_eq(ST_ARG_INVALID,
             st_rbtree_replace(&tree, original, NULL),
             "failed to handle NULL replacement");

    st_rbtree_init(&tree, rbtree_cmp);

    /** test: replace node in empty tree */
    int ret = st_rbtree_replace(&tree, NULL, NULL);
    st_ut_eq(ST_ARG_INVALID, ret, "failed to replace empty tree");

    /** add nodes in tree */
    test_object nodes[] = {
        {.key = 10},
        {.key = 20},
        {.key = 20},
        {.key = 20},
        {.key = 30},
        {.key = 40},
        {.key = 50},
    };

    for (int i = 0; i < st_nelts(nodes); i++) {
        st_rbtree_insert(&tree, &nodes[i].rb_node, 1, NULL);
    }

    /** test: replace sentinel */
    replacement = &tree.sentinel;
    original    = &tree.sentinel;

    st_ut_eq(ST_ARG_INVALID,
             st_rbtree_replace(&tree, original, replacement),
             "failed to handle sentinel original");
    original = &nodes[0].rb_node;
    st_ut_eq(ST_ARG_INVALID,
             st_rbtree_replace(&tree, original, replacement),
             "failed to handle sentinel replacement");

    /** test: nodes not equal */
    test_object obj         = { .key = 100, .rb_node = {NULL, NULL, NULL} };
    replacement             = &obj.rb_node;
    original                = &nodes[0].rb_node;
    st_rbtree_node_t backup = *original;

    st_ut_eq(ST_NOT_EQUAL,
             st_rbtree_replace(&tree, original, replacement),
             "failed to handle non-equal nodes");
    CHECK_SAME_NODE_VALUES(&backup, original);

    /** test: replacement node initialized */
    replacement = &nodes[3].rb_node;
    original    = &nodes[2].rb_node;
    backup      = *original;

    st_ut_eq(ST_ARG_INVALID,
             st_rbtree_replace(&tree, original, replacement),
             "failed to handle replacement not in tree");
    CHECK_SAME_NODE_VALUES(&backup, original);

    /** test: replace every node in tree */
    test_object *new_nodes = (test_object *)malloc(sizeof(nodes));
    memset(new_nodes, 1, sizeof(nodes));
    for (int i = 0; i < st_nelts(nodes); i++) {
        int is_root       = 0;
        int is_left_child = 0;
        new_nodes[i].key  = nodes[i].key;
        replacement       = &new_nodes[i].rb_node;
        *replacement      = (st_rbtree_node_t) st_rbtree_node_empty;
        original          = &nodes[i].rb_node;
        backup            = *original;

        if (tree.root == original) {
            is_root = 1;
        } else if (st_rbtree_is_left_child(original)) {
            is_left_child = 1;
        }

        /** test: check replacement */
        st_ut_eq(ST_OK,
                 st_rbtree_replace(&tree, original, replacement),
                 "failed to replace node");
        st_ut_eq(backup.color, replacement->color, "failed to set replacement color");
        st_ut_eq(backup.left, replacement->left, "failed to set replacement left");
        st_ut_eq(backup.right, replacement->right, "failed to set replacement right");
        st_ut_eq(backup.parent, replacement->parent, "failed to set replacement parent");

        if (is_root) {
            st_ut_eq(replacement, tree.root, "failed to set tree root");
        } else if (is_left_child) {
            st_ut_eq(replacement, backup.parent->left, "failed to set left child");
        } else {
            st_ut_eq(replacement, backup.parent->right, "failed to set right child");
        }

        if (backup.left != &tree.sentinel) {
            st_ut_eq(replacement,
                     backup.left->parent,
                     "failed to set parent of left child");
        }

        if (backup.right != &tree.sentinel) {
            st_ut_eq(replacement,
                     backup.right->parent,
                     "failed to set parent of right child");
        }

        /** test: original node would be cleared */
        st_ut_eq(NULL, original->parent, "failed to set parent of original");
        st_ut_eq(NULL, original->left, "failed to set left of original");
        st_ut_eq(NULL, original->right, "failed to set right of original");
        st_ut_eq(0, original->color, "failed to set color of original");
    }

    free(new_nodes);
}

st_test(rbtree, insert_use_flag) {

    st_rbtree_t tree;
    st_rbtree_node_t *existed_node = NULL;

    st_rbtree_init(&tree, rbtree_cmp);

    test_object obj1 = {.key = 50};
    test_object obj2 = {.key = 50};

    st_ut_eq(ST_OK, st_rbtree_insert(&tree, &obj1.rb_node, 1, NULL), "");

    st_ut_eq(ST_EXISTED, st_rbtree_insert(&tree, &obj2.rb_node, 0, &existed_node), "");
    st_ut_eq(existed_node, &obj1.rb_node, "");

    st_ut_eq(ST_OK, st_rbtree_insert(&tree, &obj2.rb_node, 1, &existed_node), "");
    st_ut_eq(existed_node, &obj1.rb_node, "");

    st_ut_eq(ST_ARG_INVALID, st_rbtree_insert(NULL, &obj2.rb_node, 0, NULL), "");
    st_ut_eq(ST_ARG_INVALID, st_rbtree_insert(&tree, NULL, 0, NULL), "");
    st_ut_eq(ST_ARG_INVALID, st_rbtree_insert(&tree, &obj2.rb_node, 2, NULL), "");
}

st_test(rbtree, get_next) {

    st_rbtree_node_t *node, *next;
    test_object objects[1000];
    st_rbtree_t tree;

    st_rbtree_init(&tree, rbtree_cmp);

    objects[0].rb_node = (st_rbtree_node_t)st_rbtree_node_empty;
    st_ut_eq(NULL, st_rbtree_get_next(&tree, &objects[0].rb_node), "");

    for (int i = 0; i < 1000; i++) {
        objects[i].key = i;
        st_rbtree_insert(&tree, &objects[i].rb_node, 1, NULL);
    }

    for (int i = 0; i < 1000; i++) {
        node = &objects[i].rb_node;

        next = st_rbtree_get_next(&tree, node);
        if (i == 999) {
            st_ut_eq(NULL, next, "");
        } else {
            st_ut_eq(&objects[i + 1].rb_node, next, "");
        }
    }
}

st_ut_main;

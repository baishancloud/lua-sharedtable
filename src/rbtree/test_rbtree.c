#include "unittest/unittest.h"
#include "rbtree.h"

typedef struct test_object {
    st_rbtree_node_t rb_node;
    int64_t key;
} test_object;

static int rbtree_cmp(st_rbtree_node_t *a, st_rbtree_node_t *b) {

    test_object *oa = (test_object *)a;
    test_object *ob = (test_object *)b;

    if (oa->key > ob->key) {
        return 1;
    } else if (oa->key < ob->key) {
        return -1;
    } else {
        return 0;
    }
}

st_test(rbtree, init) {

    st_rbtree_t tree;
    st_rbtree_node_t sentinel;

    st_ut_eq(st_rbtree_init(NULL, &sentinel, rbtree_cmp), ST_ARG_INVALID, "tree arg should not be NULL");

    st_ut_eq(st_rbtree_init(&tree, NULL, rbtree_cmp), ST_ARG_INVALID, "sentine arg should not be NULL");

    st_ut_eq(st_rbtree_init(&tree, &sentinel, NULL), ST_ARG_INVALID, "cmp arg should not be NULL");

    st_ut_eq(st_rbtree_init(&tree, &sentinel, rbtree_cmp), ST_OK, "rbtree_init ok");

    st_ut_eq(tree.root, &sentinel, "root should be sentinel");
    st_ut_eq(tree.sentinel, &sentinel, "sentinel should be sentinel");
    st_ut_eq(tree.cmp, &rbtree_cmp, "compare should be equal rbtree_cmp");
}

st_test(rbtree, min) {

    st_rbtree_t tree;
    st_rbtree_node_t sentinel;
    st_rbtree_node_t *node;

    st_rbtree_init(&tree, &sentinel, rbtree_cmp);

    node = st_rbtree_min(&tree);
    st_ut_eq(node, NULL, "no value, so min get NULl");

    test_object objects[] = {
        {.key = 50},
        {.key = 40},
        {.key = 30},
        {.key = 20},
        {.key = 10},
    };

    for (int i = 0; i < st_nelts(objects); i++) {
        st_rbtree_insert(&tree, &objects[i].rb_node);

        test_object *obj = (test_object *)st_rbtree_min(&tree);

        st_ut_eq(objects[i].key, obj->key, "compare min value");
    }
}

st_test(rbtree, max) {

    st_rbtree_t tree;
    st_rbtree_node_t sentinel;
    st_rbtree_node_t *node;

    st_rbtree_init(&tree, &sentinel, rbtree_cmp);

    node = st_rbtree_max(&tree);
    st_ut_eq(node, NULL, "no value, so max get NULl");

    test_object objects[] = {
        {.key = 10},
        {.key = 20},
        {.key = 30},
        {.key = 40},
        {.key = 50},
    };

    for (int i = 0; i < st_nelts(objects); i++) {
        st_rbtree_insert(&tree, &objects[i].rb_node);

        test_object *obj = (test_object *)st_rbtree_max(&tree);

        st_ut_eq(objects[i].key, obj->key, "compare max value");
    }
}

st_test(rbtree, empty) {

    st_rbtree_t tree;
    st_rbtree_node_t sentinel;

    st_rbtree_init(&tree, &sentinel, rbtree_cmp);

    st_ut_eq(st_rbtree_is_empty(&tree), 1, "rbtree is empty");

    test_object object = {.key = 1};
    st_rbtree_insert(&tree, &object.rb_node);

    st_ut_eq(st_rbtree_is_empty(&tree), 0, "rbtree is not empty");
}

st_test(rbtree, search) {

    st_rbtree_t tree;
    st_rbtree_node_t sentinel;
    test_object *obj;
    test_object tmp;
    test_object objects[] = {
        {.key = 10},
        {.key = 20},
        {.key = 30},
        {.key = 40},
    };

    st_rbtree_init(&tree, &sentinel, rbtree_cmp);

    tmp.key = 11;
    obj = (test_object *)s3_rbtree_search(&tree, &tmp.rb_node);
    st_ut_eq(obj, NULL, "tree is empty");

    for (int i = 0; i < st_nelts(objects); i++) {
        st_rbtree_insert(&tree, &objects[i].rb_node);
    }

    for (int i = 0; i < st_nelts(objects); i++) {
        tmp.key = objects[i].key;
        obj = (test_object *)s3_rbtree_search(&tree, &tmp.rb_node);
        st_ut_eq(obj, &objects[i], "search object is right");
    }

    tmp.key = 12;
    obj = (test_object *)s3_rbtree_search(&tree, &tmp.rb_node);
    st_ut_eq(obj, NULL, "not found key");
}

void test_insert_with_search(int object_num) {

    st_rbtree_t tree;
    st_rbtree_node_t sentinel;
    test_object *obj;
    test_object objects[1000];
    test_object tmp;

    st_rbtree_init(&tree, &sentinel, rbtree_cmp);

    for (int i = 0; i < object_num; i++) {
        objects[i].key = i;

        st_rbtree_insert(&tree, &objects[i].rb_node);

        for (int j = 0; j <= i; j++) {
            tmp.key = objects[j].key;

            obj = (test_object *)s3_rbtree_search(&tree, &tmp.rb_node);

            st_ut_eq(obj, &objects[j], "search object is right");
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
    st_rbtree_node_t sentinel;
    st_rbtree_node_t *node;
    test_object *obj;
    test_object objects[1000];
    test_object tmp;

    st_rbtree_init(&tree, &sentinel, rbtree_cmp);

    for (int i = 0; i < object_num; i++) {
        objects[i].key = i;
        st_rbtree_insert(&tree, &objects[i].rb_node);
    }

    for (int i = object_num-1; i >= 0; i--) {
        tmp.key = objects[i].key;
        obj = (test_object *)s3_rbtree_search(&tree, &tmp.rb_node);
        st_ut_eq(obj, &objects[i], "search object is right");

        node = &objects[i].rb_node;
        st_rbtree_delete(&tree, node);

        obj = (test_object *)s3_rbtree_search(&tree, &tmp.rb_node);
        st_ut_eq(obj, NULL, "object has been deleted");

        for (int j = 0; j <= i-1; j++) {
            tmp.key = objects[j].key;
            obj = (test_object *)s3_rbtree_search(&tree, &tmp.rb_node);
            st_ut_eq(obj, &objects[j], "search object is right");
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
    st_rbtree_node_t sentinel;
    st_rbtree_node_t *node;
    test_object *obj;
    test_object tmp;
    int start;
    int end;
    test_object objects[1000];

    st_rbtree_init(&tree, &sentinel, rbtree_cmp);

    int i = 0;

    while (i < object_num) {

        for (int j = 0; j < average_add; j++) {
            objects[i].key = i;
            st_rbtree_insert(&tree, &objects[i].rb_node);
            i++;
        }

        for (int j = i - average_delete; j < i; j++) {
            node = &objects[j].rb_node;
            st_rbtree_delete(&tree, node);
        }

        for (int j = 0; j < i; j++) {
            start = (j / average_add) * average_add;
            end = start + average_add - average_delete;

            tmp.key = objects[j].key;

            if (j >= start && j < end) {
                obj = (test_object *)s3_rbtree_search(&tree, &tmp.rb_node);
                st_ut_eq(obj, &objects[j], "found object");
            } else {
                obj = (test_object *)s3_rbtree_search(&tree, &tmp.rb_node);
                st_ut_eq(obj, NULL, "object has been deleted");
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
        {100*10, 100, 1},
        {100*10, 100, 50},
        {100*10, 100, 99},

        {10*100, 10, 1},
        {10*100, 10, 5},
        {10*100, 10, 9},
    };


    for (int i = 0; i < st_nelts(cases); i++) {
        st_typeof(cases[0]) c = cases[i];
        test_add_with_delete(c.obj_num, c.average_add, c.average_delete);
    }
}

st_ut_main;

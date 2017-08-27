#include "list.h"
#include "unittest/unittest.h"

typedef struct test_object {
    int x;
    st_list_t node;
} test_object;

void init_list_with_objects(st_list_t *head, test_object *objects, int num) {
    st_list_t *node;

    for (int i = 0; i < num; i++) {
        node = &((objects+i)->node);

        st_list_init(node);
        st_list_insert_last(head, node);
    }
}

void init_list_with_nodes(st_list_t *head, st_list_t*nodes, int num, int tail) {

    for (int i = 0; i < num; i++) {
        st_list_init(nodes+i);
        if (tail == 1) {
            st_list_insert_last(head, nodes+i);
        } else {
            st_list_insert_first(head, nodes+i);
        }
    }
}

st_test(list, init) {
    st_list_t list1;
    st_list_t list2 = ST_LIST_INIT(list2);

    st_list_init(&list1);
    st_ut_eq(list1.prev, &list1, "list1 prev equal list address");
    st_ut_eq(list1.next, &list1, "list1 next equal list address");

    st_ut_eq(list2.prev, &list2, "list2 prev equal list address");
    st_ut_eq(list2.next, &list2, "list2 next equal list address");
}

st_test(list, insert_tail) {
    st_list_t *prt = NULL;
    st_list_t nodes[10];
    st_list_t head = ST_LIST_INIT(head);

    init_list_with_nodes(&head, nodes, 10, 1);

    prt = &head;
    for (int i = 0; i < 10; i++, prt = prt->next) {
        st_ut_eq(prt->next, nodes+i, "list next equal insert node");
        st_ut_eq((nodes+i)->prev, prt, "insert node prev equal last node");
    }

    st_ut_eq(prt->next, &head, "last node next equal head");
    st_ut_eq(head.prev, prt, "head prev equal last node");
}

st_test(list, insert_head) {
    st_list_t *prt = NULL;
    st_list_t nodes[10];
    st_list_t head = ST_LIST_INIT(head);

    init_list_with_nodes(&head, nodes, 10, 0);

    prt = &head;
    for (int i = 0; i < 10; i++, prt = prt->prev) {
        st_ut_eq(prt->prev, nodes + i, "list prev equal insert node");
        st_ut_eq((nodes+i)->next, prt, "insert node next equal last node");
    }

    st_ut_eq(prt->prev, &head, "first node prev equal head");
    st_ut_eq(head.next, prt, "head next equal first node");
}

st_test(list, remove) {
    st_list_t nodes[10];
    st_list_t head = ST_LIST_INIT(head);
    st_list_t removed = ST_LIST_INIT(removed);

    init_list_with_nodes(&head, nodes, 10, 1);

    for (int i = 0; i < 10; i++) {
        removed.prev = (nodes + i)->prev;
        removed.next = (nodes + i)->next;

        st_list_remove(nodes + i);

        st_ut_eq(removed.prev, removed.next->prev, "removed prev node order is right");
        st_ut_eq(removed.next, removed.prev->next, "removed next node order is right");
    }

    st_ut_eq(st_list_empty(&head), 1, "list empty");
}

st_test(list, empty) {
    st_list_t node = {0};
    st_list_t head = ST_LIST_INIT(head);

    st_list_insert_last(&head, &node);

    st_ut_eq(st_list_empty(&head), 0, "list not empty");

    st_list_remove(&node);
    st_ut_eq(st_list_empty(&head), 1, "list empty");
}

st_test(list, move_head) {
    st_list_t nodes[10];
    st_list_t head1 = ST_LIST_INIT(head1);
    st_list_t head2 = ST_LIST_INIT(head2);
    st_list_t moved = ST_LIST_INIT(moved);
    st_list_t *head2_first = NULL;

    init_list_with_nodes(&head1, nodes, 10, 1);

    st_ut_eq(st_list_empty(&head1), 0, "list1 not empty");
    st_ut_eq(st_list_empty(&head2), 1, "list2 empty");

    for (int i = 0; i < 10; i++) {
        moved.prev = (nodes + i)->prev;
        moved.next = (nodes + i)->next;

        head2_first = head2.next;

        st_list_move_head(nodes + i, &head2);

        st_ut_eq(moved.prev, moved.next->prev, "moved prev node order is right");
        st_ut_eq(moved.next, moved.prev->next, "moved next node order is right");

        st_ut_eq((nodes + i)->prev, &head2, "moved prev node new order is right");
        st_ut_eq((nodes + i)->next, head2_first, "moved next node new order is right");
        st_ut_eq(head2.next, nodes + i, "head2 next node order is right");
        st_ut_eq(head2_first->prev, nodes + i, "head2 prev first node order is right");
    }

    st_ut_eq(st_list_empty(&head1), 1, "list1 empty");
    st_ut_eq(st_list_empty(&head2), 0, "list2 not empty");
}

st_test(list, move_tail) {
    st_list_t nodes[10];
    st_list_t head1 = ST_LIST_INIT(head1);
    st_list_t head2 = ST_LIST_INIT(head2);
    st_list_t moved = ST_LIST_INIT(moved);
    st_list_t *head2_tail = NULL;

    init_list_with_nodes(&head1, nodes, 10, 1);

    st_ut_eq(st_list_empty(&head1), 0, "list1 not empty");
    st_ut_eq(st_list_empty(&head2), 1, "list2 empty");

    for (int i = 0; i < 10; i++) {
        moved.prev = (nodes + i)->prev;
        moved.next = (nodes + i)->next;

        head2_tail = head2.prev;

        st_list_move_tail(nodes + i, &head2);

        st_ut_eq(moved.prev, moved.next->prev, "moved prev node order is right");
        st_ut_eq(moved.next, moved.prev->next, "moved next node order is right");

        st_ut_eq((nodes + i)->prev, head2_tail, "moved prev node new order is right");
        st_ut_eq((nodes + i)->next, &head2, "moved next node new order is right");
        st_ut_eq(head2.prev, nodes + i, "head2 prev node order is right");
        st_ut_eq(head2_tail->next, nodes + i, "head2 prev tail next node order is right");
    }

    st_ut_eq(st_list_empty(&head1), 1, "list1 empty");
    st_ut_eq(st_list_empty(&head2), 0, "list2 not empty");
}

st_test(list, for_each) {

    st_list_t nodes[10];
    st_list_t head = ST_LIST_INIT(head);
    int i = 0;
    st_list_t *prt = NULL;

    init_list_with_nodes(&head, nodes, 10, 1);

    st_list_for_each(prt, &head) {
        st_ut_eq(prt, nodes + i, "for each node order is right");
        i++;
    }

    st_ut_eq(i, 10, "list2 not empty");
}

st_test(list, for_each_safe) {

    st_list_t nodes[10];
    st_list_t head = ST_LIST_INIT(head);
    int i = 0;
    st_list_t *prt = NULL;
    st_list_t *next = NULL;

    init_list_with_nodes(&head, nodes, 10, 1);

    st_list_for_each_safe(prt, next, &head) {
        st_ut_eq(prt, nodes + i, "for each node order is right");
        st_list_remove(nodes + i);
        i++;
    }

    st_ut_eq(st_list_empty(&head), 1, "list empty");
    st_ut_eq(i, 10, "list2 not empty");
}

st_test(list, entry) {

    test_object objects[10];
    st_list_t head = ST_LIST_INIT(head);
    test_object *prt = NULL;

    prt = st_list_first_entry(&head, test_object, node);
    st_ut_eq(prt, NULL, "first entry is NULL");

    prt = st_list_last_entry(&head, test_object, node);
    st_ut_eq(prt, NULL, "first entry is NULL");

    init_list_with_objects(&head, objects, 10);

    prt = st_list_first_entry(&head, test_object, node);
    st_ut_eq(prt, objects+0, "first entry is right");

    prt = st_list_last_entry(&head, test_object, node);
    st_ut_eq(prt, objects+9, "last entry is right");
}

st_test(list, for_each_entry) {

    test_object objects[10];
    st_list_t head = ST_LIST_INIT(head);
    int i = 0;
    test_object *prt = NULL;

    init_list_with_objects(&head, objects, 10);

    st_list_for_each_entry(prt, &head, node) {
        st_ut_eq(prt, objects + i, "object is right");
        i++;
    }

}

st_test(list, for_each_entry_safe) {

    test_object objects[10];
    st_list_t head = ST_LIST_INIT(head);
    int i = 0;
    test_object *prt = NULL;
    test_object *next = NULL;

    init_list_with_objects(&head, objects, 10);

    s3_list_for_each_entry_safe(prt, next, &head, node) {
        st_ut_eq(prt, objects + i, "object is right");
        st_list_remove(&(prt->node));
        i++;
    }

    st_ut_eq(st_list_empty(&head), 1, "list empty");
}

st_ut_main;

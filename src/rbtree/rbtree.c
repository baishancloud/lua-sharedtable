#include "rbtree.h"

int st_rbtree_init(st_rbtree_t *tree, st_rbtree_compare_pt cmp) {
    st_must(tree != NULL, ST_ARG_INVALID);
    st_must(cmp != NULL, ST_ARG_INVALID);

    memset(tree, 0, sizeof(*tree));

    st_rbtree_black(&tree->sentinel);

    tree->root = &tree->sentinel;
    tree->cmp = cmp;

    return ST_OK;
}

st_rbtree_node_t *st_rbtree_left_most(st_rbtree_t *tree) {
    st_rbtree_node_t *node = NULL;
    st_rbtree_node_t *sentinel = &tree->sentinel;

    node = tree->root;

    if (node == sentinel) {
        return NULL;
    }

    while (node->left != sentinel) {
        node = node->left;
    }

    return node;
}

st_rbtree_node_t *st_rbtree_right_most(st_rbtree_t *tree) {
    st_rbtree_node_t *node = NULL;
    st_rbtree_node_t *sentinel = &tree->sentinel;

    node = tree->root;

    if (node == sentinel) {
        return NULL;
    }

    while (node->right != sentinel) {
        node = node->right;
    }

    return node;
}

static void st_rbtree_left_rotate(st_rbtree_node_t **root, st_rbtree_node_t *sentinel,
                                  st_rbtree_node_t *node) {
    st_rbtree_node_t  *child;

    /*
     *     (N)           (C)
     *     / \           / \
     *   (X) (C)  ->   (N) (CR)
     *       / \       / \
     *    (CL) (CR)  (X) (CL)
     */

    child = node->right;
    node->right = child->left;

    if (child->left != sentinel) {
        child->left->parent = node;
    }

    child->parent = node->parent;

    if (node == *root) {
        *root = child;

    } else if (st_rbtree_is_left_child(node)) {
        node->parent->left = child;

    } else {
        node->parent->right = child;
    }

    child->left = node;
    node->parent = child;
}

static void st_rbtree_right_rotate(st_rbtree_node_t **root, st_rbtree_node_t *sentinel,
                                   st_rbtree_node_t *node) {
    st_rbtree_node_t  *child;

    child = node->left;
    node->left = child->right;

    if (child->right != sentinel) {
        child->right->parent = node;
    }

    child->parent = node->parent;

    if (node == *root) {
        *root = child;

    } else if (st_rbtree_is_right_child(node)) {
        node->parent->right = child;

    } else {
        node->parent->left = child;
    }

    child->right = node;
    node->parent = child;
}

static int insert_node(st_rbtree_t *tree, st_rbtree_node_t *node, int allow_equal,
                       st_rbtree_node_t **existed_node, int *need_rebalance) {
    st_rbtree_node_t *curr = tree->root;
    st_rbtree_node_t  **p = NULL;
    st_rbtree_node_t *sentinel = &tree->sentinel;

    if (tree->root == sentinel) {
        node->parent = NULL;
        node->left = sentinel;
        node->right = sentinel;
        st_rbtree_black(node);
        tree->root = node;

        *need_rebalance = 0;
        return ST_OK;
    }

    while (1) {

        if (tree->cmp(node, curr) < 0) {
            p = &curr->left;
        } else if (tree->cmp(node, curr) > 0) {
            p = &curr->right;
        } else {
            if (existed_node != NULL) {
                *existed_node = curr;
            }

            if (allow_equal) {
                p = &curr->right;
            } else {
                return ST_EXISTED;
            }
        }

        if (*p == sentinel) {
            break;
        }

        curr = *p;
    }

    *p = node;
    node->parent = curr;
    node->left = sentinel;
    node->right = sentinel;
    st_rbtree_red(node);

    *need_rebalance = 1;
    return ST_OK;
}

int st_rbtree_insert(st_rbtree_t *tree, st_rbtree_node_t *node, int allow_equal,
                     st_rbtree_node_t **existed_node) {
    st_rbtree_node_t  **root, *uncle, *sentinel;
    int ret, need_rebalance;

    st_must(tree != NULL, ST_ARG_INVALID);
    st_must(node != NULL, ST_ARG_INVALID);
    st_must(allow_equal == 0 || allow_equal == 1, ST_ARG_INVALID);

    root = (st_rbtree_node_t **) &tree->root;
    sentinel = &tree->sentinel;

    ret = insert_node(tree, node, allow_equal, existed_node, &need_rebalance);
    if (ret != ST_OK || need_rebalance == 0) {
        return ret;
    }

    /* insert fixup, re-balance tree */

    while (node != *root && st_rbtree_is_red(node->parent)) {

        if (st_rbtree_is_left_child(node->parent)) {
            uncle = node->parent->parent->right;

            if (st_rbtree_is_red(uncle)) {
                st_rbtree_black(node->parent);
                st_rbtree_black(uncle);
                st_rbtree_red(node->parent->parent);
                node = node->parent->parent;

            } else {
                if (st_rbtree_is_right_child(node)) {
                    node = node->parent;
                    st_rbtree_left_rotate(root, sentinel, node);
                }

                st_rbtree_black(node->parent);
                st_rbtree_red(node->parent->parent);
                st_rbtree_right_rotate(root, sentinel, node->parent->parent);
            }

        } else {
            uncle = node->parent->parent->left;

            if (st_rbtree_is_red(uncle)) {
                st_rbtree_black(node->parent);
                st_rbtree_black(uncle);
                st_rbtree_red(node->parent->parent);
                node = node->parent->parent;

            } else {
                if (st_rbtree_is_left_child(node)) {
                    node = node->parent;
                    st_rbtree_right_rotate(root, sentinel, node);
                }

                st_rbtree_black(node->parent);
                st_rbtree_red(node->parent->parent);
                st_rbtree_left_rotate(root, sentinel, node->parent->parent);
            }
        }
    }

    st_rbtree_black(*root);

    return ST_OK;
}

static st_rbtree_node_t *get_left_most(st_rbtree_node_t *node, st_rbtree_node_t *sentinel) {

    while (node->left != sentinel) {
        node = node->left;
    }

    return node;
}

static st_rbtree_node_t *delete_node(st_rbtree_t *tree, st_rbtree_node_t *node) {
    uint8_t red;
    st_rbtree_node_t **root, *successor, *child;
    st_rbtree_node_t *sentinel = &tree->sentinel;

    root = (st_rbtree_node_t **) &tree->root;

    // get successor and his child
    if (node->left == sentinel) {
        successor = node;
        child = node->right;

    } else if (node->right == sentinel) {
        successor = node;
        child = node->left;

    } else {
        successor = get_left_most(node->right, sentinel);
        child = successor->right;
    }

    // delete if node is root
    if (successor == *root) {
        *root = child;
        st_rbtree_black(child);

        node->left = NULL;
        node->right = NULL;
        node->parent = NULL;

        return NULL;
    }

    red = st_rbtree_is_red(successor);

    // replace successor position from his child
    if (st_rbtree_is_left_child(successor)) {
        successor->parent->left = child;

    } else {
        successor->parent->right = child;
    }

    if (successor == node) {

        // Case 1: node to erase has no more than 1 child
        child->parent = successor->parent;

    } else {

        if (successor->parent == node) {
            /*
             * Case 2: node's successor is its right child
             *
             *     (n)          (s)
             *     / \          / \
             *   (x) (s)  ->  (x) (c)
             *         \
             *         (c)
             */

            child->parent = successor;

        } else {
            /*
             *  Case 3: node's successor is leftmost under
             *  node's right child subtree
             *
             *     (n)          (s)
             *     / \          / \
             *   (x) (y)  ->  (x) (y)
             *       /            /
             *     (p)          (p)
             *     /            /
             *   (s)          (c)
             *     \
             *     (c)
             */

            child->parent = successor->parent;
        }

        successor->left = node->left;
        successor->right = node->right;
        successor->parent = node->parent;
        st_rbtree_copy_color(successor, node);

        if (node == *root) {
            *root = successor;

        } else {
            if (st_rbtree_is_left_child(node)) {
                node->parent->left = successor;
            } else {
                node->parent->right = successor;
            }
        }

        if (successor->left != sentinel) {
            successor->left->parent = successor;
        }

        if (successor->right != sentinel) {
            successor->right->parent = successor;
        }
    }

    node->left = NULL;
    node->right = NULL;
    node->parent = NULL;

    if (red) {
        return NULL;
    }

    return child;
}

int st_rbtree_delete(st_rbtree_t *tree, st_rbtree_node_t *node) {
    st_rbtree_node_t **root, *brother, *rebalance, *sentinel;

    st_must(tree != NULL, ST_ARG_INVALID);
    st_must(node != NULL, ST_ARG_INVALID);

    root = (st_rbtree_node_t **) &tree->root;
    sentinel = &tree->sentinel;

    rebalance = delete_node(tree, node);
    if (rebalance == NULL) {
        return ST_OK;
    }

    /* delete fixup, re-balance tree */

    while (rebalance != *root && st_rbtree_is_black(rebalance)) {

        if (st_rbtree_is_left_child(rebalance)) {
            brother = rebalance->parent->right;

            if (st_rbtree_is_red(brother)) {
                st_rbtree_black(brother);
                st_rbtree_red(rebalance->parent);
                st_rbtree_left_rotate(root, sentinel, rebalance->parent);
                brother = rebalance->parent->right;
            }

            if (st_rbtree_is_black(brother->left) && st_rbtree_is_black(brother->right)) {
                st_rbtree_red(brother);
                rebalance = rebalance->parent;

            } else {
                if (st_rbtree_is_black(brother->right)) {
                    st_rbtree_black(brother->left);
                    st_rbtree_red(brother);
                    st_rbtree_right_rotate(root, sentinel, brother);
                    brother = rebalance->parent->right;
                }

                st_rbtree_copy_color(brother, rebalance->parent);
                st_rbtree_black(rebalance->parent);
                st_rbtree_black(brother->right);
                st_rbtree_left_rotate(root, sentinel, rebalance->parent);
                rebalance = *root;
            }

        } else {
            brother = rebalance->parent->left;

            if (st_rbtree_is_red(brother)) {
                st_rbtree_black(brother);
                st_rbtree_red(rebalance->parent);
                st_rbtree_right_rotate(root, sentinel, rebalance->parent);
                brother = rebalance->parent->left;
            }

            if (st_rbtree_is_black(brother->left) && st_rbtree_is_black(brother->right)) {
                st_rbtree_red(brother);
                rebalance = rebalance->parent;

            } else {
                if (st_rbtree_is_black(brother->left)) {
                    st_rbtree_black(brother->right);
                    st_rbtree_red(brother);
                    st_rbtree_left_rotate(root, sentinel, brother);
                    brother = rebalance->parent->left;
                }

                st_rbtree_copy_color(brother, rebalance->parent);
                st_rbtree_black(rebalance->parent);
                st_rbtree_black(brother->left);
                st_rbtree_right_rotate(root, sentinel, rebalance->parent);
                rebalance = *root;
            }
        }
    }

    st_rbtree_black(rebalance);

    return ST_OK;
}

st_rbtree_node_t *st_rbtree_search_eq(st_rbtree_t *tree, st_rbtree_node_t *node) {

    st_must(tree != NULL, NULL);
    st_must(node != NULL, NULL);

    st_rbtree_node_t *curr = tree->root;
    st_rbtree_node_t *sentinel = &tree->sentinel;

    int ret;

    while (curr != sentinel) {

        ret = tree->cmp(node, curr);
        if (ret < 0) {
            curr = curr->left;
        } else if (ret > 0) {
            curr = curr->right;
        } else {
            return curr;
        }
    }

    return NULL;
}

st_rbtree_node_t *st_rbtree_search_le(st_rbtree_t *tree, st_rbtree_node_t *node) {

    st_must(tree != NULL, NULL);
    st_must(node != NULL, NULL);

    st_rbtree_node_t *curr = tree->root;
    st_rbtree_node_t *sentinel = &tree->sentinel;

    st_rbtree_node_t *smaller = NULL;

    int ret;

    while (curr != sentinel) {

        ret = tree->cmp(node, curr);
        if (ret < 0) {
            curr = curr->left;
        } else if (ret > 0) {
            smaller = curr;
            curr = curr->right;
        } else {
            break;
        }
    }

    return curr != sentinel ? curr : smaller;
}

st_rbtree_node_t *st_rbtree_search_ge(st_rbtree_t *tree, st_rbtree_node_t *node) {

    st_must(tree != NULL, NULL);
    st_must(node != NULL, NULL);

    st_rbtree_node_t *curr = tree->root;
    st_rbtree_node_t *sentinel = &tree->sentinel;

    st_rbtree_node_t *bigger = NULL;

    int ret;

    while (curr != sentinel) {

        ret = tree->cmp(node, curr);
        if (ret < 0) {
            bigger = curr;
            curr = curr->left;
        } else if (ret > 0) {
            curr = curr->right;
        } else {
            break;
        }
    }

    return curr != sentinel ? curr : bigger;
}

// node maybe not in tree, so need search node.
st_rbtree_node_t *st_rbtree_search_next(st_rbtree_t *tree, st_rbtree_node_t *node) {

    st_must(tree != NULL, NULL);
    st_must(node != NULL, NULL);

    st_rbtree_node_t *curr = tree->root;
    st_rbtree_node_t *sentinel = &tree->sentinel;

    st_rbtree_node_t *next = NULL;

    int ret;

    while (curr != sentinel) {

        ret = tree->cmp(node, curr);
        if (ret < 0) {
            next = curr;
            curr = curr->left;
        } else if (ret > 0) {
            curr = curr->right;
        } else {
            break;
        }
    }

    if (curr == sentinel || curr->right == sentinel) {
        return next;
    } else {
        return get_left_most(curr->right, sentinel);
    }
}

// node must be in tree, so need search node, just get node next.
st_rbtree_node_t *st_rbtree_get_next(st_rbtree_t *tree, st_rbtree_node_t *node) {

    st_must(tree != NULL, NULL);
    st_must(node != NULL, NULL);

    if (!st_rbtree_node_is_inited(node)) {
        return NULL;
    }

    st_rbtree_node_t *sentinel = &tree->sentinel;

    if (node == sentinel) {
        return NULL;
    }

    if (node->right != sentinel) {
        return get_left_most(node->right, sentinel);
    }

    st_rbtree_node_t *parent, *curr = node;

    while (curr != tree->root) {
        parent = curr->parent;

        if (st_rbtree_is_left_child(curr)) {
            return parent;
        } else {
            curr = parent;
        }
    }

    return NULL;
}

int
st_rbtree_replace(st_rbtree_t *tree,
                  st_rbtree_node_t *original,
                  st_rbtree_node_t *replacement) {
    st_must(tree != NULL, ST_ARG_INVALID);
    st_must(tree->root != &tree->sentinel, ST_ARG_INVALID);

    st_must(original != NULL, ST_ARG_INVALID);
    st_must(replacement != NULL, ST_ARG_INVALID);
    st_must(original != &tree->sentinel, ST_ARG_INVALID);
    st_must(replacement != &tree->sentinel, ST_ARG_INVALID);

    /**
     * keep simple, replacement node must be uninitialized
     */
    if (st_rbtree_node_is_inited(replacement)) {
        return ST_ARG_INVALID;
    }

    if (tree->cmp(original, replacement) != 0) {
        return ST_NOT_EQUAL;
    }

    *replacement = *original;

    if (tree->root == original) {
        tree->root = replacement;

    } else {
        if (st_rbtree_is_left_child(original)) {
            original->parent->left = replacement;

        } else {
            original->parent->right = replacement;

        }
    }

    if (original->left != &tree->sentinel) {
        original->left->parent = replacement;
    }

    if (original->right != &tree->sentinel) {
        original->right->parent = replacement;
    }

    *original = (st_rbtree_node_t) st_rbtree_node_empty;

    return ST_OK;
}

/*
 * Copyright (C) 2006-2012 by Benedict Paten (benedictpaten@gmail.com)
 *
 * Released under the MIT license, see LICENSE.txt
 */

#include "sonLibGlobalsInternal.h"

const char *SORTED_SET_EXCEPTION_ID = "SORTED_SET_EXCEPTION";

////////////////////////////////////////////////
////////////////////////////////////////////////
////////////////////////////////////////////////
//The AVL tree that holds a sorted set
////////////////////////////////////////////////
////////////////////////////////////////////////
////////////////////////////////////////////////

/*
 * A sorted set is an AVL tree whose nodes point to their parents.
 *
 * An iterator holds the node of the item it returned last and the path down to it from the root,
 * so it steps without comparisons, and without waiting on one parent pointer after another to
 * come in from memory, which on a set too big for the cache is several times slower. The set
 * counts the nodes added and removed, which is when the tree can be rearranged, and if that count
 * has changed since an iterator's last step, the iterator rebuilds its path from the parent
 * pointers. So each step goes to the neighbour of the item in the set as it is then, and the set
 * can be changed between steps, provided the item the iterator is on is not removed.
 *
 * A node keeps its balance factor, the height of its right subtree less that of its left, in the
 * two low bits of its parent pointer, which are free because nodes come from malloc. That keeps a
 * node to four words.
 *
 * The rebalancing is the classic one, and a removed node with two children is replaced by its
 * successor, as in the libavl tree that sorted sets used to be built on. So a sequence of
 * operations builds the same tree as it did there, and a search compares the same items in the
 * same order. That matters for a comparison function that is not a consistent ordering, as
 * happens if items are changed while in a set.
 */

typedef struct _stSortedSetNode stSortedSetNode;

struct _stSortedSetNode {
    stSortedSetNode *child[2]; //The left and right subtrees.
    uintptr_t parentAndBalance; //The parent node (NULL at the root), plus 1 + the balance factor.
    void *item;
};

struct _stSortedSet {
    stSortedSetNode *root;
    int64_t size;
    uint64_t generation; //The number of nodes added and removed.
    int (*compareFn)(const void *, const void *);
    void (*destructElementFn)(void *);
    //int numberOfLiveIterators;  // number of currently allocated iterators
};

//The most nodes there can be above a node: an AVL tree with a node any deeper has more than 10^13.
#define ST_SORTED_SET_MAX_DEPTH 64

struct _stSortedSetIterator {
    stSortedSet *sortedSet;
    uint64_t generation; //The set's generation when path was last right.
    stSortedSetNode *node; //The node of the item last returned, or NULL if off either end of the set.
    stSortedSetNode *path[ST_SORTED_SET_MAX_DEPTH]; //The nodes above node, from the root down.
    //Each step writes node and depth, and the next step reads them, along with sortedSet and
    //generation. Kept apart, the compiler does not read two at once, which would be slow, as the
    //read would have to wait for the separate writes to reach the cache.
    int64_t depth; //The number of nodes above node.
};

static stSortedSetNode *getParent(stSortedSetNode *node) {
    return (stSortedSetNode *)(node->parentAndBalance & ~(uintptr_t)3);
}

static void setParent(stSortedSetNode *node, stSortedSetNode *parent) {
    node->parentAndBalance = (uintptr_t)parent | (node->parentAndBalance & 3);
}

static int getBalance(stSortedSetNode *node) {
    return (int)(node->parentAndBalance & 3) - 1;
}

static void setBalance(stSortedSetNode *node, int balance) {
    node->parentAndBalance = (node->parentAndBalance & ~(uintptr_t)3) | (uintptr_t)(balance + 1);
}

/*
 * Puts newChild where oldChild was under parent, or at the root if parent is NULL.
 */
static void replaceChild(stSortedSet *sortedSet, stSortedSetNode *parent, stSortedSetNode *oldChild,
                         stSortedSetNode *newChild) {
    if (parent == NULL) {
        sortedSet->root = newChild;
    } else {
        parent->child[parent->child[1] == oldChild] = newChild;
    }
}

/*
 * Rotates node down to side d (d = 0 is a left rotation), lifting its child on the other side into
 * its place. Balance factors are left to the caller.
 */
static void rotate(stSortedSet *sortedSet, stSortedSetNode *node, int d) {
    stSortedSetNode *parent = getParent(node);
    stSortedSetNode *pivot = node->child[!d];
    stSortedSetNode *middle = pivot->child[d];
    node->child[!d] = middle;
    if (middle != NULL) {
        setParent(middle, node);
    }
    pivot->child[d] = node;
    setParent(node, pivot);
    setParent(pivot, parent);
    replaceChild(sortedSet, parent, node, pivot);
}

/*
 * Rebalances the subtree at node, which is two taller on side d than on the other, when its child
 * on side d leans the other way, by lifting that child's child on the other side to the top.
 * Returns the new root of the subtree.
 */
static stSortedSetNode *rotateTwice(stSortedSet *sortedSet, stSortedSetNode *node, int d) {
    int s = d ? 1 : -1;
    stSortedSetNode *child = node->child[d];
    stSortedSetNode *grandChild = child->child[!d];
    int balance = getBalance(grandChild);
    rotate(sortedSet, child, d);
    rotate(sortedSet, node, !d);
    setBalance(node, balance == s ? -s : 0);
    setBalance(child, balance == -s ? s : 0);
    setBalance(grandChild, 0);
    return grandChild;
}

/*
 * Walks up from a newly added leaf, fixing balance factors, until a subtree is found whose height
 * did not change, rotating where needed (which also stops the growth).
 */
static void rebalanceAfterInsert(stSortedSet *sortedSet, stSortedSetNode *node) {
    stSortedSetNode *parent;
    while ((parent = getParent(node)) != NULL) {
        int d = parent->child[1] == node; //node's subtree, on side d of parent, is one taller
        int s = d ? 1 : -1;
        int balance = getBalance(parent) + s;
        if (balance == 0) {
            setBalance(parent, 0);
            return;
        }
        if (balance == s) {
            setBalance(parent, s);
            node = parent;
            continue;
        }
        if (getBalance(node) == s) {
            rotate(sortedSet, parent, !d);
            setBalance(parent, 0);
            setBalance(node, 0);
        } else {
            rotateTwice(sortedSet, parent, d);
        }
        return;
    }
}

/*
 * Walks up from node, whose subtree on side d has just got one shorter, fixing balance factors and
 * rotating where needed, until a subtree is found whose height did not change.
 */
static void rebalanceAfterRemove(stSortedSet *sortedSet, stSortedSetNode *node, int d) {
    while (node != NULL) {
        int s = d ? 1 : -1;
        int balance = getBalance(node) - s;
        stSortedSetNode *parent = getParent(node);
        stSortedSetNode *root = node; //The root of node's subtree once it is rebalanced.
        if (balance == -s) { //It was balanced, so it is no shorter.
            setBalance(node, -s);
            return;
        }
        if (balance == 0) {
            setBalance(node, 0);
        } else { //It is two taller on the side other than d.
            stSortedSetNode *sibling = node->child[!d];
            int siblingBalance = getBalance(sibling);
            if (siblingBalance == 0) { //One rotation fixes it, without changing its height.
                rotate(sortedSet, node, d);
                setBalance(node, -s);
                setBalance(sibling, s);
                return;
            }
            if (siblingBalance == -s) {
                rotate(sortedSet, node, d);
                setBalance(node, 0);
                setBalance(sibling, 0);
                root = sibling;
            } else {
                root = rotateTwice(sortedSet, node, !d);
            }
        }
        if (parent != NULL) {
            d = parent->child[1] == root;
        }
        node = parent;
    }
}

/*
 * Takes the node out of the tree, without freeing it, and rebalances.
 */
static void removeNode(stSortedSet *sortedSet, stSortedSetNode *node) {
    stSortedSetNode *parent; //Where the rebalancing starts.
    int d; //The side of parent that got shorter.
    if (node->child[0] != NULL && node->child[1] != NULL) {
        //The successor of the node, which has no left child, takes its place.
        stSortedSetNode *successor = node->child[1];
        if (successor->child[0] == NULL) {
            parent = successor;
            d = 1;
        } else {
            do {
                successor = successor->child[0];
            } while (successor->child[0] != NULL);
            parent = getParent(successor);
            d = 0;
            parent->child[0] = successor->child[1];
            if (successor->child[1] != NULL) {
                setParent(successor->child[1], parent);
            }
            successor->child[1] = node->child[1];
            setParent(node->child[1], successor);
        }
        successor->child[0] = node->child[0];
        setParent(node->child[0], successor);
        successor->parentAndBalance = node->parentAndBalance;
        replaceChild(sortedSet, getParent(node), node, successor);
    } else {
        //The node's only child, if it has one, takes its place.
        stSortedSetNode *child = node->child[node->child[0] == NULL];
        parent = getParent(node);
        if (child != NULL) {
            setParent(child, parent);
        }
        if (parent == NULL) {
            sortedSet->root = child;
            return;
        }
        d = parent->child[1] == node;
        parent->child[d] = child;
    }
    rebalanceAfterRemove(sortedSet, parent, d);
}

static stSortedSetNode *findNode(stSortedSet *sortedSet, void *item) {
    stSortedSetNode *node = sortedSet->root;
    while (node != NULL) {
        int cmp = sortedSet->compareFn(item, node->item);
        if (cmp < 0) {
            node = node->child[0];
        } else if (cmp > 0) {
            node = node->child[1];
        } else {
            return node;
        }
    }
    return NULL;
}

/*
 * Returns the greatest item less than the given item if greater is 0, else the least item greater
 * than it, or NULL if there is no such item. If orEqual is non-zero an item equal to the given one
 * is returned in preference.
 */
static void *findNeighbour(stSortedSet *sortedSet, void *item, int greater, int orEqual) {
    void *neighbour = NULL;
    stSortedSetNode *node = sortedSet->root;
    while (node != NULL) {
        int cmp = sortedSet->compareFn(item, node->item);
        if (cmp < 0) {
            if (greater) {
                neighbour = node->item;
            }
            node = node->child[0];
        } else if (cmp > 0) {
            if (!greater) {
                neighbour = node->item;
            }
            node = node->child[1];
        } else if (orEqual) {
            return node->item;
        } else {
            node = node->child[greater];
        }
    }
    return neighbour;
}

/*
 * Returns the last node of the subtree on side d (d = 0 for the first in order).
 */
static stSortedSetNode *getEnd(stSortedSetNode *node, int d) {
    while (node->child[d] != NULL) {
        node = node->child[d];
    }
    return node;
}

/*
 * Makes the iterator's path that of its node in the set as it is now, from the parent pointers.
 */
static void buildPath(stSortedSetIterator *iterator) {
    iterator->generation = iterator->sortedSet->generation;
    iterator->depth = 0;
    if (iterator->node == NULL) {
        return;
    }
    int64_t depth = 0;
    for (stSortedSetNode *node = getParent(iterator->node); node != NULL; node = getParent(node)) {
        depth++;
    }
    assert(depth <= ST_SORTED_SET_MAX_DEPTH);
    iterator->depth = depth;
    for (stSortedSetNode *node = getParent(iterator->node); node != NULL; node = getParent(node)) {
        iterator->path[--depth] = node;
    }
}

/*
 * Moves the iterator to the next node in order if d is 1, or the previous one if d is 0, or to
 * NULL if there is none. From NULL it moves to the first node in order if d is 1, or the last if
 * d is 0.
 */
static inline stSortedSetNode *step(stSortedSetIterator *iterator, int d) {
    if (iterator->generation != iterator->sortedSet->generation) {
        buildPath(iterator);
    }
    //The path is written through the iterator rather than a pointer to it, so that the compiler
    //knows it is not a node's child pointer and need not read those again after each write.
    stSortedSetNode *node = iterator->node;
    int64_t depth = iterator->depth;
    if (node == NULL) { //Down from the root to the end on side !d.
        assert(depth == 0);
        node = iterator->sortedSet->root;
        if (node == NULL) {
            return NULL;
        }
        while (node->child[!d] != NULL) {
            iterator->path[depth++] = node;
            node = node->child[!d];
        }
    } else if (node->child[d] != NULL) { //Down to the end on side !d of the subtree on side d.
        iterator->path[depth++] = node;
        node = node->child[d];
        while (node->child[!d] != NULL) {
            iterator->path[depth++] = node;
            node = node->child[!d];
        }
    } else { //Up to the first ancestor that node is on the other side of.
        while (depth > 0 && iterator->path[depth - 1]->child[d] == node) {
            node = iterator->path[--depth];
        }
        node = depth > 0 ? iterator->path[--depth] : NULL;
    }
    iterator->depth = depth;
    iterator->node = node;
    return node;
}

////////////////////////////////////////////////
////////////////////////////////////////////////
////////////////////////////////////////////////
//Functions on a sorted set and its iterator
////////////////////////////////////////////////
////////////////////////////////////////////////
////////////////////////////////////////////////

static int st_sortedSet_cmpFn( const void *key1, const void *key2 ) {
    return key1 > key2 ? 1 : key1 < key2 ? -1 : 0;
}

/* check if the sorted set is in a state that it can be modified */
static void checkModifiable(stSortedSet *sortedSet) {
    //if (sortedSet->numberOfLiveIterators > 0) {
        //assert(0);
        //stThrowNew(SORTED_SET_EXCEPTION_ID, "attempt to modify an stSortedSet while iterators are active");
    //}
}

stSortedSet *stSortedSet_construct(void) {
    return stSortedSet_construct3(st_sortedSet_cmpFn, NULL);
}

stSortedSet *stSortedSet_construct2(void (*destructElementFn)(void *)) {
    return stSortedSet_construct3(st_sortedSet_cmpFn, destructElementFn);
}

stSortedSet *stSortedSet_construct3(int (*compareFn)(const void *, const void *),
                                      void (*destructElementFn)(void *)) {
    stSortedSet *sortedSet = st_malloc(sizeof(stSortedSet));
    sortedSet->root = NULL;
    sortedSet->size = 0;
    sortedSet->generation = 0;
    sortedSet->compareFn = compareFn == NULL ? st_sortedSet_cmpFn : compareFn;
    sortedSet->destructElementFn = destructElementFn;
    //sortedSet->numberOfLiveIterators = 0;
    return sortedSet;
}

void stSortedSet_setDestructor(stSortedSet *set, void (*destructElement)(void *)) {
    set->destructElementFn = destructElement;
}

stSortedSet *stSortedSet_copyConstruct(stSortedSet *sortedSet, void (*destructElementFn)(void *)) {
    stSortedSet *sortedSet2 = stSortedSet_construct3(sortedSet->compareFn, destructElementFn);
    stSortedSetIterator *it = stSortedSet_getIterator(sortedSet);
    void *o;
    while((o = stSortedSet_getNext(it)) != NULL) {
        stSortedSet_insert(sortedSet2, o);
    }
    stSortedSet_destructIterator(it);
    return sortedSet2;
}

void stSortedSet_destruct(stSortedSet *sortedSet) {
    //Repeatedly take off the first node, which has no left child, replacing it with its right
    //subtree, so that the elements are destructed in order.
    stSortedSetNode *node = sortedSet->root == NULL ? NULL : getEnd(sortedSet->root, 0);
    while (node != NULL) {
        stSortedSetNode *parent = getParent(node), *right = node->child[1];
        if (parent != NULL) {
            parent->child[0] = right;
        }
        if (right != NULL) {
            setParent(right, parent);
        }
        if (sortedSet->destructElementFn != NULL) {
            sortedSet->destructElementFn(node->item);
        }
        free(node);
        node = right != NULL ? getEnd(right, 0) : parent;
    }
    free(sortedSet);
}

void stSortedSet_insert(stSortedSet *sortedSet, void *object) {
    checkModifiable(sortedSet);
    stSortedSetNode *parent = NULL, **link = &sortedSet->root;
    while (*link != NULL) {
        parent = *link;
        int cmp = sortedSet->compareFn(object, parent->item);
        if (cmp < 0) {
            link = &parent->child[0];
        } else if (cmp > 0) {
            link = &parent->child[1];
        } else { //An equal item is already in the set, the new one replaces it.
            parent->item = object;
            return;
        }
    }
    stSortedSetNode *node = st_malloc(sizeof(stSortedSetNode));
    assert(((uintptr_t)node & 3) == 0);
    node->child[0] = NULL;
    node->child[1] = NULL;
    node->parentAndBalance = (uintptr_t)parent;
    setBalance(node, 0);
    node->item = object;
    *link = node;
    sortedSet->size++;
    sortedSet->generation++;
    rebalanceAfterInsert(sortedSet, node);
}

void *stSortedSet_search(stSortedSet *sortedSet, void *object) {
    stSortedSetNode *node = findNode(sortedSet, object);
    return node == NULL ? NULL : node->item;
}

void *stSortedSet_searchLessThanOrEqual(stSortedSet *sortedSet, void *object) {
    return findNeighbour(sortedSet, object, 0, 1);
}

void *stSortedSet_searchLessThan(stSortedSet *sortedSet, void *object) {
    return findNeighbour(sortedSet, object, 0, 0);
}

void *stSortedSet_searchGreaterThanOrEqual(stSortedSet *sortedSet, void *object) {
    return findNeighbour(sortedSet, object, 1, 1);
}

void *stSortedSet_searchGreaterThan(stSortedSet *sortedSet, void *object) {
    return findNeighbour(sortedSet, object, 1, 0);
}

void *stSortedSet_remove(stSortedSet *sortedSet, void *object) {
    checkModifiable(sortedSet);
    stSortedSetNode *node = findNode(sortedSet, object);
    if (node == NULL) {
        return NULL;
    }
    void *item = node->item;
    removeNode(sortedSet, node);
    free(node);
    sortedSet->size--;
    sortedSet->generation++;
    return item;
}

int64_t stSortedSet_size(stSortedSet *sortedSet) {
    return sortedSet->size;
}

void *stSortedSet_getFirst(stSortedSet *items) {
    return items->root == NULL ? NULL : getEnd(items->root, 0)->item;
}

void *stSortedSet_getLast(stSortedSet *items) {
    return items->root == NULL ? NULL : getEnd(items->root, 1)->item;
}

stSortedSetIterator *stSortedSet_getIterator(stSortedSet *items) {
    stSortedSetIterator *iterator;
    iterator = st_malloc(sizeof(stSortedSetIterator));
    iterator->sortedSet = items;
    iterator->node = NULL;
    iterator->generation = items->generation;
    iterator->depth = 0;
    //items->numberOfLiveIterators++;
    return iterator;
}

stSortedSetIterator *stSortedSet_getIteratorFrom(stSortedSet *items, void *item) {
    stSortedSetIterator *iterator = stSortedSet_getIterator(items);
    //Search for the item as findNode does, noting the path down to it.
    stSortedSetNode *node = items->root;
    int64_t depth = 0;
    while (node != NULL) {
        int cmp = items->compareFn(item, node->item);
        if (cmp == 0) {
            break;
        }
        assert(depth < ST_SORTED_SET_MAX_DEPTH);
        iterator->path[depth++] = node;
        if (cmp < 0) {
            node = node->child[0];
        } else {
            node = node->child[1];
        }
    }
    if(node == NULL) {
        stSortedSet_destructIterator(iterator);
        stThrowNew(SORTED_SET_EXCEPTION_ID, "Tried to create an iterator with an item that is not in the list of items");
    }
    iterator->node = node;
    iterator->depth = depth;
    stSortedSet_getPrevious(iterator);
    return iterator;
}

stSortedSetIterator *stSortedSet_getReverseIterator(stSortedSet *items) {
    stSortedSetIterator *it = stSortedSet_getIteratorFrom(items, stSortedSet_getLast(items));
    stSortedSet_getNext(it);
    stSortedSet_getNext(it);
    return it;
}

void stSortedSet_destructIterator(stSortedSetIterator *iterator) {
    //assert(iterator->sortedSet->numberOfLiveIterators > 0);
    //iterator->sortedSet->numberOfLiveIterators--;
    free(iterator);
}

void *stSortedSet_getNext(stSortedSetIterator *iterator) {
    stSortedSetNode *node = step(iterator, 1);
    return node == NULL ? NULL : node->item;
}

stSortedSetIterator *stSortedSet_copyIterator(stSortedSetIterator *iterator) {
    stSortedSetIterator *copyIterator;
    copyIterator = st_malloc(sizeof(stSortedSetIterator));
    memcpy(copyIterator, iterator, sizeof(stSortedSetIterator));
    //copyIterator->sortedSet->numberOfLiveIterators++;
    return copyIterator;
}

void *stSortedSet_getPrevious(stSortedSetIterator *iterator) {
    stSortedSetNode *node = step(iterator, 0);
    return node == NULL ? NULL : node->item;
}

static int stSortedSet_comparatorsEqual(stSortedSet *sortedSet1, stSortedSet *sortedSet2) {
    return sortedSet1->compareFn == sortedSet2->compareFn;
}

int stSortedSet_equals(stSortedSet *sortedSet1, stSortedSet *sortedSet2) {
    if(stSortedSet_size(sortedSet1) != stSortedSet_size(sortedSet2)) {
        return 0;
    }
    if(!stSortedSet_comparatorsEqual(sortedSet1, sortedSet2)) {
        return 0;
    }
    int (*cmpFn)(const void *, const void *) = sortedSet1->compareFn;

    stSortedSetIterator *it1 = stSortedSet_getIterator(sortedSet1);
    stSortedSetIterator *it2 = stSortedSet_getIterator(sortedSet2);
    void *o1 = stSortedSet_getNext(it1), *o2 = stSortedSet_getNext(it2);
    while(o1 != NULL && o2 != NULL) {
        if(cmpFn(o1, o2) != 0) {
            stSortedSet_destructIterator(it1);
            stSortedSet_destructIterator(it2);
            return 0;
        }
        o1 = stSortedSet_getNext(it1);
        o2 = stSortedSet_getNext(it2);
    }
    stSortedSet_destructIterator(it1);
    stSortedSet_destructIterator(it2);
    return 1;
}

stList *stSortedSet_getList(stSortedSet *sortedSet) {
    stList *list = stList_construct2(stSortedSet_size(sortedSet));
    stSortedSetIterator *it = stSortedSet_getIterator(sortedSet);
    void *o;
    int64_t i=0;
    while((o = stSortedSet_getNext(it)) != NULL) {
        stList_set(list, i++, o);
    }
    assert(i == stSortedSet_size(sortedSet));
    stSortedSet_destructIterator(it);
    return list;
}

stSortedSet *stSortedSet_getUnion(stSortedSet *sortedSet1, stSortedSet *sortedSet2) {
    if(!stSortedSet_comparatorsEqual(sortedSet1, sortedSet2)) {
        stThrowNew(SORTED_SET_EXCEPTION_ID, "Comparators are not equal for creating the union of two sorted sets");
    }
    stSortedSet *sortedSet3 = stSortedSet_construct3(sortedSet1->compareFn, NULL);

    //Add those from sortedSet1
    stSortedSetIterator *it= stSortedSet_getIterator(sortedSet1);
    void *o;
    while((o = stSortedSet_getNext(it)) != NULL) {
        stSortedSet_insert(sortedSet3, o);
    }
    stSortedSet_destructIterator(it);

    //Add those from sortedSet2
    it= stSortedSet_getIterator(sortedSet2);
    while((o = stSortedSet_getNext(it)) != NULL) {
        stSortedSet_insert(sortedSet3, o);
    }
    stSortedSet_destructIterator(it);

    return sortedSet3;
}

stSortedSet *stSortedSet_getIntersection(stSortedSet *sortedSet1, stSortedSet *sortedSet2) {
    if(!stSortedSet_comparatorsEqual(sortedSet1, sortedSet2)) {
        stThrowNew(SORTED_SET_EXCEPTION_ID, "Comparators are not equal for creating an intersection of two sorted sets");
    }
    stSortedSet *sortedSet3 = stSortedSet_construct3(sortedSet1->compareFn, NULL);

    //Add those from sortedSet1 only if they are also in sortedSet2
    stSortedSetIterator *it= stSortedSet_getIterator(sortedSet1);
    void *o;
    while((o = stSortedSet_getNext(it)) != NULL) {
        if(stSortedSet_search(sortedSet2, o) != NULL) {
            stSortedSet_insert(sortedSet3, o);
        }
    }
    stSortedSet_destructIterator(it);

    return sortedSet3;
}

stSortedSet *stSortedSet_getDifference(stSortedSet *sortedSet1, stSortedSet *sortedSet2) {
    if(!stSortedSet_comparatorsEqual(sortedSet1, sortedSet2)) {
        stThrowNew(SORTED_SET_EXCEPTION_ID, "Comparators are not equal for creating the sorted set difference");
    }
    stSortedSet *sortedSet3 = stSortedSet_construct3(sortedSet1->compareFn, NULL);

    //Add those from sortedSet1 only if they are not in sortedSet2
    stSortedSetIterator *it= stSortedSet_getIterator(sortedSet1);
    void *o;
    while((o = stSortedSet_getNext(it)) != NULL) {
        if(stSortedSet_search(sortedSet2, o) == NULL) {
            stSortedSet_insert(sortedSet3, o);
        }
    }
    stSortedSet_destructIterator(it);

    return sortedSet3;
}

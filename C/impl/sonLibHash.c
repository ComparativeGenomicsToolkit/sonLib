/*
 * Copyright (C) 2006-2012 by Benedict Paten (benedictpaten@gmail.com)
 *
 * Released under the MIT license, see LICENSE.txt
 */

/*
 * sonLibHash.c
 *
 *  Created on: 4 Apr 2010
 *      Author: benedictpaten
 */
#include "sonLibGlobalsInternal.h"
#include "hashTableC.h"
#include "hashTableC_itr.h"
#include "coucal.h"

struct _stHash {
    coucal hash;
    bool destructKeys, destructValues;
};

struct _stHashIterator {
    struct_coucal_enum e;
};

static coucal_hashkeys hash_uint64(void *arg, const void *data) {
    //printf("Hashing key %p: %" PRIu64 "\n", data, *((uint64_t *) data));
    coucal_hashkeys ret = coucal_hash_data(&data, sizeof(void *));
    printf("Got hash: %" PRIu32 " %" PRIu32 "\n", ret.hash1, ret.hash2);
    return ret;
}

uint64_t stHash_pointer(const void *k) {
    /*if (sizeof(const void *) > 4) {
        int64_t key = (int64_t) (size_t) k;
        key = (~key) + (key << 18); // key = (key << 18) - key - 1;
        key = key ^ (key >> 31);
        key = key * 21; // key = (key + (key << 2)) + (key << 4);
        key = key ^ (key >> 11);
        key = key + (key << 6);
        key = key ^ (key >> 22);
        return (uint64_t) key;
    }*/
    return (uint64_t) (size_t) k; //Just use the low order bits
}

static int stHash_equalKey(void *arg, const void *key1, const void *key2) {
    printf("%p == %p?\n", key1, key2);
    return key1 == key2;
}

stHash *stHash_construct(void) {
    return stHash_construct3(stHash_pointer, stHash_equalKey, NULL, NULL);
}

stHash *stHash_construct2(void(*destructKeys)(void *), void(*destructValues)(void *)) {
    return stHash_construct3(stHash_pointer, stHash_equalKey, destructKeys, destructValues);
}

stHash *stHash_construct3(uint64_t(*hashKey)(const void *), int(*hashEqualsKey)(const void *, const void *), void(*destructKeys)(void *), void(*destructValues)(void *)) {
    stHash *hash = st_malloc(sizeof(stHash));
    hash->hash = coucal_new(0);
    coucal_value_set_key_handler(hash->hash, NULL, NULL, hash_uint64, stHash_equalKey, NULL);
    hash->destructKeys = destructKeys != NULL;
    hash->destructValues = destructValues != NULL;
    return hash;
}

void stHash_destruct(stHash *hash) {
    coucal_delete(&hash->hash);
    free(hash);
}

void stHash_insert(stHash *hash, void *key, void *value) {
    coucal_write_pvoid(hash->hash, key, value);
}

void *stHash_search(stHash *hash, void *key) {
    return coucal_get_pvoid(hash->hash, key);
}

void *stHash_remove(stHash *hash, void *key) {
    return coucal_remove(hash->hash, key);
}

void *stHash_removeAndFreeKey(stHash *hash, void *key) {
    return coucal_remove(hash->hash, key);
}

int64_t stHash_size(stHash *hash) {
    return (int64_t) coucal_nitems(hash->hash);
}

stHashIterator *stHash_getIterator(stHash *hash) {
    stHashIterator *ret = malloc(sizeof(stHashIterator));
    ret->e = coucal_enum_new(hash->hash);
    return ret;
}

void *stHash_getNext(stHashIterator *iterator) {
    coucal_item *item = coucal_enum_next(&iterator->e);
    if (item == NULL) {
        return NULL;
    }
    return item->name;
}

stHashIterator *stHash_copyIterator(stHashIterator *iterator) {
    return NULL;
}

void stHash_destructIterator(stHashIterator *iterator) {
    free(iterator);
}

stList *stHash_getKeys(stHash *hash) {
    /* stList *list = stList_construct(); */
    /* stHashIterator *iterator = stHash_getIterator(hash); */
    /* void *item; */
    /* while ((item = stHash_getNext(iterator)) != NULL) { */
    /*     stList_append(list, item); */
    /* } */
    /* stHash_destructIterator(iterator); */
    /* return list; */
    return NULL;
}

stList *stHash_getValues(stHash *hash) {
    /* stList *list = stList_construct(); */
    /* stHashIterator *iterator = stHash_getIterator(hash); */
    /* void *item; */
    /* while ((item = stHash_getNext(iterator)) != NULL) { */
    /*     stList_append(list, stHash_search(hash, item)); */
    /* } */
    /* stHash_destructIterator(iterator); */
    /* return list; */
    return NULL;
}

/*
 * Useful hash keys/equals functions
 */

uint64_t stHash_stringKey(const void *k) {
    // djb2
    // This algorithm was first reported by Dan Bernstein
    // many years ago in comp.lang.c
    //
    uint64_t hash = 0; //5381;
    int c;
    char *cA;
    cA = (char *) k;
    while ((c = *cA++) != '\0') {
        hash = c + (hash << 6) + (hash << 16) - hash;
        //hash = ((hash << 5) + hash) + c; // hash*33 + c
    }
    return hash;
}

int stHash_stringEqualKey(const void *key1, const void *key2) {
    return strcmp(key1, key2) == 0;
}

stHash *stHash_invert(stHash *hash, uint64_t(*hashKey)(const void *), int(*equalsFn)(const void *, const void *),
        void(*destructKeys)(void *), void(*destructValues)(void *)) {
    /*
     * Inverts the hash.
     */
    stHash *invertedHash = stHash_construct3(hashKey, equalsFn, destructKeys, destructValues);
    stHashIterator *hashIt = stHash_getIterator(hash);
    void *key;
    while ((key = stHash_getNext(hashIt)) != NULL) {
        void *value = stHash_search(hash, key);
        assert(value != NULL);
        if (stHash_search(invertedHash, value) == NULL) {
            stHash_insert(invertedHash, value, key);
        }
    }
    stHash_destructIterator(hashIt);
    return invertedHash;
}
// interface to underlying functions
uint64_t (*stHash_getHashFunction(stHash *hash))(const void *) {
    return NULL;
}
int (*stHash_getEqualityFunction(stHash *hash))(const void *, const void *) {
    return NULL;
}
void (*stHash_getKeyDestructorFunction(stHash *hash))(void *) {
    return NULL;
}
void (*stHash_getValueDestructorFunction(stHash *hash))(void *) {
    return NULL;
}

static int unsigned_cmp(const unsigned *x, const unsigned *y) {
    if (*x < *y) {
        return -1;
    } else if (*x == *y) {
        return 0;
    } else {
        return 1;
    }
}

void stHash_printDiagnostics(stHash *hash) {
    return;
}

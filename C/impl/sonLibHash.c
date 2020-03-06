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
#include "khash.h"

KHASH_MAP_INIT_INT64(map, void *)

struct _stHash {
    khash_t(map) *hash;
    uint64_t(*hashKey)(const void *);
    bool destructKeys, destructValues;
};

uint64_t stHash_pointer(const void *k) {
    // Size doesn't matter; just promote to 64 bits
    uint64_t key = (uint64_t) k;
    
    // Use the hash from <https://stackoverflow.com/a/12996028>
    // We can't just -ull these until C++11, if we're in C++
    key = (key ^ (key >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    key = (key ^ (key >> 27)) * UINT64_C(0x94d049bb133111eb);
    return key ^ (key >> 31);
}

static int stHash_equalKey(const void *key1, const void *key2) {
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
    hash->hash = kh_init(map, hashEqualsKey);
    printf("hash %p, hash->hash %p\n", (void *) hash, (void *) hash->hash);
    hash->hashKey = hashKey;
    hash->destructKeys = destructKeys != NULL;
    hash->destructValues = destructValues != NULL;
    return hash;
}

void stHash_destruct(stHash *hash) {
    kh_destroy(map, hash->hash);
    free(hash);
}

void stHash_insert(stHash *hash, void *key, void *value) {
    int ret;
    if (hash->hashKey == NULL) {
        khiter_t it = kh_put(map, hash->hash, (uint64_t) key, &ret);
        kh_value(hash->hash, it) = value;
    } else {
        khiter_t it = kh_put(map, hash->hash, hash->hashKey(key), &ret);
        kh_value(hash->hash, it) = value;
    }
}

void *stHash_search(stHash *hash, void *key) {
    if (hash->hashKey == NULL) {
        return kh_value(hash->hash, (uint64_t) key);
    } else {
        return kh_value(hash->hash, hash->hashKey(key));
    }
}

void *stHash_remove(stHash *hash, void *key) {
    return NULL;
}

void *stHash_removeAndFreeKey(stHash *hash, void *key) {
    return NULL;
}

int64_t stHash_size(stHash *hash) {
    return 0;
}

stHashIterator *stHash_getIterator(stHash *hash) {
    return NULL;
}

void *stHash_getNext(stHashIterator *iterator) {
    return NULL;
}

stHashIterator *stHash_copyIterator(stHashIterator *iterator) {
    return NULL;
}

void stHash_destructIterator(stHashIterator *iterator) {
    free(iterator);
}

stList *stHash_getKeys(stHash *hash) {
    stList *list = stList_construct();
    stHashIterator *iterator = stHash_getIterator(hash);
    void *item;
    while ((item = stHash_getNext(iterator)) != NULL) {
        stList_append(list, item);
    }
    stHash_destructIterator(iterator);
    return list;
}

stList *stHash_getValues(stHash *hash) {
    stList *list = stList_construct();
    stHashIterator *iterator = stHash_getIterator(hash);
    void *item;
    while ((item = stHash_getNext(iterator)) != NULL) {
        stList_append(list, stHash_search(hash, item));
    }
    stHash_destructIterator(iterator);
    return list;
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

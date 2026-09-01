/* Copyright (C) 2002, 2004 Christopher Clark <firstname.lastname@cl.cam.ac.uk> */

#ifndef __HASHTABLE_PRIVATE_CWC22_H__
#define __HASHTABLE_PRIVATE_CWC22_H__

#include <stddef.h>
#include "hashTableC.h"

#ifdef __cplusplus
extern "C" {
#endif

/*****************************************************************************/

/*
 * Entries are on two lists: "next" chains the entries that share a bucket,
 * while insertNext/insertPrev chain every entry in the table in the order it
 * was inserted.  Iteration walks the insertion list rather than the buckets,
 * so that the order a hash yields its entries does not depend on the numeric
 * value of the keys.  That matters because most hashes here are keyed on
 * pointers: walking the buckets hands entries back in an order that shifts
 * with the heap layout, which makes the output of anything downstream of the
 * iteration differ from run to run.
 */
struct entry
{
    void *k, *v;
    uint64_t h;
    struct entry *next;
    struct entry *insertNext, *insertPrev;
};

struct hashtable {
    uint64_t tablelength;
    struct entry **table;
    uint64_t entrycount;
    uint64_t loadlimit;
    uint64_t primeindex;
    uint64_t (*hashfn) (const void *k);
    int (*eqfn) (const void *k1, const void *k2);
    void (*keyFree)(void *);
    void (*valueFree)(void *);
    struct entry *insertFirst, *insertLast; /* insertion-order list, for iteration */
};

/*****************************************************************************/
/* Insertion-order list maintenance, used by hashTableC.c and hashTableC_itr.c */
static inline void hashtable_insertListAppend(struct hashtable *h, struct entry *e) {
    e->insertNext = NULL;
    e->insertPrev = h->insertLast;
    if (h->insertLast != NULL) {
        h->insertLast->insertNext = e;
    } else {
        h->insertFirst = e;
    }
    h->insertLast = e;
}

static inline void hashtable_insertListRemove(struct hashtable *h, struct entry *e) {
    if (e->insertPrev != NULL) {
        e->insertPrev->insertNext = e->insertNext;
    } else {
        h->insertFirst = e->insertNext;
    }
    if (e->insertNext != NULL) {
        e->insertNext->insertPrev = e->insertPrev;
    } else {
        h->insertLast = e->insertPrev;
    }
}

/*****************************************************************************/
uint64_t
hashP(struct hashtable *h, void *k);

/*****************************************************************************/
/* indexFor
static uint64_t
indexFor(uint64_t tablelength, uint64_t hashvalue) {
    return (hashvalue % tablelength);
}*/

/* Only works if tablelength == 2^N */
/*static UNSIGNED_INT_32
indexFor(UNSIGNED_INT_32 tablelength, UNSIGNED_INT_32 hashvalue)
{
    return (hashvalue & (tablelength - 1u));
}
*/

/*****************************************************************************/
#define freekey(X) free(X) /* this is used by hashTableC_itr */
/*define freekey(X) ; */


/*****************************************************************************/

#ifdef __cplusplus
}
#endif
#endif /* __HASHTABLE_PRIVATE_CWC22_H__*/

/*
 * Copyright (c) 2002, Christopher Clark
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * * Neither the name of the original author; nor the names of any contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

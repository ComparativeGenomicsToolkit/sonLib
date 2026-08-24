/* Copyright (C) 2002, 2004 Christopher Clark  <firstname.lastname@cl.cam.ac.uk> */

#include "hashTableC.h"
#include "hashTablePrivateC.h"
#include "hashTableC_itr.h"
#include <stdlib.h> /* defines NULL */
#include "sonLibGlobalsInternal.h"


/* indexFor */
static uint64_t
indexFor(uint64_t tablelength, uint64_t hashvalue) {
    return (hashvalue % tablelength);
}

/*****************************************************************************/
/* hashtable_iterator    - iterator constructor */

/* Walks the insertion-order list, not the buckets, so that the order does not
 * depend on the numeric value of the keys.  See hashTablePrivateC.h. */
struct hashtable_itr *
hashtable_iterator(struct hashtable *h)
{
    struct hashtable_itr *itr = (struct hashtable_itr *)
        st_malloc(sizeof(struct hashtable_itr));
    if (NULL == itr) return NULL;
    itr->h = h;
    itr->e = h->insertFirst;
    itr->parent = NULL;
    itr->index = 0;
    return itr;
}

/*****************************************************************************/
/* advance - advance the iterator to the next element
 *           returns zero if advanced to end of table */

int
hashtable_iterator_advance(struct hashtable_itr *itr)
{
    if (NULL == itr->e) return 0; /* stupidity check */
    itr->e = itr->e->insertNext;
    return (NULL == itr->e) ? 0 : -1;
}

/*****************************************************************************/
/* remove - remove the entry at the current iterator position
 *          and advance the iterator, if there is a successive
 *          element.
 *          If you want the value, read it before you remove:
 *          beware memory leaks if you don't.
 *          Returns zero if end of iteration. */

int
hashtable_iterator_remove(struct hashtable_itr *itr)
{
    struct entry *remember_e, **pE;
    int ret;

    remember_e = itr->e;

    /* Unlink from the bucket chain.  The iterator walks the insertion list, so
     * the chain predecessor has to be looked up rather than remembered. */
    pE = &(itr->h->table[indexFor(itr->h->tablelength, remember_e->h)]);
    while (*pE != remember_e) {
        pE = &((*pE)->next);
    }
    *pE = remember_e->next;

    /* Advance before unlinking, so the successor is still reachable */
    ret = hashtable_iterator_advance(itr);

    hashtable_insertListRemove(itr->h, remember_e);
    itr->h->entrycount--;
    freekey(remember_e->k);
    free(remember_e);
    return ret;
}

/*****************************************************************************/
int /* returns zero if not found */
hashtable_iterator_search(struct hashtable_itr *itr,
                          struct hashtable *h, void *k)
{
    struct entry *e, *parent;
    unsigned int hashvalue, index;

    hashvalue = hashP(h,k);
    index = indexFor(h->tablelength,hashvalue);

    e = h->table[index];
    parent = NULL;
    while (NULL != e)
    {
        /* Check hash value to short circuit heavier comparison */
        if ((hashvalue == e->h) && (h->eqfn(k, e->k)))
        {
            itr->index = index;
            itr->e = e;
            itr->parent = parent;
            itr->h = h;
            return -1;
        }
        parent = e;
        e = e->next;
    }
    return 0;
}


/*
 * Copyright (c) 2002, 2004, Christopher Clark
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

/*
 * Copyright (C) 2006-2012 by Benedict Paten (benedictpaten@gmail.com)
 *
 * Released under the MIT license, see LICENSE.txt
 */

#include "sonLibGlobalsInternal.h"

const char *RANDOM_EXCEPTION_ID = "RANDOM_EXCEPTION";

/*
 * The generator state is per-thread rather than per-process.  rand() draws from
 * one shared sequence, so threads racing for it get a different interleaving on
 * every run, and anything built from those draws differs run to run.  With the
 * state thread-local, a parallel loop gets reproducible draws by seeding each
 * iteration itself, from the work item rather than from the thread -- see the
 * st_randomSeed calls in cactus's parallel loops.
 *
 * Every thread starts from the same default seed, so a single-threaded program
 * that never calls st_randomSeed sees the same sequence it always did (a fixed
 * one: rand() was never seeded from the clock either).
 */
#if defined(__GNUC__) || defined(__clang__)
#define ST_THREAD_LOCAL __thread
#else
#define ST_THREAD_LOCAL
#endif

static ST_THREAD_LOCAL uint64_t st_randomState = 1;

/* splitmix64 - small, fast, and passes the usual statistical batteries */
static uint64_t st_randomNext(void) {
    uint64_t z = (st_randomState += UINT64_C(0x9E3779B97F4A7C15));
    z = (z ^ (z >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
    z = (z ^ (z >> 27)) * UINT64_C(0x94D049BB133111EB);
    return z ^ (z >> 31);
}

void st_randomSeed(int64_t seed) {
    st_randomState = (uint64_t) seed;
}

int64_t st_randomInt64(int64_t min, int64_t max) {
    int64_t i;
    if (min < INT32_MIN && max > INT32_MAX) { //Possible overflow condition, deal with by switching to doubles
        i = min + (((double)max) - ((double)min)) * st_random();
    }
    else {
        if (max - min < 1) {
            stThrowNew(RANDOM_EXCEPTION_ID, "Range for random int is not positive, min: %" PRIi64 ", max %" PRIi64 "\n", min, max);
        }
        i = min + (max - min) * st_random();
    }
    if(i >= max) {
        //return st_randomInt64(min, max);
        i = max-1;
    }
    assert(i >= min);
    assert(i < max);
    return i;
}

int64_t st_randomInt(int64_t min, int64_t max) {
    return st_randomInt64(min, max);
}

double st_random(void) {
    // 31 bits, the resolution this returned when it was rand()/(RAND_MAX+1.0).
    // Callers have been written against values of that granularity, so keep it.
    double d = (double) (st_randomNext() >> 33) / 2147483648.0; // 2^31
    return d >= 1.0 ? 0.9999 : (d < 0.0 ? 0.0 : d);
}

void *st_randomChoice(stList *list) {
    if(stList_length(list) == 0) {
        stThrowNew(RANDOM_EXCEPTION_ID, "Can not return a random choice from an empty list\n");
    }
    return stList_get(list, st_randomInt(0, stList_length(list)));
}

char stRandom_getRandomNucleotide(bool includeNs, bool useLowerCase, bool useRandomCase) {
    double d = st_random();
    char c = includeNs ? (d >= 0.8 ? 'A' : (d >= 0.6 ? 'T' : (d >= 0.4 ? 'G' : (d >= 0.2 ? 'C' : 'N')))) :
            d >= 0.75 ? 'A' : (d >= 0.5 ? 'T' : (d >= 0.25 ? 'G' : 'C'));
    return useLowerCase || (useRandomCase && st_random() > 0.5) ? tolower(c) : c;
}

char *stRandom_getRandomDNAString(int64_t length, bool includeNs, bool useLowerCase, bool useRandomCase) {
    char *string = st_malloc(sizeof(char) * (length + 1));
    for (int64_t i = 0; i < length; i++) {
        string[i] = stRandom_getRandomNucleotide(includeNs, useLowerCase, useRandomCase);
    }
    string[length] = '\0';
    return string;
}


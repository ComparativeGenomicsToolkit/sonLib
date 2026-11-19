/*
 * Copyright (C) 2006-2012 by Benedict Paten (benedictpaten@gmail.com)
 *
 * Released under the MIT license, see LICENSE.txt
 */

/*
 * sonLibLogging.c
 *
 *  Created on: 24 May 2010
 *      Author: benedictpaten
 */

#define _POSIX_C_SOURCE 1 // needed for fileno()

#include "sonLibGlobalsInternal.h"
#include <errno.h>
// We need features.h to check if __GNU_LIBRARY__ is
// defined. Unfortunately, features.h is only available in glibc. The
// common workaround is to include limits.h, which includes features.h
// in glibc.
#include <limits.h>
#include <stdio.h>
#include <unistd.h>
#if defined(__linux__)
#include <fcntl.h>
#include <sys/resource.h>
#endif
#ifdef __GNU_LIBRARY__
#define USE_BACKTRACE 1
#define MAX_BACKTRACE_DEPTH 256
#include <execinfo.h>
#endif // __GLIBC__

static enum stLogLevel LOG_LEVEL = critical;

static void st_backtrace(void);

#if defined(__linux__)
/*
 * What the kernel says about this process's memory when an allocation fails.  An allocation can
 * fail with plenty of RAM free: the process has run out of memory mappings (vm.max_map_count),
 * reached its address-space limit (ulimit -v), or, under strict overcommit accounting
 * (vm.overcommit_memory = 2), the machine has reserved its whole commit limit.  jemalloc under
 * strict overcommit hits the first one by itself, since it gives freed memory back by remapping
 * it and so splits its mappings (https://github.com/jemalloc/jemalloc/issues/1328), and that
 * otherwise reads as an ordinary out-of-memory.  The allocator has already said no, so nothing
 * here touches the heap: stack buffers, and open/read rather than stdio streams.
 */

// A small file's contents, NUL-terminated, in buf.  Returns the length read, or -1.
static ssize_t st_readSmallFile(const char *path, char *buf, size_t size) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        return -1;
    }
    ssize_t n = read(fd, buf, size - 1);
    close(fd);
    if (n < 0) {
        return -1;
    }
    buf[n] = '\0';
    return n;
}

static int64_t st_readProcInt64(const char *path) {
    char buf[64];
    if (st_readSmallFile(path, buf, sizeof(buf)) <= 0) {
        return -1;
    }
    return strtoll(buf, NULL, 10);
}

// Lines in /proc/self/maps, one per mapping vm.max_map_count counts.
static int64_t st_countMappings(void) {
    int fd = open("/proc/self/maps", O_RDONLY);
    if (fd < 0) {
        return -1;
    }
    char buf[16384];
    int64_t lines = 0;
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        for (ssize_t i = 0; i < n; i++) {
            lines += buf[i] == '\n';
        }
    }
    close(fd);
    return lines;
}

// A "Key:   1234 kB" field of /proc/meminfo or /proc/self/status, in kB, or -1.
static int64_t st_procFieldKb(const char *text, const char *key) {
    const char *p = strstr(text, key);
    return p != NULL ? strtoll(p + strlen(key), NULL, 10) : -1;
}

static double st_gib(double bytes) {
    return bytes / (1024.0 * 1024.0 * 1024.0);
}

// request: the size of the allocation that failed, in bytes
static void st_reportMemoryState(double request) {
    int64_t mappings = st_countMappings();
    int64_t maxMappings = st_readProcInt64("/proc/sys/vm/max_map_count");
    int64_t overcommit = st_readProcInt64("/proc/sys/vm/overcommit_memory");
    int64_t rssKb = -1, vsizeKb = -1;
    char status[8192];
    if (st_readSmallFile("/proc/self/status", status, sizeof(status)) > 0) {
        rssKb = st_procFieldKb(status, "VmRSS:");
        vsizeKb = st_procFieldKb(status, "VmSize:");
    }
    struct rlimit asLimit;
    bool asLimited = getrlimit(RLIMIT_AS, &asLimit) == 0 && asLimit.rlim_cur != RLIM_INFINITY;
    int64_t commitLimitKb = -1, committedKb = -1;
    if (overcommit == 2) {
        char meminfo[8192];
        if (st_readSmallFile("/proc/meminfo", meminfo, sizeof(meminfo)) > 0) {
            commitLimitKb = st_procFieldKb(meminfo, "CommitLimit:");
            committedKb = st_procFieldKb(meminfo, "Committed_AS:");
        }
    }

    fprintf(stderr, "Memory when the allocation failed: rss %.1f GiB, virtual %.1f GiB",
            st_gib(rssKb * 1024.0), st_gib(vsizeKb * 1024.0));
    if (asLimited) {
        fprintf(stderr, " (ulimit -v %.1f GiB)", st_gib((double) asLimit.rlim_cur));
    }
    fprintf(stderr, ", %" PRIi64 " memory mappings (vm.max_map_count %" PRIi64 "), "
            "vm.overcommit_memory %" PRIi64, mappings, maxMappings, overcommit);
    if (commitLimitKb >= 0 && committedKb >= 0) {
        fprintf(stderr, ", machine-wide Committed_AS %.1f GiB of CommitLimit %.1f GiB",
                st_gib(committedKb * 1024.0), st_gib(commitLimitKb * 1024.0));
    }
    fputc('\n', stderr);

    // Other threads keep allocating and freeing while this reads, so "at the limit" allows 1%.
    if (maxMappings > 0 && mappings >= maxMappings - maxMappings / 100) {
        fprintf(stderr, "The process ran out of memory mappings (vm.max_map_count), not out of memory.");
        if (overcommit == 2) {
            fprintf(stderr, " With vm.overcommit_memory = 2, jemalloc gives freed memory back by "
                    "remapping it, which splits the process's mappings until it reaches the limit "
                    "(https://github.com/jemalloc/jemalloc/issues/1328).");
        }
        fprintf(stderr, " A larger vm.max_map_count, or vm.overcommit_memory = 0, on this machine "
                "avoids it.\n");
    } else if (asLimited && vsizeKb >= 0 &&
               vsizeKb * 1024.0 + request >= 0.95 * (double) asLimit.rlim_cur) {
        fprintf(stderr, "The process reached its address-space limit (ulimit -v), which counts "
                "memory reserved as well as memory used.\n");
    } else if (commitLimitKb > 0 && committedKb >= commitLimitKb - commitLimitKb / 20) {
        fprintf(stderr, "This machine has reserved nearly all of its commit limit.  With "
                "vm.overcommit_memory = 2 the kernel refuses new memory once the total reserved by "
                "every process reaches CommitLimit (swap plus vm.overcommit_ratio%% of RAM), "
                "however much is actually in use.\n");
    } else if (overcommit == 2) {
        fprintf(stderr, "Note this machine uses strict overcommit accounting "
                "(vm.overcommit_memory = 2), under which allocations can fail with memory free.\n");
    }
}
#endif

/*
 * st_errnoAbort for a failed allocation of request bytes, with what the kernel says about memory
 * after the message.  When several threads fail at once the first one reports and ends the
 * process, and the rest wait for it rather than exiting under its report.
 */
static void st_allocAbort(double request, const char *format, ...) {
    int allocErrno = errno;
    static volatile int aborting = 0;
    static __thread int abortingHere = 0;
    if (abortingHere) {
        _exit(1);   // failed again inside this report: nothing more can be said
    }
    abortingHere = 1;
    if (__sync_lock_test_and_set(&aborting, 1)) {
        for (;;) {
            pause();
        }
    }
    st_backtrace();
    fprintf(stderr, "ERROR: ");
    va_list ap;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
    fprintf(stderr, ": %s\n", strerror(allocErrno));
#if defined(__linux__)
    st_reportMemoryState(request);
#else
    (void) request;
#endif
    exit(1);
}

void *st_malloc(size_t i) {
    void *j;
    j = malloc(i);
    if (i > 0 && j == NULL) {
        st_allocAbort((double) i, "Malloc failed with a request for: %zu bytes", i);
    }
    return j;
}

void *st_realloc(void *buffer, size_t desiredSize) {
    void *newBuffer = realloc(buffer, desiredSize);
    if(desiredSize > 0 && newBuffer == NULL) {
        int reallocErrno = errno;
        if(buffer != NULL) {
            free(buffer); // Free the old buffer
        }
        errno = reallocErrno;
        st_allocAbort((double) desiredSize, "Realloc failed with a request for: %zu bytes", desiredSize);
    }
    return newBuffer;
}

void *st_calloc(int64_t elementNumber, size_t elementSize) {
    void *k;
    k = calloc(elementNumber, elementSize);
    if (elementNumber > 0 && k == NULL) {
        st_allocAbort((double) elementNumber * (double) elementSize,
                      "Calloc failed with request for %lld lots of %zu bytes",
                      (long long int) elementNumber, elementSize);
    }
    return k;
}

void st_setLogLevelFromString(const char *string) {
    if (string != NULL) {
        char *string2 = stString_copy(string);
        for (int64_t i = 0; i < strlen(string); i++) {
            string2[i] = tolower(string2[i]);
        }
        if (strcmp(string2, "off") == 0) {
            LOG_LEVEL = off;
        } else if (strcmp(string2, "critical") == 0) {
            LOG_LEVEL = critical;
        } else if (strcmp(string2, "error") == 0) {
            LOG_LEVEL = error;
        } else if (strcmp(string2, "warning") == 0) {
            LOG_LEVEL = warning;
        } else if (strcmp(string2, "info") == 0) {
            LOG_LEVEL = info;
        } else if (strcmp(string2, "debug") == 0) {
            LOG_LEVEL = debug;
        } else {
            st_errAbort("Unrecognised logging string %s", string);
        }
        free(string2);
    }
}

void st_setLogLevel(enum stLogLevel level) {
    LOG_LEVEL = level;
}

enum stLogLevel st_getLogLevel(void) {
    return LOG_LEVEL;
}

void st_logCritical(const char *string, ...) {
    if (st_getLogLevel() >= critical) {
        va_list ap;
        va_start(ap, string);
        vfprintf(stderr, string, ap);
        va_end(ap);
    }
}

void st_logInfo(const char *string, ...) {
    if (st_getLogLevel() >= info) {
        va_list ap;
        va_start(ap, string);
        vfprintf(stderr, string, ap);
        va_end(ap);
    }
}

void st_logDebug(const char *string, ...) {
    if (st_getLogLevel() >= debug) {
        va_list ap;
        va_start(ap, string);
        vfprintf(stderr, string, ap);
        va_end(ap);
    }
}

void st_uglyf(const char *string, ...) {
    va_list ap;
    va_start(ap, string);
    vfprintf(stderr, string, ap);
    va_end(ap);
}

int64_t st_system(const char *string, ...) {
    char cA[100000];
    int64_t i;
    va_list ap;
    va_start(ap, string);
    vsprintf(cA, string, ap);
    va_end(ap);
    assert(strlen(cA) < 100000);
    st_logDebug("Running command %s\n", cA);
    i = system(cA);
    //vfprintf(stdout, string, ap);
    return i;
}

static void st_backtrace(void) {
#ifdef USE_BACKTRACE
    // on the stack: this runs when malloc has failed, and from inside st_malloc
    void *backtraceArray[MAX_BACKTRACE_DEPTH];
    int depth = backtrace(backtraceArray, MAX_BACKTRACE_DEPTH);
    backtrace_symbols_fd(backtraceArray, depth, fileno(stderr));
#endif // USE_BACKTRACE
}

void st_errAbort(char *format, ...) {
    st_backtrace();
    fprintf(stderr, "ERROR: ");
    va_list ap;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(1);
}

void st_errnoAbort(char *format, ...) {
    st_backtrace();
    fprintf(stderr, "ERROR: ");
    va_list ap;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
    fprintf(stderr, ": %s\n", strerror(errno));
    exit(1);
}

static int64_t reverse8Bytes(int64_t in) {
    int64_t out;
    char *inByte = (char *) &in;
    char *outByte = (char *) &out;
    // looped for conciseness, hopefully it will be unrolled in
    // practice.
    for (int64_t i = 0; i < sizeof(int64_t); i++) {
        outByte[i] = inByte[sizeof(int64_t) - i];
    }
    return out;
}

int64_t st_nativeInt64FromLittleEndian(int64_t in) {
    static const int64_t one = 1;
    if (*(char *)&one == 0) {
        // big-endian
        return reverse8Bytes(in);
    } else {
        // little-endian
        return in;
    }
}

int64_t st_nativeInt64ToLittleEndian(int64_t in) {
    static const int64_t one = 1;
    if (*(char *)&one == 0) {
        // big-endian
        return reverse8Bytes(in);
    } else {
        // little-endian
        return in;
    }
}

int64_t st_nativeInt64ToBigEndian(int64_t in) {
    static const int64_t one = 1;
    if (*(char *)&one == 0) {
        // big-endian
        return in;
    } else {
        // little-endian
        return reverse8Bytes(in);
    }
}

int64_t st_nativeInt64FromBigEndian(int64_t in) {
    static const int64_t one = 1;
    if (*(char *)&one == 0) {
        // big-endian
        return in;
    } else {
        // little-endian
        return reverse8Bytes(in);
    }
}

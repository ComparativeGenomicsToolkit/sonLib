/*
 * Copyright (C) 2006-2012 by Benedict Paten (benedictpaten@gmail.com)
 *
 * Released under the MIT license, see LICENSE.txt
 */

// fork and waitpid are POSIX.1-2001.  On Linux/glibc they are suppressed when
// compiling with -std=c99 (which sets __STRICT_ANSI__); _GNU_SOURCE restores
// them and must be defined before any system headers.  _GNU_SOURCE is used
// rather than _POSIX_C_SOURCE because it expands the Darwin C level instead of
// restricting it, matching what sonLibFile.c does.
#if defined(__linux__) || (defined(__unix__) && !defined(__APPLE__))
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#endif

#include "sonLibGlobalsTest.h"

// fork and waitpid exist on every Unix-like platform; the test that needs them
// is compiled out only where they do not (e.g. Windows).
#if defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__))
#define ST_TEST_HAVE_FORK 1
#include <unistd.h>
#include <sys/wait.h>
#endif

static char *tempFileDir = "sonLibFileTestTempDir";
static char *tempFileName1 =
        "sonLibFileTestTempDir/sonLibFileTestTempFile1.txt";
static char *tempFileName2 =
        "sonLibFileTestTempDir/sonLibFileTestTempFile2.txt";

static void teardown() {
    if (stFile_exists(tempFileDir)) {
        stFile_rmrf(tempFileDir);
    }
}

static void setup() {
    teardown();

    //Make the temp dir
    stFile_mkdir(tempFileDir);
    //Now the containing files

    //Temp file 1
    FILE *fileHandle = st_fopen(tempFileName1, "w");
    fprintf(fileHandle, "hello world\n");
    fprintf(fileHandle, "foo bar 123456\n");
    fprintf(fileHandle, " \n");
    fprintf(fileHandle, "\n");
    fprintf(fileHandle, "bye bye\n");
    fprintf(fileHandle, "\t");
    fclose(fileHandle);

    //Temp file 2
    fileHandle = st_fopen(tempFileName2, "w");
    fclose(fileHandle);
}

static void test_stFile_getLineFromFile(CuTest *testCase) {
    setup();
    FILE *fileHandle = st_fopen(tempFileName1, "r");
    char *s = stFile_getLineFromFile(fileHandle);
    CuAssertStrEquals(testCase, "hello world", s);
    free(s);
    s = stFile_getLineFromFile(fileHandle);
    CuAssertStrEquals(testCase, "foo bar 123456", s);
    free(s);
    s = stFile_getLineFromFile(fileHandle);
    CuAssertStrEquals(testCase, " ", s);
    free(s);
    s = stFile_getLineFromFile(fileHandle);
    CuAssertStrEquals(testCase, "", s);
    free(s);
    s = stFile_getLineFromFile(fileHandle);
    CuAssertStrEquals(testCase, "bye bye", s);
    free(s);
    s = stFile_getLineFromFile(fileHandle);
    CuAssertStrEquals(testCase, "\t", s);
    free(s);
    CuAssertTrue(testCase, NULL == stFile_getLineFromFile(fileHandle));
    CuAssertTrue(testCase, NULL == stFile_getLineFromFile(fileHandle));
    CuAssertTrue(testCase, NULL == stFile_getLineFromFile(fileHandle));
    CuAssertTrue(testCase, NULL == stFile_getLineFromFile(fileHandle));
    CuAssertTrue(testCase, NULL == stFile_getLineFromFile(fileHandle));
    fclose(fileHandle);
    teardown();
}

static void test_stFile_exists(CuTest *testCase) {
    teardown();
    CuAssertTrue(testCase, !stFile_exists(tempFileDir));
    CuAssertTrue(testCase, !stFile_exists(tempFileName1));
    CuAssertTrue(testCase, !stFile_exists(tempFileName2));
    setup();
    CuAssertTrue(testCase, stFile_exists(tempFileDir));
    CuAssertTrue(testCase, stFile_exists(tempFileName1));
    CuAssertTrue(testCase, stFile_exists(tempFileName2));
    teardown();
}

static void test_stFile_isDir(CuTest *testCase) {
    setup();
    CuAssertTrue(testCase, stFile_isDir(tempFileDir));
    CuAssertTrue(testCase, !stFile_isDir(tempFileName1));
    CuAssertTrue(testCase, !stFile_isDir(tempFileName2));
    teardown();
    stTry {
            stFile_isDir(tempFileDir);
            CuAssertTrue(testCase, 0);
        }
        stCatch(except)
            {
                CuAssertStrEquals(testCase, stExcept_getId(except),
                        ST_FILE_EXCEPTION);
            }stTryEnd;
}

static void test_stFile_getFileNamesInDirectory(CuTest *testCase) {
    setup();
    stList *childFiles = stFile_getFileNamesInDirectory(tempFileDir);
    CuAssertIntEquals(testCase, stList_length(childFiles), 2);
    char *file1 = stFile_pathJoin(tempFileDir, stList_get(childFiles, 0));
    char *file2 = stFile_pathJoin(tempFileDir, stList_get(childFiles, 1));
    CuAssertTrue(testCase, stString_eq(file1, tempFileName1) || stString_eq(file1, tempFileName2));
    CuAssertTrue(testCase, stString_eq(file2, tempFileName1) || stString_eq(file2, tempFileName2));
    CuAssertTrue(testCase, !stString_eq(file1, file2));
    stList_destruct(childFiles);
    free(file1);
    free(file2);
    stTry {
            stFile_getFileNamesInDirectory(tempFileName1);
            CuAssertTrue(testCase, 0);
        }
        stCatch(except)
            {
                CuAssertStrEquals(testCase, stExcept_getId(except),
                        ST_FILE_EXCEPTION);
            }stTryEnd;
    teardown();
    stTry {
            stFile_getFileNamesInDirectory(tempFileDir);
            CuAssertTrue(testCase, 0);
        }
        stCatch(except)
            {
                CuAssertStrEquals(testCase, stExcept_getId(except),
                        ST_FILE_EXCEPTION);
            }stTryEnd;
}

static void test_stFile_pathJoin(CuTest *testCase) {
    char *s = stFile_pathJoin("hello", "world");
    CuAssertStrEquals(testCase, "hello/world", s);
    free(s);
    s = stFile_pathJoin("hello/", "world");
    CuAssertStrEquals(testCase, "hello/world", s);
    free(s);
    s = stFile_pathJoin("", "world");
    CuAssertStrEquals(testCase, "world", s);
    free(s);
    s = stFile_pathJoin("hello", "");
    CuAssertStrEquals(testCase, "hello/", s);
    free(s);
    s = stFile_pathJoin("", "");
    CuAssertStrEquals(testCase, "", s);
    free(s);
}

static void test_st_fclose(CuTest *testCase) {
    setup();
    // a write that succeeds must be left completely alone by the checks
    FILE *fileHandle = st_fopen(tempFileName1, "w");
    fprintf(fileHandle, "hello world\n");
    st_fcheck(fileHandle, tempFileName1);
    fprintf(fileHandle, "bye bye\n");
    st_fclose(fileHandle, tempFileName1);

    fileHandle = st_fopen(tempFileName1, "r");
    char *s = stFile_getLineFromFile(fileHandle);
    CuAssertStrEquals(testCase, "hello world", s);
    free(s);
    s = stFile_getLineFromFile(fileHandle);
    CuAssertStrEquals(testCase, "bye bye", s);
    free(s);
    CuAssertPtrEquals(testCase, NULL, stFile_getLineFromFile(fileHandle));
    fclose(fileHandle);
    teardown();
}

#ifdef ST_TEST_HAVE_FORK
/*
 * The failure the checks exist for: a write that cannot land.  Every write to
 * /dev/full fails with ENOSPC, which is the disk-full case without needing a
 * full disk.  st_fcheck must not return, so the check runs in a child process.
 * /dev/full is a Linux thing; where it is absent the child reports a skip.
 */
static void test_st_fcheck_detectsFailedWrite(CuTest *testCase) {
    fflush(stdout);
    fflush(stderr);
    pid_t pid = fork();
    CuAssertTrue(testCase, pid >= 0);
    if (pid == 0) {
        // no CuAssert in here -- the parent judges this child by its exit status
        FILE *fileHandle = fopen("/dev/full", "w");
        if (fileHandle == NULL) {
            _exit(66); // no /dev/full, reported as a skip below
        }
        fprintf(fileHandle, "this cannot possibly be written\n");
        st_fcheck(fileHandle, "/dev/full");
        _exit(0); // st_fcheck returned, which is the bug this test is for
    }
    int status = 0;
    CuAssertTrue(testCase, waitpid(pid, &status, 0) == pid);
    CuAssertTrue(testCase, WIFEXITED(status));
    if (WEXITSTATUS(status) == 66) {
        return; // /dev/full not present, nothing to assert
    }
    CuAssertIntEquals(testCase, 1, WEXITSTATUS(status));
}
#endif

CuSuite* sonLibFileTestSuite(void) {
    CuSuite* suite = CuSuiteNew();
    SUITE_ADD_TEST(suite, test_stFile_getLineFromFile);
    SUITE_ADD_TEST(suite, test_stFile_pathJoin);
    SUITE_ADD_TEST(suite, test_stFile_exists);
    SUITE_ADD_TEST(suite, test_stFile_isDir);
    SUITE_ADD_TEST(suite, test_stFile_getFileNamesInDirectory);
    SUITE_ADD_TEST(suite, test_st_fclose);
#ifdef ST_TEST_HAVE_FORK
    SUITE_ADD_TEST(suite, test_st_fcheck_detectsFailedWrite);
#endif

    return suite;
}

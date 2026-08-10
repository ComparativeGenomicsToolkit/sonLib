/*
 * Copyright (C) 2006-2012 by Benedict Paten (benedictpaten@gmail.com)
 *
 * Released under the MIT license, see LICENSE.txt
 */

/*
 * sonLibFile.h
 *
 *  Created on: 7 Sep 2010
 *      Author: benedictpaten
 */

#ifndef SONLIBFILE_H_
#define SONLIBFILE_H_

#include "sonLibTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

//The exception string
extern const char *ST_FILE_EXCEPTION;

/*
 * Reads line into buffer *s with length *n, reallocating buffer size if needed
 */
int64_t stFile_getLineFromFileWithBuffer(char **s, int64_t *n, FILE *f);

 /*
  * As stFile_getLineFromFileWithBuffer, but using non-thread safe functions.
  */
 int64_t stFile_getLineFromFileWithBufferUnlocked(char **s, int64_t *n, FILE *f);


/*
 * Reads a line from a file (which may be terminated by a newline char or EOF),
 * returning the line excluding the newline character.
 * If the file has hit the EOF then it returns NULL.
 */
char *stFile_getLineFromFile(FILE *fileHandle);

 /*
  * As stFile_getLineFromFile, but using non-thread safe functions. Sometimes much faster
  * than stFile_getLineFromFile.
  */
 char *stFile_getLineFromFileUnlocked(FILE *fileHandle);

/*
 * Reads the line from the give file, returning lines in a list in order. Newlines/EOF characters are removed from the lines.
 */
stList *stFile_getLinesFromFile(char *fileName);

/*
 * Joins together two strings.
 */
char *stFile_pathJoin(const char *pathPrefix, const char *pathSuffix);

/*
 * Returns non-zero iff the file exists.
 */
bool stFile_exists(const char *fileName);

/*
 * Returns non-zero iff the file is a directory. Raises an exception if the file does not exist.
 */
bool stFile_isDir(const char *fileName);

/*
 * Get list of file names (as strings) in a directory. Raises an exception if dir is not a directory.
 */
stList *stFile_getFileNamesInDirectory(const char *dir);

/*
 * Creates a directory with 777 access permissions, throws exceptions if unsuccessful.
 */
void stFile_mkdir(const char *dirName);

/*
 * Creates a directory with 777 access permissions, throws exceptions if unsuccessful.
 */
void stFile_mkdir(const char *dirName);

/*
 * Creates a directory and missing parents with 777 access permissions, throws exceptions if unsuccessful.
 */
void stFile_mkdirp(const char *dirName);

/*
 * Forceably remove a file. If a dir, removes dir and children. Be careful.
 * The file must exist.
 */
void stFile_rmrf(const char *fileName);

/*
 * Forceably remove a file. If a dir, removes dir and children. Be careful.
 * The file does not have to exist.
 */
void stFile_rmtree(const char *fileName);

/*
 * Thin wrapper around fopen that dies with a perror if the operation
 * fails. Meant to be more informative than a random segfault later
 * down the road.
 */
FILE *st_fopen(const char *fileName, const char *mode);

/*
 * Die if an output file has failed to write.  stdio reports a failed write --
 * a full disk, a quota, a read-only mount -- by setting the stream's error
 * indicator, and nothing is obliged to look at it, so without a check a
 * truncated output file is indistinguishable from a complete one and the
 * program still exits successfully.  A short fasta is still a valid fasta.
 *
 * The flush is part of the check rather than an optimisation: the buffer is
 * not necessarily handed to the operating system until it happens, so a
 * stream that has already lost data can still look clean beforehand.
 *
 * Use on stdout, or mid-stream, wherever the file is not being closed here.
 */
void st_fcheck(FILE *fileHandle, const char *fileName);

/*
 * st_fcheck followed by a close whose return value is also checked.  Closing
 * is the last point at which buffered data reaches the operating system, so a
 * write that fails there is reported nowhere else.  Prefer this to a bare
 * fclose on any file the program has written.
 */
void st_fclose(FILE *fileHandle, const char *fileName);

#ifdef __cplusplus
}
#endif
#endif /* SONLIBFILE_H_ */

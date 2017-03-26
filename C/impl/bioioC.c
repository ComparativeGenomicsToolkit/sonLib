/*
 * Copyright (C) 2006-2012 by Benedict Paten (benedictpaten@gmail.com)
 *
 * Released under the MIT license, see LICENSE.txt
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <float.h>

#include "fastCMaths.h"
#include "commonC.h"
#include "ctype.h"
#include "bioioC.h"

/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////
//integer reader / writer
/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////

void readIntegers(FILE *file, int64_t intNumber, int64_t *iA) {
    int64_t i;

    for(i=0; i<intNumber; i++) {
        int j = fscanf(file, INT_STRING, iA + i);
        (void)j;
        assert(j == 1);
    }
}

void writeIntegers(FILE *file, int64_t intNumber, int64_t *iA) {
    int64_t i;

    for(i=0; i<intNumber; i++) {
        fprintf(file, INT_STRING "\n", iA[i]);
    }
}

/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////
//double reader / writer
/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////

void readDoubles(const char *string, int64_t number, double *dA) {
    FILE *fileHandle;
    char *tempFile;
    int64_t i;

    tempFile = getTempFile(); //This is kind of a hack.
    fileHandle = fopen(tempFile, "w");
    fprintf(fileHandle, "%s", string);
    fclose(fileHandle);
    fileHandle = fopen(tempFile, "r");
    for(i=0; i<number; i++) {
        int j = fscanf(fileHandle, "%lf", &(dA[i]));
        (void)j;
        assert(j == 1);
    }
    fclose(fileHandle);
    removeTempFile(tempFile);
}

/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////
//fasta reader/writer
/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////

char *fastaNormaliseHeader(const char *fastaHeader) {
    /*
     * Removes white space which is treated weirdly by many programs.
     */
    const char *c;
    int64_t i, j;
    char *c2;

    i = 0;
    c = fastaHeader;
    while(c[0] != ' ' && c[0] != '\t' && c[0] != '\n' && c[0] != '\0') {
        c++;
        i++;
    }
    c2 = st_malloc(sizeof(char)*(i+1));
    for(j=0; j<i; j++) {
        c2[j] = fastaHeader[j];
    }
    c2[i] = '\0';
    return c2;
}

/*
 * Decodes the fasta header
 */
stList *fastaDecodeHeader(const char *fastaHeader) {
    stList *attributes = stList_construct3(0, free);
    char *cA = stString_copy(fastaHeader);
    char *prev = cA;
    char *tok = strchr(cA, '|');
    while (tok != NULL) {
        *tok = '\0';
        stList_append(attributes, stString_copy(prev));
        prev = tok + 1;
        tok = strchr(prev, '|');
    }
    stList_append(attributes, stString_copy(prev));
    free(cA);
    return attributes;
}

/*
 * Encodes the fasta header
 */
char *fastaEncodeHeader(stList *attributes) {
    return stString_join2("|", attributes);
}

void fastaWrite(char *sequence, char *header, FILE *file) {
    fprintf(file, ">%s\n", header);
    int64_t lineLength = 100;
    char line[lineLength+1];
    int64_t k = strlen(sequence);
    for(int64_t i=0; i<k;i += lineLength) {
        int64_t l = i+lineLength > k ? k-i : lineLength;
        memcpy(line, sequence+i, sizeof(char)*l);
        line[l] = '\0';
        for(int64_t m=0; m<l; m++) {
            char j = line[l];
            if(isalpha(j) && j != '-') {
                //For safety and sanity I only allows roman alphabet characters and gaps in fasta sequences.
                st_errAbort("Got an unexpected character in output fasta sequence: %c, position %" PRIi64 "\n", j, i+m);
            }
        }
        fprintf(file, "%s\n", line);
    }
}

char *addSeqToList(char *seq, int64_t *length, int64_t *maxLength, char *fastaName, void (*addSeq)(const char *, const char *, int64_t)) {
    seq = arrayPrepareAppend(seq, maxLength, *length+1, sizeof(char));
    seq[*length] = '\0';
    addSeq(fastaName, seq, *length);
    (*length)  = 0;
    return seq;
}

void fastaReadToFunction(FILE *fastaFile, void (*addSeq)(const char *, const char *, int64_t)) {
    //reads in group of sequences INT_32o lists
    char j;
    int64_t k;
    static char *seq;
    static int64_t seqLength;
    char cA[STRING_ARRAY_SIZE];
    int64_t l;

    k=0;
    while((j = getc(fastaFile)) != EOF) { //initial terminating characters
        if(j == '>') { //fasta start
            fastaStart:
            cA[0] = '\0';
            l = 0;
            while(TRUE) {
                j = getc(fastaFile);
                if(j == EOF) {
                    seq = addSeqToList(seq, &k, &seqLength, cA, addSeq); //lax qualification for a sequence
                    return;
                }
                if(j == '\n')  {
                    break;
                }
                //add to name string
                cA[l++] = j;
                cA[l] = '\0';
            }
            while(TRUE) { //start of sequence
                j = getc(fastaFile);
                if(j == EOF) {
                    seq = addSeqToList(seq, &k, &seqLength, cA, addSeq);
                    return;
                }
                if(j != '\n' && j != ' ' && j != '\t') {
                    if(j == '>') {
                        //end of seq
                        seq = addSeqToList(seq, &k, &seqLength, cA, addSeq);
                        goto fastaStart;
                    }
                    else { //valid char
                        if(!isalpha(j) && j != '-') {
                             //For safety and sanity I only allows roman alphabet characters and gaps in fasta sequences.
                             st_errAbort("!!Got an unexpected character in input fasta sequence: '%c' \n", j);
                        }
                        seq = arrayPrepareAppend(seq, &seqLength, k+1, sizeof(char));
                        seq[k++] = j;
                    }
                }
            }
        }
    }
}

struct List *fastaRead_fastaNames;
struct List *fastaRead_seqs;
struct List *fastaRead_seqLengths;

void fastaRead_function(const char *fastaHeader, const char *sequence, int64_t length) {
    listAppend(fastaRead_fastaNames, stString_copy(fastaHeader));
    listAppend(fastaRead_seqs, stString_copy(sequence));
    listAppend(fastaRead_seqLengths, constructInt(length));
}

void fastaRead(FILE *fastaFile, struct List *seqs, struct List *seqLengths, struct List *fastaNames) {
    fastaRead_fastaNames = fastaNames;
    fastaRead_seqs = seqs;
    fastaRead_seqLengths = seqLengths;
    fastaReadToFunction(fastaFile, fastaRead_function);
}

/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////
//useful scanf functions
/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////


char *eatWhiteSpace(char *string) {
    while(*string != '\0' && isspace(*string)) {
        string++;
    }
    return string;
}

int64_t parseInt(char **string, int64_t *j) {
    /*
     * Parses a string from the input string, and moves up the pointer.
     * Returns 1 for success, 0 for failures, no action on the pointer occurs if failed.
     */
    int64_t i;

    i = sscanf(*string, INT_STRING, j);
    if(i == 1) {
        *string = eatWhiteSpace(*string);
        while(*string != '\0' && !isspace(**string)) {
            (*string)++;
        }
        *string = eatWhiteSpace(*string);
        return 1;
    }
    else {
        return 0;
    }
}

int64_t parseFloat(char **string, float *j) {
    /*
     * Parses a string from the input string, and moves up the pointer.
     * Returns 1 for success, 0 for failures, no action on the pointer occurs if failed.
     */
    int64_t i;

    i = sscanf(*string, "%f", j);
    if(i == 1) {
        *string = eatWhiteSpace(*string);
        while(*string != '\0' && !isspace(**string)) {
            (*string)++;
        }
        *string = eatWhiteSpace(*string);
        return 1;
    }
    else {
        return 0;
    }
}

int64_t parseString(char **string, char *cA) {
    /*
     * Parses a string from the input string, and moves up the pointer.
     * Returns 1 for success, 0 for failures, no action on the pointer occurs if failed.
     */
    int64_t i;

    i = sscanf(*string, "%s", cA);
    if(i == 1) {
        *string = eatWhiteSpace(*string);
        while(*string != '\0' && !isspace(**string)) {
            (*string)++;
        }
        *string = eatWhiteSpace(*string);
        return 1;
    }
    else {
        return 0;
    }
}

/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////
//newick tree parser
/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////


char* eatString(char *string, char **newString) {
    char *i;

    i = string;
    while(*string != '\0' && !isspace(*string)) {
           string++;
    }
    (*newString) = memcpy(st_malloc((string-i + 1)*sizeof(char)), i, (string-i)*sizeof(char));
    (*newString)[string - i] = '\0';
    return eatWhiteSpace(string);
}

char *replaceString(char *oldString, char old, char *new, int64_t newLength) {
    char *i;
    int64_t j;
    char *k;
    char *newString;
    j=0;
    k=oldString;
    for(i=oldString; *i != '\0'; i++) {
        if(*i == old) {
            j++;
        }
    }
    newString = st_malloc(sizeof(char)*(j*(newLength-1) + (i-k) + 1));
    k=newString;
    for(i=oldString; *i != '\0'; i++) {
        if(*i == old) {
            for(j=0; j<newLength; j++) {
                *k++ = new[j];
            }
        }
        else {
            *k++ = *i;
        }
    }
    *k = '\0';
    return newString;
}

char *replaceAndFreeString(char *oldString, char old, char *new, int64_t newLength) {
    char *i;

    i = replaceString(oldString, old, new, newLength);
    free(oldString);
    return i;
}


char *_parseFloat(char *string, float *f) {
    float f2;

    f2 = FLT_MIN;
    int j = sscanf(string, "%g", &f2);
    (void)j;
    assert(j == 1);
    assert(f2 != FLT_MIN);
    *f = f2;
    while(*string != '\0' && !isspace(*string)) {
        string++;
    }
    return eatWhiteSpace(string);
}

char *newickTreeParser_fn(char *newickTreeString, float *distance) {
    if(*newickTreeString != '\0') {
        if (*newickTreeString == ':') {
            newickTreeString++;
            return _parseFloat(eatWhiteSpace(newickTreeString), distance);
        }
    }
    return newickTreeString;
}

static char *newickTreeParser_getLabel(char *newickTreeString, char **label) {
    if(*newickTreeString != ':' && *newickTreeString != ',' && *newickTreeString != ';' && *newickTreeString != ')' && *newickTreeString != '\0') {
        return eatString(newickTreeString, label);
    }
    *label = stString_copy("");
    return newickTreeString;
}

char *newickTreeParser_fn2(char *newickTreeString, float defaultDistance, struct BinaryTree **binaryTree, int64_t unaryNodes) {
    struct BinaryTree *temp1;
    struct BinaryTree *temp2;
    float f;
    int64_t leaves;

    temp1 = NULL;
    temp2 = NULL;

    if(*newickTreeString == '(') {
        temp1 = NULL;
        newickTreeString = eatWhiteSpace(++newickTreeString);
        assert(*newickTreeString != ')');
        leaves = 0;
        while(1) {
            leaves++;
            newickTreeString = newickTreeParser_fn2(newickTreeString, defaultDistance, &temp2, unaryNodes);
            if(temp1 != NULL) {
                //merge node
                temp1 = constructBinaryTree(0.0f, TRUE, "", temp1, temp2); //default to zero distance for nodes of
            }
            else {
                temp1 = temp2;
            }
            if(*newickTreeString == ',') {
                newickTreeString = eatWhiteSpace(++newickTreeString);
            }
            else {
                assert(*newickTreeString == ')');
                break;
            }
        }
        if(unaryNodes && leaves == 1) { //make a unary node
            temp1 = constructBinaryTree(0.0f, TRUE, "", temp1, NULL);
        }
        else { //eat any label
            char *cA;
            newickTreeString = newickTreeParser_getLabel(newickTreeString, &cA);
            free(cA);
        }
        newickTreeString = eatWhiteSpace(++newickTreeString);
    }
    else {
        temp1 = constructBinaryTree(0.0f, FALSE, "", NULL, NULL);
    }
    free(temp1->label);
    newickTreeString = newickTreeParser_getLabel(newickTreeString, &temp1->label);
    f = defaultDistance;
    newickTreeString = newickTreeParser_fn(newickTreeString, &f);
    temp1->distance += f;
    (*binaryTree) = temp1;
    assert(*newickTreeString == ',' || *newickTreeString == ';' || *newickTreeString == ')' || *newickTreeString == '\0');

    return newickTreeString;
}

struct BinaryTree *newickTreeParser(char *newickTreeString, float defaultDistance, int64_t unaryNodes) {
    struct BinaryTree *binaryTree;
    char *i;
    //lax newick tree parser
    newickTreeString = replaceString(newickTreeString, '(', " ( ", 3);
    newickTreeString = replaceAndFreeString(newickTreeString, ')', " ) ", 3);
    newickTreeString = replaceAndFreeString(newickTreeString, ':', " : ", 3);
    newickTreeString = replaceAndFreeString(newickTreeString, ',', " , ", 3);
    newickTreeString = replaceAndFreeString(newickTreeString, ';', " ; ", 3);
    i = newickTreeString;
    newickTreeString = eatWhiteSpace(newickTreeString);
    newickTreeParser_fn2(newickTreeString, defaultDistance, &binaryTree, unaryNodes);
    free(i);

    return binaryTree;
}

/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////
//read in fasta file, and turn into a column alignment
/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////

struct CharColumnAlignment *multiFastaRead(char *fastaFile) {
    struct List *seqs;
    struct List *seqLengths;
    struct List *fastaNames;
    FILE *fileHandle;
    int64_t alignmentLength = 0;
    struct CharColumnAlignment *charColumnAlignment;
    int64_t i;
    int64_t j;
    int64_t k;

    seqs = constructEmptyList(0, free);
    seqLengths = constructEmptyList(0, free);
    fastaNames = constructEmptyList(0, free);
    fileHandle = fopen(fastaFile, "r");
    fastaRead(fileHandle, seqs, seqLengths, fastaNames);
    fclose(fileHandle);

    alignmentLength = 0;
    if(seqLengths->length != 0) {
        alignmentLength = listGetInt(seqLengths, 0);
    }
    for(i=0; i<seqLengths->length; i++) {
        assert(alignmentLength == listGetInt(seqLengths, 0));
    }
    charColumnAlignment = st_malloc(sizeof(struct CharColumnAlignment));
    charColumnAlignment->columnNo = alignmentLength;
    charColumnAlignment->seqNo = seqLengths->length;
    charColumnAlignment->columnAlignment = st_malloc(sizeof(char)*(charColumnAlignment->columnNo)*(charColumnAlignment->seqNo));
    k=0;
    for(i=0; i<alignmentLength; i++) {
        for(j=0; j<seqLengths->length; j++) {
            charColumnAlignment->columnAlignment[k++] = ((char *)seqs->list[j])[i];
        }
    }
    destructList(seqs);
    destructList(seqLengths);
    destructList(fastaNames);
    return charColumnAlignment;
}

char *charColumnAlignment_getColumn(struct CharColumnAlignment *charColumnAlignment, int64_t col) {
    return &charColumnAlignment->columnAlignment[col*charColumnAlignment->seqNo];
}

void destructCharColumnAlignment(struct CharColumnAlignment *charColumnAlignment) {
    free(charColumnAlignment->columnAlignment);
    free(charColumnAlignment);
}

/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////
//Get line function, while getline is not standard.
/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////

int64_t benLine(char **s, int64_t *n, FILE *f) {
    register int64_t nMinus1= ((*n)-1), i= 0;

    char *s2 = *s;
    while(TRUE) {
        register int64_t ch = (char)getc(f);

        if(ch == '\r') {
            ch= getc(f);
        }

        if(i == nMinus1) {
            *n = 2*(*n) + 1;
            *s = realloc(*s, (*n + 1)*sizeof(char));
            assert(*s != NULL);
            s2 = *s + i;
            nMinus1 = ((*n)-1);
        }

        if((ch == '\n') || (ch == EOF)) {
            *s2 = '\0';
            return(feof(f) ? -1 : i);
        }
        else {
            *s2 = ch;
            s2++;
        }
        ++i;
    }
    return 0;
}

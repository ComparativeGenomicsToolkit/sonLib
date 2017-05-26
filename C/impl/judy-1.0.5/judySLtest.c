#include <stdio.h>
#include <Judy.h>

#define MAXLINE 1000000                 // max string (line) length

uint8_t   Index[MAXLINE];               // string to insert

int     // Usage:  JudySort < file_to_sort
main()
{
    Pvoid_t   PJArray = (PWord_t)NULL;  // Judy array.
    PWord_t   PValue;                   // Judy array element.
    Word_t    Bytes;                    // size of JudySL array.

    while (fgets(Index, MAXLINE, stdin) != (char *)NULL)
    {
        if (strcmp(Index, "") == 10){
            break;
        }
        JSLI(PValue, PJArray, Index);   // store string into array
        if (PValue == PJERR)            // if out of memory?
        {                               // so do something
            printf("Malloc failed -- get more ram\n");
            exit(1);
        }
        //printf("%lu", *PValue);
        ++(*PValue);                    // count instances of string
    }

    Index[0] = '\0';                    // start with smallest string.
    JSLF(PValue, PJArray, Index);       // get first string
    while (PValue != NULL)
    {
        while ((*PValue)--)             // print duplicates
            printf("%s", Index);
        JSLN(PValue, PJArray, Index);   // get next string
    }
    JSLFA(Bytes, PJArray);              // free array

    fprintf(stderr, "The JudySL array used %lu bytes of memory\n", Bytes);
    return (0);
}

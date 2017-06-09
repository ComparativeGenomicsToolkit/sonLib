#include <stdio.h>
#include <string.h>
#include "Judy.h"

#define MAXLINE 1000000

uint8_t Index[MAXLINE];
uint8_t curVal[MAXLINE];

int main(){

	Pvoid_t   PJArray = (PWord_t)NULL;  // Judy array.
    //void       PJArray = (PWord_t) NULL; //Judy array.
    PWord_t   PValue;                   // Judy array element.
    Word_t    Bytes;                    // size of JudySL array.

    printf("Please enter an index\n");
    while (fgets(Index, MAXLINE, stdin) != (char *)NULL){
        if (strcmp(Index, "") == 10){
            break;
        }
        printf("Please print the value for entered Index\n");
    	fgets(curVal, MAXLINE, stdin);

        JSLI(PValue, PJArray, Index);   // store string into array
        
    	if (PValue == PJERR){            // if out of memory?                              // so do something
                printf("Malloc failed -- get more ram\n");
                exit(1);
        }
        strcpy(PValue, curVal);
        printf("Please enter an index\n");
    }

	Index[0] = '\0';                    // start with smallest string.
    JSLF(PValue, PJArray, Index);       // get first string
    while (PValue != NULL){

    	JSLG(PValue, PJArray, Index);

        printf("%s", Index);
        
        printf("%s", PValue);

        JSLN(PValue, PJArray, Index);   // get next string
    }

    JSLFA(Bytes, PJArray);              // free array
    fprintf(stderr, "The JudySL array used %lu bytes of memory\n", Bytes);
    return (0);

}

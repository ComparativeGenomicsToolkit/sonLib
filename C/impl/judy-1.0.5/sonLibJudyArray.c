#include <stdio.h>
#include <string.h>
#include "Judy.h"

#define MAXLINE 1000000

uint8_t Index[MAXLINE];
uint8_t curVal[MAXLINE];
Word_t Bytes;

static int judy_equalKey(const void *key1, const void *key2){
	return key1 == key2;
}

Pvoid_t judy_construct(){ //Constructs an empty Judy Array
	return (Pword_t)NULL;
}

void judy_destruct(Pvoid_t JSLArray){ //Frees the array
	JSLFA(Bytes, JSLArray);
}


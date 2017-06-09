#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "Judy.h"

#define MAXLINE 1000000

uint8_t Index[MAXLINE];
uint8_t curVal[MAXLINE];
Word_t Bytes;
PWord_t PValue;
uint8_t Rc_int;

st/

static int stjudy_equalKey(const void *key1, const void *key2){
	return key1 == key2;
}

Pvoid_t stjudy_construct(){ //Constructs an empty Judy Array
	return (Pword_t)NULL;
}

void stjudy_destruct(Pvoid_t *JSLArray){ //Frees the array
	JSLFA(Bytes, JSLArray);
	printf("The JudySL array used %lu bytes of memory\n", Bytes);
}

void stjudy_insert(Pvoid_t  *JSLArray, void *key, void *value){ //I assume key and value are char buffers
	PValue = NULL;
	JSLI(PValue, JSLArray, key);
	strcpy(PValue, value);
}

Pword_t stjudy_search(Pvoid_t *JSLArray, void *key){
	JSLG(PValue, JSLArray, key);
	return PValue;

}

void stjudy_remove(Pvoid_t *JSLArray, void *key){
	JSLD(Rc_int, JSLArray, key);
	//Sets Rc_int to 1 if successfull or 0 if not
	if (Rc_int == 0){
		printf("judy_remove Error: Key not present");
	}
}

void stjudy_removeAndFreeKey(Pvoid_t *JSLArray, void *key){
	//Same as doing JSLD
	judy_remove(*JSLArray, *key);
}

void  stjudy_size(Pvoid_t *JSLArray){
	//Can only get BYTE after judy_remove
	//DO NOTHING
}

uint8_t stjudy_getNextKey(Pvoid_t *JSLArray, void *key){
	//Returns next key
	JSLN(PValue, JSLArray, key);
	return key;
}

PWord_t stjudy_getNextValue(Pvoid_t *JSLArray, void *key){
	JSLN(PValue, JSLArray, key);
}
void stjudy_printAll(Pvoid_t *JSLArray){
	//Prints everything in JSLArray in alphanumeric order
	Index[0] = '\0';
	JSLF(PValue, JSLArray, Index);
	while (PValue != NULL){
		JSLG(PValue, JSLArray, Index)
	}

}





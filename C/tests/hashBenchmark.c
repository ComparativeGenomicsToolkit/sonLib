#include <stdlib.h>
#include <stdio.h>
#include "sonLib.h"

#define NUM_INSERTS 12

int main(void) {
    char **stuff[NUM_INSERTS];
    // Generate arbitrary pointers. Random data would be just as good,
    // but this can test value freeing.
    for (int64_t i = 0; i < NUM_INSERTS; i++) {
        stuff[i] = malloc(sizeof(char));
    }

    // Inserts
    stHash *hash = stHash_construct();
    for (int64_t i = 0; i < NUM_INSERTS - 1; i++) {
        stHash_insert(hash, stuff[i], stuff[i + 1]);
    }

    printf("%" PRIi64 "\n", stHash_size(hash));

    // Pause
    getc(stdin);

    // Check the data isn't wrong

    for (int64_t i = 0; i < NUM_INSERTS - 1; i++) {
        printf("%p %p\n", stuff[i + 1], stHash_search(hash, stuff[i]));
    }

    /* getc(stdin); */

    // Iterators

    /* stHashIterator *it = stHash_getIterator(hash); */
    /* void *p; */
    /* while ((p = stHash_getNext(it)) != NULL) { */
    /*     printf("hash has key %p\n", p); */
    /* } */

    return 0;
}

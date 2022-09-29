#include <stdlib.h>
#include "memory.h"

void* reallocate(void* pointer, size_t oldSize, size_t newSize) {
    if (newSize == 0) {
        //Free up memory.
        free(pointer);
        return NULL;
    }

    void* Result = realloc(pointer, newSize);

    //Exit if there is no more memory left.
    if (Result == NULL)
        exit(1);

    //Return pointer to memory.
    return Result;
}
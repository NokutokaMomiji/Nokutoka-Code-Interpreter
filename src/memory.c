#include <stdlib.h>

#include "memory.h"
#include "vm.h"

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

static void FreeObject(Object* object) {
    switch(object->Type) {
        case OBJ_STRING: {
            ObjString* string = (ObjString*)object;
            FREE_ARRAY(char, string->Chars, string->Length + 1);
            FREE(ObjString, object);
            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction* function = (ObjFunction*)object;
            ChunkFree(&function->chunk);
            FREE(ObjFunction, object);
            break;
        }
    }
}

void FreeObjects() {
    Object* object = vm.Objects;
    while (object != NULL) {
        Object* next = object->Next;
        FreeObject(object);
        object = next;
    }
}
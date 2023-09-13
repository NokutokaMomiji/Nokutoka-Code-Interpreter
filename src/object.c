#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJ(type, objectType) (type*)ObjectAllocate(sizeof(type), objectType)

static Object* ObjectAllocate(size_t size, ObjectType objectType) {
    Object* object = (Object*)reallocate(NULL, 0, size);
    object->Type = objectType;

    object->Next = vm.Objects;
    vm.Objects = object;
    return object;
}

static ObjString* StringAllocate(char* chars, int length) {
    ObjString* String = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    String->Length = length;
    String->Chars = chars;
    return String;
}

ObjString* StringTake(const char* chars, int length) {
    return StringAllocate(chars, length);
}

ObjString* StringCopy(const char* chars, int length) {
    char* heapChars = ALLOCATE(char, length + 1);

    // To make sure that ObjString does own its character array.
    memcpy(heapChars, chars, length);

    heapChars[length] = '\0'; // Because we allocated length + 1, length is the last index.
    return StringAllocate(heapChars, length);
}

void ObjectPrint(Value value) {
    switch(OBJECT_TYPE(value)) {
        case OBJ_STRING:
        printf("%s", AS_CSTRING(value));
        break;
    }
}
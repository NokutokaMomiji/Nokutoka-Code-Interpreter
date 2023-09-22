#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"
#include "table.h"

#define ALLOCATE_OBJ(type, objectType) (type*)ObjectAllocate(sizeof(type), objectType)

static Object* ObjectAllocate(size_t size, ObjectType objectType) {
    Object* object = (Object*)reallocate(NULL, 0, size);
    object->Type = objectType;

    object->Next = vm.Objects;
    vm.Objects = object;
    return object;
}

ObjFunction* FunctionNew() {
    ObjFunction* newFunction = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
    newFunction->Arity = 0;
    newFunction->Name = NULL;
    ChunkInit(&newFunction->chunk);
    return newFunction;
}

ObjNative* NativeNew(NativeFn function) {
    ObjNative* newNative = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);
    newNative->Function = function;
    return newNative;
}

static ObjString* StringAllocate(char* chars, int length, uint32_t hash) {
    ObjString* String = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    String->Length = length;
    String->Chars = chars;
    String->Hash = hash;
    TableSet(&vm.Strings, String, NULL_VALUE);
    return String;
}

//  FNV-1a Hashing.
static uint32_t StringHash(const char* key, int length) {
    uint32_t Hash = 2166136261u;
    for (int i = 0; i < length; i++) {
        Hash ^= (uint8_t)key[i];
        Hash *= 16777619;
    }
    return Hash;
}

ObjString* StringTake(const char* chars, int length) {
    uint32_t Hash = StringHash(chars, length);
    ObjString* Interned = TableFindString(&vm.Strings, chars, length, Hash);
    if (Interned != NULL) {
        FREE_ARRAY(char, chars, length + 1);
        return Interned;
    }

    return StringAllocate(chars, length, Hash);
}

ObjString* StringCopy(const char* chars, int length) {
    uint32_t Hash = StringHash(chars, length);
    ObjString* Interned = TableFindString(&vm.Strings, chars, length, Hash);
    if (Interned != NULL)
        return Interned;

    char* heapChars = ALLOCATE(char, length + 1);

    // To make sure that ObjString does own its character array.
    memcpy(heapChars, chars, length);

    heapChars[length] = '\0'; // Because we allocated length + 1, length is the last index.
    return StringAllocate(heapChars, length, Hash);
}

static void FunctionPrint(ObjFunction* function) {
    if (function->Name == NULL) {
        printf("<script>");
        return;
    }
    printf("<function %s at 0x%p>", function->Name->Chars, function);
}

void ObjectPrint(Value value) {
    switch(OBJECT_TYPE(value)) {
        case OBJ_STRING:
        printf("%s", AS_CSTRING(value));
        break;

        case OBJ_NATIVE:
        printf("<native function>");
        break;

        case OBJ_FUNCTION:
        FunctionPrint(AS_FUNCTION(value));
        break;
    }
}
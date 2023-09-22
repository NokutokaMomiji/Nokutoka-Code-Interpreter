#ifndef NKCODE_OBJECT_H
#define NKCODE_OBJECT_H

#include "common.h"
#include "value.h"
#include "chunk.h"

#define OBJECT_TYPE(value)     (AS_OBJECT(value)->Type)

#define IS_STRING(value)    IsObjectType(value, OBJ_STRING)
#define IS_NATIVE(value)    IsObjectType(value, OBJ_NATIVE)
#define IS_FUNCTION(value)  IsObjectType(value, OBJ_FUNCTION)

#define AS_STRING(value)    ((ObjString*)AS_OBJECT(value))
#define AS_CSTRING(value)   (((ObjString*)AS_OBJECT(value))->Chars)
#define AS_NATIVE(value)    (((ObjNative*)AS_OBJECT(value))->Function)
#define AS_FUNCTION(value)  ((ObjFunction*)AS_OBJECT(value))

typedef enum {
    OBJ_STRING,
    OBJ_FUNCTION,
    OBJ_NATIVE
} ObjectType;

struct Object {
    ObjectType Type;
    struct Object* Next;
};

typedef struct {
    Object object;
    int Arity;
    Chunk chunk;
    ObjString* Name;
} ObjFunction;

typedef Value (*NativeFn)(int argumentCount, Value* arguments);

typedef struct {
    Object object;
    NativeFn Function;
} ObjNative;

struct ObjString {
    Object object;
    int Length;
    char* Chars;
    uint32_t Hash;
};

ObjFunction* FunctionNew();
ObjNative* NativeNew(NativeFn function);

ObjString* StringTake(const char* chars, int length);
ObjString* StringCopy(const char* chars, int length);
void ObjectPrint(Value value);

static inline bool IsObjectType(Value value, ObjectType type) {
    return IS_OBJECT(value) && AS_OBJECT(value)->Type == type;
}

#endif
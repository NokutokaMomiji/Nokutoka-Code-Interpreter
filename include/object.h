#ifndef NKCODE_OBJECT_H
#define NKCODE_OBJECT_H

#include "common.h"
#include "value.h"

#define OBJECT_TYPE(value)     (AS_OBJECT(value)->Type)

#define IS_STRING(value)    IsObjectType(value, OBJ_STRING)

#define AS_STRING(value)    ((ObjString*)AS_OBJECT(value))
#define AS_CSTRING(value)   (((ObjString*)AS_OBJECT(value))->Chars)

typedef enum {
    OBJ_STRING
} ObjectType;

struct Object {
    ObjectType Type;
    struct Object* Next;
};

struct ObjString {
    Object object;
    int Length;
    char* Chars;
};

ObjString* StringTake(const char* chars, int length);
ObjString* StringCopy(const char* chars, int length);
void ObjectPrint(Value value);

static inline bool IsObjectType(Value value, ObjectType type) {
    return IS_OBJECT(value) && AS_OBJECT(value)->Type == type;
}

#endif
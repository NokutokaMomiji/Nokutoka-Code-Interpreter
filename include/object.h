#ifndef MOMIJI_OBJECT_H
#define MOMIJI_OBJECT_H

#include "Common.h"
#include "Value.h"
#include "Chunk.h"
#include "Table.h"

#define OBJECT_TYPE(value)     (AS_OBJECT(value)->type)

#define IS_STRING(value)        IsObjectType(value, OBJ_STRING)
#define IS_ARRAY(value)         IsObjectType(value, OBJ_ARRAY)
#define IS_MAP(value)           IsObjectType(value, OBJ_MAP)
#define IS_NATIVE(value)        IsObjectType(value, OBJ_NATIVE)
#define IS_FUNCTION(value)      IsObjectType(value, OBJ_FUNCTION)
#define IS_CLOSURE(value)       IsObjectType(value, OBJ_CLOSURE)
#define IS_CLASS(value)         IsObjectType(value, OBJ_CLASS)
#define IS_INSTANCE(value)      IsObjectType(value, OBJ_INSTANCE)
#define IS_BOUND_METHOD(value)  IsObjectType(value, OBJ_BOUND_METHOD)


#define AS_STRING(value)        ((ObjString*)AS_OBJECT(value))
#define AS_CSTRING(value)       (((ObjString*)AS_OBJECT(value))->chars)
#define AS_ARRAY(value)         ((ObjArray*)AS_OBJECT(value))
#define AS_MAP(value)           ((ObjMap*)AS_OBJECT(value))
#define AS_NATIVE(value)        (((ObjNative*)AS_OBJECT(value))->function)
#define AS_FUNCTION(value)      ((ObjFunction*)AS_OBJECT(value))
#define AS_CLOSURE(value)       ((ObjClosure*)AS_OBJECT(value))
#define AS_CLASS(value)         ((ObjClass*)AS_OBJECT(value))
#define AS_INSTANCE(value)      ((ObjInstance*)AS_OBJECT(value))
#define AS_BOUND_METHOD(value)  ((ObjBoundMethod*)AS_OBJECT(value))

typedef enum {
    OBJ_STRING,
    OBJ_ARRAY,
    OBJ_MAP,

    OBJ_FUNCTION,
    OBJ_NATIVE,
    OBJ_CLOSURE,
    OBJ_UPVALUE,

    OBJ_CLASS,
    OBJ_INSTANCE,
    OBJ_BOUND_METHOD,
    OBJ_STATIC_METHOD
} ObjectType;

struct Object {
    ObjectType type;
    bool isMarked;
    struct Object* next;
};

typedef struct {
    Object object;
    int arity;
    int upvalueCount;
    MJ_Chunk chunk;
    ObjString* name;
} ObjFunction;

typedef Value (*NativeFn)(int argumentCount, Value* arguments);

typedef struct {
    Object object;
    NativeFn function;
} ObjNative;

struct ObjString {
    Object object;
    int length;
    char* chars;
    uint32_t hash;
};

typedef struct {
    Object object;
    ValueArray items;
} ObjArray;

typedef struct {
    Object object;
    ValueArray keys;
    Table items;
} ObjMap;

typedef struct ObjUpvalue {
    Object object;
    Value* location;
    Value closed;
    struct ObjUpvalue* next;
} ObjUpvalue;

typedef struct {
    Object object;
    ObjFunction* function;
    ObjUpvalue** upvalues;
    int upvalueCount;
} ObjClosure;

typedef struct {
    Object object;
    ObjString* className;
    ValueArray methodNames;
    Table methods;
    Table defaultFields;
    Value constructor;
} ObjClass;

typedef struct {
    Object object;
    ObjClass* class;
    ValueArray fieldNames;
    Table fields;
} ObjInstance;

typedef struct {
    Object object;
    Value receiver;
    ObjClosure* method;
} ObjBoundMethod;

ObjClosure* ClosureNew(ObjFunction* function);
ObjFunction* FunctionNew();
ObjNative* NativeNew(NativeFn function);

ObjArray* ArrayNew();
ObjMap* MapNew();

ObjString* StringTake(const char* chars, int length);
ObjString* StringCopy(const char* chars, int length);

ObjUpvalue* UpvalueNew(Value* slot);

ObjClass* ClassNew(ObjString* name);
ObjInstance* InstanceNew(ObjClass* classObject);
ObjBoundMethod* BoundMethodNew(Value receiver, ObjClosure* method);

void ObjectPrint(Value value);

static inline bool IsObjectType(Value value, ObjectType type) {
    return IS_OBJECT(value) && AS_OBJECT(value)->type == type;
}

#endif
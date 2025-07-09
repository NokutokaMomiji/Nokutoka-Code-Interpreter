#include <stdio.h>
#include <string.h>

#include "Memory.h"
#include "Object.h"
#include "Value.h"
#include "VM.h"
#include "Table.h"

#define ALLOCATE_OBJ(type, objectType) (type*)ObjectAllocate(sizeof(type), objectType)

static Object* ObjectAllocate(size_t size, ObjectType objectType) {
    Object* object = (Object*)reallocate(NULL, 0, size);
    object->type = objectType;
    object->isMarked = false;
    object->next = vm.objects;
    vm.objects = object;

#ifdef DEBUG_LOG_GC
    printf("> %p allocate %zu for %d\n", (void*)object, size, objectType);
#endif

    return object;
}

ObjClosure* ClosureNew(ObjFunction* function) {
    ObjUpvalue** Upvalues = ALLOCATE(ObjUpvalue*, function->upvalueCount);
    
    for (int i = 0; i < function->upvalueCount; i++) {
        Upvalues[i] = NULL;
    }

    ObjClosure* Closure = ALLOCATE_OBJ(ObjClosure, OBJ_CLOSURE);
    Closure->function = function;
    Closure->upvalues = Upvalues;
    Closure->upvalueCount = function->upvalueCount;
    return Closure;
}

ObjFunction* FunctionNew() {
    ObjFunction* newFunction = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
    newFunction->arity = 0;
    newFunction->upvalueCount = 0;
    newFunction->name = NULL;
    MJ_ChunkInit(&newFunction->chunk);
    return newFunction;
}

ObjNative* NativeNew(NativeFn function) {
    ObjNative* newNative = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);
    newNative->function = function;
    return newNative;
}

static ObjString* StringAllocate(char* chars, int length, uint32_t hash) {
    ObjString* String = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    String->length = length;
    String->chars = chars;
    String->hash = hash;

    Push(OBJECT_VALUE(String));
    TableSet(&vm.strings, String, NULL_VALUE);
    Pop();
    
    return String;
}

//  FNV-1a Hashing.
static uint32_t StringHash(const char* key, int length) {
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; i++) {
        hash ^= (uint8_t)key[i];
        hash *= 16777619;
    }
    return hash;
}

ObjString* StringTake(const char* chars, int length) {
    uint32_t hash = StringHash(chars, length);
    ObjString* Interned = TableFindString(&vm.strings, chars, length, hash);
    if (Interned != NULL) {
        FREE_ARRAY(char, chars, length + 1);
        return Interned;
    }

    return StringAllocate(chars, length, hash);
}

ObjString* StringCopy(const char* chars, int length) {
    uint32_t hash = StringHash(chars, length);
    ObjString* Interned = TableFindString(&vm.strings, chars, length, hash);
    if (Interned != NULL)
        return Interned;

    char* heapChars = ALLOCATE(char, length + 1);

    // To make sure that ObjString does own its character array.
    memcpy(heapChars, chars, length);

    heapChars[length] = '\0'; // Because we allocated length + 1, length is the last index.
    return StringAllocate(heapChars, length, hash);
}

ObjUpvalue* UpvalueNew(Value* slot) {
    ObjUpvalue* Upvalue = ALLOCATE_OBJ(ObjUpvalue, OBJ_UPVALUE);
    Upvalue->location = slot;
    Upvalue->next = NULL;
    Upvalue->closed = NULL_VALUE;
    return Upvalue;
}

ObjArray* ArrayNew() {
    ObjArray* Array = ALLOCATE_OBJ(ObjArray, OBJ_ARRAY);
    ValueArrayInit(&Array->items);
    return Array;
}

ObjMap* MapNew() {
    ObjMap* Map = ALLOCATE_OBJ(ObjMap, OBJ_MAP);
    ValueArrayInit(&Map->keys);
    TableInit(&Map->items);
    return Map;
}

ObjClass* ClassNew(ObjString* name) {
    ObjClass* Class = ALLOCATE_OBJ(ObjClass, OBJ_CLASS);
    Class->className = name;
    ValueArrayInit(&Class->methodNames);
    TableInit(&Class->methods);
    TableInit(&Class->defaultFields);
    return Class;
}

ObjInstance* InstanceNew(ObjClass* classObj) {
    ObjInstance* Instance = ALLOCATE_OBJ(ObjInstance, OBJ_INSTANCE);
    Instance->class = classObj;
    ValueArrayInit(&Instance->fieldNames);
    TableInit(&Instance->fields);
    TableAddAll(&classObj->defaultFields, &Instance->fields);
    return Instance;
}

ObjBoundMethod* BoundMethodNew(Value receiver, ObjClosure* method) {
    ObjBoundMethod* bound = ALLOCATE_OBJ(ObjBoundMethod, OBJ_BOUND_METHOD);
    bound->receiver = receiver;
    bound->method = method;
    return bound;
}

static void FunctionPrint(ObjFunction* function) {
    if (function->name == NULL) {
        printf("<script>");
        return;
    }
    printf("<function %s at 0x%p>", function->name->chars, function);
}

static void ArrayPrint(ObjArray* array) {
    printf("[");
    for (int i = 0; i < array->items.count; i++) {
        ValuePrint(array->items.values[i]);
	if (i != (array->items.count - 1))
		printf(", ");
    }
    printf("]");
}

static void MapPrint(ObjMap* map) {
    printf("{");
    for (int i = 0; i < map->keys.count; i++) {
        Value Key = map->keys.values[i];
        Value Item;

        TableGet(&map->items, AS_STRING(Key), &Item);
        
        ValuePrint(Key);
        printf(": ");
        ValuePrint(Item);

        if (i != (map->keys.count - 1))
            printf(", ");
    }
    printf("}");
}

void ObjectPrint(Value value) {
    switch(OBJECT_TYPE(value)) {
        case OBJ_STRING:
            printf("%s", AS_CSTRING(value));
            break;

        case OBJ_ARRAY:
            ArrayPrint(AS_ARRAY(value));
            break;

        case OBJ_MAP:
            MapPrint(AS_MAP(value));
            break;

        case OBJ_NATIVE:
            printf("<native function>");
            break;

        case OBJ_FUNCTION:
            FunctionPrint(AS_FUNCTION(value));
            break;

        case OBJ_CLOSURE:
            FunctionPrint(AS_CLOSURE(value)->function);
            break;

        case OBJ_UPVALUE:
            printf("Upvalue");
            break;

        case OBJ_CLASS:
            printf("<class \"%s\">", AS_CLASS(value)->className->chars);
            break;

        case OBJ_INSTANCE:
            printf("<%s instance at 0x%zu>", AS_INSTANCE(value)->class->className->chars, AS_INSTANCE(value));
            break;
            
        case OBJ_BOUND_METHOD:
            FunctionPrint(AS_BOUND_METHOD(value)->method->function);
            break;
    }
}

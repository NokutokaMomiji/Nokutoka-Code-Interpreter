#include <stdlib.h>
#include <stdio.h>

#include "Compiler.h"
#include "Memory.h"
#include "VM.h"

#ifdef DEBUG_LOG_GC
#include <stdio.h>
#include "debug.h"
#endif

#define GC_HEAP_GROW_FACTOR 2

void* reallocate(void* pointer, size_t oldSize, size_t newSize) {
    vm.allocatedBytes += newSize - oldSize;
    if (newSize > oldSize) {
#ifdef DEBUG_STRESS_GC
        CollectGarbage();
#endif
        if (vm.allocatedBytes >= vm.nextCollection)
            CollectGarbage();
    }

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

void MarkObject(Object* object) {
    if (object == NULL)
        return;

    if (object->isMarked)
        return;

#ifdef DEBUG_LOG_GC
    printf("> %p mark ", (void*)object);
    ValuePrint(OBJECT_VALUE(object));
    printf("\n");
#endif

    object->isMarked = true;

    if (vm.grayCapacity < vm.grayCount + 1) {
        vm.grayCapacity = GROW_CAPACITY(vm.grayCapacity);
        vm.grayStack = (Object**)realloc(vm.grayStack, sizeof(Object*) * vm.grayCapacity);

        if (vm.grayStack == NULL && vm.safeguardStack != NULL) {
            free(vm.safeguardStack);
            vm.grayStack = (Object**)realloc(vm.grayStack, sizeof(Object*) * vm.grayCapacity);

            if (vm.grayStack == NULL)
                exit(1);
        }
        else if (vm.grayStack == NULL)
            exit(1);
    }

    vm.grayStack[vm.grayCount++] = object;
}

void MarkValue(Value value) {
    if (IS_OBJECT(value))
        MarkObject(AS_OBJECT(value));
}

static void MarkArray(ValueArray* array) {
    for (int i = 0; i < array->count; i++) {
        MarkValue(array->values[i]);
    }
}

static void BlackenObject(Object* object) {
#ifdef DEBUG_LOG_GC
    printf("%p blacken ", (void*)object);
    ValuePrint(OBJECT_VALUE(object));
    printf("\n");
#endif
    switch (object->type) {
        case OBJ_CLOSURE: {
            ObjClosure* Closure = (ObjClosure*)object;
            MarkObject((Object*)Closure->function);
            for (int i = 0; i < Closure->upvalueCount; i++) {
                MarkObject((Object*)Closure->upvalues[i]);
            }
            break;
        }

        case OBJ_FUNCTION: {
            ObjFunction* Function = (ObjFunction*)object;
            MarkObject((Object*)Function->name);
            MarkArray(&Function->chunk.constants);
            break;
        }

        case OBJ_UPVALUE:
            MarkValue(((ObjUpvalue*)object)->closed);
            break;
        
        case OBJ_CLASS: {
            ObjClass* Class = (ObjClass*)object;
            MarkObject((Object*)Class->className);
            MarkArray(&Class->methodNames);
            TableMark(&Class->methods);
            TableMark(&Class->defaultFields);
            break;
        }

        case OBJ_INSTANCE: {
            ObjInstance* Instance = (ObjInstance*)object;
            MarkObject((Object*)Instance->class);
            MarkArray(&Instance->fieldNames);
            TableMark(&Instance->fields);
            break;
        }

        case OBJ_BOUND_METHOD: {
            ObjBoundMethod* bound = (ObjBoundMethod*)object;
            MarkValue(bound->receiver);
            MarkObject((Object*)bound->method);
            break;
        }

        case OBJ_ARRAY: {
            ObjArray* Array = (ObjArray*)object;
            MarkArray(&Array->items);
        }

        case OBJ_MAP: {
            ObjMap* Map = (ObjMap*)object;
            MarkArray(&Map->keys);
            TableMark(&Map->items);
        }

        case OBJ_NATIVE:
        case OBJ_STRING:
            break;

        
    }
}

static void FreeObject(Object* object) {
#ifdef DEBUG_LOG_GC
    printf("> %p free type %d\n", (void*)object, object->type);
#endif
    switch(object->type) {
        case OBJ_STRING: {
            ObjString* string = (ObjString*)object;
            FREE_ARRAY(char, string->chars, string->length + 1);
            FREE(ObjString, object);
            break;
        }

        case OBJ_ARRAY: {
            ObjArray* Array = (ObjArray*)object;
            ValueArrayFree(&Array->items);
            FREE(ObjArray, object);
            break;
        }

        case OBJ_NATIVE:
            FREE(ObjNative, object);
            break;

        case OBJ_CLOSURE: {
            ObjClosure* Closure = (ObjClosure*)object;
            FREE_ARRAY(ObjUpvalue*, Closure->upvalues, Closure->upvalueCount);
            FREE(ObjClosure, object);
            break;
        }
        
        case OBJ_UPVALUE:
            FREE(ObjUpvalue, object);
            break;
        
        case OBJ_FUNCTION: {
            ObjFunction* function = (ObjFunction*)object;
            MJ_ChunkFree(&function->chunk);
            FREE(ObjFunction, object);
            break;
        }

        case OBJ_CLASS: {
            FREE(OBJ_CLASS, object);
            break;
        }

        case OBJ_INSTANCE: {
            ObjInstance* Instance = (ObjInstance*)object;
            ValueArrayFree(&Instance->fieldNames);
            TableFree(&Instance->fields);
            FREE(ObjInstance, object);
        }

        case OBJ_BOUND_METHOD:
            FREE(ObjBoundMethod, object);
            break;
    }
}

void FreeObjects() {
    Object* object = vm.objects;
    while (object != NULL) {
        Object* next = object->next;
        FreeObject(object);
        object = next;
    }

    free(vm.grayStack);
    free(vm.safeguardStack);
}

static void MarkRoots() {
    for (Value* slot = vm.Stack; slot < vm.stackTop; slot++) {
        MarkValue(*slot);
    }

    for (int i = 0; i < vm.frameCount; i++) {
        MarkObject((Object*)vm.frames[i].closure);
    }

    for (ObjUpvalue* upvalue = vm.openUpvalues; upvalue != NULL; upvalue = upvalue->next) {
        MarkObject((Object*)upvalue);
    }

    TableMark(&vm.globals); 
    CompilerMarkRoots();
}

static void TraceReferences() {
    while (vm.grayCount > 0) {
        Object* object = vm.grayStack[--vm.grayCount];
        BlackenObject(object);
    }
}

static void Sweep() {
    Object* Previous = NULL;
    Object* Current = vm.objects;

    while (Current != NULL) {
        if (Current->isMarked) {
            Current->isMarked = false;
            // If the current object is marked, then we know we don't have to get rid of it.
            // We merely set it as the previous object and move to the next.
            Previous = Current;
            Current = Current->next;
        }
        else {
            // We keep the currently unreached object.
            Object* Unreached = Current;

            // We check the following object to see if we ought to link the elements.
            Current = Current->next;

            // There is a previous element in the linked list, so we link the elements.
            if (Previous != NULL)
                Previous->next = Current;
            else    // There isn't, so the current element is now the beginning element in the linked list.i
                vm.objects = Current;

            FreeObject(Unreached);
        }
    }
}

void CollectGarbage() {
#ifdef DEBUG_LOG_GC
    printf("-- [GC BEGIN] --\n");
    size_t Before = vm.allocatedBytes;
#endif

    MarkRoots();
    TraceReferences();
    TableRemoveWhite(&vm.strings);
    Sweep();

    vm.nextCollection = vm.allocatedBytes * GC_HEAP_GROW_FACTOR;

#ifdef DEBUG_LOG_GC
    printf("-- [GC END] --\n");
    printf("   > Collected %zu (from %zu to %zu). Next at %zu.\n", Before - vm.allocatedBytes, Before, vm.allocatedBytes, vm.nextCollection);
#endif
}
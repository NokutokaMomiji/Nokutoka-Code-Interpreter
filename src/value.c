#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "value.h"
#include "object.h"

void ValueArrayInit(ValueArray* array) {
    //Intialize an empty array.
    array->Values = NULL;
    array->Capacity = 0;
    array->Count = 0;
}

void ValueArrayWrite(ValueArray* array, Value value) {
    //If there is not enough capacity for the new byte, then increase the size of the array.
    if (array->Capacity < array->Count + 1) {
        int oldCapacity = array->Capacity;
        array->Capacity = GROW_CAPACITY(oldCapacity);
        array->Values = GROW_ARRAY(Value, array->Values, oldCapacity, array->Capacity);
    }

    //Store byte on array and increase the number of elements.
    array->Values[array->Count] = value;
    array->Count++;
}

void ValueArrayFree(ValueArray* array) {
    //Free array memory.
    FREE_ARRAY(Value, array->Values, array->Capacity);

    //Reintialize array.
    ValueArrayInit(array);
}

void ValuePrint(Value value) {
    switch(value.Type) {
        case VALUE_BOOL:    printf(AS_BOOL(value) ? "true" : "false"); break;
        case VALUE_NULL:    printf("null"); break;
        case VALUE_NUMBER:  printf("%g", AS_NUMBER(value)); break;
        case VALUE_OBJECT:  ObjectPrint(value);       break;
    }
}

bool ValuesEqual(Value a, Value b) {
    if (a.Type != b.Type)
        return false;

    switch (a.Type) {
        case VALUE_BOOL:    return AS_BOOL(a) == AS_BOOL(b);
        case VALUE_NULL:    return true;
        case VALUE_NUMBER:  return AS_NUMBER(a) == AS_NUMBER(b);
        case VALUE_OBJECT: {
            switch(OBJECT_TYPE(a)) {
                case OBJ_STRING: {
                    ObjString* aString = AS_STRING(a);
                    ObjString* bString = AS_STRING(b);
                    return aString->Length == bString->Length && memcmp(aString->Chars, bString->Chars, aString->Length) == 0;
                }
            }
        }
        default:            return false;
    }
}
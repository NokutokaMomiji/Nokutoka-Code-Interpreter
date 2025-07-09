#include <stdio.h>
#include <string.h>

#include "Memory.h"
#include "Value.h"
#include "Object.h"

void ValueArrayInit(ValueArray* array) {
    //Intialize an empty array.
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}

void ValueArrayWrite(ValueArray* array, Value value) {
    //If there is not enough capacity for the new byte, then increase the size of the array.
    if (array->capacity < array->count + 1) {
        int oldCapacity = array->capacity;
        array->capacity = GROW_CAPACITY(oldCapacity);
        array->values = GROW_ARRAY(Value, array->values, oldCapacity, array->capacity);
    }

    //Store byte on array and increase the number of elements.
    array->values[array->count] = value;
    array->count++;
}

Value* ValueArrayGet(ValueArray* array, int position) {
    if (position < 0 || position > array->count)
        return NULL;
    
    return &array->values[position];
}

void ValueArrayFree(ValueArray* array) {
    //Free array memory.
    FREE_ARRAY(Value, array->values, array->capacity);

    //Reintialize array.
    ValueArrayInit(array);
}

void ValuePrint(Value value) {
    switch(value.type) {
        case VALUE_BOOL:    printf(AS_BOOL(value) ? "true" : "false");  break;
        case VALUE_NULL:    printf("null");                             break;
        case VALUE_NUMBER:  printf("%g", AS_NUMBER(value));             break;
        case VALUE_OBJECT:  ObjectPrint(value);                         break;
        default:    return;
    }
}

bool ValuesEqual(Value a, Value b) {
    if (a.type != b.type)
        return false;

    switch (a.type) {
        case VALUE_BOOL:    return AS_BOOL(a) == AS_BOOL(b);
        case VALUE_NULL:    return true;
        case VALUE_NUMBER:  return AS_NUMBER(a) == AS_NUMBER(b);
        case VALUE_OBJECT:  return AS_OBJECT(a) == AS_OBJECT(b);
        default:            return false;
    }
}
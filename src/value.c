#include <stdio.h>
#include "memory.h"
#include "value.h"

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
    printf("%g", value);
}
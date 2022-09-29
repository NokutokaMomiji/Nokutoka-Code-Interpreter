#ifndef NKCODE_VALUE_H
#define NKCODE_VALUE_H

#include "common.h"

typedef double Value;

typedef struct {
    int Capacity;   //Contains the full capacity of the array.
    int Count;      //Number of elements in array.
    Value* Values;  //Elements.
} ValueArray;

void ValueArrayInit(ValueArray* array);
void ValueArrayWrite(ValueArray* array, Value value);
void ValueArrayFree(ValueArray* array);

void ValuePrint(Value value);

#endif
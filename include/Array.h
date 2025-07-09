#ifndef MOMIJI_LIST_H
#define MOMIJI_LIST_H

#include "Object.h"
#include "Value.h"

extern void MJ_ArrayAdd(ObjArray* array, Value value);

bool MJ_ArraySet(ObjArray* array, Value index, Value value);
bool MJ_ArraySetRange(ObjArray* array, Value min, Value max, Value value);

bool MJ_ArrayGet(ObjArray* array, Value index, Value* value);
bool MJ_ArrayGetRange(ObjArray* array, Value min, Value max, Value step, Value* value);


#endif
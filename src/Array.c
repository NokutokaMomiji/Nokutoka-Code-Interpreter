#include <stdio.h>

#include "Array.h"
#include "VM.h"
#include "Utilities.h"

inline void MJ_ArrayAdd(ObjArray* array, Value value) {
    Push(value);
    ValueArrayWrite(&array->items, value);
    Pop();
}

bool MJ_ArraySet(ObjArray* array, Value index, Value value) {
    // Arrays only allow number indexes, so we need to check that the value
    // we're dealing with is a number value.
    if (!IS_NUMBER(index))
        return false;

    int indexNumber = AS_NUMBER(index);
    
    // Negative array indexes start counting from the top of the array. 
    // We check for that here to get the actual valid index.
    indexNumber = (indexNumber < 0) ? array->items.count + indexNumber : indexNumber;

    // We return if the index is out of bounds.
    if (indexNumber < 0 || indexNumber > array->items.count)
        return false;

    // However, we allow to write to the very next array index on the list.
    if (indexNumber == array->items.count) {
        ValueArrayWrite(&array->items, value);
        return true;
    }

    // We write the value to the given index.
    array->items.values[indexNumber] = value;
    return true;
}

bool MJ_ArraySetRange(ObjArray* array, Value min, Value max, Value value) {
    // Array ranges allow either number indexes or Null values indicating ends of the list.
    // We check that the min and max range values are valid.
    if ((!IS_NUMBER(min) && !IS_NULL(min)) || (!IS_NUMBER(min) && !IS_NULL(max)))
        return false;

    int rangeMin = (IS_NUMBER(min)) ? AS_NUMBER(min) : 0;
    int rangeMax = (IS_NUMBER(max)) ? AS_NUMBER(max) : array->items.count;

    rangeMin = (rangeMin < 0) ? array->items.count + rangeMin : rangeMin;
    rangeMax = (rangeMax < 0) ? array->items.count + rangeMax : rangeMax;

    rangeMin = Min(rangeMin, rangeMax);
    rangeMax = Max(rangeMin, rangeMax);

    if (rangeMin < 0 || rangeMax < 0 || rangeMin > array->items.count)
        return false;

    int Step = Sign(rangeMax - rangeMin);

    for (int i = rangeMin; i < rangeMax; i++) {
        if (Between(i, 0, array->items.count - 1)) {
            array->items.values[i] = value;
            continue;

            ValueArrayWrite(&array->items, value);
        }
    }

    return true;
}

bool MJ_ArrayGet(ObjArray* array, Value index, Value* value) {
    if (!IS_NUMBER(index))
        return false;

    int indexNumber = AS_NUMBER(index);

    indexNumber = (indexNumber < 0) ? array->items.count + indexNumber : indexNumber;

    if (indexNumber < 0 || indexNumber >= array->items.count) {
        return false;
    }

    *value = array->items.values[indexNumber];
    return true;
}

bool MJ_ArrayGetRange(ObjArray* array, Value min, Value max, Value step, Value* value) {
    if ((!IS_NUMBER(min) && !IS_NULL(min)) || (!IS_NUMBER(max) && !IS_NULL(max))) {
        printf("%d %d\n", OBJECT_TYPE(min), OBJECT_TYPE(max));
        return false;
    }

    int rangeMin = (IS_NUMBER(min)) ? AS_NUMBER(min) : 0;
    int rangeMax = (IS_NUMBER(max)) ? AS_NUMBER(max) : array->items.count - 1;

    rangeMin = (rangeMin < 0) ? array->items.count + rangeMin : rangeMin;
    rangeMax = (rangeMax < 0) ? array->items.count + rangeMax : rangeMax;

    if ((rangeMin < 0 || rangeMin >= array->items.count) || (rangeMax < 0 || rangeMax >= array->items.count)) {
        printf("%d - %d\n", rangeMin, rangeMax);
        return false;
    }

    int Step = (IS_NUMBER(step)) ? AS_NUMBER(step) : 1;

    if (Step < 0 && (rangeMax > rangeMin)) {
        int temp = rangeMin;
        rangeMin = rangeMax;
        rangeMax = temp;
    }

    printf("> [%d:%d:%d]\n", rangeMin, rangeMax, Step);

    ObjArray* newArray = ArrayNew();

    for (int i = rangeMin; (Step < 0) ? (i >= rangeMax) : (i <= rangeMax); i += Step) {
        ValueArrayWrite(&newArray->items, array->items.values[i]);
    }

    *value = OBJECT_VALUE(newArray);
    return true;
}
#ifndef NKCODE_VALUE_H
#define NKCODE_VALUE_H

#include "common.h"

typedef struct Object Object;
typedef struct ObjString ObjString; 

typedef enum {
    VALUE_BOOL,
    VALUE_NULL,
    VALUE_NUMBER,
    VALUE_OBJECT
} ValueType;

typedef struct {
    ValueType Type;
    union {
        bool Boolean;
        double Number;
        Object* object;
    } As;
} Value;

// These macros check if the type assigned to a Value object is the one we are looking for
// This is in order to make sure that the type of the value is the one that we want when trying 
//  to get the actual value from the union

#define IS_BOOL(value)      ((value).Type == VALUE_BOOL)    
#define IS_NULL(value)      ((value).Type == VALUE_NULL)
#define IS_NUMBER(value)    ((value).Type == VALUE_NUMBER)
#define IS_OBJECT(value)    ((value).Type == VALUE_OBJECT)

// Unpack the C value from the VAlue object.
// Since these access the union, we need to make sure that the value type is the correct one before using the macro.

#define AS_BOOL(value)      ((value).As.Boolean)
#define AS_NUMBER(value)    ((value).As.Number)
#define AS_OBJECT(value)    ((value).As.object)

// Allow us to pass a C value to our custom Value object with the appropriate tag and value.

#define BOOL_VALUE(value)   ((Value){VALUE_BOOL,   {.Boolean = value}})
#define NULL_VALUE          ((Value){VALUE_NULL,   {.Number = 0}})
#define NUMBER_VALUE(value) ((Value){VALUE_NUMBER, {.Number = value}})
#define OBJECT_VALUE(value) ((Value){VALUE_OBJECT, {.object = value}})

typedef struct {
    int Capacity;   //Contains the full capacity of the array.
    int Count;      //Number of elements in array.
    Value* Values;  //Elements.
} ValueArray;

void ValueArrayInit(ValueArray* array);
void ValueArrayWrite(ValueArray* array, Value value);
void ValueArrayFree(ValueArray* array);

void ValuePrint(Value value);

bool ValuesEqual(Value a, Value b);

#endif
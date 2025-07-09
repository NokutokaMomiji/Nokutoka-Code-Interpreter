#ifndef MOMIJI_VALUE_H
#define MOMIJI_VALUE_H

#include "Common.h"

typedef struct Object Object;
typedef struct ObjString ObjString; 

typedef enum {
    VALUE_BOOL,
    VALUE_NULL,
    VALUE_NUMBER,
    VALUE_OBJECT
} ValueType;

typedef struct {
    ValueType type;
    union {
        bool boolean;
        double number;
        Object* object;
    } as;
} Value;

// These macros check if the type assigned to a Value object is the one we are looking for
// This is in order to make sure that the type of the value is the one that we want when trying 
//  to get the actual value from the union

#define IS_BOOL(value)      ((value).type == VALUE_BOOL)    
#define IS_NULL(value)      ((value).type == VALUE_NULL)
#define IS_NUMBER(value)    ((value).type == VALUE_NUMBER)
#define IS_OBJECT(value)    ((value).type == VALUE_OBJECT)

// Unpack the C value from the VAlue object.
// Since these access the union, we need to make sure that the value type is the correct one before using the macro.

#define AS_BOOL(value)      ((value).as.boolean)
#define AS_NUMBER(value)    ((value).as.number)
#define AS_OBJECT(value)    ((value).as.object)

// Allow us to pass a C value to our custom Value object with the appropriate tag and value.

#define BOOL_VALUE(value)   ((Value){VALUE_BOOL,   {.boolean = value}})
#define NULL_VALUE          ((Value){VALUE_NULL,   {.number = 0}})
#define NUMBER_VALUE(value) ((Value){VALUE_NUMBER, {.number = value}})
#define OBJECT_VALUE(value) ((Value){VALUE_OBJECT, {.object = (Object*)value}})

typedef struct {
    int capacity;   //Contains the full capacity of the array.
    int count;      //Number of elements in array.
    Value* values;  //Elements.
} ValueArray;

void ValueArrayInit(ValueArray* array);
void ValueArrayWrite(ValueArray* array, Value value);
void ValueArrayFree(ValueArray* array);

void ValuePrint(Value value);

bool ValuesEqual(Value a, Value b);

#endif
#ifndef MOMIJI_VM_H
#define MOMIJI_VM_H

#include "Object.h"
#include "Chunk.h"
#include "Value.h"
#include "Table.h"

#define FRAMES_MAX 1000
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

typedef struct {
    ObjClosure* closure;
    uint8_t* ip;
    Value* slots;
} CallFrame;

typedef struct {
    CallFrame frames[FRAMES_MAX];
    int frameCount;

    Value Stack[STACK_MAX];
    Value* stackTop;
    Table strings;
    Table globals;
    Object* objects;
    int grayCount;
    int grayCapacity;
    Object** grayStack;
    Object** safeguardStack;
    ObjUpvalue* openUpvalues;
    ObjString* initString;

    size_t allocatedBytes;
    size_t nextCollection;
} VM;

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResultStatus;

typedef struct {
    InterpretResultStatus status;
    Value value;
} InterpretResult;

#define RUNTIME_ERROR(value) ((InterpretResult){INTERPRET_RUNTIME_ERROR, (Value)value})
#define COMPILE_ERROR(value) ((InterpretResult){INTERPRET_COMPILE_ERROR, (Value)value})
#define RUNTIME_OK(value) ((InterpretResult){INTERPRET_OK, (Value)value})

extern VM vm;

void VMInit();
void VMFree();

InterpretResult Interpret(const char* sourceFile);
InterpretResult InterpretChunk(MJ_Chunk* chunk);

void Push(Value value);
Value Pop();
Value PopN(int n);

#endif
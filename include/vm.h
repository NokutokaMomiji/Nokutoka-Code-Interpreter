#ifndef NKCODE_VM_H
#define NKCODE_VM_H

#include "object.h"
#include "chunk.h"
#include "value.h"
#include "table.h"

#define FRAMES_MAX 1000
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

typedef struct {
    ObjFunction* Function;
    uint8_t* IP;
    Value* Slots;
} CallFrame;

typedef struct {
    CallFrame Frames[FRAMES_MAX];
    int frameCount;

    Value Stack[STACK_MAX];
    Value* stackTop;
    Table Strings;
    Table Globals;
    Object* Objects;
} VM;

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

extern VM vm;

void VMInit();
void VMFree();

InterpretResult Interpret(const char* sourceFile);
InterpretResult InterpretChunk(Chunk* chunk);


void Push(Value value);
Value Pop();
Value PopN(int n);
static Value Peek(int distance);
static bool CallValue(Value callee, int argumentCount);
static bool Call(ObjFunction* function, int argumentCount);
static bool IsFalsey(Value Value);
static void Concatenate();

#endif
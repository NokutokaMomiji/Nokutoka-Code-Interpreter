#ifndef NKCODE_VM_H
#define NKCODE_VM_H

#include "chunk.h"
#include "value.h"

#define STACK_MAX 256

typedef struct {
    Chunk* chunk;
    uint8_t* IP;
    Value Stack[STACK_MAX];
    Value* stackTop;
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
static Value Peek(int distance);
static bool IsFalsey(Value Value);
static void Concatenate();

#endif
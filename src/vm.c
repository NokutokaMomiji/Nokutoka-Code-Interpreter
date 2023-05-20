#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h> 
#include "common.h"
#include "scanner.h"
#include "compiler.h"
#include "debug.h"
#include "vm.h"

VM vm;

static void ResetStack() {
    vm.stackTop = vm.Stack;
}

static void RuntimeError(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    size_t instruction = vm.IP - vm.chunk->Code - 1;
    int line = 0;
    int currentNumOfInstructions = 0;
    for (int i = 0; i < vm.chunk->lineCapacity; i++){
        currentNumOfInstructions += vm.chunk->Lines[i];
        if (currentNumOfInstructions >= instruction) {
            break;
        }
        line++;
    }

    fprintf(stderr, "[Line %d] in script\n", line);
    ResetStack();
}

void VMInit() {
    ResetStack();
    srand(time(NULL));
}
void VMFree() {}

static InterpretResult Run() {
    #define READ_BYTE() (*vm.IP++)
    #define READ_CONSTANT() (vm.chunk->Constants.Values[READ_BYTE()])
    #define READ_CONSTANT_LONG() (vm.chunk->Constants.Values[(READ_BYTE() << 24) + (READ_BYTE() << 16) + (READ_BYTE() << 8) + READ_BYTE()])
    #define BINARY_OP(ValueType, op) \
        do { \
            if (!IS_NUMBER(Peek(0)) || !IS_NUMBER(Peek(1))) { \
                RuntimeError("Operands must be numbers."); \
                return INTERPRET_RUNTIME_ERROR; \
            } \
            double b = AS_NUMBER(Pop()); \
            double a = AS_NUMBER(Pop()); \
            Push(ValueType(a op b)); \
        } while (false)

    for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
        printf("          ");
        for (Value* Slot = vm.Stack; Slot < vm.stackTop; Slot++) {
            printf("[");
            ValuePrint(*Slot);
            printf(" ]");
        }
        printf("\n");
        DisassembleInstruction(vm.chunk, (int)(vm.IP - vm.chunk->Code));
#endif
        uint8_t Instruction;
        switch(Instruction = READ_BYTE()) {
            case OP_CONSTANT: {
                Value Constant = READ_CONSTANT();
                Push(Constant);
                break;
            }
            case OP_CONSTANT_LONG: {
                Value Constant = READ_CONSTANT_LONG();
                Push(Constant);
                break;
            }
            case OP_NULL:       Push(NULL_VALUE);           break;
            case OP_TRUE:       Push(BOOL_VALUE(true));     break;
            case OP_FALSE:      Push(BOOL_VALUE(false));    break;
            case OP_MAYBE:      Push(BOOL_VALUE((bool)(rand() % 2))); break;
            case OP_ADD:        BINARY_OP(NUMBER_VALUE, +); break;
            case OP_SUBTRACT:   BINARY_OP(NUMBER_VALUE, -); break;
            case OP_MULTIPLY:   BINARY_OP(NUMBER_VALUE, *); break;
            case OP_DIVIDE:     BINARY_OP(NUMBER_VALUE, /); break;
            case OP_NOT:        Push(BOOL_VALUE(IsFalsey(Pop()))); break;
            case OP_NEGATE: {
                bool isNum = IS_NUMBER(Peek(0));
                bool isBool = IS_BOOL(Peek(0));
                if (!isNum && !isBool) {
                    RuntimeError("Type %s cannot be negated.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                Value popedValue = Pop();
                Push(NUMBER_VALUE((isNum) ? -AS_NUMBER(popedValue) : -(double)(AS_BOOL(popedValue))));
            } 
            break;
            case OP_RETURN: {
                ValuePrint(Pop());
                printf("\n");
                return INTERPRET_OK;
            }
        }
    }

    #undef READ_BYTE
    #undef READ_CONSTANT
    #undef BINARY_OP
}

InterpretResult Interpret(const char* source) {
    Chunk chunk;
    ChunkInit(&chunk);

    if (!Compile(source, &chunk)) {
        ChunkFree(&chunk);
        return INTERPRET_COMPILE_ERROR;
    }

    InterpretResult Result = InterpretChunk(&chunk);

    ChunkFree(&chunk);
    return Result;
}

InterpretResult InterpretChunk(Chunk* chunk) {
    vm.chunk = chunk;
    vm.IP = vm.chunk->Code;
    return Run();
}

void Push(Value value) {
    *vm.stackTop = value;
    vm.stackTop++;
}

Value Pop() {
    vm.stackTop--;
    return *vm.stackTop;
}

static Value Peek(int distance) {
    return vm.stackTop[-1 - distance];
}

static bool IsFalsey(Value value) {
    return (IS_NULL(value) || (IS_BOOL(value) && !AS_BOOL(value)));
}
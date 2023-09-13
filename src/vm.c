#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h> 
#include <string.h>

#include "common.h"
#include "scanner.h"
#include "compiler.h"
#include "debug.h"
#include "vm.h"
#include "object.h"
#include "memory.h"

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
    vm.Objects = NULL;
}
void VMFree() {
    FreeObjects();
}

static InterpretResult Run() {
    #define READ_BYTE() (*vm.IP++)
    #define READ_CONSTANT() (vm.chunk->Constants.Values[READ_BYTE()])
    #define READ_CONSTANT_LONG() (vm.chunk->Constants.Values[(READ_BYTE() << 24) + (READ_BYTE() << 16) + (READ_BYTE() << 8) + READ_BYTE()])
    #define BINARY_OP(ValueType, op) \
        do { \
            if ((!IS_NUMBER(Peek(0)) && !IS_BOOL(Peek(0))) || (!IS_NUMBER(Peek(1)) && !IS_BOOL(Peek(1)))) { \
                RuntimeError("Operands must be numbers."); \
                return INTERPRET_RUNTIME_ERROR; \
            } \
            Value first = Pop(); \
            Value second = Pop(); \
            double b = (IS_BOOL(first)) ? (double)AS_BOOL(first) : AS_NUMBER(first); \
            double a = (IS_BOOL(second)) ? (double)AS_BOOL(second) : AS_NUMBER(second); \
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
            case OP_EQUAL: {
                Value b = Pop();
                Value a = Pop();
                Push(BOOL_VALUE(ValuesEqual(a, b)));
                break;   
            }
            case OP_NOT_EQUAL: {
                Value b = Pop();
                Value a = Pop();
                Push(BOOL_VALUE(!ValuesEqual(a, b)));
                break;
            }
            case OP_GREATER:    BINARY_OP(BOOL_VALUE, >);   break;
            case OP_SMALLER:    BINARY_OP(BOOL_VALUE, <);   break;
            case OP_GREATER_EQ: {
                Value b = Peek(0);
                Value a = Peek(1);
                bool valuesAreEqual = ValuesEqual(a, b);
                if (valuesAreEqual) {
                    Pop();
                    Pop();
                    Push(BOOL_VALUE(valuesAreEqual));
                    break;
                }

                BINARY_OP(BOOL_VALUE, >);
                break;
            }
            case OP_SMALLER_EQ: {
                Value b = Peek(0);
                Value a = Peek(1);
                bool valuesAreEqual = ValuesEqual(a, b);
                if (valuesAreEqual) {
                    Pop();
                    Pop();
                    Push(BOOL_VALUE(valuesAreEqual));
                    break;
                }

                BINARY_OP(BOOL_VALUE, <);
                break;
            }
            case OP_IS: {
                Value b = Pop();
                Value a = Pop();

                if (!IS_OBJECT(a) || !IS_OBJECT(b)) {
                    RuntimeError("Non-object type cannot be checked.");
                }

                
            }
            case OP_ADD: {
                if (IS_STRING(Peek(0)) && IS_STRING(Peek(1))) {
                    Concatenate();
                    break;
                }
                BINARY_OP(NUMBER_VALUE, +);
                break;
            }
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
    #undef READ_CONSTANT_LONG
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

static void Concatenate() {
    ObjString* b = AS_STRING(Pop());
    ObjString* a = AS_STRING(Pop());

    int Length = a->Length + b->Length;
    char* Chars = ALLOCATE(char, Length + 1);
    memcpy(Chars, a->Chars, a->Length);
    memcpy(Chars + a->Length, b->Chars, b->Length);

    Chars[Length] = '\0';

    ObjString* Result = StringTake(Chars, Length);
    Push(OBJECT_VALUE(Result));
}
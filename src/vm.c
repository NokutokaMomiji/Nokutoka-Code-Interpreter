#include <stdio.h>
#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "vm.h"

VM vm;

static void ResetStack() {
    vm.stackTop = vm.Stack;
}

void VMInit() {
    ResetStack();
}
void VMFree() {}

static InterpretResult Run() {
    #define READ_BYTE() (*vm.IP++)
    #define READ_CONSTANT() (vm.chunk->Constants.Values[READ_BYTE()])
    #define BINARY_OP(op) \
        do { \
            double b = Pop(); \
            double a = Pop(); \
            Push(a op b); \
        } while(false)

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
            case OP_ADD: BINARY_OP(+); break;
            case OP_SUBTRACT: BINARY_OP(-); break;
            case OP_MULTIPLY: BINARY_OP(*); break;
            case OP_DIVIDE: BINARY_OP(/); break;
            case OP_NEGATE: Push(-Pop()); break;
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
    Compile(source);
    return INTERPRET_OK;
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
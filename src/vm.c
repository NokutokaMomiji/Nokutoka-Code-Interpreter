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
    vm.frameCount = 0;
}

static void RuntimeError(const char* format, ...) {
    CallFrame* Frame = &vm.Frames[vm.frameCount - 1];

    fprintf(stderr, "Exception Stacktrace (most recent call FIRST):\n");
    for (int n = vm.frameCount - 1; n > 0; n--) {
        CallFrame* frame = &vm.Frames[n];
        ObjFunction* function = frame->Function;
        size_t instruction = frame->IP - function->chunk.Code - 1;

        int line = ChunkGetLine(&function->chunk, instruction);

        fprintf(stderr, "   %4d | ", line);
        if (function->Name == NULL)
            fprintf(stderr, "<script>\n");
        else
            fprintf(stderr, "%s()\n", function->Name->Chars);
    }

    va_list args;
    va_start(args, format);
    fprintf(stderr, "\nRuntimeError: ");
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    if (Frame->Function->Name == NULL)
        fprintf(stderr, "In <script>: \n");
    else
        fprintf(stderr, "In <%s()>: \n", Frame->Function->Name->Chars);

    ResetStack();
}

void VMInit() {
    ResetStack();
    srand(time(NULL));
    vm.Objects = NULL;
    TableInit(&vm.Strings);
    TableInit(&vm.Globals);
}
void VMFree() {
    TableFree(&vm.Strings);
    TableFree(&vm.Globals);
    FreeObjects();
}

static InterpretResult Run() {
    CallFrame* Frame = &vm.Frames[vm.frameCount - 1];
    #define READ_BYTE() (*Frame->IP++)
    #define READ_CONSTANT() (Frame->Function->chunk.Constants.Values[READ_BYTE()])
    #define READ_CONSTANT_LONG() (Frame->Function->chunk.Constants.Values[(READ_BYTE() << 24) + (READ_BYTE() << 16) + (READ_BYTE() << 8) + READ_BYTE()])
    #define READ_SHORT() (Frame->IP += 2, (uint16_t)((Frame->IP[-2] << 8) | Frame->IP[-1]))
    #define READ_STRING() AS_STRING(READ_CONSTANT())
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
        printf("( ");
        for (Value* Slot = vm.Stack; Slot < vm.stackTop; Slot++) {
            printf("[");
            ValuePrint(*Slot);
            printf(" ]");
        }
        printf(" )");
        printf("\n");
        DisassembleInstruction(&Frame->Function->chunk, (int)(Frame->IP - Frame->Function->chunk.Code));
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
            case OP_POP:        Pop();                      break;
            case OP_DUPLICATE:  Push(Peek(0));              break;
            case OP_DEFINE_GLOBAL: {
                ObjString* Name = READ_STRING();
                TableSet(&vm.Globals, Name, Peek(0));
                Pop();
                break;
            }
            case OP_GET_GLOBAL: {
                ObjString* Name = READ_STRING();
                Value value;
                if (!TableGet(&vm.Globals, Name, &value)) {
                    RuntimeError("Global variable '%s' not set before reading it.", Name->Chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                Push(value);
                break;
            }
            case OP_SET_GLOBAL: {
                ObjString* Name = READ_STRING();
                if (TableSet(&vm.Globals, Name, Peek(0))) {
                    TableDelete(&vm.Globals, Name);
                    RuntimeError("Global variable '%s' not set before reading it.", Name->Chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SET_LOCAL: {
                uint8_t Slot = READ_BYTE();
                Frame->Slots[Slot] = Peek(0);
                break;
            }
            case OP_GET_LOCAL: {
                uint8_t Slot = READ_BYTE();
                Push(Frame->Slots[Slot]);
                break;
            }
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

                if ((!IS_OBJECT(a) || !IS_OBJECT(b)) || (IS_STRING(a) && IS_STRING(b))) {
                    Push(BOOL_VALUE(ValuesEqual(a, b)));
                    break;
                }

                if (OBJECT_TYPE(a) != OBJECT_TYPE(b)) {
                    Push(BOOL_VALUE(false));
                    break;
                }

                Push(BOOL_VALUE(&AS_OBJECT(a) > &AS_OBJECT(b)));
                break;
            }
            case OP_ADD: {
                if (IS_STRING(Peek(0)) && IS_STRING(Peek(1))) {
                    Concatenate();
                    break;
                }
                BINARY_OP(NUMBER_VALUE, +);
                break;
            }
            case OP_POSTINCREASE: {
                if (!IS_NUMBER(Peek(0))) {
                    RuntimeError("Cannot post-increase a variable with a non-number value.");
                    break;
                }
                Value a = Pop();
                Push(a);
                Push(NUMBER_VALUE(AS_NUMBER(a) + 1));
                break;
            }
            case OP_PREINCREASE: {
                if (!IS_NUMBER(Peek(0))) {
                    RuntimeError("Cannot pre-increase a variable with a non-number value.");
                    break;
                }

                Value a = Pop();
                Value b = NUMBER_VALUE(AS_NUMBER(a) + 1);
                Push(b);
                Push(b);
                break;
            }
            case OP_SUBTRACT:   BINARY_OP(NUMBER_VALUE, -); break;
            case OP_POSTDECREASE: {
                if (!IS_NUMBER(Peek(0))) {
                    RuntimeError("Cannot post-decrease a variable with a non-number value.");
                    break;
                }

                Value a = Peek(0);
                Push(NUMBER_VALUE(AS_NUMBER(a) - 1));
                break;   
            }
            case OP_PREDECREASE: {
                if (!IS_NUMBER(Peek(0))) {
                    RuntimeError("Cannot pre-decrease a variable with a non-number value.");
                    break;
                }

                Value a = Pop();
                Value b = NUMBER_VALUE(AS_NUMBER(a) - 1);
                Push(b);
                Push(b);
                break;
            }
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
            case OP_PRINT: {
                ValuePrint(Pop());
                printf("\n");
                break;
            }
            case OP_JUMP: {
                uint16_t Offset = READ_SHORT();
                Frame->IP += Offset;
                break;
            }
            case OP_JUMP_IF_FALSE: {
                uint16_t Offset = READ_SHORT();
                if (IsFalsey(Peek(0)))
                    Frame->IP += Offset;
                break;
            }
            case OP_LOOP: {
                uint16_t Offset = READ_SHORT();
                Frame->IP -= Offset;
                break;
            }
            case OP_CALL: {
                int argumentCount = READ_BYTE();
                if (!CallValue(Peek(argumentCount), argumentCount))
                    return INTERPRET_RUNTIME_ERROR;
                
                Frame = &vm.Frames[vm.frameCount - 1];
                break;
            }
            case OP_RETURN: {
                return INTERPRET_OK;
            }
        }
    }

    #undef READ_BYTE
    #undef READ_CONSTANT
    #undef READ_CONSTANT_LONG
    #undef READ_SHORT
    #undef READ_STRING
    #undef BINARY_OP
}

InterpretResult Interpret(const char* source) {
    ObjFunction* Function = Compile(source);
    if (Function == NULL)
        return INTERPRET_COMPILE_ERROR;
    
    Push(OBJECT_VALUE(Function));
    CallFrame* Frame = &vm.Frames[vm.frameCount++];
    Frame->Function = Function;
    Frame->IP = Function->chunk.Code;
    Frame->Slots = vm.Stack;

    Call(Function, 0);

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

static bool Call(ObjFunction* function, int argumentCount) {
    CallFrame* Frame = &vm.Frames[vm.frameCount++];
    Frame->Function = function;
    Frame->IP = function->chunk.Code;
    Frame->Slots = vm.stackTop - argumentCount - 1;
    return true;
}

static bool CallValue(Value callee, int argumentCount) {
    if (IS_OBJECT(callee)) {
        switch (OBJECT_TYPE(callee)) {
            case OBJ_FUNCTION:
                return Call(AS_FUNCTION(callee), argumentCount);
            default:
                break;
        }
    }
    RuntimeError("Can only call functions and classes.");
    return false;
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
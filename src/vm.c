#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h> 
#include <string.h>
#include <math.h>

#include "Common.h"
#include "Scanner.h"
#include "Compiler.h"
#include "Debug.h"
#include "Object.h"
#include "Memory.h"
#include "Array.h"
#include "Map.h"
#include "VM.h"

VM vm;

/// @brief Resets the VM's value stack.
static void ResetStack() {
    vm.stackTop = vm.Stack;
    vm.frameCount = 0;
    vm.openUpvalues = NULL;
}

/// @brief For reporting a Runtime Error.
static void RuntimeError(const char* format, ...) {
    fprintf(stderr, "Exception Stacktrace (most recent call FIRST):\n");

    // We iterate through all of the frames to get the full traceback.
    for (int n = vm.frameCount - 1; n >= 0; n--) {
        CallFrame* frame = &vm.frames[n];
        ObjFunction* function = frame->closure->function;
        size_t instruction = frame->ip - function->chunk.code - 1;

        int line = MJ_ChunkGetLine(&function->chunk, instruction);
        char* content = MJ_ChunkGetSource(&function->chunk, instruction);

        fprintf(stderr, COLOR_MAGENTA "   %4d " COLOR_RESET "| ", line);
        if (function->name == NULL)
            fprintf(stderr, COLOR_CYAN "<script>" COLOR_RESET);
        else
            fprintf(stderr, COLOR_CYAN "%s()" COLOR_RESET, function->name->chars);
        fprintf(stderr, " | %s\n", content);
    }

    // We print out the error message.
    va_list args;
    va_start(args, format);
    fprintf(stderr, "\n" COLOR_RED "RuntimeError" COLOR_RESET ": ");
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    // We print out the current line that triggered the error.
    CallFrame* frame = &vm.frames[vm.frameCount - 1];
    ObjFunction* function = frame->closure->function;
    size_t instruction = frame->ip - function->chunk.code - 1;
    int line = MJ_ChunkGetLine(&function->chunk, instruction);
    char* content = MJ_ChunkGetSource(&function->chunk, instruction);

    if (function->name == NULL)
        fprintf(stderr, "In <script>: \n");
    else
        fprintf(stderr, "In <%s()>: \n", function->name->chars);
    
    fprintf(stderr, "   %4d | %s\n", line, content);
    ResetStack();
}

static void DefineNative(const char* name, NativeFn function) {
    Push(OBJECT_VALUE(StringCopy(name, (int)strlen(name))));
    Push(OBJECT_VALUE(NativeNew(function)));
    TableSet(&vm.globals, AS_STRING(vm.Stack[0]), vm.Stack[1]);
    PopN(2);
}

static Value ToNumberNative(int argumentCount, Value* arguments) {
    if (argumentCount != 1)
        RuntimeError("Excepted 1 argument, got %d.", argumentCount);

    if (!IS_STRING(arguments[0]))
        RuntimeError("Expected string value.");

    
}

static Value ExitNative(int argumentCount, Value* arguments) {
    if (argumentCount == 1 && IS_NUMBER(arguments[0]))
        exit(AS_NUMBER(arguments[0]));
    else
        exit(0);
}

static Value InputNative(int argumentCount, Value* arguments) {
    if (argumentCount > 0 && IS_STRING(arguments[0]))
        printf(AS_CSTRING(arguments[0]));
    
    char input[2048];

    fgets(input, sizeof(input), stdin);

    input[strcspn(input, "\n")] = 0;

    return OBJECT_VALUE(StringCopy(input, 2048));
}

static Value ClockNative(int argumentCount, Value* arguments) {
    return NUMBER_VALUE((double)clock() / CLOCKS_PER_SEC);
}

static Value LengthNative(int argumentCount, Value* arguments) {
    if (argumentCount != 1) {
        RuntimeError("len expected 1 argument, got %d.", argumentCount);
    }

    if (IS_STRING(arguments[0])) {
        ObjString* string = AS_STRING(arguments[0]);
        return NUMBER_VALUE(string->length);
    }

    if (IS_ARRAY(arguments[0])) {
        ObjArray* array = AS_ARRAY(arguments[0]);
        return NUMBER_VALUE(array->items.count);
    }

    RuntimeError("len expected a valid argument.", argumentCount);
    return NULL_VALUE;
}

static Value ExecNative(int argumentCount, Value* arguments) {
    if (argumentCount != 1) {
        RuntimeError("\"exec\" expected 1 argument, got %d.", argumentCount);
        return NULL_VALUE;
    }

    if (!IS_STRING(arguments[0])) {
        RuntimeError("\"exec\" expected a string.");
        return NULL_VALUE;
    }

    return NULL_VALUE;
}

static Value SystemNative(int argumentCount, Value* arguments) {
    if (argumentCount != 1) {
        RuntimeError("\"system\" expected 1 argument, got %d.", argumentCount);
        return NULL_VALUE;
    }

    if (!IS_STRING(arguments[0])) {
        RuntimeError("\"system\" expected a string.");
        return NULL_VALUE;
    }

    int result = system(AS_CSTRING(arguments[0]));

    return NUMBER_VALUE(result);
}

static void PrintCallFrame(const CallFrame* frame) {
    ObjFunction* function = frame->closure->function;
    // Figure out the instruction‐pointer offset
    size_t ipOffset = (size_t)(frame->ip - function->chunk.code);
    // Figure out which slot in the VM stack this frame's locals start at
    int slotIndex = (int)(frame->slots - vm.Stack);

    // Print header line
    printf(
        "=== CallFrame @ stack[%d] ===\n"
        "Function : '%s'\n"
        "Arity    : %d\n"
        "IP Offset: %zu\n",
        slotIndex,
        function->name ? function->name->chars : "<script>",
        function->arity,
        ipOffset
    );

    // Print the argument/local values
    if (function->arity > 0) {
        printf("Locals/Args:");
        for (int i = 0; i < function->arity; i++) {
            printf(" ");
            ValuePrint(frame->slots[i]);
        }
        printf("\n");
    }
    printf("===========================\n");
}

void VMInit() {
    ResetStack();
    srand(time(NULL));
    vm.objects = NULL;

    vm.allocatedBytes = 0;
    vm.nextCollection = 1024 * 1024;

    vm.grayCapacity = 0;
    vm.grayCount = 0;
    vm.grayStack = NULL;

    vm.safeguardStack = malloc(sizeof(Object*) * 4);
    if (vm.safeguardStack == NULL)
        printf("> Failed to allocate safeguard stack.\n");

    TableInit(&vm.strings);
    TableInit(&vm.globals);

    vm.initString = NULL;

    DefineNative("clock", ClockNative);
    DefineNative("input", InputNative);
    DefineNative("exit", ExitNative);
    DefineNative("len", LengthNative);
    DefineNative("exec", ExecNative);
    DefineNative("system", SystemNative);
}

void VMFree() {
    TableFree(&vm.strings);
    TableFree(&vm.globals);
    vm.initString = NULL;
    FreeObjects();
}

void Push(Value value) {
    *vm.stackTop = value;
    vm.stackTop++;
}

Value Pop() {
    vm.stackTop--;
    return *vm.stackTop;
}

Value PopN(int n) {
    vm.stackTop -= n;
    return *vm.stackTop;
}

static Value Peek(int distance) {
    return vm.stackTop[-1 - distance];
}

static bool Call(ObjClosure* closure, int argumentCount) {
    if (argumentCount != closure->function->arity) {
        RuntimeError("Expected %d arguments but got %d instead.", closure->function->arity, argumentCount);
        return false;
    }

    if (vm.frameCount == FRAMES_MAX) {
        RuntimeError("Stack Overflow. Limit is %d.", FRAMES_MAX);
        return false;
    }

    CallFrame* frame = &vm.frames[vm.frameCount++];
    frame->closure = closure;
    frame->ip = closure->function->chunk.code;
    frame->slots = vm.stackTop - argumentCount - 1;
    PrintCallFrame(frame);
    return true;
}

static bool CallValue(Value callee, int argumentCount) {
    if (IS_OBJECT(callee)) {
        switch (OBJECT_TYPE(callee)) {
            case OBJ_CLASS: {
                ObjClass* class = AS_CLASS(callee);
                vm.stackTop[-argumentCount - 1] = OBJECT_VALUE(InstanceNew(class));
                if (IS_CLOSURE(class->constructor)) {
                    return Call(AS_CLOSURE(class->constructor), argumentCount);
                } else if (argumentCount != 0) {
                    RuntimeError("Constructor expected 0 arguments but got %d.", argumentCount);
                    return false;
                }
                return true;
            }
            case OBJ_NATIVE: {
                NativeFn Native = AS_NATIVE(callee);
                Value Result = Native(argumentCount, vm.stackTop - argumentCount);
                vm.stackTop -= argumentCount + 1;
                Push(Result);
                return true;
            }
            case OBJ_CLOSURE: 
                return Call(AS_CLOSURE(callee), argumentCount);
            case OBJ_BOUND_METHOD: {
                ObjBoundMethod* bound = AS_BOUND_METHOD(callee);
                vm.stackTop[-argumentCount - 1] = bound->receiver;
                return Call(bound->method, argumentCount);
            }
            default:
                break;
        }
    }
    RuntimeError("Can only call functions and classes.");
    return false;
}

static bool InvokeFromClass(ObjClass* class, ObjString* name, int argumentCount) {
    Value method;

    if (!TableGet(&class->methods, name, &method)) {
        RuntimeError("\"%s\" object has no property \"%s\".", class->className->chars, name->chars);
        return false;
    }

    return Call(AS_CLOSURE(method), argumentCount);
}

static bool Invoke(ObjString* name, int argumentCount) {
    Value receiver = Peek(argumentCount);

    if (!IS_INSTANCE(receiver)) {
        RuntimeError("Only instances have methods.");
        return false;
    }

    ObjInstance* instance = AS_INSTANCE(receiver);

    Value value;
    if (TableGet(&instance->fields, name, &value)) {
        vm.stackTop[-argumentCount - 1] = value;
        return CallValue(value, argumentCount);
    }

    return InvokeFromClass(instance->class, name, argumentCount);
}

static bool BindMethod(ObjClass* class, ObjString* name) {
    Value method;

    if (!TableGet(&class->methods, name, &method)) {
        RuntimeError("\"%s\" object has no property \"%s\".", class->className->chars, name->chars);
        return false;
    }

    ObjBoundMethod* bound = BoundMethodNew(Peek(0), AS_CLOSURE(method));

    Pop();
    Push(OBJECT_VALUE(bound));
    return true;
}

static ObjUpvalue* CaptureUpvalue(Value* local) {
    ObjUpvalue* previousUpvalue = NULL;
    ObjUpvalue* Upvalue = vm.openUpvalues;
    while (Upvalue != NULL && Upvalue->location > local) {
        previousUpvalue = Upvalue;
        Upvalue = Upvalue->next;
    }

    if (Upvalue != NULL && Upvalue->location == local) 
        return Upvalue;

    ObjUpvalue* createdUpvalue = UpvalueNew(local);

    createdUpvalue->next = Upvalue;
    if (previousUpvalue == NULL) {
        vm.openUpvalues = createdUpvalue;
    }
    else {
        previousUpvalue->next = createdUpvalue;
    }

    return createdUpvalue;
}

static void CloseUpvalues(Value* last) {
    while (vm.openUpvalues != NULL && vm.openUpvalues->location >= last) {
        ObjUpvalue* Upvalue = vm.openUpvalues;
        Upvalue->closed = *Upvalue->location;
        Upvalue->location = &Upvalue->closed;
        vm.openUpvalues = Upvalue->next;
    }
}

static bool DefineMethod(ObjString* name) {
    Value method = Peek(0);
    ObjClass* class = AS_CLASS(Peek(1));

    if (strcmp(name->chars, class->className->chars) == 0) {
        if (!IS_NULL(class->constructor)) {
            RuntimeError("Duplicate constructor defined for class \"%s\".", class->className->chars);
            return false;
        }
        class->constructor = method;
    } else {
        TableSet(&class->methods, name, method);
    }

    Pop();
    return true;
}

static bool IsFalsey(Value value) {
    return (IS_NULL(value) || (IS_BOOL(value) && !AS_BOOL(value)));
}

static void Concatenate() {
    ObjString* b = AS_STRING(Peek(0));
    ObjString* a = AS_STRING(Peek(1));

    int length = a->length + b->length;
    char* chars = ALLOCATE(char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);

    chars[length] = '\0';

    ObjString* Result = StringTake(chars, length);
    PopN(2);
    Push(OBJECT_VALUE(Result));
}

static InterpretResult Run() {
    CallFrame* frame = &vm.frames[vm.frameCount - 1];
    #define READ_BYTE() (*frame->ip++)
    #define READ_CONSTANT() (frame->closure->function->chunk.constants.values[READ_BYTE()])
    #define READ_CONSTANT_LONG() (frame->closure->function->chunk.constants.values[(READ_BYTE() << 24) + (READ_BYTE() << 16) + (READ_BYTE() << 8) + READ_BYTE()])
    #define READ_SHORT() (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
    #define READ_STRING() AS_STRING(READ_CONSTANT())
    #define BINARY_OP(ValueType, op) \
        do { \
            if ((!IS_NUMBER(Peek(0)) && !IS_BOOL(Peek(0))) || (!IS_NUMBER(Peek(1)) && !IS_BOOL(Peek(1)))) { \
                RuntimeError("Operands must be numbers."); \
                return RUNTIME_ERROR(NULL_VALUE); \
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
        DisassembleInstruction(&frame->closure->function->chunk, (int)(frame->ip - frame->closure->function->chunk.code));
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
            case OP_POP: {
                Pop();
                break;
            }
            case OP_DUPLICATE:  Push(Peek(0));              break;
            case OP_DEFINE_GLOBAL: {
                ObjString* name = READ_STRING();
                TableSet(&vm.globals, name, Peek(0));
                Pop();
                break;
            }
            case OP_GET_GLOBAL: {
                ObjString* name = READ_STRING();
                Value value;
                if (!TableGet(&vm.globals, name, &value)) {
                    RuntimeError("Global variable '%s' not set before reading it.", name->chars);
                    return RUNTIME_ERROR(NULL_VALUE);
                }
                Push(value);
                break;
            }
            case OP_SET_GLOBAL: {
                ObjString* name = READ_STRING();
                if (TableSet(&vm.globals, name, Peek(0))) {
                    TableDelete(&vm.globals, name);
                    RuntimeError("Global variable '%s' not set before reading it.", name->chars);
                    return RUNTIME_ERROR(NULL_VALUE);
                }
                break;
            }
            case OP_SET_LOCAL: {
                uint8_t Slot = READ_BYTE();
                frame->slots[Slot] = Peek(0);
                break;
            }
            case OP_GET_LOCAL: {
                uint8_t Slot = READ_BYTE();
                Push(frame->slots[Slot]);
                break;
            }
            case OP_SET_INDEX: {
                if (!IS_OBJECT(Peek(2))) {
                    RuntimeError("Cannot access the index of a non-object.");
                    return RUNTIME_ERROR(NULL_VALUE);
                }

                
                Value value = Peek(0);
                
                switch(OBJECT_TYPE(Peek(2))) {
                    case OBJ_ARRAY: {
                        ObjArray* Array = AS_ARRAY(Peek(2));
                        if (!MJ_ArraySet(Array, Peek(1), value)) {
                            RuntimeError("Invalid array setting.");
                            return RUNTIME_ERROR(NULL_VALUE);
                        }
                        break;
                    }

                    case OBJ_MAP: {
                        ObjMap* Map = AS_MAP(Peek(2));
                        if (!MapSet(Map, Peek(1), value)) {
                            RuntimeError("Invalid array setting.");
                            return RUNTIME_ERROR(NULL_VALUE);
                        }
                        break;
                    }
                }
                
                PopN(3);    // We pop out the value, the index and the list from the stack.
                Push(value);
                break;
            }
            case OP_GET_INDEX: {
                if (!IS_OBJECT(Peek(1))) {
                    RuntimeError("Cannot access the index of a non-object.");
                    return RUNTIME_ERROR(NULL_VALUE);
                }

                Value value;
                switch(OBJECT_TYPE(Peek(1))) {
                    case OBJ_ARRAY: {
                        ObjArray* Array = AS_ARRAY(Peek(1));
                        if (!MJ_ArrayGet(Array, Peek(0), &value)) {
                            RuntimeError("Invalid array access.");
                            return RUNTIME_ERROR(NULL_VALUE);
                        }
                        break;
                    }

                    case OBJ_MAP: {
                        ObjMap* Map = AS_MAP(Peek(1));
                        if (!MapGet(Map, Peek(0), &value)) {
                            RuntimeError("Invalid map access.");
                            return RUNTIME_ERROR(NULL_VALUE);
                        }
                        break;
                    }
                }

                PopN(2);
                Push(value);
                break;
            }
            case OP_GET_INDEX_RANGED: {
                if (!IS_OBJECT(Peek(3))) {
                    RuntimeError("Cannot access the indexes of a non-object.");
                    return RUNTIME_ERROR(NULL_VALUE);
                }

                ObjArray* Array = AS_ARRAY(Peek(3));
                Value newArray;
                if (!MJ_ArrayGetRange(Array, Peek(2), Peek(1), Peek(0), &newArray)) {
                    RuntimeError("Invalid array access.");
                    return RUNTIME_ERROR(NULL_VALUE);
                }

                PopN(3);
                Push(newArray);
                break;
            }
            case OP_SET_UPVALUE: {
                uint8_t Slot = READ_BYTE();
                *frame->closure->upvalues[Slot]->location = Peek(0);
                break;
            }
            case OP_GET_UPVALUE: {
                uint8_t Slot = READ_BYTE();
                Push(*frame->closure->upvalues[Slot]->location);
                break;
            }
            case OP_INIT_PROPERTY: {
                if (!IS_CLASS(Peek(1))) {
                    RuntimeError("Only classes have fields.");
                    return RUNTIME_ERROR(NULL_VALUE);
                }

                ObjClass* class = AS_CLASS(Peek(1));
                ObjString* string = READ_STRING();

                if (TableContains(&class->defaultFields, string)) {
                    RuntimeError(
                        "Duplicate field \"%s\" on class \"%s\".",
                        string->chars, 
                        class->className->chars
                    );
                    return RUNTIME_ERROR(NULL_VALUE);
                }

                TableSet(&class->defaultFields, string, Peek(0));
                Pop();
                break;
            }
            case OP_SET_PROPERTY: {
                if (!IS_INSTANCE(Peek(1))) {
                    RuntimeError("Only class instances have fields.");
                    return RUNTIME_ERROR(NULL_VALUE);
                }

                ObjInstance* instance = AS_INSTANCE(Peek(1));
                ObjString* string = READ_STRING();

                if (!TableContains(&instance->fields, string)) {
                    RuntimeError(
                        "Instance of class \"%s\" has no field \"%s\".", 
                        instance->class->className->chars,
                        string->chars
                    );
                    return RUNTIME_ERROR(NULL_VALUE);
                }

                TableSet(&instance->fields, string, Peek(0));
                Value value = Pop();
                Pop();
                Push(value);
                break;
            }
            case OP_GET_PROPERTY: {
                if (!IS_INSTANCE(Peek(0))) {
                    RuntimeError("Only class instances have properties.");
                    return RUNTIME_ERROR(NULL_VALUE);
                }

                ObjInstance* instance = AS_INSTANCE(Peek(0));
                ObjString* name = READ_STRING();

                Value value;
                if (TableGet(&instance->fields, name, &value)) {
                    Pop();
                    Push(value);
                    break;
                }

                if (!BindMethod(instance->class, name)) {
                    return RUNTIME_ERROR(NULL_VALUE);
                }
                break;
            }
            case OP_GET_SUPER: {
                ObjString* name = READ_STRING();
                ObjClass* superclass = AS_CLASS(Pop());

                if (!BindMethod(superclass, name)) {
                    return RUNTIME_ERROR(NULL_VALUE);
                }
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
                Value b = Peek(0);
                Value a = Peek(1);

                if ((!IS_OBJECT(a) || !IS_OBJECT(b)) || (IS_STRING(a) && IS_STRING(b))) {
                    Push(BOOL_VALUE(ValuesEqual(a, b)));
                    break;
                }

                if (OBJECT_TYPE(a) != OBJECT_TYPE(b)) {
                    Push(BOOL_VALUE(false));
                    break;
                }
                PopN(2);
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
                    return RUNTIME_ERROR(NULL_VALUE);
                }
                Value a = Peek(0);
                Pop();
                Push(a);
                Push(NUMBER_VALUE(AS_NUMBER(a) + 1));
                break;
            }
            case OP_PREINCREASE: {
                if (!IS_NUMBER(Peek(0))) {
                    RuntimeError("Cannot pre-increase a variable with a non-number value.");
                    return RUNTIME_ERROR(NULL_VALUE);
                }

                Value a = Peek(0);
                Value b = NUMBER_VALUE(AS_NUMBER(a) + 1);
                Pop();
                Push(b);
                Push(b);
                break;
            }
            case OP_SUBTRACT:   BINARY_OP(NUMBER_VALUE, -); break;
            case OP_POSTDECREASE: {
                if (!IS_NUMBER(Peek(0))) {
                    RuntimeError("Cannot post-decrease a variable with a non-number value.");
                    return RUNTIME_ERROR(NULL_VALUE);
                }

                Value a = Peek(0);
                Push(NUMBER_VALUE(AS_NUMBER(a) - 1));
                break;   
            }
            case OP_PREDECREASE: {
                if (!IS_NUMBER(Peek(0))) {
                    RuntimeError("Cannot pre-decrease a variable with a non-number value.");
                    return RUNTIME_ERROR(NULL_VALUE);
                }

                Value a = Peek(0);
                Value b = NUMBER_VALUE(AS_NUMBER(a) - 1);
                Pop();
                Push(b);
                Push(b);
                break;
            }
            case OP_MULTIPLY:   BINARY_OP(NUMBER_VALUE, *); break;
            case OP_DIVIDE:     BINARY_OP(NUMBER_VALUE, /); break;
            case OP_MOD: {
                Value a = Peek(0);
                Value b = Peek(1);

                if (!IS_NUMBER(a) || !IS_NUMBER(b)) {
                    RuntimeError("Operands must be numbers.");
                    return RUNTIME_ERROR(NULL_VALUE);
                }

                double result = fmod(AS_NUMBER(b), AS_NUMBER(a));

                PopN(2);
                Push(NUMBER_VALUE(result));

                break;
            }
            case OP_BITWISE_AND: {
                Value a = Peek(0);
                Value b = Peek(1);

                if (IS_OBJECT(a) || IS_OBJECT(b)) {
                    RuntimeError("Cannot perform an and operation between two objects");
                    return RUNTIME_ERROR(NULL_VALUE);
                }

                unsigned char* aBytes;
                unsigned char* bBytes;

                switch (a.type) {
                    case VALUE_BOOL:    aBytes = (unsigned char*)AS_BOOL(a);           break;
                    case VALUE_NUMBER:  aBytes = (unsigned char*)((int)AS_NUMBER(a));  break;
                    case VALUE_NULL:    aBytes = 0;
                }

                switch (b.type) {
                    case VALUE_BOOL:    bBytes = (unsigned char*)AS_BOOL(b);           break;
                    case VALUE_NUMBER:  bBytes = (unsigned char*)((int)AS_NUMBER(b));  break;
                    case VALUE_NULL:    bBytes = 0;
                }

                int result = (int)aBytes & (int)bBytes;

                PopN(2);
                Push(NUMBER_VALUE(result));
                break;
            }
            case OP_BITWISE_OR: {
                Value a = Peek(0);
                Value b = Peek(1);

                if (!IS_NUMBER(a) || !IS_NUMBER(b)) {
                    RuntimeError("Invalid operands for operation.");
                    return RUNTIME_ERROR(NULL_VALUE);
                }

                int result = (int)floor(AS_NUMBER(a)) | (int)floor(AS_NUMBER(b));

                PopN(2);
                Push(NUMBER_VALUE(result));
                break;
            }
            case OP_NOT:        Push(BOOL_VALUE(IsFalsey(Pop()))); break;
            case OP_NEGATE: {
                bool isNum = IS_NUMBER(Peek(0));
                bool isBool = IS_BOOL(Peek(0));
                if (!isNum && !isBool) {
                    RuntimeError("Type %s cannot be negated.");
                    return RUNTIME_ERROR(NULL_VALUE);
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
                frame->ip += Offset;
                break;
            }
            case OP_JUMP_IF_FALSE: {
                uint16_t Offset = READ_SHORT();
                if (IsFalsey(Peek(0)))
                    frame->ip += Offset;
                break;
            }
            case OP_LOOP: {
                uint16_t Offset = READ_SHORT();
                frame->ip -= Offset;
                break;
            }
            case OP_CALL: {
                int argumentCount = READ_BYTE();
                if (!CallValue(Peek(argumentCount), argumentCount))
                    return RUNTIME_ERROR(NULL_VALUE);
                
                frame = &vm.frames[vm.frameCount - 1];
                break;
            }
            case OP_INVOKE: {
                ObjString* method = READ_STRING();
                int argumentCount = READ_BYTE();

                if (!Invoke(method, argumentCount)) {
                    return RUNTIME_ERROR(NULL_VALUE);
                }

                frame = &vm.frames[vm.frameCount - 1];
                break;
            }
            case OP_SUPER_INVOKE: {
                ObjString* method = READ_STRING();
                int argumentCount = READ_BYTE();
                ObjClass* superclass = AS_CLASS(Pop());

                if (strcmp(method->chars, "super") == 0) {
                    if (!IS_CLOSURE(superclass->constructor)) {
                        RuntimeError("Cannot call super since superclass has no constructor");
                        return RUNTIME_ERROR(NULL_VALUE);
                    }
                    
                    if (!Call(AS_CLOSURE(superclass->constructor), argumentCount)) {
                        return RUNTIME_ERROR(NULL_VALUE);
                    }

                    frame = &vm.frames[vm.frameCount - 1];
                    break;
                }

                if (!InvokeFromClass(superclass, method, argumentCount)) {
                    return RUNTIME_ERROR(NULL_VALUE);
                }
                frame = &vm.frames[vm.frameCount - 1];
                break;
            }
            case OP_CLOSURE: {
                ObjFunction* function = AS_FUNCTION(READ_CONSTANT());
                ObjClosure* closure = ClosureNew(function);
                Push(OBJECT_VALUE(closure));

                for (int i = 0; i < closure->upvalueCount; i++) {
                    uint8_t isLocal = READ_BYTE();
                    uint8_t index = READ_BYTE();
                    if (isLocal)
                        closure->upvalues[i] = CaptureUpvalue(frame->slots + index);
                    else
                        closure->upvalues[i] = frame->closure->upvalues[index];
                }
                break;
            }
            case OP_ARRAY: {
                int numOfItems = READ_SHORT();
                ObjArray* Array = ArrayNew();

                // We jump over all of the item values and move to the NULL placeholder value.
                vm.stackTop[-numOfItems - 1] = OBJECT_VALUE(Array);

                for (int i = numOfItems - 1; i >= 0; i--) {
                    MJ_ArrayAdd(Array, Peek(i));
                }
                PopN(numOfItems);
                break;
            }

            case OP_MAP: {
                int numOfItems = READ_SHORT();
                ObjMap* Map = MapNew();

                vm.stackTop[-numOfItems - 1] = OBJECT_VALUE(Map);

                for (int i = numOfItems - 1; i >= 0; i -= 2) {
                    MapSet(Map, Peek(i), Peek(i - 1));
                }

                PopN(numOfItems);
                break;
            }
            case OP_CLASS: {
                Push(OBJECT_VALUE(ClassNew(READ_STRING())));
                break;
            }
            case OP_INHERIT: {
                Value superclass = Peek(1);

                if (!IS_CLASS(superclass)) {
                    RuntimeError("Classes can only inherit from other classes.");
                    return RUNTIME_ERROR(NULL_VALUE);
                }

                ObjClass* subclass = AS_CLASS(Peek(0));
                TableAddAll(&AS_CLASS(superclass)->methods, &subclass->methods);
                TableAddAll(&AS_CLASS(superclass)->defaultFields, &subclass->defaultFields);
                Pop();
                break;
            }
            case OP_METHOD: {
                if (!DefineMethod(READ_STRING())) {
                    return RUNTIME_ERROR(NULL_VALUE);
                }
                break;
            }
            case OP_CLOSE_UPVALUE: {
                CloseUpvalues(vm.stackTop - 1);
                Pop();
                break;
            }
            case OP_RETURN: {
                Value result = Pop();
                CloseUpvalues(frame->slots);
                vm.frameCount--;
                if (vm.frameCount == 0) {
                    InterpretResult interpretResult = RUNTIME_OK(result);
                    
                    if (!IS_NULL(result)) {
                        ValuePrint(result);
                        printf("\n");
                    }
                    
                    Pop();
                    return interpretResult;
                }

                vm.stackTop = frame->slots;
                Push(result);
                frame = &vm.frames[vm.frameCount - 1];
                break;
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
    ObjFunction* function = Compile(source);

    if (function == NULL)
        return COMPILE_ERROR(NULL_VALUE);

    Push(OBJECT_VALUE(function));
    ObjClosure* closure = ClosureNew(function);
    Pop();
    Push(OBJECT_VALUE(closure));

    Call(closure, 0);

    return Run();
}

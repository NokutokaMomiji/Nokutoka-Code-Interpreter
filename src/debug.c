#include <stdio.h>
#include "Debug.h"
#include "Object.h"
#include "Value.h"

/// @brief ``[DEBUG]`` Displays the stored information in a chunk array.
/// @param chunk Chunk that contains the information to display.
/// @param name Name for identification.
void DisassembleChunk(MJ_Chunk* chunk, const char* name) {
    printf("|==[ %s ]==|\n", name);

    for (int offset = 0; offset < chunk->count;) {
        offset = DisassembleInstruction(chunk, offset);
    }
}

/// @brief [INTERNAL] Displays an instruction and increases the offset.
/// @param name Name of the instruction.
/// @param offset Current instruction offset.
/// @return Next offset.
static int SimpleInstruction(const char* name, int offset) {
    printf("%s\n", name);
    return offset + 1;
}

static int ByteInstruction(const char* name, MJ_Chunk* chunk, int offset) {
    uint8_t Slot = chunk->code[offset + 1];
    printf("%-16s %4d\n", name, Slot);
    return offset + 2;
}

static int JumpInstruction(const char* name, int sign, MJ_Chunk* chunk, int offset) {
    uint16_t Jump = (uint16_t)(chunk->code[offset + 1] << 8);
    Jump |= chunk->code[offset + 2];
    printf("%-16s %4d -> %d\n", name, offset + 3 + sign * Jump);
    return offset + 3;
}
/// @brief Shows value of constant (byte)
/// @param name Name of the constant instruction (OP_CONSTANT)
/// @param chunk Chunk that contains the constants.
/// @param offset Offset inside progam.
/// @return New offset (+2).
static int ConstantInstruction(const char* name, MJ_Chunk* chunk, int offset) {
    uint8_t Constant = chunk->code[offset + 1]; //Grab constant index from chunk.
    printf("%-16s %4d '", name, Constant);
    ValuePrint(chunk->constants.values[Constant]);
    printf("'\n");
    return offset + 2; //We skip over the Constant Operation Code + the Constant Index.
}

static int ConstantLongInstruction(const char* name, MJ_Chunk* chunk, int offset) {
    long constantIndex = chunk->code[offset + 1] << 24;
    constantIndex += chunk->code[offset + 2] << 16;
    constantIndex += chunk->code[offset + 3] << 8;
    constantIndex += chunk->code[offset + 4];

    printf("%-16s %4ld '", name, constantIndex);
    ValuePrint(chunk->constants.values[constantIndex]);
    printf("'\n");
    return offset + 5;  // We skip over the Constant Operation Code + the 4 bytes that make up the long index.
}

static int InvokeInstruction(const char* name, MJ_Chunk* chunk, int offset) {
    uint8_t constant = chunk->code[offset + 1];
    uint8_t argumentCount = chunk->code[offset + 2];

    printf("%-16s (%d args) %4d '", name, argumentCount, constant);
    ValuePrint(chunk->constants.values[constant]);
    printf("'\n");

    return offset + 3;
}
/// @brief [DEBUG] Prints out an instruction from a Chunk array at the given offset.
/// @param chunk Chunk array with instructions.
/// @param offset Instruction offset.
/// @return Next offset.
int DisassembleInstruction(MJ_Chunk* chunk, int offset) {
    printf("%04d ", offset);

    printf("%4d ", MJ_ChunkGetLine(chunk, offset));

    uint8_t instruction = chunk->code[offset];
    switch(instruction) {
        case OP_CONSTANT:
            return ConstantInstruction("OP_CONSTANT", chunk, offset);
        case OP_CONSTANT_LONG:
            return ConstantLongInstruction("OP_CONSTANT_LONG", chunk, offset);
        case OP_NULL:
            return SimpleInstruction("OP_NULL", offset);
        case OP_TRUE:   
            return SimpleInstruction("OP_TRUE", offset);
        case OP_FALSE:
            return SimpleInstruction("OP_FALSE", offset);
        case OP_MAYBE:
            return SimpleInstruction("OP_MAYBE", offset);
        case OP_POP:
            return SimpleInstruction("OP_POP", offset);
	    case OP_DUPLICATE:
	        return SimpleInstruction("OP_DUPLICATE", offset);
	    case OP_DEFINE_GLOBAL:
            return ConstantInstruction("OP_DEFINE_GLOBAL", chunk, offset);
        case OP_GET_GLOBAL:
            return ConstantInstruction("OP_GET_GLOBAL", chunk, offset);
        case OP_SET_GLOBAL:
            return ConstantInstruction("OP_SET_GLOBAL", chunk, offset);
        case OP_SET_LOCAL:
            return ByteInstruction("OP_SET_LOCAL", chunk, offset);
        case OP_GET_LOCAL:
            return ByteInstruction("OP_SET_LOCAL", chunk, offset);
        case OP_SET_INDEX:
            return SimpleInstruction("OP_SET_INDEX", offset);
        case OP_GET_INDEX:
            return SimpleInstruction("OP_GET_INDEX", offset);
        case OP_GET_INDEX_RANGED:
            return SimpleInstruction("OP_GET_INDEX_RANGED", offset);
        case OP_SET_UPVALUE:
            return ByteInstruction("OP_SET_UPVALUE", chunk, offset);
        case OP_GET_UPVALUE:
            return ByteInstruction("OP_GET_UPVALUE", chunk, offset);
        case OP_SET_PROPERTY:
            return ConstantInstruction("OP_SET_PROPERTY", chunk, offset);
        case OP_GET_PROPERTY:
            return ConstantInstruction("OP_GET_PROPERTY", chunk, offset);
        case OP_INIT_PROPERTY:
            return ConstantInstruction("OP_INIT_PROPERTY", chunk, offset);
        case OP_GET_SUPER:
            return ConstantInstruction("OP_GET_SUPER", chunk, offset);
        case OP_EQUAL:
            return SimpleInstruction("OP_EQUAL", offset);
        case OP_NOT_EQUAL:
            return SimpleInstruction("OP_NOT_EQUAL", offset);
        case OP_GREATER:
            return SimpleInstruction("OP_GREATER", offset);
        case OP_SMALLER:
            return SimpleInstruction("OP_SMALLER", offset);
        case OP_GREATER_EQ:
            return SimpleInstruction("OP_GREATER_EQ", offset);
        case OP_SMALLER_EQ:
            return SimpleInstruction("OP_SMALLER_EQ", offset);
        case OP_IS:
            return SimpleInstruction("OP_IS", offset);
        case OP_ADD:
            return SimpleInstruction("OP_ADD", offset);
        case OP_POSTINCREASE:
            return SimpleInstruction("OP_POSTINCREASE", offset);
        case OP_PREINCREASE:
            return SimpleInstruction("OP_PREINCREASE", offset);
        case OP_SUBTRACT:
            return SimpleInstruction("OP_SUBTRACT", offset);
        case OP_POSTDECREASE:
            return SimpleInstruction("OP_POSTDECREASE", offset);
        case OP_PREDECREASE:
            return SimpleInstruction("OP_PREDECREASE", offset);
        case OP_MULTIPLY:
            return SimpleInstruction("OP_MULTIPLY", offset);
        case OP_DIVIDE:
            return SimpleInstruction("OP_DIVIDE", offset);
        case OP_MOD:
            return SimpleInstruction("OP_MOD", offset);
        case OP_BITWISE_AND:
            return SimpleInstruction("OP_BITWISE_AND", offset);
        case OP_BITWISE_OR:
            return SimpleInstruction("OP_BITWISE_OR", offset);
        case OP_NEGATE:
            return SimpleInstruction("OP_NEGATE", offset);
        case OP_RETURN:
            return SimpleInstruction("OP_RETURN", offset);
        case OP_NOT:
            return SimpleInstruction("OP_NOT", offset);
        case OP_PRINT:
            return SimpleInstruction("OP_PRINT", offset);
        case OP_JUMP:
            return JumpInstruction("OP_JUMP", 1, chunk, offset);
        case OP_JUMP_IF_FALSE:
            return JumpInstruction("OP_JUMP_IF_FALSE", 1, chunk, offset);
        case OP_LOOP:
            return JumpInstruction("OP_LOOP", -1, chunk, offset);
        case OP_CALL:
            return ByteInstruction("OP_CALL", chunk, offset);
        case OP_ARRAY:
            return ByteInstruction("OP_ARRAY", chunk, offset);
        case OP_MAP:
            return ByteInstruction("OP_MAP", chunk, offset);
        case OP_CLASS:
            return ConstantInstruction("OP_CLASS", chunk, offset);
        case OP_INVOKE:
            return InvokeInstruction("OP_INVOKE", chunk, offset);
        case OP_INHERIT:
            return SimpleInstruction("OP_INHERIT", offset);
        case OP_METHOD:
            return ConstantInstruction("OP_METHOD", chunk, offset);
        case OP_SUPER_INVOKE:
            return InvokeInstruction("OP_SUPER_INVOKE", chunk, offset);
        case OP_CLOSURE: {
            offset++;
            uint8_t Constant = chunk->code[offset++];
            printf("%-16s %4d ", "OP_CLOSURE", Constant);
            ValuePrint(chunk->constants.values[Constant]);
            printf("\n");

            ObjFunction* function = AS_FUNCTION(chunk->constants.values[Constant]);
            for (int n = 0; n < function->upvalueCount; n++) {
                int isLocal = chunk->code[offset++];
                int index = chunk->code[offset++];
                printf("%04d      |                     %s %d\n", offset - 2, (isLocal) ? "local" : "upvalue", index);
            }

            return offset;
        }
        case OP_CLOSE_UPVALUE:
            return SimpleInstruction("OP_CLOSE_UPVALUE", offset);
        default:
            printf("Unknown Operation Code %d\n", instruction);
            return offset + 1;
    }
}
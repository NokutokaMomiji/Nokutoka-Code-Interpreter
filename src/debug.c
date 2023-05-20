#include <stdio.h>
#include "debug.h"

/// @brief [DEBUG] Displays the stored information in a chunk array.
/// @param chunk Chunk that contains the information to display.
/// @param name Name for identification.
void DisassembleChunk(Chunk* chunk, const char* name) {
    printf("|==[ %s ]==|\n", name);

    for (int offset = 0; offset < chunk->Count;) {
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

/// @brief Shows value of constant (byte)
/// @param name Name of the constant instruction (OP_CONSTANT)
/// @param chunk Chunk that contains the constants.
/// @param offset Offset inside progam.
/// @return New offset (+2).
static int ConstantInstruction(const char* name, Chunk* chunk, int offset) {
    uint8_t Constant = chunk->Code[offset + 1]; //Grab constant index from chunk.
    printf("%-16s %4d '", name, Constant);
    ValuePrint(chunk->Constants.Values[Constant]);
    printf("'\n");
    return offset + 2; //We skip over the Constant Operation Code + the Constant Index.
}

static int ConstantLongInstruction(const char* name, Chunk* chunk, int offset) {
    long constantIndex = chunk->Code[offset + 1] << 24;
    constantIndex += chunk->Code[offset + 2] << 16;
    constantIndex += chunk->Code[offset + 3] << 8;
    constantIndex += chunk->Code[offset + 4];

    printf("%-16s %4ld '", name, constantIndex);
    ValuePrint(chunk->Constants.Values[constantIndex]);
    printf("'\n");
    return offset + 5;  // We skip over the Constant Operation Code + the 4 bytes that make up the long index.
}

/// @brief [DEBUG] Prints out an instruction from a Chunk array at the given offset.
/// @param chunk Chunk array with instructions.
/// @param offset Instruction offset.
/// @return Next offset.
int DisassembleInstruction(Chunk* chunk, int offset) {
    printf("%04d ", offset);

    printf("%4d ", GetLine(chunk, offset));

    uint8_t Instruction = chunk->Code[offset];
    switch(Instruction) {
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
        case OP_ADD:
            return SimpleInstruction("OP_ADD", offset);
        case OP_SUBTRACT:
            return SimpleInstruction("OP_SUBTRACT", offset);
        case OP_MULTIPLY:
            return SimpleInstruction("OP_MULTIPLY", offset);
        case OP_DIVIDE:
            return SimpleInstruction("OP_DIVIDE", offset);
        case OP_NEGATE:
            return SimpleInstruction("OP_NEGATE", offset);
        case OP_RETURN:
            return SimpleInstruction("OP_RETURN", offset);
        case OP_NOT:
            return SimpleInstruction("OP_NOT", offset);
        default:
            printf("Unknown Operation Code %d\n", Instruction);
            return offset + 1;
    }
}

int GetLine(Chunk* chunk, int offset) {
    int lineNumber = 1;
    int totalInstructions = chunk->Lines[lineNumber];

    while(totalInstructions < offset) {
        lineNumber++;
        totalInstructions += chunk->Lines[lineNumber];
    }

    return lineNumber;
}
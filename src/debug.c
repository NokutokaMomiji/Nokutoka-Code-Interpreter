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

static int ConstantInstruction(const char* name, Chunk* chunk, int offset) {
    uint8_t Constant = chunk->Code[offset + 1]; //Grab constant index from chunk.
    printf("%-16s %4d '", name, Constant);
    ValuePrint(chunk->Constants.Values[Constant]);
    printf("'\n");
    return offset + 2; //We skip over the Constant Operation Code + the Constant Index.
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
        case OP_RETURN:
            return SimpleInstruction("OP_RETURN", offset);
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
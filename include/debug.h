#ifndef MOMIJI_DEBUG_H
#define MOMIJI_DEBUG_H

#include "Chunk.h"
#include "Value.h"

void DisassembleChunk(MJ_Chunk* chunk, const char* name);
int DisassembleInstruction(MJ_Chunk* chunk, int offset);
int GetLine(MJ_Chunk* chunk, int offset);

#endif
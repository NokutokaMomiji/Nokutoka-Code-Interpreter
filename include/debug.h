#ifndef NKCODE_DEBUG_H
#define NKCODE_DEBUG_H

#include "chunk.h"
#include "value.h"

void DisassembleChunk(Chunk* chunk, const char* name);
int DisassembleInstruction(Chunk* chunk, int offset);
int GetLine(Chunk* chunk, int offset);

#endif
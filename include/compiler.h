#ifndef NKCODE_COMPILER_H
#define NKCODE_COMPILER_H

#include "chunk.h"
#include "vm.h"
#include "object.h"

bool Compile(const char* source, Chunk* chunk);

#endif
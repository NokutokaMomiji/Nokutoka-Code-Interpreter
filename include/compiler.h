#ifndef MOMIJI_COMPILER_H
#define MOMIJI_COMPILER_H

#include "Chunk.h"
#include "VM.h"
#include "Object.h"

ObjFunction* Compile(const char* source);
void CompilerMarkRoots();

#endif
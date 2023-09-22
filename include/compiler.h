#ifndef NKCODE_COMPILER_H
#define NKCODE_COMPILER_H

#include "chunk.h"
#include "vm.h"
#include "object.h"

ObjFunction* Compile(const char* source);

#endif
#ifndef NKCODE_CHUNK_H
#define NKCODE_CHUNK_H

#include "common.h" //Common stuff.
#include "value.h"  //Value and ValueArray.

typedef enum {
    OP_CONSTANT,        //Represents a constant value.
    OP_CONSTANT_LONG,   //Represents a long constant value.
    OP_NULL,
    OP_TRUE,
    OP_FALSE,
    OP_MAYBE,
    OP_POP,
    OP_DUPLICATE,
    OP_DEFINE_GLOBAL,
    OP_GET_GLOBAL,
    OP_SET_GLOBAL,
    OP_GET_LOCAL,
    OP_SET_LOCAL,
    OP_EQUAL,
    OP_NOT_EQUAL,
    OP_GREATER,
    OP_SMALLER,
    OP_GREATER_EQ,
    OP_SMALLER_EQ,
    OP_IS,
    OP_ADD,
    OP_PREINCREASE,
    OP_POSTINCREASE,
    OP_SUBTRACT,
    OP_PREDECREASE,
    OP_POSTDECREASE,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_NOT,
    OP_NEGATE,          //Negates a value.
    OP_PRINT,
    OP_JUMP_IF_FALSE,   
    OP_JUMP,
    OP_LOOP,
    OP_CALL,
    OP_RETURN,          //Return from current function.
} OpCode;

typedef struct {
    int Offset;
    int Line;
    char* Content;
} LineStart;

typedef struct {
    int Count;              //Number of elements in array.
    int Capacity;           //Number of available slots.
    uint8_t* Code;          //Instruction elements.
    ValueArray Constants;   //Array of constant values.
    LineStart* Lines;       //Contains number of instruction elements per line.
    int lineCount;          //Current line in program.
    int lineCapacity;       //Total capacity of the Lines array.
} Chunk;

void ChunkInit(Chunk* chunk);                               //Initializes a chunk.
void ChunkWrite(Chunk* chunk, uint8_t byte, int line, char* source);      //Writes an instruction byte to a chunk array.
void ChunkWriteLong(Chunk* chunk, long number, int line, char* source);
int ChunkAddConstant(Chunk* chunk, Value value);            //Writes a constant to the constant array inside a chunk.
int ChunkWriteConstant(Chunk* chunk, Value value);
int ChunkGetLine(Chunk* chunk, int instruction);
char* ChunkGetSource(Chunk* chunk, int instruction);
void ChunkFree(Chunk* chunk);

#endif
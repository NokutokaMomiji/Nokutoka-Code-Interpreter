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
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_NOT,
    OP_NEGATE,          //Negates a value.
    OP_RETURN,          //Return from current function.
} OpCode;

typedef struct {
    int Count;              //Number of elements in array.
    int Capacity;           //Number of available slots.
    uint8_t* Code;          //Instruction elements.
    ValueArray Constants;   //Array of constant values.
    int* Lines;             //Contains number of instruction elements per line.
    int currentLine;        //Current line in program.
    int lineCapacity;       //Total capacity of the Lines array.
} Chunk;

void ChunkInit(Chunk* chunk);                       //Initializes a chunk.
void ChunkWrite(Chunk* chunk, uint8_t byte);        //Writes an instruction byte to a chunk array.
void ChunkWriteLong(Chunk* chunk, long number);
int ChunkAddConstant(Chunk* chunk, Value value);    //Writes a constant to the constant array inside a chunk.
int ChunkWriteConstant(Chunk* chunk, Value value);
void ChunkIncreaseLine(Chunk* chunk);               //Increases the current line number.

void ChunkFree(Chunk* chunk);

#endif
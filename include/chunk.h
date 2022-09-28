#ifndef NKCODE_CHUNK_H
#define NKCODE_CHUNK_H

#include "common.h" //Common stuff.

typedef enum {
    OP_RETURN,  //Return from current function.
} OpCode;

typedef struct {
    int Count;      //Number of elements in array.
    int Capacity;   //Number of available slots.
    uint8_t* Code;
} Chunk;

void ChunkInit(Chunk* chunk);                   //Initializes a chunk.
void ChunkWrite(Chunk* chunk, uint8_t byte);    //Writes a byte to a chunk array.
void ChunkFree(Chunk* chunk);

#endif
#include "chunk.h"
#include "memory.h"

void ChunkInit(Chunk* chunk) {
    chunk->Count = 0;
    chunk->Capacity = 0;
    chunk->Code = NULL;
}

void ChunkWrite(Chunk* chunk, uint8_t byte) {
    //If there is not enough capacity for the new byte, then increase the size of the array.
    if (chunk->Capacity < chunk->Count + 1) {
        int oldCapacity = chunk->Capacity;
        chunk->Capacity = GROW_CAPACITY(oldCapacity);
        chunk->Code = GROW_ARRAY(uint8_t, chunk->Code, oldCapacity, chunk->Capacity);
    }

    //Store byte on array and increase the number of elements.
    chunk->Code[chunk->Count] = byte;
    chunk->Count++;
}

void ChunkFree(Chunk* chunk) {
    FREE_ARRAY(uint8_t, chunk->Code, chunk->Capacity);
    ChunkInit(chunk);
}
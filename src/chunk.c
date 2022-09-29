#include "chunk.h"
#include "memory.h"

/// @brief Initializes a Chunk.
/// @param chunk Chunk to initialize.
void ChunkInit(Chunk* chunk) {
    //Intialize empty chunk array.
    chunk->Count = 0;
    chunk->Capacity = 0;
    chunk->Code = NULL;

    //Initialize line data.
    chunk->currentLine = 0;
    chunk->lineCapacity = 0;
    chunk->Lines = NULL;

    //Initialize internal constant array.
    ValueArrayInit(&chunk->Constants);
}

/// @brief Writes a byte to a Chunk array.
/// @param chunk Chunk to write the byte to.
/// @param byte Byte to store on Chunk.
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

    //Increase number of elements in current line.
    chunk->Lines[chunk->currentLine]++;
}

void ChunkWriteLong(Chunk* chunk, long number) {
    uint8_t firstByte = (number & 0xff000000UL) >> 24;
    uint8_t secondByte = (number & 0x00ff0000UL) >> 16;
    uint8_t thirdByte = (number & 0x0000ff00UL) >> 8;
    uint8_t fourthByte = (number & 0x000000ffUL);

    ChunkWrite(chunk, firstByte);
    ChunkWrite(chunk, secondByte);
    ChunkWrite(chunk, thirdByte);
    ChunkWrite(chunk, fourthByte);
}

int ChunkAddConstant(Chunk* chunk, Value value) {
    ValueArrayWrite(&chunk->Constants, value);
    return chunk->Constants.Count - 1;
}


void ChunkIncreaseLine(Chunk* chunk) {
    /*
        In order to try and not be wasteful and have an array full of the same line numbers, we use a line
        array that contains the number of instructions in each line, that way we can calculate the line number
        based on the instruction offset.
        Ex:
            chunk->Lines[0] = 128; //128 instructions.
    */
    chunk->currentLine++;

    if (chunk->lineCapacity < chunk->currentLine) {
        int oldLineCapacity = chunk->lineCapacity;
        chunk->lineCapacity = GROW_CAPACITY(oldLineCapacity);
        chunk->Lines = GROW_ARRAY(int, chunk->Lines, oldLineCapacity, chunk->lineCapacity);
    }

    chunk->Lines[chunk->currentLine] = 0;
}

/// @brief Frees up the memory occupied by an array.
/// @param chunk Chunk array to free the memory of.
void ChunkFree(Chunk* chunk) {
    //Free memory from array.
    FREE_ARRAY(uint8_t, chunk->Code, chunk->Capacity);
    
    //Free chunk constants.
    ValueArrayFree(&chunk->Constants);

    //Free memory from line array.
    FREE_ARRAY(int, chunk->Lines, chunk->lineCapacity);

    //Reinitialize chunk.
    ChunkInit(chunk);
}
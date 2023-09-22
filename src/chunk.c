#include "chunk.h"
#include "memory.h"

/// @brief Initializes a Chunk.
/// @param chunk Chunk to initialize.
void ChunkInit(Chunk* chunk) {
    //Intialize empty chunk array.
    chunk->Count = 0;
    chunk->Capacity = 0;
    chunk->Code = NULL;

    // Initialize line stuff.
    chunk->lineCount = 0;
    chunk->lineCapacity = 0;
    chunk->Lines = NULL;

    //Initialize internal constant array.
    ValueArrayInit(&chunk->Constants);
}

/// @brief Writes a byte to a Chunk array.
/// @param chunk Chunk to write the byte to.
/// @param byte Byte to store on Chunk.
void ChunkWrite(Chunk* chunk, uint8_t byte, int line) {
    //If there is not enough capacity for the new byte, then increase the size of the array.
    if (chunk->Capacity < chunk->Count + 1) {
        int oldCapacity = chunk->Capacity;
        chunk->Capacity = GROW_CAPACITY(oldCapacity);
        chunk->Code = GROW_ARRAY(uint8_t, chunk->Code, oldCapacity, chunk->Capacity);
    }
    
    //Store byte on array and increase the number of elements.
    chunk->Code[chunk->Count] = byte;
    chunk->Count++;

    if (chunk->lineCount > 0 && chunk->Lines[chunk->lineCount - 1].Line == line)
        return;
    
    if (chunk->lineCapacity < chunk->lineCount + 1) {
        int oldCapacity = chunk->lineCapacity;
        chunk->lineCapacity = GROW_CAPACITY(oldCapacity);
        chunk->Lines = GROW_ARRAY(LineStart, chunk->Lines, oldCapacity, chunk->lineCapacity);
    }

    LineStart* lineStart = &chunk->Lines[chunk->lineCount++];
    lineStart->Offset = chunk->Count - 1;
    lineStart->Line = line;
}

void ChunkWriteLong(Chunk* chunk, long number, int line) {
    uint8_t firstByte = (number & 0xff000000UL) >> 24;
    uint8_t secondByte = (number & 0x00ff0000UL) >> 16;
    uint8_t thirdByte = (number & 0x0000ff00UL) >> 8;
    uint8_t fourthByte = (number & 0x000000ffUL);

    ChunkWrite(chunk, firstByte, line);
    ChunkWrite(chunk, secondByte, line);
    ChunkWrite(chunk, thirdByte, line);
    ChunkWrite(chunk, fourthByte, line);
}

int ChunkAddConstant(Chunk* chunk, Value value) {
    ValueArrayWrite(&chunk->Constants, value);
    return chunk->Constants.Count - 1;
}

int ChunkGetLine(Chunk* chunk, int instruction) {
    int Start = 0;
    int End = chunk->lineCount - 1;

    for (;;) {
        int Mid = (Start + End) / 2;
        LineStart* Line = &chunk->Lines[Mid];
        if (instruction < Line->Offset)
            End = Mid - 1;
        else if (Mid == chunk->lineCount - 1 || instruction < chunk->Lines[Mid + 1].Offset)
            return Line->Line;
        else
            Start = Mid + 1;
    }
}

/// @brief Frees up the memory occupied by an array.
/// @param chunk Chunk array to free the memory of.
void ChunkFree(Chunk* chunk) {
    //Free memory from array.
    FREE_ARRAY(uint8_t, chunk->Code, chunk->Capacity);
    
    //Free chunk constants.
    ValueArrayFree(&chunk->Constants);

    //Free memory from line array.
    FREE_ARRAY(LineStart, chunk->Lines, chunk->lineCapacity);

    //Reinitialize chunk.
    ChunkInit(chunk);
}
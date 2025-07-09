#include <string.h>
#include "Chunk.h"
#include "Memory.h"
#include "VM.h"

/// @brief Initializes a Chunk.
/// @param chunk Chunk to initialize.
void MJ_ChunkInit(MJ_Chunk* chunk) {
    //Intialize empty chunk array.
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;

    // Initialize line stuff.
    chunk->lineCount = 0;
    chunk->lineCapacity = 0;
    chunk->lines = NULL;

    //Initialize internal constant array.
    ValueArrayInit(&chunk->constants);
}

/// @brief Writes a byte to a Chunk array.
/// @param chunk Chunk to write the byte to.
/// @param byte Byte to store on Chunk.
/// @param line The current line number (for exception purposes).
/// @param source The current source code line (for exception purposes).
void MJ_ChunkWrite(MJ_Chunk* chunk, uint8_t byte, int line, char* source) {
    //If there is not enough capacity for the new byte, then increase the size of the array.
    if (chunk->capacity < chunk->count + 1) {
        int oldCapacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(oldCapacity);
        chunk->code = GROW_ARRAY(uint8_t, chunk->code, oldCapacity, chunk->capacity);
    }
    
    //Store byte on array and increase the number of elements.
    chunk->code[chunk->count] = byte;
    chunk->count++;

    // If we are still in the current line, we don't add more to the array.
    if (chunk->lineCount > 0 && chunk->lines[chunk->lineCount - 1].line == line)
        return;
    
    // If there is not enough capacity for the new line data, we grow the array.
    if (chunk->lineCapacity < chunk->lineCount + 1) {
        int oldCapacity = chunk->lineCapacity;
        chunk->lineCapacity = GROW_CAPACITY(oldCapacity);
        chunk->lines = GROW_ARRAY(MJ_LineStart, chunk->lines, oldCapacity, chunk->lineCapacity);
    }

    // We set the data for the current line.
    MJ_LineStart* lineStart = &chunk->lines[chunk->lineCount++];
    lineStart->offset = chunk->count - 1;
    lineStart->line = line;
    
    // We copy the string from the source code to the line data array.
    if (source == NULL) {
        lineStart->content = '\0';
        return;
    }

    int contentLength = strlen(source);
    lineStart->content = ALLOCATE(char, contentLength + 1);
    memcpy(lineStart->content, source, contentLength);
    lineStart->content[contentLength] = '\0';
}

/// @brief Writes a long to the chunk.
/// @param chunk Chunk to write the number to.
/// @param number The number to write.
/// @param line The current line number (for exception purposes).
/// @param source The current source code line (for exception purposes).
void MJ_ChunkWriteLong(MJ_Chunk* chunk, long number, int line, char* source) {
    // Some bit shifting to get each individual byte.
    uint8_t firstByte = (number & 0xff000000UL) >> 24;
    uint8_t secondByte = (number & 0x00ff0000UL) >> 16;
    uint8_t thirdByte = (number & 0x0000ff00UL) >> 8;
    uint8_t fourthByte = (number & 0x000000ffUL);

    // We write the bytes in sequence.
    MJ_ChunkWrite(chunk, firstByte, line, source);
    MJ_ChunkWrite(chunk, secondByte, line, source);
    MJ_ChunkWrite(chunk, thirdByte, line, source);
    MJ_ChunkWrite(chunk, fourthByte, line, source);
}

/// @brief Writes a value to the chunk's value array.
/// @param chunk The chunk to write to.
/// @param value The value to write to the chunk.
/// @return Next index.
int MJ_ChunkAddConstant(MJ_Chunk* chunk, Value value) {
    Push(value);
    ValueArrayWrite(&chunk->constants, value);
    Pop();
    return chunk->constants.count - 1;
}

/// @brief For getting the current line number based on the instruction number (from the VM).
/// @param chunk Chunk to get the line number from.
/// @param instruction The current instruction offset (from the VM).
/// @return Line number.
int MJ_ChunkGetLine(MJ_Chunk* chunk, int instruction) {
    int start = 0;
    int End = chunk->lineCount - 1;

    for (;;) {
        int Mid = (start + End) / 2;    
        MJ_LineStart* line = &chunk->lines[Mid];
        if (instruction < line->offset)
            End = Mid - 1;
        else if (Mid == chunk->lineCount - 1 || instruction < chunk->lines[Mid + 1].offset)
            return line->line;
        else
            start = Mid + 1;
    }
}

/// @brief For getting the current source code line based on the instruction number (from the VM).
/// @param chunk The chunk to get the source code info from.
/// @param instruction The instruction offset (from the VM).
/// @return Source code line.
char* MJ_ChunkGetSource(MJ_Chunk* chunk, int instruction) {
    int start = 0;
    int End = chunk->lineCount - 1;

    for (;;) {
        int Mid = (start + End) / 2;    
        MJ_LineStart* line = &chunk->lines[Mid];
        if (instruction < line->offset)
            End = Mid - 1;
        else if (Mid == chunk->lineCount - 1 || instruction < chunk->lines[Mid + 1].offset)
            return line->content;
        else
            start = Mid + 1;
    }
}

/// @brief Frees up the memory occupied by an array.
/// @param chunk Chunk array to free the memory of.
void MJ_ChunkFree(MJ_Chunk* chunk) {
    //Free memory from array.
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
    
    //Free chunk constants.
    ValueArrayFree(&chunk->constants);

    //Free memory from line array.
    FREE_ARRAY(MJ_LineStart, chunk->lines, chunk->lineCapacity);

    //Reinitialize chunk.
    MJ_ChunkInit(chunk);
}
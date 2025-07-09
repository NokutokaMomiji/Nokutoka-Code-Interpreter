#ifndef MOMIJI_CHUNK_H
#define MOMIJI_CHUNK_H

#include "Common.h" //Common stuff.
#include "Value.h"  //Value and ValueArray.

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
    OP_SET_INDEX,
    OP_GET_INDEX,
    OP_GET_INDEX_RANGED,
    OP_GET_UPVALUE,
    OP_SET_UPVALUE,
    OP_CLOSE_UPVALUE,
    OP_SET_PROPERTY,
    OP_GET_PROPERTY,
    OP_INIT_PROPERTY,

    OP_ARRAY,
    OP_MAP,
    OP_CLASS,
    OP_METHOD,
    
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
    OP_MOD,
    OP_BITWISE_OR,
    OP_BITWISE_AND,
    OP_NOT,
    OP_NEGATE,          //Negates a value.
    OP_PRINT,
    OP_JUMP_IF_FALSE,   
    OP_JUMP,
    OP_LOOP,
    OP_CALL,
    OP_CLOSURE,
    OP_RETURN,          //Return from current function.
} MJ_OpCode;

typedef struct {
    int offset;
    int line;
    char* content;
} MJ_LineStart;

typedef struct {
    int count;              // Number of elements in array.
    int capacity;           // Number of available slots.
    uint8_t* code;          // Instruction elements.
    ValueArray constants;   // Array of constant values.
    MJ_LineStart* lines;    // Contains number of instruction elements per line.
    int lineCount;          // Current line in program.
    int lineCapacity;       // Total capacity of the Lines array.
} MJ_Chunk;

void MJ_ChunkInit(MJ_Chunk* chunk);                                             // Initializes a chunk.
void MJ_ChunkWrite(MJ_Chunk* chunk, uint8_t byte, int line, char* source);      // Writes an instruction byte to a chunk array.
void MJ_ChunkWriteLong(MJ_Chunk* chunk, long number, int line, char* source);
int MJ_ChunkAddConstant(MJ_Chunk* chunk, Value value);                          // Writes a constant to the constant array inside a chunk.
int MJ_ChunkWriteConstant(MJ_Chunk* chunk, Value value);
int MJ_ChunkGetLine(MJ_Chunk* chunk, int instruction);
char* MJ_ChunkGetSource(MJ_Chunk* chunk, int instruction);
void MJ_ChunkFree(MJ_Chunk* chunk);

#endif

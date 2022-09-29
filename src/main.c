#include <stdio.h>
#include "common.h"
#include "chunk.h"
#include "debug.h"

int main(int argc, const char* argv[]) {
    printf("Running.\n");

    Chunk chunk;
    
    ChunkInit(&chunk);
    ChunkIncreaseLine(&chunk);

    int myConstant = ChunkAddConstant(&chunk, 1.2); //Add a constant to the value array inside the chunk.
    ChunkWrite(&chunk, OP_CONSTANT); //Add a constant instruction.
    ChunkWrite(&chunk, myConstant); //Write the constant index.
    
    ChunkIncreaseLine(&chunk); //"Move" to next line.

    ChunkWrite(&chunk, OP_RETURN); 
    
    DisassembleChunk(&chunk, "test program");
    ChunkFree(&chunk);

    return 0;
}

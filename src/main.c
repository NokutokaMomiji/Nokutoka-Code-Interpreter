#include <stdio.h>
#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "vm.h"

int main(int argc, const char* argv[]) {
    printf("Initializing VM.\n");

    VMInit();

    Chunk chunk;
    
    ChunkInit(&chunk);
    ChunkIncreaseLine(&chunk);

    int myConstant = ChunkAddConstant(&chunk, 1.2); //Add a constant to the value array inside the chunk.
    ChunkWrite(&chunk, OP_CONSTANT); //Add a constant instruction.
    ChunkWrite(&chunk, myConstant); //Write the constant index.

    myConstant = ChunkAddConstant(&chunk, 3.4);
    ChunkWrite(&chunk, OP_CONSTANT);
    ChunkWrite(&chunk, myConstant);

    ChunkWrite(&chunk, OP_ADD);

    myConstant = ChunkAddConstant(&chunk, 5.6);
    ChunkWrite(&chunk, OP_CONSTANT);
    ChunkWrite(&chunk, myConstant);

    ChunkWrite(&chunk, OP_DIVIDE);

    ChunkWrite(&chunk, OP_NEGATE);

    ChunkIncreaseLine(&chunk);

    //ChunkIncreaseLine(&chunk); //"Move" to next line.

    ChunkWrite(&chunk, OP_RETURN); 
    
    DisassembleChunk(&chunk, "test program");
    Interpret(&chunk);
    VMFree();
    ChunkFree(&chunk);

    return 0;
}

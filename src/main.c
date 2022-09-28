#include <stdio.h>
#include "common.h"
#include "chunk.h"

int main(int argc, const char* argv[]) {
    printf("Running.\n");

    Chunk chunk;
    ChunkInit(&chunk);
    ChunkWrite(&chunk, OP_RETURN);
    ChunkFree(&chunk);

    return 0;
}

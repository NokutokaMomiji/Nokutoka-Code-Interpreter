#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "vm.h"

static void HandmadeTest() {
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
    InterpretChunk(&chunk);
    ChunkFree(&chunk);
}

static void Repl() {
    char Line[1024];
    for (;;) {
        printf("> ");

        if (!fgets(Line, sizeof(Line), stdin)) {
            printf("\n");
            break;
        }

        Interpret(Line);
    }
}

static char* ReadFile(const char* path) {
    FILE* sourceFile = fopen(path, "rb");

    if (sourceFile == NULL) {
        fprintf(stderr, "[ERROR]: Could not open file \"%s\".\n", path);
        exit(74);
    }

    fseek(sourceFile, 0L, SEEK_END);
    size_t fileSize = ftell(sourceFile);
    rewind(sourceFile);

    char* dataBuffer = (char*)malloc(fileSize + 1);

    if (dataBuffer == NULL) {
        fprintf(stderr, "[ERROR]: Not enough memory to read \"%s\".", path);
        exit(74);
    }

    size_t bytesRead = fread(dataBuffer, sizeof(char), fileSize, sourceFile);
    if (bytesRead == NULL) {
        fprintf(stderr, "[ERROR]: Failed to read file \"%s\".", path);
        exit(74);
    }
    dataBuffer[bytesRead] = '\0';

    fclose(sourceFile);
    return dataBuffer;
}

static void RunFile(const char* path) {
    char* Source = ReadFile(path);
    InterpretResult Result = Interpret(Source);
    free(Source);

    if (Result == INTERPRET_COMPILE_ERROR) exit(65);
    if (Result == INTERPRET_RUNTIME_ERROR) exit(70);
}



int main(int argc, const char* argv[]) {
    printf("Initializing VM.\n");

    VMInit();

    if (argc == 1)
        Repl();
    else if (argc == 2)
        RunFile(argv[1]);
    else {
        fprintf(stderr, "Usage: nkcode [path]\n");
        exit(64);
    }

    HandmadeTest();

    VMFree();

    return 0;
}

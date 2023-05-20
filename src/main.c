#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "vm.h"

static void Repl() {
    char Line[1024];
    for (;;) {
        printf(">>> ");

        if (!fgets(Line, sizeof(Line), stdin)) {
            printf("\n");
            break;
        }

        InterpretResult result = Interpret(Line);

        printf("%d\n", result);
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

    //HandmadeTest();

    VMFree();

    return 0;
}

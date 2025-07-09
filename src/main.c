#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <locale.h>
#include "Common.h"
#include "Chunk.h"
#include "Debug.h"
#include "VM.h"

static bool hasUnclosed(const char* src, size_t len) {
    int parenthesis = 0, braces = 0, squares = 0;

    for (size_t i = 0; i < len; i++) {
        switch (src[i]) {
            case '(': parenthesis++; break;
            case ')': parenthesis--; break;
            case '{': braces++; break;
            case '}': braces--; break;
            case '[': squares++; break;
            case ']': squares--; break;
            default: break;
        }
    }
    return parenthesis > 0 || braces > 0 || squares > 0;
}

static void Repl() {
    char *source = NULL;
    size_t capacity = 0;
    size_t length = 0;
    bool firstLine = true;

    while (1) {
        printf(firstLine ? COLOR_MAGENTA ">>> " COLOR_RESET : COLOR_GRAY "... " COLOR_RESET);

        char line[1024];
        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }

        size_t lineLen = strlen(line);
        bool isBlank = (lineLen == 1 && line[0] == '\n');

        if (isBlank && length > 0 && !hasUnclosed(source, length)) {
            source[length] = '\0';
            InterpretResult result = Interpret(source);
            length = 0;
            firstLine = true;
            continue;
        }

        if (length + lineLen + 1 > capacity) {
            size_t oldCap = capacity;
            capacity = (oldCap == 0 ? 1024 : oldCap * 2);
            source = (char*)realloc(source, capacity + 1);
            if (!source) {
                fprintf(stderr, "[ERROR]: Out of memory in REPL.\n");
                exit(74);
            }
        }
        memcpy(source + length, line, lineLen);
        length += lineLen;

        if (!hasUnclosed(source, length)) {
            source[length] = '\0';
            InterpretResult result = Interpret(source);
            length = 0;
            firstLine = true;
            continue;
        }

        firstLine = false;
    }

    free(source);
}

static char* ReadFile(const char* path) {
    FILE* sourceFile = fopen(path, "rb");
    if (!sourceFile) {
        fprintf(stderr, "[ERROR]: Could not open file \"%s\".\n", path);
        exit(74);
    }
    fseek(sourceFile, 0L, SEEK_END);
    size_t fileSize = ftell(sourceFile);
    rewind(sourceFile);
    char* dataBuffer = (char*)malloc(fileSize + 1);
    if (!dataBuffer) {
        fprintf(stderr, "[ERROR]: Not enough memory to read \"%s\".", path);
        exit(74);
    }
    size_t bytesRead = fread(dataBuffer, sizeof(char), fileSize, sourceFile);
    if (bytesRead == 0 && fileSize > 0) {
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
    //setlocale(LC_ALL, "");

    VMInit();

    if (argc == 1) {
        Repl();
    } else if (argc == 2) {
        RunFile(argv[1]);
    } else {
        fprintf(stderr, COLOR_MAGENTA "Usage" COLOR_RESET ": momiji [path]\n");
        exit(64);
    }

    VMFree();
    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include "common.h"
#include "compiler.h"
#include "scanner.h"

typedef struct {
    Token Current;
    Token Previous;
    bool hadError;
    bool panicMode;
} Parser;

Parser parser;
Chunk* compilingChunk;

static Chunk* CurrentChunk() {
    return compilingChunk;
}

static void ErrorAt(Token* token, const char* msg) {
    if (parser.panicMode)
        return;

    parser.panicMode = true;

    fprintf(stderr, "[ERROR]: %s ", msg);

    if (token->Type == TOKEN_EOF)
        fprintf(stderr, " at end.");
    else if (token->Type != TOKEN_ERROR)
        fprintf(stderr, " at line: %d | pos: %.*s.", token->Line, token->Length, token->Start);

    fprintf(stderr, "\n");
    parser.hadError = true;
}

static void Error(const char* msg) {
    ErrorAt(&parser.Previous, msg);
}

static void ErrorAtCurrent(const char* msg) {
    ErrorAt(&parser.Current, msg);
}

static void CompilerAdvance() {
    parser.Previous = parser.Current;

    for (;;) {
        parser.Current = ScannerScanToken();

        if (parser.Current.Line != parser.Previous.Line)
            ChunkIncreaseLine(CurrentChunk());

        if (parser.Current.Type != TOKEN_ERROR)
            break;

        ErrorAtCurrent(parser.Current.Start);
    }
}

static void CompilerConsume(TokenType type, const char* msg) {
    if (parser.Current.Type == type) {
        CompilerAdvance();
        return;
    }

    ErrorAtCurrent(msg);
}

static void CompilerEmitByte(uint8_t byte) {
    ChunkWrite(CurrentChunk(), byte);
}

static void CompilerEmitBytes(uint8_t firstByte, uint8_t secondByte) {
    CompilerEmitByte(firstByte);
    CompilerEmitByte(secondByte);
}

static void CompilerEmitLong(long longNumber) {
    ChunkWriteLong(CurrentChunk(), longNumber);
}

static void CompilerEmitByteLong(uint8_t byte, long longNumber) {
    CompilerEmitByte(byte);
    CompilerEmitLong(longNumber);
}

bool Compile(const char* source, Chunk* chunk) {
    ScannerInit(source);

    compilingChunk = chunk;

    parser.hadError = false;
    parser.panicMode = false;

    ScannerAdvance();
    ScannerExpression();
    ScannerConsume(TOKEN_EOF, "Expected end of expression");
    return !parser.hadError;
}

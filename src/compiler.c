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

typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,  // =
    PREC_OR,          // or
    PREC_AND,         // and
    PREC_EQUALITY,    // == !=
    PREC_COMPARISON,  // < > <= >=
    PREC_TERM,        // + -
    PREC_FACTOR,      // * /
    PREC_UNARY,       // ! -
    PREC_CALL,        // . ()
    PREC_PRIMARY
} Precedence;

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

static void CompilerEmitReturn() {
    CompilerEmitByte(OP_RETURN);
}

static long CompilerMakeConstant(Value value) {
    int Constant = ChunkAddConstant(CurrentChunk(), value);
    if (Constant > LONG_MAX) {
        Error("Too many constants in one chunk.");
        return 0;
    }
    return (long)Constant;
}

static void CompilerEmitConstant(Value value) {
    CompilerEmitByteLong(OP_CONSTANT_LONG, CompilerMakeConstant(value));
}

static void CompilerEnd() {
    CompilerEmitReturn();
}   

static void CompilerGrouping() {
    CompilerExpression();
    CompilerConsume(TOKEN_PARENTHESIS_CLOSE, "Expected ')' after expression.");
}

static void CompilerExpression() {

}

static void CompilerNumber() {
    double Value = strtod(parser.Previous.Start, NULL);
    CompilerEmitConstant(Value);
}

static void CompilerUnary() {
    TokenType operatorType = parser.Previous.Type;

    //Compile operand.
    CompilerExpression();

    switch(operatorType) {
        case TOKEN_MINUS: CompilerEmitByte(OP_NEGATE); break;
        default: return;
    }
}

static void CompilerParsePrecedence()

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

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

typedef void (*ParseFn)(); // Essentially a void.

typedef struct {
    ParseFn Prefix;
    ParseFn Infix;
    Precedence precedence;
} ParseRule;

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

static void CompilerExpression();
static ParseRule* CompilerGetRule(TokenType type);
static void CompilerParsePrecedence(Precedence precedence);

static void CompilerBinary() {
    TokenType operatorType = parser.Previous.Type;
    ParseRule* Rule = CompilerGetRule(operatorType);
    CompilerParsePrecedence((Precedence)(Rule->precedence + 1));
    
    switch(operatorType) {
        case TOKEN_PLUS:    CompilerEmitByte(OP_ADD);       break;
        case TOKEN_MINUS:   CompilerEmitByte(OP_SUBTRACT);  break;
        case TOKEN_STAR:    CompilerEmitByte(OP_MULTIPLY);  break;
        case TOKEN_SLASH:   CompilerEmitByte(OP_DIVIDE);    break;
        default: return;
    }
}

static void CompilerExpression() {
    CompilerParsePrecedence(PREC_ASSIGNMENT);
}

static void CompilerGrouping() {
    CompilerExpression();
    CompilerConsume(TOKEN_PARENTHESIS_CLOSE, "Expected ')' after expression.");
}

static void CompilerNumber() {
    double Value = strtod(parser.Previous.Start, NULL);
    CompilerEmitConstant(Value);
}

static void CompilerUnary() {
    TokenType operatorType = parser.Previous.Type;

    //Compile operand.
    CompilerParsePrecedence(PREC_UNARY);

    switch(operatorType) {
        case TOKEN_MINUS: CompilerEmitByte(OP_NEGATE); break;
        default: return;
    }
}

ParseRule rules[] = {
  //    [TOKEN]                     [Functions]
  [TOKEN_PARENTHESIS_OPEN]    = {CompilerGrouping, NULL,           PREC_NONE},
  [TOKEN_PARENTHESIS_CLOSE]   = {NULL,             NULL,           PREC_NONE},
  [TOKEN_BRACKET_OPEN]        = {NULL,             NULL,           PREC_NONE}, 
  [TOKEN_BRACKET_CLOSE]       = {NULL,             NULL,           PREC_NONE},
  [TOKEN_COMMA]               = {NULL,             NULL,           PREC_NONE},
  [TOKEN_DOT]                 = {NULL,             NULL,           PREC_NONE},
  [TOKEN_MINUS]               = {CompilerUnary,    CompilerBinary, PREC_TERM},
  [TOKEN_PLUS]                = {NULL,             CompilerBinary, PREC_TERM},
  [TOKEN_SEMICOLON]           = {NULL,             NULL,           PREC_NONE},
  [TOKEN_SLASH]               = {NULL,             CompilerBinary, PREC_FACTOR},
  [TOKEN_STAR]                = {NULL,             CompilerBinary, PREC_FACTOR},
  [TOKEN_NOT]                 = {NULL,             NULL,           PREC_NONE},
  [TOKEN_NOT_EQUAL]           = {NULL,             NULL,           PREC_NONE},
  [TOKEN_ASSIGN]              = {NULL,             NULL,           PREC_NONE},
  [TOKEN_EQUAL]               = {NULL,             NULL,           PREC_NONE},
  [TOKEN_GREATER_THAN]        = {NULL,             NULL,           PREC_NONE},
  [TOKEN_GREATER_THAN_EQUAL]  = {NULL,             NULL,           PREC_NONE},
  [TOKEN_SMALLER_THAN]        = {NULL,             NULL,           PREC_NONE},
  [TOKEN_SMALLER_THAN_EQUAL]  = {NULL,             NULL,           PREC_NONE},
  [TOKEN_IDENTIFIER]          = {NULL,             NULL,           PREC_NONE},
  [TOKEN_STRING]              = {NULL,             NULL,           PREC_NONE},
  [TOKEN_NUMBER]              = {CompilerNumber,   NULL,           PREC_NONE},
  [TOKEN_AND]                 = {NULL,             NULL,           PREC_NONE},
  [TOKEN_CLASS]               = {NULL,             NULL,           PREC_NONE},
  [TOKEN_ELSE]                = {NULL,             NULL,           PREC_NONE},
  [TOKEN_FALSE]               = {NULL,             NULL,           PREC_NONE},
  [TOKEN_FOR]                 = {NULL,             NULL,           PREC_NONE},
  [TOKEN_FUNCTION]            = {NULL,             NULL,           PREC_NONE},
  [TOKEN_IF]                  = {NULL,             NULL,           PREC_NONE},
  [TOKEN_NULL]                = {NULL,             NULL,           PREC_NONE},
  [TOKEN_OR]                  = {NULL,             NULL,           PREC_NONE},
  [TOKEN_PRINT]               = {NULL,             NULL,           PREC_NONE},
  [TOKEN_RETURN]              = {NULL,             NULL,           PREC_NONE},
  [TOKEN_SUPER]               = {NULL,             NULL,           PREC_NONE},
  [TOKEN_THIS]                = {NULL,             NULL,           PREC_NONE},
  [TOKEN_TRUE]                = {NULL,             NULL,           PREC_NONE},
  [TOKEN_LOCAL]               = {NULL,             NULL,           PREC_NONE},
  [TOKEN_WHILE]               = {NULL,             NULL,           PREC_NONE},
  [TOKEN_ERROR]               = {NULL,             NULL,           PREC_NONE},
  [TOKEN_EOF]                 = {NULL,             NULL,           PREC_NONE},
};

static void CompilerParsePrecedence(Precedence precedence) {

}

static ParseRule* CompilerGetRule(TokenType type) {
    return &rules[type];
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

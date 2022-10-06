#include <stdio.h>
#include "common.h"
#include "compiler.h"
#include "scanner.h"

typedef struct {
    Token Current;
    Token Previous;
    bool hadError;
} Parser;

Parser parser;

static void ErrorAt(Token* token, const char* msg) {
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
        if (parser.Current.Type != TOKEN_ERROR)
            break;

        ErrorAtCurrent(parser.Current.Start);
    }
}

bool Compile(const char* source, Chunk* chunk) {
    ScannerInit(source);
    ScannerAdvance();
    ScannerExpression();
    ScannerConsume(TOKEN_EOF, "Expected end of expression");
}

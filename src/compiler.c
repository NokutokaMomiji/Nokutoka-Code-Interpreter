#include <stdio.h>
#include <stdlib.h>
#include "common.h"
#include "compiler.h"
#include "scanner.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

#define MAX_CASES 256

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

typedef void (*ParseFn)(bool canAssign); // Essentially a void.

typedef struct {
    ParseFn Prefix;
    ParseFn Infix;
    Precedence precedence;
} ParseRule;

typedef struct {
    Token Name;
    int Depth;
} Local;

typedef enum {
    TYPE_FUNCTION,
    TYPE_SCRIPT
} FunctionType;

typedef struct Compiler {
    struct Compiler* Enclosing;
    ObjFunction* Function;
    FunctionType Type;

    Local Locals[UINT16_COUNT];
    int localCount;
    int scopeDepth;
} Compiler;

Parser parser;
Compiler* Current = NULL;

static Chunk* CurrentChunk() {
    return &Current->Function->chunk;
}

static void ErrorAt(Token* token, const char* msg) {
    if (parser.panicMode)
        return;

    parser.panicMode = true;

    fprintf(stderr, "[ERROR]: %s", msg);

    if (token->Type == TOKEN_EOF)
        fprintf(stderr, " at end.");
    else if (token->Type != TOKEN_ERROR)
        fprintf(stderr, " at line: %d | '%.*s'.", token->Line, token->Length, token->Start);

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
        parser.Current = ScannerScanToken(&Current->Function->chunk);

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

static bool Check(TokenType type) {
    return parser.Current.Type == type;
}

static bool Match(TokenType type) {
    if (!Check(type))
        return false;
    
    CompilerAdvance();
    return true;
}

static void CompilerEmitByte(uint8_t byte) {
    ChunkWrite(CurrentChunk(), byte, parser.Previous.Line);
}

static void CompilerEmitBytes(uint8_t firstByte, uint8_t secondByte) {
    CompilerEmitByte(firstByte);
    CompilerEmitByte(secondByte);
}

static void CompilerEmitLong(long longNumber) {
    ChunkWriteLong(CurrentChunk(), longNumber, parser.Previous.Line);
}

static void CompilerEmitByteLong(uint8_t byte, long longNumber) {
    CompilerEmitByte(byte);
    CompilerEmitLong(longNumber);
}

static int CompilerEmitJump(uint8_t instruction) {
    CompilerEmitByte(instruction);
    CompilerEmitByte(0xff);
    CompilerEmitByte(0xff);
    return CurrentChunk()->Count - 2;
}

static int CompilerEmitLoop(int loopStart) {
    CompilerEmitByte(OP_LOOP);

    int Offset = CurrentChunk()->Count - loopStart + 2;
    if (Offset > UINT16_MAX)
        Error("Loop body too large.");
    
    CompilerEmitByte((Offset >> 8) & 0xff);
    CompilerEmitByte(Offset & 0xff);
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
    //CompilerEmitByte(CompilerMakeConstant(value))
}

static void CompilerPatchJump(int offset) {
    int Jump = CurrentChunk()->Count - offset - 2;

    if (Jump > UINT16_MAX) {
        Error("Too much code to jump over.");
    }

    CurrentChunk()->Code[offset] = (Jump >> 8) & 0xff;
    CurrentChunk()->Code[offset + 1] = Jump & 0xff;
}

static void CompilerInit(Compiler* compiler, FunctionType type) {
    compiler->Enclosing = Current;
    compiler->Function = NULL;
    compiler->Type = type;
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
    compiler->Function = FunctionNew();
    Current = compiler;

    if (type != TYPE_SCRIPT) {
        Current->Function->Name = StringCopy(parser.Previous.Start, parser.Previous.Length);
    }
    
    Local* local = &Current->Locals[Current->localCount++];
    local->Depth = 0;
    local->Name.Start = "";
    local->Name.Length = 0;
}

static ObjFunction* CompilerEnd() {
    CompilerEmitReturn();
    ObjFunction* Function = Current->Function;
#ifdef DEBUG_PRINT_CODE
    if (!parser.hadError || true)
        DisassembleChunk(CurrentChunk(), Function->Name != NULL ? Function->Name->Chars : "<script>");
#endif

    Current = Current->Enclosing;
    return Function;
}   

static void CompilerBeginScope() {
    Current->scopeDepth++;
}

static void CompilerEndScope() {
    Current->scopeDepth--;

    if (Current->localCount > 0 && 
        Current->Locals[Current->localCount - 1].Depth > 
        Current->scopeDepth) {
        
        CompilerEmitByte(OP_POP);
        Current->localCount--;
    }
}

static void CompilerExpression();
static void CompilerStatement();
static void CompilerDeclaration();
static void VariableDeclaration();
static void LocalVariableDeclaration();
static void VariableSet(Token name, bool canAssign);
static void VariableSetPrevious(bool canAssign);
static ParseRule* CompilerGetRule(TokenType type);
static void CompilerParsePrecedence(Precedence precedence);

static uint8_t IdentifierConstant(Token* name) {
    return CompilerMakeConstant(OBJECT_VALUE(StringCopy(name->Start, name->Length)));
}

static bool IdentifiersEqual(Token* a, Token* b) {
    if (a->Length != b->Length)
        return false;

    return memcmp(a->Start, b->Start, a->Length) == 0;
}

static int ResolveLocal(Compiler* compiler, Token* name) {
    for (int i = compiler->localCount - 1; i >= 0; i--) {
        Local* local = &compiler->Locals[i];
        if (IdentifiersEqual(name, &local->Name)) {
            if (local->Depth == -1)
                Error("Cannot read local variable in its own initializer");
            return i;
        }
    }

    return -1;
}

static void AddLocal(Token name) {
    if (Current->localCount == UINT16_COUNT) {
        Error("Too many local variables in function");
        return;
    }

    Local* local = &Current->Locals[Current->localCount++];
    local->Name = name;
    local->Depth = -1;
}

static void DeclareVariable() {
    Token* Name = &parser.Previous;

    for (int i = Current->localCount - 1; i >= 0; i--) {
        Local* local = &Current->Locals[i];
        if (local->Depth != -1 && local->Depth < Current->scopeDepth) {
            break;
        }

        if (IdentifiersEqual(Name, &local->Name)) {
            Error("Already a variable with this name in this scope");
        }
    }

    AddLocal(*Name);
}

static uint8_t ParseLocalVariable(const char* errorMessage) {
    CompilerConsume(TOKEN_IDENTIFIER, errorMessage);

    if (Current->scopeDepth > 0) DeclareVariable();
    if (Current->scopeDepth > 0) return 0;

    return IdentifierConstant(&parser.Previous);
}

static uint8_t ParseVariable(const char* errorMessage) {
    CompilerConsume(TOKEN_IDENTIFIER, errorMessage);

    return IdentifierConstant(&parser.Previous);
}

static void MarkInitialized() {
    if (Current->scopeDepth == 0)
        return;
    Current->Locals[Current->localCount - 1].Depth = Current->scopeDepth;
}

static void DefineVariable(uint8_t global) {
    CompilerEmitBytes(OP_DEFINE_GLOBAL, global);
}

static void DefineLocalVariable(uint8_t global) {
    if (Current->scopeDepth > 0) {
        MarkInitialized();
        return;
    }

    DefineVariable(global);
}

static uint8_t ArgumentList() {
    uint8_t argumentCount = 0;
    if (!Check(TOKEN_PARENTHESIS_CLOSE)) {
        do {
            CompilerExpression();
            if (argumentCount == 255)
                Error("Cannot have more than 255 arguments in a function call.");
            argumentCount++;
        } while (Match(TOKEN_COMMA));
    }

    CompilerConsume(TOKEN_PARENTHESIS_CLOSE, "Expected ')' after function call parameters.");
    return argumentCount;
}

static void CompilerAnd(bool canAssign) {
    int endJump = CompilerEmitJump(OP_JUMP_IF_FALSE);

    CompilerEmitByte(OP_POP);
    CompilerParsePrecedence(PREC_AND);

    CompilerPatchJump(endJump);
}

static void CompilerOr(bool canAssign) {
    int elseJump = CompilerEmitJump(OP_JUMP_IF_FALSE);
    int endJump = CompilerEmitJump(OP_JUMP);

    CompilerPatchJump(elseJump);
    CompilerEmitByte(OP_POP);

    CompilerParsePrecedence(PREC_OR);
    CompilerPatchJump(endJump);
}

static void CompilerBinary(bool canAssign) {
    TokenType operatorType = parser.Previous.Type;
    ParseRule* Rule = CompilerGetRule(operatorType);
    CompilerParsePrecedence((Precedence)(Rule->precedence + 1));

    switch(operatorType) {
        case TOKEN_PLUS:        CompilerEmitByte(OP_ADD);           break;
        case TOKEN_MINUS:       CompilerEmitByte(OP_SUBTRACT);      break;
        case TOKEN_STAR:        CompilerEmitByte(OP_MULTIPLY);      break;
        case TOKEN_SLASH:       CompilerEmitByte(OP_DIVIDE);        break;
        case TOKEN_ADD_EQUAL:   CompilerEmitByte(OP_ADD);           break;
        case TOKEN_SUB_EQUAL:   CompilerEmitByte(OP_SUBTRACT);      break;
        case TOKEN_EQUAL:       CompilerEmitByte(OP_EQUAL);         break;
        case TOKEN_NOT_EQUAL:   CompilerEmitByte(OP_NOT_EQUAL);     break;
        case TOKEN_GREATER:     CompilerEmitByte(OP_GREATER);       break;
        case TOKEN_GREATER_EQ:  CompilerEmitByte(OP_GREATER_EQ);    break;
        case TOKEN_SMALLER:     CompilerEmitByte(OP_SMALLER);       break;
        case TOKEN_SMALLER_EQ:  CompilerEmitByte(OP_SMALLER_EQ);    break;
        case TOKEN_IS:          CompilerEmitByte(OP_IS);            break;
        default: return;
    }
}

static void CompilerCall(bool canAssign) {
    uint8_t argumentCount = ArgumentList();
    CompilerEmitBytes(OP_CALL, argumentCount);
}

static void CompilerLiteral(bool canAssign) {
    switch(parser.Previous.Type) {
        case TOKEN_TRUE:    CompilerEmitByte(OP_TRUE);  break;
        case TOKEN_FALSE:   CompilerEmitByte(OP_FALSE); break;
        case TOKEN_NULL:    CompilerEmitByte(OP_NULL);  break;
        case TOKEN_MAYBE:   CompilerEmitByte(OP_MAYBE); break;
        default: return; //Unreachable.
    }
}

static void CompilerExpression() {
    CompilerParsePrecedence(PREC_ASSIGNMENT);
}

static void CompilerBlock() {
    while (!Check(TOKEN_BRACKET_CLOSE) && !Check(TOKEN_EOF)) {
        CompilerDeclaration();
    }

    CompilerConsume(TOKEN_BRACKET_CLOSE, "Expected '}' after block.");
}

static void CompilerFunction(FunctionType type) {
    Compiler compiler;
    CompilerInit(&compiler, type);
    CompilerBeginScope();

    CompilerConsume(TOKEN_PARENTHESIS_OPEN, "Expected '(' after function name.");
    if (!Check(TOKEN_PARENTHESIS_CLOSE)) {
        do {
            Current->Function->Arity++;
            if (Current->Function->Arity > 255) {
                ErrorAtCurrent("Cannot have more that 255 parameters for a function.");
            }
            uint8_t Constant = ParseLocalVariable("Expected parameter name.");
            DefineLocalVariable(Constant);
        } while (Match(TOKEN_COMMA));
    }
    CompilerConsume(TOKEN_PARENTHESIS_CLOSE, "Expected ')' after function parameters.");

    CompilerConsume(TOKEN_BRACKET_OPEN, "Expected '{' before function body.");
    CompilerBlock();

    ObjFunction* Function = CompilerEnd();
    CompilerEmitBytes(OP_CONSTANT, CompilerMakeConstant(OBJECT_VALUE(Function)));
}

static void FunctionDeclaration() {
    uint8_t Global = ParseVariable("Expected function name.");
    MarkInitialized();
    CompilerFunction(TYPE_FUNCTION);
    DefineVariable(Global);
}

static void StatementExpression() {
    CompilerExpression();
    CompilerConsume(TOKEN_SEMICOLON, "Expect ';' after expression");
    CompilerEmitByte(OP_POP);
}

static void StatementSwitch() {
    CompilerConsume(TOKEN_PARENTHESIS_OPEN, "Expected '(' after 'switch'.");
    CompilerExpression();
    CompilerConsume(TOKEN_PARENTHESIS_CLOSE, "Expected ')' after value.");
    CompilerConsume(TOKEN_BRACKET_OPEN, "Expected '{' before switch cases.");

    int State = 0;
    int caseEnds[MAX_CASES];
    int caseCount = 0;
    int previousCaseSkip = -1;

    while (!Match(TOKEN_BRACKET_CLOSE) && !Check(TOKEN_EOF)) {
        if (Match(TOKEN_CASE) || Match(TOKEN_DEFAULT)) {
            TokenType caseType = parser.Previous.Type;

            if (State == 2) {
                Error("Cannot have a case or default after the default case.");
            }

            if (State == 1) {
                caseEnds[caseCount++] = CompilerEmitJump(OP_JUMP);

                CompilerPatchJump(previousCaseSkip);
                CompilerEmitByte(OP_POP);
            }

            if (caseType == TOKEN_CASE) {
                State = 1;
        
                CompilerEmitByte(OP_DUPLICATE);
                CompilerExpression();

                CompilerConsume(TOKEN_COLON, "Expected ':' after case value.");
                
                CompilerEmitByte(OP_EQUAL);
                previousCaseSkip = CompilerEmitJump(OP_JUMP_IF_FALSE);

                CompilerEmitByte(OP_POP);
            }
            else {
                State = 2;
                CompilerConsume(TOKEN_COLON, "Expected ':' after default case.");
                previousCaseSkip = -1;
            }
        }
        else {
            if (State == 0) {
                Error("Cannot have statements before any case.");
            }
            CompilerBeginScope();
            CompilerDeclaration();
            CompilerEndScope();
        }
    }
    
    if (State == 1) {
        CompilerPatchJump(previousCaseSkip);
        CompilerEmitByte(OP_POP);
    }

    for (int i = 0; i < caseCount; i++) {
        CompilerPatchJump(caseEnds[i]);
    }

    CompilerEmitByte(OP_POP);
}

static void StatementFor() {
    CompilerBeginScope();
    CompilerConsume(TOKEN_PARENTHESIS_OPEN, "Expected '(' after 'for'.");
    if (Match(TOKEN_SEMICOLON)) {

    }
    else if (Match(TOKEN_LOCAL)) {
        LocalVariableDeclaration();
    }
    else if (Match(TOKEN_GLOBAL)) {
        VariableDeclaration();
    }
    else {
        StatementExpression();
    }

    int loopStart = CurrentChunk()->Count;
    int exitJump = -1;
    if (!Match(TOKEN_SEMICOLON)) {
        CompilerExpression();
        CompilerConsume(TOKEN_SEMICOLON, "Expected ';' after loop condition.");

        exitJump = CompilerEmitJump(OP_JUMP_IF_FALSE);
        CompilerEmitByte(OP_POP);
    }
    
    if (!Match(TOKEN_PARENTHESIS_CLOSE)) {
        int bodyJump = CompilerEmitJump(OP_JUMP);
        int incrementStart = CurrentChunk()->Count;

        CompilerExpression();

        CompilerEmitByte(OP_POP);
        CompilerConsume(TOKEN_PARENTHESIS_CLOSE, "Expected ')' after for clauses.");

        CompilerEmitLoop(loopStart);
        loopStart = incrementStart;
        CompilerPatchJump(bodyJump);
    }

    CompilerStatement();
    CompilerEmitLoop(loopStart);

    if (exitJump != -1) {
        CompilerPatchJump(exitJump);
        CompilerEmitByte(OP_POP);
    }

    CompilerEndScope();
}

static void StatementIf() {
    CompilerConsume(TOKEN_PARENTHESIS_OPEN, "Expected '(' after 'if'.");
    CompilerExpression();
    CompilerConsume(TOKEN_PARENTHESIS_CLOSE, "Expected ')' after condition.");

    int thenJump = CompilerEmitJump(OP_JUMP_IF_FALSE);
    CompilerEmitByte(OP_POP);
    CompilerStatement();

    int elseJump = CompilerEmitJump(OP_JUMP);

    CompilerPatchJump(thenJump);
    CompilerEmitByte(OP_POP);

    if (Match(TOKEN_ELSE)) 
        CompilerStatement();

    CompilerPatchJump(elseJump);
}


static void VariableDeclaration() {
    uint8_t Global = ParseVariable("Expect variable name");
    if (Match(TOKEN_ASSIGN))
        CompilerExpression();
    else
        CompilerEmitByte(OP_NULL);
    
    CompilerConsume(TOKEN_SEMICOLON, "Expect ';' after variable declaration");
    DefineVariable(Global);
}

static void LocalVariableDeclaration() {
    uint8_t Local = ParseLocalVariable("Expected variable name");
    if (Match(TOKEN_ASSIGN))
        CompilerExpression();
    else
        CompilerEmitByte(OP_NULL);

    CompilerConsume(TOKEN_SEMICOLON, "Expected ';' after variable declaration");
    DefineLocalVariable(Local);
}

static void StatementPrint() {
    CompilerExpression();
    CompilerConsume(TOKEN_SEMICOLON, "Expect ; after value.");
    CompilerEmitByte(OP_PRINT);
}

static void StatementWhile() {
    int loopStart = CurrentChunk()->Count;

    CompilerConsume(TOKEN_PARENTHESIS_OPEN, "Expeccted '(' after 'while'.");
    CompilerExpression();
    CompilerConsume(TOKEN_PARENTHESIS_CLOSE, "Expected ')' after condition.");

    int exitJump = CompilerEmitJump(OP_JUMP_IF_FALSE);
    CompilerEmitByte(OP_POP);
    CompilerStatement();

    CompilerEmitLoop(loopStart);

    CompilerPatchJump(exitJump);
    CompilerEmitByte(OP_POP);
}

static void CompilerSynchronize() {
    parser.panicMode = false;

    while (parser.Current.Type != TOKEN_EOF) {
        if (parser.Previous.Type == TOKEN_SEMICOLON) 
            return;
        
        switch (parser.Current.Type) {
            case TOKEN_CLASS:
            case TOKEN_FUNCTION:
            case TOKEN_GLOBAL:
            case TOKEN_LOCAL:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_PRINT:
            case TOKEN_RETURN:
                return;
            default: ;
        }

        CompilerAdvance();
    }
}

static void CompilerDeclaration() {
    if (Match(TOKEN_FUNCTION))
        FunctionDeclaration();
    else if (Match(TOKEN_GLOBAL))
        VariableDeclaration();
    else if (Match(TOKEN_LOCAL))
        LocalVariableDeclaration();
    else
        CompilerStatement();

    if (parser.panicMode)
        CompilerSynchronize();
}

static void CompilerStatement() {
    if (Match(TOKEN_PRINT)) {
        StatementPrint();
    }
    else if (Match(TOKEN_IF)) {
        StatementIf();
    }
    else if (Match(TOKEN_WHILE)) {
        StatementWhile();
    }
    else if (Match(TOKEN_FOR)) {
        StatementFor();
    }
    else if (Match(TOKEN_BRACKET_OPEN)) {
        CompilerBeginScope();
        CompilerBlock();
        CompilerEndScope();
    }
    else if (Match(TOKEN_SWITCH)) {
        StatementSwitch();
    }
    else {
        StatementExpression();
    }
}

static void CompilerGrouping(bool canAssign) {
    CompilerExpression();
    CompilerConsume(TOKEN_PARENTHESIS_CLOSE, "Expected ')' after expression.");
}

static void CompilerNumber(bool canAssign) {
    double value = strtod(parser.Previous.Start, NULL);
    CompilerEmitConstant(NUMBER_VALUE(value));
}

static void CompilerString(bool canAssign) {
    CompilerEmitConstant(OBJECT_VALUE(StringCopy(parser.Previous.Start + 1, parser.Previous.Length - 2)));
}

static void VariableSet(Token name, bool canAssign) {
    uint8_t getOp, setOp;
    int Arg = ResolveLocal(Current, &name);

    if (Arg != -1) {
        setOp = OP_SET_LOCAL;
    }
    else {
        Arg = IdentifierConstant(&name);
        setOp = OP_SET_GLOBAL;
    }

    CompilerEmitBytes(setOp, (uint8_t)Arg);
    CompilerEmitByte(OP_POP);
}

static void VariableSetPrevious(bool canAssign) {
    VariableSet(parser.Previous, canAssign);
}

static void NamedVariable(Token name, bool canAssign) {
    uint8_t getOp, setOp;
    int Arg = ResolveLocal(Current, &name);

    if (Arg != -1) {
        getOp = OP_GET_LOCAL;
        setOp = OP_SET_LOCAL;
    }
    else {
        Arg = IdentifierConstant(&name);
        getOp = OP_GET_GLOBAL;
        setOp = OP_SET_GLOBAL;
    }

    if (canAssign && Match(TOKEN_ASSIGN)) {
        CompilerExpression();
        CompilerEmitBytes(setOp, (uint8_t)Arg);
    }
    else {
        Token currentToken = parser.Current;

        CompilerEmitBytes(getOp, (uint8_t)Arg);
        if (Match(TOKEN_INCREASE)) {
            CompilerEmitByte(OP_POSTINCREASE);
            CompilerEmitBytes(setOp, (uint8_t)Arg);
            CompilerEmitByte(OP_POP);
        }
        else if (Match(TOKEN_DECREASE)) {
            CompilerEmitByte(OP_POSTDECREASE);
            CompilerEmitBytes(setOp, (uint8_t)Arg);
            CompilerEmitByte(OP_POP);
        }
        else if (Match(TOKEN_ADD_EQUAL) || Match(TOKEN_SUB_EQUAL) || Match(TOKEN_MULT_EQUAL) || Match(TOKEN_DIV_EQUAL)) {
            CompilerExpression();
            if (currentToken.Type == TOKEN_ADD_EQUAL)
                CompilerEmitByte(OP_ADD);
            else if (currentToken.Type == TOKEN_SUB_EQUAL)
                CompilerEmitByte(OP_SUBTRACT);
            else if (currentToken.Type == TOKEN_MULT_EQUAL)
                CompilerEmitByte(OP_MULTIPLY);
            else if (currentToken.Type == TOKEN_DIV_EQUAL)
                CompilerEmitByte(OP_DIVIDE);
            CompilerEmitBytes(setOp, (uint8_t)Arg);
        }
    }
}

static void CompilerVariable(bool canAssign) {
    NamedVariable(parser.Previous, canAssign);
}

static void CompilerUnary(bool canAssign) {
    TokenType operatorType = parser.Previous.Type;

    //Compile operand.
    CompilerParsePrecedence(PREC_UNARY);

    switch(operatorType) {
        case TOKEN_MINUS:       CompilerEmitByte(OP_NEGATE);        break;
        case TOKEN_NOT:         CompilerEmitByte(OP_NOT);           break;
        case TOKEN_INCREASE:    CompilerEmitByte(OP_PREINCREASE);   VariableSetPrevious(canAssign); break;
        case TOKEN_DECREASE:    CompilerEmitByte(OP_PREDECREASE);   VariableSetPrevious(canAssign); break;
        default: return;
    }
}

ParseRule rules[] = {
  //    [TOKEN]                     [Functions]
  [TOKEN_PARENTHESIS_OPEN]    = {CompilerGrouping, CompilerCall,   PREC_CALL},
  [TOKEN_PARENTHESIS_CLOSE]   = {NULL,             NULL,           PREC_NONE},
  [TOKEN_BRACKET_OPEN]        = {NULL,             NULL,           PREC_NONE}, 
  [TOKEN_BRACKET_CLOSE]       = {NULL,             NULL,           PREC_NONE},
  [TOKEN_COMMA]               = {NULL,             NULL,           PREC_NONE},
  [TOKEN_DOT]                 = {NULL,             NULL,           PREC_NONE},
  [TOKEN_MINUS]               = {CompilerUnary,    CompilerBinary, PREC_TERM},
  [TOKEN_PLUS]                = {NULL,             CompilerBinary, PREC_TERM},
  [TOKEN_ADD_EQUAL]           = {NULL,             CompilerBinary, PREC_TERM},
  [TOKEN_SUB_EQUAL]           = {NULL,             CompilerBinary, PREC_TERM},
  [TOKEN_MULT_EQUAL]          = {NULL,             CompilerBinary, PREC_TERM},
  [TOKEN_DIV_EQUAL]           = {NULL,             CompilerBinary, PREC_TERM},
  [TOKEN_INCREASE]            = {CompilerUnary,    NULL,           PREC_TERM},
  [TOKEN_DECREASE]            = {CompilerUnary,    NULL,           PREC_TERM},
  [TOKEN_SEMICOLON]           = {NULL,             NULL,           PREC_NONE},
  [TOKEN_SLASH]               = {NULL,             CompilerBinary, PREC_FACTOR},
  [TOKEN_STAR]                = {NULL,             CompilerBinary, PREC_FACTOR},
  [TOKEN_NOT]                 = {CompilerUnary,    NULL,           PREC_NONE},
  [TOKEN_NOT_EQUAL]           = {NULL,             CompilerBinary, PREC_EQUALITY},
  [TOKEN_ASSIGN]              = {NULL,             NULL,           PREC_NONE},
  [TOKEN_EQUAL]               = {NULL,             CompilerBinary, PREC_EQUALITY},
  [TOKEN_GREATER]             = {NULL,             CompilerBinary, PREC_COMPARISON},
  [TOKEN_GREATER_EQ]          = {NULL,             CompilerBinary, PREC_COMPARISON},
  [TOKEN_SMALLER]             = {NULL,             CompilerBinary, PREC_COMPARISON},
  [TOKEN_SMALLER_EQ]          = {NULL,             CompilerBinary, PREC_COMPARISON},
  [TOKEN_IS]                  = {NULL,             CompilerBinary, PREC_COMPARISON},
  [TOKEN_IDENTIFIER]          = {CompilerVariable, NULL,           PREC_NONE},
  [TOKEN_STRING]              = {CompilerString,   NULL,           PREC_NONE},
  [TOKEN_NUMBER]              = {CompilerNumber,   NULL,           PREC_NONE},
  [TOKEN_AND]                 = {NULL,             CompilerAnd,    PREC_AND},
  [TOKEN_CLASS]               = {NULL,             NULL,           PREC_NONE},
  [TOKEN_ELSE]                = {NULL,             NULL,           PREC_NONE},
  [TOKEN_FALSE]               = {CompilerLiteral,  NULL,           PREC_NONE},
  [TOKEN_FOR]                 = {NULL,             NULL,           PREC_NONE},
  [TOKEN_FUNCTION]            = {NULL,             NULL,           PREC_NONE},
  [TOKEN_IF]                  = {NULL,             NULL,           PREC_NONE},
  [TOKEN_NULL]                = {CompilerLiteral,  NULL,           PREC_NONE},
  [TOKEN_OR]                  = {NULL,             CompilerOr,     PREC_OR},
  [TOKEN_PRINT]               = {NULL,             NULL,           PREC_NONE},
  [TOKEN_RETURN]              = {NULL,             NULL,           PREC_NONE},
  [TOKEN_SUPER]               = {NULL,             NULL,           PREC_NONE},
  [TOKEN_THIS]                = {NULL,             NULL,           PREC_NONE},
  [TOKEN_TRUE]                = {CompilerLiteral,  NULL,           PREC_NONE},
  [TOKEN_MAYBE]               = {CompilerLiteral,  NULL,           PREC_NONE},
  [TOKEN_LOCAL]               = {NULL,             NULL,           PREC_NONE},
  [TOKEN_WHILE]               = {NULL,             NULL,           PREC_NONE},
  [TOKEN_ERROR]               = {NULL,             NULL,           PREC_NONE},
  [TOKEN_EOF]                 = {NULL,             NULL,           PREC_NONE},
};

static void CompilerParsePrecedence(Precedence precedence) {
    CompilerAdvance();
    ParseFn prefixRule = CompilerGetRule(parser.Previous.Type)->Prefix;

    if (prefixRule == NULL) {
        Error("Expected expression.");
        return;
    }

    bool canAssign = precedence <= PREC_ASSIGNMENT;
    prefixRule(canAssign);

    while (precedence <= CompilerGetRule(parser.Current.Type)->precedence) {
        CompilerAdvance();
        ParseFn infixRule = CompilerGetRule(parser.Previous.Type)->Infix;
        infixRule(canAssign);
    }

    if (canAssign && Match(TOKEN_ASSIGN)) {
        Error("Invalid assignment target.");
    }
}

static ParseRule* CompilerGetRule(TokenType type) {
    return &rules[type];
}

ObjFunction* Compile(const char* source) {
    ScannerInit(source);
    Compiler compiler;
    CompilerInit(&compiler, TYPE_SCRIPT);

    parser.hadError = false;
    parser.panicMode = false;

    CompilerAdvance();
    
    while (!Match(TOKEN_EOF)) {
        CompilerDeclaration();
    }

    ObjFunction* Function = CompilerEnd();
    return (parser.hadError ? NULL : Function);
}

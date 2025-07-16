#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Common.h"
#include "Compiler.h"
#include "Scanner.h"
#include "Memory.h"

#ifdef DEBUG_PRINT_CODE
#include "Debug.h"
#endif

#define MAX_CASES 256

typedef struct {
    Token current;
    Token previous;
    bool hadError;
    bool panicMode;
} Parser;   // For parsing the tokenized source code into OP codes.

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
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

typedef struct {
    Token name;
    int depth;
    bool isCaptured;
} Local;

typedef struct {
    uint8_t index;
    bool isLocal;
} Upvalue;

typedef enum {
    TYPE_FUNCTION,
    TYPE_SCRIPT,
    TYPE_METHOD,
    TYPE_CONSTRUCTOR,
    TYPE_LAMBDA
} FunctionType;

typedef struct Compiler {
    struct Compiler* enclosing;
    ObjFunction* function;
    FunctionType type;

    Local locals[UINT16_COUNT];
    Upvalue upvalues[UINT16_COUNT];
    int localCount;
    int scopeDepth;
} Compiler;

typedef struct ClassCompiler {
    struct ClassCompiler* enclosing;
    bool hasSuperclass;
} ClassCompiler;

Parser parser;
Compiler* current = NULL;
ClassCompiler* currentClass = NULL;


static MJ_Chunk* CurrentChunk() {
    return &current->function->chunk;
}

static void ErrorAt(Token* token, const char* msg) {
    if (parser.panicMode)
        return;

    parser.panicMode = true;

    fprintf(stderr, COLOR_RED "SyntaxError" COLOR_RESET ": %s", msg);

    if (token->type == TOKEN_EOF)
        fprintf(stderr, " at end");
    else if (token->type != TOKEN_ERROR) {
        fprintf(stderr, " at line %d | '%.*s'", token->line, token->length, token->start);
    }
    
    char* currentLine = ScannerGetSource();

    if (currentLine != NULL)
        fprintf(stderr, "\n   %d | %s", token->line, currentLine);

    fprintf(stderr, "\n");
    parser.hadError = true;
}

static void Error(const char* msg) {
    ErrorAt(&parser.previous, msg);
}

static void ErrorAtCurrent(const char* msg) {
    ErrorAt(&parser.current, msg);
}

static void CompilerAdvance() {
    parser.previous = parser.current;

    for (;;) {
        parser.current = ScannerScanToken(&current->function->chunk);

        if (parser.current.type != TOKEN_ERROR)
            break;

        ErrorAtCurrent(parser.current.start);
    }
}

static void CompilerConsume(TokenType type, const char* msg) {
    if (parser.current.type == type) {
        CompilerAdvance();
        return;
    }

    ErrorAtCurrent(msg);
}

static bool Check(TokenType type) {
    return parser.current.type == type;
}

static bool Match(TokenType type) {
    if (!Check(type))
        return false;
    
    CompilerAdvance();
    return true;
}

static void CompilerEmitByte(uint8_t byte) {
    MJ_ChunkWrite(CurrentChunk(), byte, parser.previous.line, ScannerGetSource());
}

static void CompilerEmitBytes(uint8_t firstByte, uint8_t secondByte) {
    CompilerEmitByte(firstByte);
    CompilerEmitByte(secondByte);
}

static void CompilerEmitLong(long longNumber) {
    MJ_ChunkWriteLong(CurrentChunk(), longNumber, parser.previous.line, ScannerGetSource());
}

static void CompilerEmitByteLong(uint8_t byte, long longNumber) {
    CompilerEmitByte(byte);
    CompilerEmitLong(longNumber);
}

static void CompilerEmitShort(int shortNumber) {
    CompilerEmitByte((shortNumber >> 8) & 0xff);
    CompilerEmitByte(shortNumber & 0xff);
}

static int CompilerEmitJump(uint8_t instruction) {
    CompilerEmitByte(instruction);
    CompilerEmitByte(0xff);
    CompilerEmitByte(0xff);
    return CurrentChunk()->count - 2;
}

static void CompilerEmitLoop(int loopStart) {
    CompilerEmitByte(OP_LOOP);

    int Offset = CurrentChunk()->count - loopStart + 2;
    if (Offset > UINT16_MAX)
        Error("Loop body too large");
    
    CompilerEmitByte((Offset >> 8) & 0xff);
    CompilerEmitByte(Offset & 0xff);
}

static void CompilerEmitReturn() {
    if (current->type == TYPE_CONSTRUCTOR) {
        CompilerEmitBytes(OP_GET_LOCAL, 0);
    } else {
        CompilerEmitByte(OP_NULL);
    }

    CompilerEmitByte(OP_RETURN);
}

static long CompilerMakeConstant(Value value) {
    int Constant = MJ_ChunkAddConstant(CurrentChunk(), value);
    if (Constant > LONG_MAX) {
        Error("Too many constants in one chunk");
        return 0;
    }
    return (long)Constant;
}

static void CompilerEmitConstant(Value value) {
    CompilerEmitByteLong(OP_CONSTANT_LONG, CompilerMakeConstant(value));
    //CompilerEmitByte(CompilerMakeConstant(value))
}

static void CompilerPatchJump(int offset) {
    int Jump = CurrentChunk()->count - offset - 2;

    if (Jump > UINT16_MAX) {
        Error("Too much code to jump over");
    }

    CurrentChunk()->code[offset] = (Jump >> 8) & 0xff;
    CurrentChunk()->code[offset + 1] = Jump & 0xff;
}

static void CompilerInit(Compiler* compiler, FunctionType type) {
    compiler->enclosing = current;
    compiler->function = NULL;
    compiler->type = type;
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
    compiler->function = FunctionNew();
    current = compiler;

    if (type != TYPE_SCRIPT && type != TYPE_LAMBDA) {
        current->function->name = StringCopy(parser.previous.start, parser.previous.length);
    }
    
    Local* local = &current->locals[current->localCount++];
    local->depth = 0;
    local->isCaptured = false;
    if (type == TYPE_METHOD || type == TYPE_CONSTRUCTOR) {
        local->name.start = "this";
        local->name.length = 4;
    } else {
        local->name.start = "";
        local->name.length = 0;
    }
}

static ObjFunction* CompilerEnd() {
    CompilerEmitReturn();
    ObjFunction* function = current->function;
#ifdef DEBUG_PRINT_CODE
    if (!parser.hadError || true)
        DisassembleChunk(CurrentChunk(), function->name != NULL ? function->name->chars : "<script>");
#endif

    current = current->enclosing;
    return function;
}   

static void CompilerBeginScope() {
    current->scopeDepth++;
}

static void CompilerEndScope() {
    current->scopeDepth--;

    if (current->localCount > 0 && 
        current->locals[current->localCount - 1].depth > 
        current->scopeDepth) {
        
        if (current->locals[current->localCount - 1].isCaptured)
            CompilerEmitByte(OP_CLOSE_UPVALUE);
        else
            CompilerEmitByte(OP_POP);
        current->localCount--;
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
static void NamedVariable(Token name, bool canAssign);
static Token SyntheticToken(const char* text);
static void CompilerVariable(bool canAssign);

static char* Substring(char* string, int length) {
    char* heapChars = ALLOCATE(char, length + 1);
    memcpy(heapChars, string, length);
    heapChars[length] = '\0';
    return heapChars;
}

static uint8_t IdentifierConstant(Token* name) {
    return CompilerMakeConstant(OBJECT_VALUE(StringCopy(name->start, name->length)));
}

static bool IdentifiersEqual(Token* a, Token* b) {
    if (a->length != b->length)
        return false;

    return memcmp(a->start, b->start, a->length) == 0;
}

static int ResolveLocal(Compiler* compiler, Token* name) {
    for (int i = compiler->localCount - 1; i >= 0; i--) {
        Local* local = &compiler->locals[i];
        if (IdentifiersEqual(name, &local->name)) {
            if (local->depth == -1)
                Error("Cannot read local variable in its own initializer");
            return i;
        }
    }

    return -1;
}

static int AddUpvalue(Compiler* compiler, uint8_t index, bool isLocal) {
    int upvalueCount = compiler->function->upvalueCount;

    for (int i = 0; i < upvalueCount; i++) {
        Upvalue* upvalue = &compiler->upvalues[i];
        if (upvalue->index == index && upvalue->isLocal == isLocal)
            return i;
    }

    if (upvalueCount == UINT16_COUNT) {
        Error("Too many closure variables in function");
        return 0;
    }

    compiler->upvalues[upvalueCount].isLocal = isLocal;
    compiler->upvalues[upvalueCount].index = index;
    return compiler->function->upvalueCount++;
}

static int ResolveUpvalue(Compiler* compiler, Token* name) {
    if (compiler->enclosing == NULL)
        return -1;

    int Local = ResolveLocal(compiler->enclosing, name);
    if (Local != -1) {
        compiler->enclosing->locals[Local].isCaptured = true;
        return AddUpvalue(compiler, (uint8_t)Local, true);
    }

    int Upvalue = ResolveUpvalue(compiler->enclosing, name);
    if (Upvalue != -1)
        return AddUpvalue(compiler, (uint8_t)Upvalue, false);

    return -1;
}

static void AddLocal(Token name) {
    if (current->localCount == UINT16_COUNT) {
        Error("Too many local variables in function");
        return;
    }

    Local* local = &current->locals[current->localCount++];
    local->name = name;
    local->depth = -1;
    local->isCaptured = false;
}

static void DeclareVariable() {
    Token* name = &parser.previous;

    for (int i = current->localCount - 1; i >= 0; i--) {
        Local* local = &current->locals[i];
        if (local->depth != -1 && local->depth < current->scopeDepth) {
            break;
        }

        if (IdentifiersEqual(name, &local->name)) {
            Error("Already a variable with this name in this scope");
        }
    }

    AddLocal(*name);
}

static uint8_t ParseLocalVariable(const char* errorMessage) {
    CompilerConsume(TOKEN_IDENTIFIER, errorMessage);

    if (current->scopeDepth > 0) DeclareVariable();
    if (current->scopeDepth > 0) return 0;

    return IdentifierConstant(&parser.previous);
}

static uint8_t ParseVariable(const char* errorMessage) {
    CompilerConsume(TOKEN_IDENTIFIER, errorMessage);

    return IdentifierConstant(&parser.previous);
}

static void MarkInitialized() {
    if (current->scopeDepth == 0)
        return;
    current->locals[current->localCount - 1].depth = current->scopeDepth;
}

static void DefineVariable(uint8_t global) {
    CompilerEmitBytes(OP_DEFINE_GLOBAL, global);
}

static void DefineLocalVariable(uint8_t global) {
    if (current->scopeDepth > 0) {
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
                Error("Cannot have more than 255 arguments in a function call");
            argumentCount++;
        } while (Match(TOKEN_COMMA));
    }

    CompilerConsume(TOKEN_PARENTHESIS_CLOSE, "Expected ')' after function call parameters");
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
    TokenType operatorType = parser.previous.type;
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
        case TOKEN_MOD:         CompilerEmitByte(OP_MOD);           break;
        case TOKEN_BITWISE_AND: CompilerEmitByte(OP_BITWISE_AND);   break;
        case TOKEN_BITWISE_OR:  CompilerEmitByte(OP_BITWISE_OR);    break;
        case TOKEN_IS:          CompilerEmitByte(OP_IS);            break;
        default: return;
    }
}

static void CompilerCall(bool canAssign) {
    uint8_t argumentCount = ArgumentList();
    CompilerEmitBytes(OP_CALL, argumentCount);
}

static void CompilerDot(bool canAssign) {
    CompilerConsume(TOKEN_IDENTIFIER, "Expected property name after '.'");
    uint8_t name = IdentifierConstant(&parser.previous);

    if (canAssign && Match(TOKEN_ASSIGN)) {
        CompilerExpression();
        CompilerEmitBytes(OP_SET_PROPERTY, name);
    } else if (Match(TOKEN_PARENTHESIS_OPEN)) {
        uint8_t argumentCount = ArgumentList();
        CompilerEmitBytes(OP_INVOKE, name);
        CompilerEmitByte(argumentCount);
    }
    else {
        CompilerEmitBytes(OP_GET_PROPERTY, name);
    }
}

static void CompilerLiteral(bool canAssign) {
    switch(parser.previous.type) {
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

    CompilerConsume(TOKEN_BRACKET_CLOSE, "Expected '}' after block");
}

static void CompilerFunction(FunctionType type) {
    Compiler compiler;
    CompilerInit(&compiler, type);
    CompilerBeginScope();

    CompilerConsume(TOKEN_PARENTHESIS_OPEN, "Expected '(' after function name");

    if (!Check(TOKEN_PARENTHESIS_CLOSE)) {
        do {
            current->function->arity++;
            if (current->function->arity > 255) {
                ErrorAtCurrent("Cannot have more that 255 parameters for a function");
            }
            
            uint8_t Constant = ParseLocalVariable("Expected parameter name");
            
            //if (Match(TOKEN_ASSIGN))
            //    CompilerExpression();

            DefineLocalVariable(Constant);
        } while (Match(TOKEN_COMMA));
    }
    CompilerConsume(TOKEN_PARENTHESIS_CLOSE, "Expected ')' after function parameters");

    if (Match(TOKEN_FAT_ARROW)) {
        CompilerExpression();
        //CompilerConsume(TOKEN_SEMICOLON, "Expected ';' after function expression");
        CompilerEmitByte(OP_RETURN);
    } else {
        CompilerConsume(TOKEN_BRACKET_OPEN, "Expected '{' before function body");
        CompilerBlock();
    }

    ObjFunction* function = CompilerEnd();
    CompilerEmitBytes(OP_CLOSURE, CompilerMakeConstant(OBJECT_VALUE(function)));

    for (int i = 0; i < function->upvalueCount; i++) {
        CompilerEmitByte((compiler.upvalues[i].isLocal) ? 1 : 0);
        CompilerEmitByte(compiler.upvalues[i].index);
    }
}

static void CompilerMethod(const char* className) {
    CompilerConsume(TOKEN_IDENTIFIER, "Expected method name");
    uint8_t constant = IdentifierConstant(&parser.previous);

    FunctionType type = TYPE_METHOD;

    size_t classNameLength = strlen(className);

    if (parser.previous.length == classNameLength && memcmp(parser.previous.start, className, classNameLength) == 0) {
        type = TYPE_CONSTRUCTOR;
    }

    CompilerFunction(type);
    CompilerEmitBytes(OP_METHOD, constant);
}

static void CompilerLambda(bool canAssign) {
    CompilerFunction(TYPE_LAMBDA);   
}

static void CompilerInitProperty() {
    CompilerConsume(TOKEN_IDENTIFIER, "Expected property name after '.'");
    uint8_t name = IdentifierConstant(&parser.previous);

    if (Match(TOKEN_ASSIGN)) {
        CompilerExpression();
    } else {
        CompilerEmitByte(OP_NULL);
    }

    CompilerEmitBytes(OP_INIT_PROPERTY, name);
    CompilerAdvance();
}

static void ClassDeclaration() {
    CompilerConsume(TOKEN_IDENTIFIER, "Expected class name");
    Token className = parser.previous;
    uint8_t nameConstant = IdentifierConstant(&parser.previous);
    if (current->scopeDepth != 0) DeclareVariable();

    CompilerEmitBytes(OP_CLASS, nameConstant);
    DefineLocalVariable(nameConstant);

    ClassCompiler classCompiler;
    classCompiler.hasSuperclass = false;
    classCompiler.enclosing = currentClass;
    currentClass = &classCompiler;

    CompilerBeginScope();

    if (Match(TOKEN_COLON)) {
        CompilerConsume(TOKEN_IDENTIFIER, "Expected superclass name");
        CompilerVariable(false);

        if (IdentifiersEqual(&className, &parser.previous)) {
            Error("A class cannot inherit from itself");
        }

        AddLocal(SyntheticToken("super"));
        DefineLocalVariable(0);

        NamedVariable(className, false);
        CompilerEmitByte(OP_INHERIT);
        classCompiler.hasSuperclass = true;
    }

    NamedVariable(className, false);

    CompilerConsume(TOKEN_BRACKET_OPEN, "Expected '{' before class body");
    
    char* classNameString = Substring(className.start, className.length);

    while (!Check(TOKEN_BRACKET_CLOSE) && !Check(TOKEN_EOF)) {
        if (Match(TOKEN_LOCAL)) {
            CompilerInitProperty();
            continue;
        }

        CompilerMethod(classNameString);
    }

    FREE(char, classNameString);

    CompilerConsume(TOKEN_BRACKET_CLOSE, "Expected '}' after class body");
    CompilerEmitByte(OP_POP);

    CompilerEndScope();

    currentClass = currentClass->enclosing;
}

static void FunctionDeclaration() {
    // Checking because CompilerFunction consumes the opening parenthesis.
    if (Check(TOKEN_PARENTHESIS_OPEN)) {
        CompilerFunction(TYPE_LAMBDA);
        return;
    }

    // Regular function;
    uint8_t global = ParseVariable("Expected function name");
    MarkInitialized();
    CompilerFunction(TYPE_FUNCTION);
    DefineVariable(global);
}

static void StatementExpression() {
    CompilerExpression();
    CompilerConsume(TOKEN_SEMICOLON, "Expect ';' after expression");
    CompilerEmitByte(OP_POP);
}

static void StatementSwitch() {
    CompilerConsume(TOKEN_PARENTHESIS_OPEN, "Expected '(' after 'switch'");
    CompilerExpression();
    CompilerConsume(TOKEN_PARENTHESIS_CLOSE, "Expected ')' after value");
    CompilerConsume(TOKEN_BRACKET_OPEN, "Expected '{' before switch cases");

    int State = 0;
    int caseEnds[MAX_CASES];
    int caseCount = 0;
    int previousCaseSkip = -1;

    while (!Match(TOKEN_BRACKET_CLOSE) && !Check(TOKEN_EOF)) {
        if (Match(TOKEN_CASE) || Match(TOKEN_DEFAULT)) {
            TokenType caseType = parser.previous.type;

            if (State == 2) {
                Error("Cannot have a case or default after the default case");
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

                CompilerConsume(TOKEN_COLON, "Expected ':' after case value");
                
                CompilerEmitByte(OP_EQUAL);
                previousCaseSkip = CompilerEmitJump(OP_JUMP_IF_FALSE);

                CompilerEmitByte(OP_POP);
            }
            else {
                State = 2;
                CompilerConsume(TOKEN_COLON, "Expected ':' after default case");
                previousCaseSkip = -1;
            }
        }
        else {
            if (State == 0) {
                Error("Cannot have statements before any case");
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
    CompilerConsume(TOKEN_PARENTHESIS_OPEN, "Expected '(' after 'for'");
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

    int loopStart = CurrentChunk()->count;
    int exitJump = -1;
    if (!Match(TOKEN_SEMICOLON)) {
        CompilerExpression();
        CompilerConsume(TOKEN_SEMICOLON, "Expected ';' after loop condition");

        exitJump = CompilerEmitJump(OP_JUMP_IF_FALSE);
        CompilerEmitByte(OP_POP);
    }
    
    if (!Match(TOKEN_PARENTHESIS_CLOSE)) {
        int bodyJump = CompilerEmitJump(OP_JUMP);
        int incrementStart = CurrentChunk()->count;

        CompilerExpression();

        CompilerEmitByte(OP_POP);
        CompilerConsume(TOKEN_PARENTHESIS_CLOSE, "Expected ')' after for clauses");

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
    CompilerConsume(TOKEN_PARENTHESIS_OPEN, "Expected '(' after 'if'");
    CompilerExpression();
    CompilerConsume(TOKEN_PARENTHESIS_CLOSE, "Expected ')' after condition");

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

static void ParameterDeclaration() {
    uint8_t Global = ParseVariable("Expected parameter name");
    if (Match(TOKEN_ASSIGN))
        CompilerExpression();
    else
        CompilerEmitByte(OP_NULL);

    DefineLocalVariable(Global);
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
    CompilerConsume(TOKEN_SEMICOLON, "Expect ; after value");
    CompilerEmitByte(OP_PRINT);
}

static void StatementReturn() {
    if (current->type == TYPE_SCRIPT) {
        Error("Cannot return from outside a function");
    }

    if (Match(TOKEN_SEMICOLON))
        CompilerEmitReturn();
    else {
        if (current->type == TYPE_CONSTRUCTOR) {
            Error("Cannot return a value from a constructor");
        }

        CompilerExpression();
        CompilerConsume(TOKEN_SEMICOLON, "Expected ';' after return value");
        CompilerEmitByte(OP_RETURN);
    }
}

static void StatementWhile() {
    int loopStart = CurrentChunk()->count;

    CompilerConsume(TOKEN_PARENTHESIS_OPEN, "Expected '(' after 'while'");
    CompilerExpression();
    CompilerConsume(TOKEN_PARENTHESIS_CLOSE, "Expected ')' after condition");

    int exitJump = CompilerEmitJump(OP_JUMP_IF_FALSE);
    CompilerEmitByte(OP_POP);
    CompilerStatement();

    CompilerEmitLoop(loopStart);

    CompilerPatchJump(exitJump);
    CompilerEmitByte(OP_POP);
}

static void CompilerSynchronize() {
    parser.panicMode = false;

    while (parser.current.type != TOKEN_EOF) {
        if (parser.previous.type == TOKEN_SEMICOLON) 
            return;
        
        switch (parser.current.type) {
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
    if (Match(TOKEN_CLASS))
        ClassDeclaration();
    else if (Match(TOKEN_FUNCTION))
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
    else if (Match(TOKEN_RETURN)) {
        StatementReturn();
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
    } else {
        StatementExpression();
    }
}

static void CompilerGrouping(bool canAssign) {
    CompilerExpression();
    CompilerConsume(TOKEN_PARENTHESIS_CLOSE, "Expected ')' after expression");
}

static void CompilerNumber(bool canAssign) {
    double value = strtod(parser.previous.start, NULL);
    CompilerEmitConstant(NUMBER_VALUE(value));
}

static void CompilerString(bool canAssign) {
    CompilerEmitConstant(OBJECT_VALUE(StringCopy(parser.previous.start + 1, parser.previous.length - 2)));
}

static void CompilerArray(bool canAssign) {
    CompilerEmitByte(OP_NULL);

    int numOfItems = 0;

    if (!Check(TOKEN_SQUARE_CLOSE)) {
        do {
            if (!Check(TOKEN_SQUARE_CLOSE)) {
                if (numOfItems >= UINT16_MAX)
                    Error("Too many items to store in array");

                CompilerExpression();
                numOfItems++;
            }
        } while (Match(TOKEN_COMMA));
    }

    CompilerConsume(TOKEN_SQUARE_CLOSE, "Expected ']' at the end of the array");
    CompilerEmitByte(OP_ARRAY);
    CompilerEmitShort(numOfItems);
}

static void CompilerMap(bool canAssign) {
    CompilerEmitByte(OP_NULL);

    int numOfItems = 0;

    if (!Check(TOKEN_BRACKET_CLOSE)) {
        do {
            if (!Check(TOKEN_BRACKET_CLOSE)) {
                CompilerExpression();
                CompilerConsume(TOKEN_COLON, "Expected ':' for value for pair");
                CompilerExpression();
                numOfItems += 2;
            }
        } while (Match(TOKEN_COMMA));
    }

    CompilerConsume(TOKEN_BRACKET_CLOSE, "Expected '}' at the end of the map");
    CompilerEmitByte(OP_MAP);
    CompilerEmitShort(numOfItems);
}

static void VariableSet(Token name, bool canAssign) {
    uint8_t getOp, setOp;
    int Arg = ResolveLocal(current, &name);

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
    VariableSet(parser.previous, canAssign);
}

static void ResolveExtraAssignments(int getOp, int setOp, int arg) {
    Token currentToken = parser.current;

    CompilerEmitByte(getOp);

    if (arg != -1)
        CompilerEmitByte((uint8_t)arg);

    if (Match(TOKEN_INCREASE)) {
        CompilerEmitByte(OP_POSTINCREASE);
        CompilerEmitByte(setOp);
        if (arg != -1)
            CompilerEmitByte((uint8_t)arg);
        CompilerEmitByte(OP_POP);
    }
    else if (Match(TOKEN_DECREASE)) {
        CompilerEmitByte(OP_POSTDECREASE);
        CompilerEmitByte(setOp);
        if (arg != -1)
            CompilerEmitByte((uint8_t)arg);
        CompilerEmitByte(OP_POP);
    }
    else if (Match(TOKEN_ADD_EQUAL) || Match(TOKEN_SUB_EQUAL) || Match(TOKEN_MULT_EQUAL) || Match(TOKEN_DIV_EQUAL)) {
        CompilerExpression();
        
        if (currentToken.type == TOKEN_ADD_EQUAL)
            CompilerEmitByte(OP_ADD);
        else if (currentToken.type == TOKEN_SUB_EQUAL)
            CompilerEmitByte(OP_SUBTRACT);
        else if (currentToken.type == TOKEN_MULT_EQUAL)
            CompilerEmitByte(OP_MULTIPLY);
        else if (currentToken.type == TOKEN_DIV_EQUAL)
            CompilerEmitByte(OP_DIVIDE);
        CompilerEmitByte(setOp);
        if (arg != -1)
            CompilerEmitByte((uint8_t)arg);
    }
}

static void NamedVariable(Token name, bool canAssign) {
    uint8_t getOp, setOp;
    int argument = ResolveLocal(current, &name);

    if (argument != -1) {
        getOp = OP_GET_LOCAL;
        setOp = OP_SET_LOCAL;
    }
    else if ((argument = ResolveUpvalue(current, &name)) != -1) {
        getOp = OP_GET_UPVALUE;
        setOp = OP_SET_UPVALUE;
    }
    else {
        argument = IdentifierConstant(&name);
        getOp = OP_GET_GLOBAL;
        setOp = OP_SET_GLOBAL;
    }

    if (canAssign && Match(TOKEN_ASSIGN)) {
        CompilerExpression();
        CompilerEmitBytes(setOp, (uint8_t)argument);
    }
    else {
        ResolveExtraAssignments(getOp, setOp, argument);
    }
}

static void CompilerVariable(bool canAssign) {
    NamedVariable(parser.previous, canAssign);
}

static Token SyntheticToken(const char* text) {
    Token token;
    token.start = text;
    token.length = (int)strlen(text);
    return token;
}

static void CompilerSuper(bool canAssign) {
    if (currentClass == NULL) {
        Error("Cannot use \"super\" outside of a class");
    } else if (!currentClass->hasSuperclass) {
        Error("Cannot use \"super\" in a class with no superclass");
    }

    if (Match(TOKEN_PARENTHESIS_OPEN)) {
        if (current->type != TYPE_CONSTRUCTOR) {
            Error("Super cannot be called outside of the class' constructor");
            return;
        }

        Token superToken = SyntheticToken("super");
        uint8_t superName = IdentifierConstant(&superToken);
        NamedVariable(SyntheticToken("this"), false);
        uint8_t argumentCount = ArgumentList();
        NamedVariable(superToken, false);
        CompilerEmitBytes(OP_SUPER_INVOKE, superName);
        CompilerEmitByte(argumentCount);
        return;
    }

    CompilerConsume(TOKEN_DOT, "Expected '.' after \"super\".");
    CompilerConsume(TOKEN_IDENTIFIER, "Expected superclass method name.");
    uint8_t name = IdentifierConstant(&parser.previous);

    NamedVariable(SyntheticToken("this"), false);
    if (Match(TOKEN_PARENTHESIS_OPEN)) {
        uint8_t argumentCount = ArgumentList();
        NamedVariable(SyntheticToken("super"), false);
        CompilerEmitBytes(OP_SUPER_INVOKE, name);
        CompilerEmitByte(argumentCount);
        return;
    }

    NamedVariable(SyntheticToken("super"), false);
    CompilerEmitBytes(OP_GET_SUPER, name);
}

static void CompilerThis(bool canAssign) {
    if (currentClass == NULL) {
        Error("Cannot use \"this\" outside of a class");
        return;
    }

    CompilerVariable(false);
}

static void CompilerIndex(bool canAssign) {
    // Arrays have numeric indexes, but can also have ranges.
    // Numeric indexes return a single value. Range indexes return a list with the contents of the range.
    bool isAssignable = true; // We can assign to a single index but we can't assign to a range.
    bool matchedColon = false;
    uint8_t getOp = OP_GET_INDEX;

    // If we find a colon, we know that this is going to be a range starting from the beginning of the array.
    //      Ex: [:2] = [(start of array):2] / [0:2]
    if (Match(TOKEN_COLON)) {
        isAssignable = false;       // We disable assigning to a range.
        matchedColon = true;
        CompilerEmitByte(OP_NULL);  // Null represents the beginning of an array.
        getOp = OP_GET_INDEX_RANGED;
    }
    else {
        // There was no colon so we try to get an expression.
        CompilerExpression();
    }

    // If we haven't yet found a bracket after the index, we might be looking at the end of a range.
    if (!Match(TOKEN_SQUARE_CLOSE)) {
        getOp = OP_GET_INDEX_RANGED;

        // If we haven't matched a token, then we got an expression. So we look for the colon again.
        if (!matchedColon)
            CompilerConsume(TOKEN_COLON, "Expected ':' or ']'");

        matchedColon = true;

        isAssignable = false;
        // If we got here, a colon token got consumed. Now we check for either the end of the range or the end of the index.
        if (Match(TOKEN_SQUARE_CLOSE)) {
            CompilerEmitByte(OP_NULL);
            CompilerEmitByte(OP_NULL);
        }
        else {
            if (Match(TOKEN_COLON))
                CompilerEmitByte(OP_NULL);
            else 
                CompilerExpression();
            
            if (!Match(TOKEN_SQUARE_CLOSE)) {
                CompilerConsume(TOKEN_COLON, "Expected ':' or ']'");
                CompilerExpression();
                CompilerConsume(TOKEN_SQUARE_CLOSE, "Expected ']' after index range");
            }
            else
                CompilerEmitByte(OP_NULL);
        }
    }
    else {
        // We found a closing square bracket.
        // If we previously matched a colon, that means we have a range to the end of the array.
        //      Ex: [2:] -> [2:(end of array)]
        if (matchedColon) {
            CompilerEmitByte(OP_NULL);
            CompilerEmitByte(OP_NULL);
        }
    }

    if (Match(TOKEN_ASSIGN)) {
        if (!isAssignable)
            Error("Cannot assign a value to a ranged index");
            
        CompilerExpression();
        CompilerEmitByte(OP_SET_INDEX);
    }
    else {
        if (matchedColon)
            ResolveExtraAssignments(getOp, OP_SET_INDEX, -1);
        else
            CompilerEmitByte(OP_GET_INDEX);
    }
}

static void CompilerUnary(bool canAssign) {
    TokenType operatorType = parser.previous.type;

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
  [TOKEN_BRACKET_OPEN]        = {CompilerMap,      NULL,           PREC_NONE}, 
  [TOKEN_BRACKET_CLOSE]       = {NULL,             NULL,           PREC_NONE},
  [TOKEN_SQUARE_OPEN]         = {CompilerArray,    CompilerIndex,  PREC_CALL},
  [TOKEN_COMMA]               = {NULL,             NULL,           PREC_NONE},
  [TOKEN_DOT]                 = {NULL,             CompilerDot,    PREC_CALL},
  [TOKEN_MINUS]               = {CompilerUnary,    CompilerBinary, PREC_TERM},
  [TOKEN_PLUS]                = {NULL,             CompilerBinary, PREC_TERM},
  [TOKEN_ADD_EQUAL]           = {NULL,             CompilerBinary, PREC_TERM},
  [TOKEN_SUB_EQUAL]           = {NULL,             CompilerBinary, PREC_TERM},
  [TOKEN_MULT_EQUAL]          = {NULL,             CompilerBinary, PREC_TERM},
  [TOKEN_DIV_EQUAL]           = {NULL,             CompilerBinary, PREC_TERM},
  [TOKEN_INCREASE]            = {CompilerUnary,    NULL,           PREC_TERM},
  [TOKEN_DECREASE]            = {CompilerUnary,    NULL,           PREC_TERM},
  [TOKEN_MOD]                 = {NULL,             CompilerBinary, PREC_TERM},
  [TOKEN_BITWISE_AND]         = {NULL,             CompilerBinary, PREC_TERM},
  [TOKEN_BITWISE_OR]          = {NULL,             CompilerBinary, PREC_TERM},
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
  [TOKEN_FUNCTION]            = {CompilerLambda,   NULL,           PREC_NONE},
  [TOKEN_IF]                  = {NULL,             NULL,           PREC_NONE},
  [TOKEN_NULL]                = {CompilerLiteral,  NULL,           PREC_NONE},
  [TOKEN_OR]                  = {NULL,             CompilerOr,     PREC_OR},
  [TOKEN_PRINT]               = {NULL,             NULL,           PREC_NONE},
  [TOKEN_RETURN]              = {NULL,             NULL,           PREC_NONE},
  [TOKEN_SUPER]               = {CompilerSuper,    NULL,           PREC_NONE},
  [TOKEN_THIS]                = {CompilerThis,     NULL,           PREC_NONE},
  [TOKEN_TRUE]                = {CompilerLiteral,  NULL,           PREC_NONE},
  [TOKEN_MAYBE]               = {CompilerLiteral,  NULL,           PREC_NONE},
  [TOKEN_LOCAL]               = {NULL,             NULL,           PREC_NONE},
  [TOKEN_WHILE]               = {NULL,             NULL,           PREC_NONE},
  [TOKEN_ERROR]               = {NULL,             NULL,           PREC_NONE},
  [TOKEN_EOF]                 = {NULL,             NULL,           PREC_NONE},
};

static void CompilerParsePrecedence(Precedence precedence) {
    CompilerAdvance();

    ParseFn prefixRule = CompilerGetRule(parser.previous.type)->prefix;

    if (prefixRule == NULL) {
        Error("Expected expression");
        return;
    }

    bool canAssign = (precedence <= PREC_ASSIGNMENT);
    prefixRule(canAssign);

    while (precedence <= CompilerGetRule(parser.current.type)->precedence) {
        CompilerAdvance();
        ParseFn infixRule = CompilerGetRule(parser.previous.type)->infix;   
        infixRule(canAssign);
    }

    if (canAssign && Match(TOKEN_ASSIGN)) {
        Error("Invalid assignment target");
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

    ObjFunction* function = CompilerEnd();
    return (parser.hadError ? NULL : function);
}

void CompilerMarkRoots() {
    Compiler* compiler = current;
    while (compiler != NULL) {
        MarkObject((Object*)compiler->function);
        compiler = compiler->enclosing;   
    }
}
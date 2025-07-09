#include <stdio.h>
#include <string.h>

#include "Common.h"
#include "Scanner.h"
#include "Utilities.h"
#include "Memory.h"

typedef struct {
    const char* start;
    const char* current;
    int line;
    int previousLine;
    char* source;
} Scanner;

Scanner scanner;
bool inStringInterpolation = false;

void ScannerInit(const char* source) {
    scanner.start = source;
    scanner.current = source;
    scanner.line = 1;
    scanner.previousLine = 0;
    scanner.source = NULL;
}

static bool ScannerAtEnd() {
    return (*scanner.current == '\0');
}

static Token TokenMake(TokenType type) {
    Token newToken;

    newToken.type = type;
    newToken.start = scanner.start;
    newToken.length = (int)(scanner.current - scanner.start);
    newToken.line = scanner.line;

    return newToken;
}

static Token TokenError(const char* msg) {
    Token errorToken;

    errorToken.type = TOKEN_ERROR;
    errorToken.start = msg;
    errorToken.length = (int)strlen(msg);
    errorToken.line = scanner.line;

    return errorToken;
}

static char ScannerAdvance() {
    scanner.current++;
    return scanner.current[-1];
}

static char ScannerMatch(char expected) {
    if (ScannerAtEnd())
        return false;

    if (*scanner.current != expected)
        return false;

    scanner.current++;
    return true;
}

static char ScannerPeek() {
    return *scanner.current;
}

static char ScannerPeekNext() {
    if (ScannerAtEnd())
        return '\0';
    return scanner.current[1];
}

static void SkipWhitespace() {
    for (;;) {
        char currentChar = ScannerPeek();
        char secondChar;
        switch(currentChar) {
            case ' ':
            case '\r':
            case '\t':
                ScannerAdvance();
                break;
            case '/':
                secondChar = ScannerPeekNext();
                if (secondChar == '/') {
                    while (ScannerPeek() != '\n' && !ScannerAtEnd())
                        ScannerAdvance();
                }
                else if (secondChar == '*') {
                    while (ScannerPeek() != '*' && ScannerPeekNext() != '/' && !ScannerAtEnd())
                        ScannerAdvance();
                }
                else {
                    return;
                }
                break;
            case '\n':
                scanner.line++;
                ScannerAdvance();
                break;
            default:
                return;
        }
    }
}

static Token ScannerScanString() {
    while (ScannerPeek() != '"' && !ScannerAtEnd()) {
        if (ScannerPeek() == '\n')
            scanner.line++;
      
        ScannerAdvance();
    }

    if (ScannerAtEnd())
        return TokenError("Unterminated string literal.");

    ScannerAdvance();
    return TokenMake(TOKEN_STRING);
}

static Token ScannerScanNumber() {
    //Checks if the current character is either a digit or a underscore separator.
    while (IsDigit(ScannerPeek()))
        ScannerAdvance();

    if (ScannerPeek() == '.' && IsDigit(ScannerPeekNext())) {
        ScannerAdvance();

        while (IsDigit(ScannerPeek()) || (ScannerPeek() == '_' && IsDigit(ScannerPeekNext())))
            ScannerAdvance();
    }

    return TokenMake(TOKEN_NUMBER);
}

static TokenType CheckKeyword(int start, const char* rest, TokenType type) {
    int restLength = (int)strlen(rest);

    if ((scanner.current - scanner.start == start + restLength) && memcmp(scanner.start + start, rest, restLength) == 0)
        return type;
    
    return TOKEN_IDENTIFIER;
}

static TokenType IdentifierType() {
    switch (scanner.start[0]) {
        case 'a':
            if (scanner.current - scanner.start > 1) {
                TokenType possible = CheckKeyword(1, "s", TOKEN_AS);

                if (possible != TOKEN_IDENTIFIER) {
                    return possible;
                }

                switch(scanner.start[1]) {
                    case 'n': return CheckKeyword(2, "d", TOKEN_AND);
                    case 'f': return CheckKeyword(2, "ter", TOKEN_AFTER);
                }
            }
        case 'b': return CheckKeyword(1, "reak", TOKEN_BREAK);
        case 'c':
            if (scanner.current - scanner.start > 1) {
                switch(scanner.start[1]) {
                    case 'l': return CheckKeyword(2, "ass", TOKEN_CLASS);
                    case 'o': {
                        TokenType possible;

                        possible = CheckKeyword(2, "nst", TOKEN_CONST);
          
                        if (possible == TOKEN_IDENTIFIER) {
                            possible = CheckKeyword(2, "ntinue", TOKEN_CONTINUE);
                        }

                        return possible;
                    }
                    case 'a': return CheckKeyword(2, "se", TOKEN_CASE);
                }
            }
        case 'd': return CheckKeyword(1, "efault", TOKEN_DEFAULT);
        case 'e': return CheckKeyword(1, "lse", TOKEN_ELSE);
        case 'f':
            if (scanner.current - scanner.start > 1) {
                switch(scanner.start[1]) {
                    case 'a': return CheckKeyword(2, "lse", TOKEN_FALSE);
                    case 'o': return CheckKeyword(2, "r", TOKEN_FOR);
                    case 'u': return CheckKeyword(2, "nction", TOKEN_FUNCTION);
                }
            }
        case 'g': return CheckKeyword(1, "lobal", TOKEN_GLOBAL);
        case 'i': {
            TokenType possible = CheckKeyword(1, "f", TOKEN_IF);

            if (possible == TOKEN_IDENTIFIER) {
                return CheckKeyword(1, "s", TOKEN_IS);
            }

            return possible;
        }
        case 'm': return CheckKeyword(1, "aybe", TOKEN_MAYBE);
        case 'n': return CheckKeyword(1, "ull", TOKEN_NULL);
        case 'o': return CheckKeyword(1, "r", TOKEN_OR);
        case 'p': return CheckKeyword(1, "rint", TOKEN_PRINT);
        case 'r': return CheckKeyword(1, "eturn", TOKEN_RETURN);
        case 's': if (scanner.current - scanner.start > 1) {
                switch(scanner.start[1]) {
                    case 't': return CheckKeyword(1, "atic", TOKEN_STATIC);
                    case 'w': return CheckKeyword(2, "itch", TOKEN_SWITCH);
                }
            }
        case 't':
            if (scanner.current - scanner.start > 1) {
                switch(scanner.start[1]) {
                    case 'h': return CheckKeyword(2, "is", TOKEN_THIS);
                    case 'r': return CheckKeyword(2, "ue", TOKEN_TRUE);
                }
            }
        case 'v': return CheckKeyword(1, "ar", TOKEN_LOCAL);
        case 'w': return CheckKeyword(1, "hile", TOKEN_WHILE);
    }
    return TOKEN_IDENTIFIER;
}

static Token ScannerScanIdentifier() {
    while(IsAlphanumeric(ScannerPeek()) || IsDigit(ScannerPeek()))
        ScannerAdvance();

    return TokenMake(IdentifierType());
}

static int ScannerGetLineLength() {
    char* p = &scanner.start[0];
    int l = 0;
    while (*p != '\0' && *p != '\n') {
        l++;
        p++;
    }
    return l;
}

static void ScannerSetSource() {
    int length = ScannerGetLineLength();

    if (length <= 0)
        return;

    if (scanner.source != NULL)
        FREE(char, scanner.source);

    scanner.source = ALLOCATE(char, length + 1);
    memcpy(scanner.source, scanner.start, length);
    scanner.source[length] = '\0';
}

char* ScannerGetSource() {
    if (scanner.source == NULL)
        ScannerSetSource();

    return scanner.source;
}

Token ScannerScanToken() {
    if (scanner.previousLine != scanner.line) {
        ScannerSetSource();
        scanner.previousLine = scanner.line;
    }

    SkipWhitespace();
    scanner.start = scanner.current;

    if (ScannerAtEnd())
        return TokenMake(TOKEN_EOF);

    char currentChar = ScannerAdvance();

    if (IsAlphanumeric(currentChar))
        return ScannerScanIdentifier();
    if (IsDigit(currentChar))
        return ScannerScanNumber();
  
    switch (currentChar) {
        case '(': return TokenMake(TOKEN_PARENTHESIS_OPEN); break;
        case ')': return TokenMake(TOKEN_PARENTHESIS_CLOSE); break;
        case '{':
            if (!inStringInterpolation)
                return TokenMake(TOKEN_BRACKET_OPEN);
        case '}': return TokenMake(TOKEN_BRACKET_CLOSE); break;
        case '[': return TokenMake(TOKEN_SQUARE_OPEN); break;
        case ']': return TokenMake(TOKEN_SQUARE_CLOSE); break;
        case ',': return TokenMake(TOKEN_COMMA); break;
        case '.': return TokenMake(TOKEN_DOT); break;
        case ':': return TokenMake(TOKEN_COLON); break;
        case ';': return TokenMake(TOKEN_SEMICOLON);
        case '%': return TokenMake(TOKEN_MOD); break;
        case '&': return TokenMake(TOKEN_BITWISE_AND); break;
        case '|': return TokenMake(TOKEN_BITWISE_OR); break;
        case '+':
            return TokenMake(ScannerMatch('=') ? TOKEN_ADD_EQUAL : (ScannerMatch('+') ? TOKEN_INCREASE : TOKEN_PLUS));
        case '-':
            return TokenMake(ScannerMatch('=') ? TOKEN_SUB_EQUAL: (ScannerMatch('-') ? TOKEN_DECREASE : TOKEN_MINUS));
        case '*':
            return TokenMake(ScannerMatch('=') ? TOKEN_MULT_EQUAL : TOKEN_STAR);
        case '/':
            return TokenMake(ScannerMatch('=') ? TOKEN_DIV_EQUAL: TOKEN_SLASH);
        case '!':
            return TokenMake(ScannerMatch('=') ? TOKEN_NOT_EQUAL : TOKEN_NOT);
        case '=':
            return TokenMake(ScannerMatch('=') ? TOKEN_EQUAL : TOKEN_ASSIGN);
        case '>':
            return TokenMake(ScannerMatch('=') ? TOKEN_GREATER_EQ : TOKEN_GREATER);
        case '<':
            return TokenMake(ScannerMatch('=') ? TOKEN_SMALLER_EQ : TOKEN_SMALLER);
        case '"': return ScannerScanString();
    }

    return TokenError("Unexpected character");
}
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "scanner.h"
#include "utilities.h"

typedef struct {
  const char* Start;
  const char* Current;
  int Line;
} Scanner;

Scanner scanner;
bool inStringInterpolation = false;

void ScannerInit(const char* source) {
  scanner.Start = source;
  scanner.Current = source;
  scanner.Line = 1;
}

static bool ScannerAtEnd() {
  return (*scanner.Current == '\0');
}

static Token TokenMake(TokenType type) {
  Token newToken;

  newToken.Type = type;
  newToken.Start = scanner.Start;
  newToken.Length = (int)(scanner.Current - scanner.Start);
  newToken.Line = scanner.Line;

  return newToken;
}

static Token TokenError(const char* msg) {
  Token errorToken;

  errorToken.Type = TOKEN_ERROR;
  errorToken.Start = msg;
  errorToken.Length = (int)strlen(msg);
  errorToken.Line = scanner.Line;

  return errorToken;
}

static char ScannerAdvance() {
  scanner.Current++;
  return scanner.Current[-1];
}

static char ScannerMatch(char expected) {
  if (ScannerAtEnd())
    return false;

  if (*scanner.Current != expected)
    return false;

  scanner.Current++;
  return true;
}

static char ScannerPeek() {
  return *scanner.Current;
}

static char ScannerPeekNext() {
  if (ScannerAtEnd())
    return '\0';
  return scanner.Current[1];
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
        scanner.Line++;
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
      scanner.Line++;
      
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

    while (IsDigit(ScannerPeek()) || (ScannerPeek() == "_" && IsDigit(ScannerPeekNext)))
      ScannerAdvance();
  }

  return TokenMake(TOKEN_NUMBER);
}

static TokenType CheckKeyword(int start, const char* rest, TokenType type) {
  int restLength = (int)strlen(rest);
  if ((scanner.Current - scanner.Start == start + restLength) && memcmp(scanner.Start + start, rest, restLength) == 0)
    return type;
  return TOKEN_IDENTIFIER;
}

static TokenType IdentifierType() {
  switch (scanner.Start[0]) {
    case 'a': 
      if (scanner.Current - scanner.Start > 1) {
        switch(scanner.Start[1]) {
          case 'n': return CheckKeyword(2, "d", TOKEN_AND);
          case 's': return TOKEN_AS;
          case 'f': return CheckKeyword(2, "ter", TOKEN_AFTER);
        }
      }
    case 'b': return CheckKeyword(1, "reak", TOKEN_BREAK);
    case 'c':
      if (scanner.Current - scanner.Start > 1) {
        switch(scanner.Start[1]) {
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
      if (scanner.Current - scanner.Start > 1) {
        switch(scanner.Start[1]) {
          case 'a': return CheckKeyword(2, "lse", TOKEN_FALSE);
          case 'o': return CheckKeyword(2, "r", TOKEN_FOR);
          case 'u': return CheckKeyword(2, "nction", TOKEN_FUNCTION);
        }
      }
    case 'g': return CheckKeyword(1, "lobal", TOKEN_GLOBAL);
    case 'i': {
      if (scanner.Current - scanner.Start > 1) {
        switch(scanner.Start[1]) {
          case 'f': return TOKEN_IF;
          case 's': return TOKEN_IS;
        }
      }
    }
    case 'm': return CheckKeyword(1, "aybe", TOKEN_MAYBE);
    case 'n': return CheckKeyword(1, "ull", TOKEN_NULL);
    case 'o': return CheckKeyword(1, "r", TOKEN_OR);
    case 'p': return CheckKeyword(1, "rint", TOKEN_PRINT);
    case 'r': return CheckKeyword(1, "eturn", TOKEN_RETURN);
    case 's': if (scanner.Current - scanner.Start > 1) {
        switch(scanner.Start[1]) {
          case 't': return CheckKeyword(1, "atic", TOKEN_STATIC);
          case 'w': return CheckKeyword(2, "itch", TOKEN_SWITCH);
        }
      }
    case 't':
      if (scanner.Current - scanner.Start > 1) {
        switch(scanner.Start[1]) {
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

Token ScannerScanToken() {
  SkipWhitespace();
  scanner.Start = scanner.Current;

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
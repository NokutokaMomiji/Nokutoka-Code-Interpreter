#include <stdio.h>
#include <string.h>

#include "common.h"
#include "scanner.h"

typedef struct {
  const char* Start;
  const char* Current;
  int Line;
} Scanner;

Scanner scanner;

void ScannerInit(const char* source) {
  scanner.Start = source;
  scanner.Current = source;
  scanner.Line = 1;
}
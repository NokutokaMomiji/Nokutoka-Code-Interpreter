#ifndef MOMIJI_UTILITIES_H
#define MOMIJI_UTILITIES_H

#include <math.h>
#include "Common.h"

#define ISINT(num) (floor(num) == num)

bool IsDigit(char digit);
bool IsNumber(const char* number);
bool IsAlphanumeric (char character);

int Min(int a, int b);
int Max(int a, int b);
int Between(int x, int min, int max);
int Clamp(int x, int min, int max);
int Sign(int x);

#endif
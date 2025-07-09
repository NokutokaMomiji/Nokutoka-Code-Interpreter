#include "Utilities.h"

bool IsDigit(char digit) {
    return (digit >= '0' && digit <= '9');
}

bool IsNumber(const char* number) {
    return false;
}

bool IsAlphanumeric (char character) {
    return ((character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') || (character == '_'));
}

int Min(int a, int b) {
    return (a <= b) ? a : b;
}

int Max(int a, int b) {
    return (a >= b) ? a : b;
}

int Between(int x, int min, int max) {
    int trueMin = Min(min, max);
    int trueMax = Max(min, max);
    return (x >= trueMin || x <= trueMax);
}

int Clamp(int x, int min, int max) {
    int trueMin = Min(min, max);
    int trueMax = Max(min, max);
    return (x > trueMax) ? trueMax : ((x < trueMin) ? trueMin : x);
}

int Sign(int x) {
    return (x < 0) ? -1 : ((x > 0) ? 1 : 0);
}
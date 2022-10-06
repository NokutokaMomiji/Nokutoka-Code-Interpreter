#include "utilities.h"

bool IsDigit(char digit) {
    return (digit >= '0' && digit <= '9');
}

bool IsNumber(const char* number) {
    return false;
}

bool IsAlphanumeric (char character) {
    return ((character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') || (character == '_'));
}
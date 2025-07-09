#ifndef MOMIJI_COMMON_H
#define MOMIJI_COMMON_H

#include <stdint.h>     //Includes integers with sepcified widths.
#include <stdbool.h>    //Includes boolean types and values.
#include <stddef.h>     //Includes standard type definitions (and NULL).

#define DEBUG_TRACE_EXECUTION
#define DEBUG_PRINT_CODE
//#define DEBUG_STRESS_GC
#define DEBUG_LOG_GC

#define COLOR_RED     "\x1b[91m"
#define COLOR_CYAN    "\x1b[96m"
#define COLOR_MAGENTA "\x1b[95m"
#define COLOR_GRAY    "\x1b[90m"
#define COLOR_RESET   "\x1b[0m"

#define UINT8_COUNT (UINT8_MAX + 1)
#define UINT16_COUNT (UINT8_MAX + 1)

#endif

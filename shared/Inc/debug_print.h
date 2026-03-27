#ifndef __DEBUG_PRINT_H
#define __DEBUG_PRINT_H

// #define DEBUG

#ifdef DEBUG
#include <inttypes.h>
#include <stdio.h>
#define DBG_PRINT(fmt, ...) printf("%s: " fmt, __func__, ##__VA_ARGS__)
#else
#define DBG_PRINT(fmt, ...) ((void) 0)
#endif

#endif // __DEBUG_PRINT_H
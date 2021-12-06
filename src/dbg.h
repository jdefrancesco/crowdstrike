#ifndef __DBG_H
#define __DBG_H

#include <stdio.h>
#include <stdlib.h>

#ifdef NDEBUG
// No debug messages.
#define dbg_print(msg, ...)
#else
#define dbg_print(msg, ...) fprintf(stderr, RED "[DEBUG]: " RESET msg " :(%s:%s:%d)\n", ##__VA_ARGS__, \
        __FILE__, __LINE__)
#endif

// Flag for turning assertions on or off.
#define ASSERT_ON 1

#if ASSERT_ON
    #define ASSERT(c, m) \
    { \
        if (!(c)) { \
            fprintf(stderr, __FILE__ ":%d: [!] Assertion %s failed: %s\n", \
                    __LINE__, #c,  m); \
            exit(1); \
        } \
    }
#else
    #define ASSERT(c, m) // Empty
#endif

// ANSI color codes. Supported by most terminals.
#define RED     "\x1B[31m"
#define GREEN   "\x1B[32m"
#define YELLOW  "\x1B[33m"
#define BLUE    "\x1B[34m"
#define MAGENTA "\x1B[35m"
#define CYAN    "\x1B[36m"
#define WHITE   "\x1B[37m"
#define RESET   "\x1B[0m"

#endif

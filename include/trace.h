#pragma once
#include <stdio.h>

#ifdef TRACE
    extern int g_trace_enabled;
    #define TRACEF(...) do { \
        if (g_trace_enabled) fprintf(stderr, __VA_ARGS__); \
    } while (0)
#else
    #define TRACEF(...) do {} while (0)
#endif
    
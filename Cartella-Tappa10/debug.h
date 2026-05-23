#ifndef _DEBUG_H
#define _DEBUG_H

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

// add -DTRACE compile flag to enable all non-fatal warning and error messages
#ifdef TRACE
#define _TRACE_PRINT 1
#else
#define _TRACE_PRINT 0
#endif

// set fancy function names with class and signature if we're using gcc
#ifdef __GNUC__
#define _FUNC __PRETTY_FUNCTION__ 
#else
#define _FUNC __func__
#endif

#define print_warning(fmt, ...)                                                 \
    do {                                                                \
        if (_TRACE_PRINT)                                               \
            fprintf(stderr, "Warning. %s:%d: %s(): " fmt, __FILE__,     \
                    __LINE__, _FUNC, ##__VA_ARGS__);                    \
    } while (0)

#define print_error(fmt, ...)                                           \
    do {                                                                \
        if (_TRACE_PRINT)                                               \
                    fprintf(stderr, "Error. %s:%d: %s(): " fmt, __FILE__,       \
                    __LINE__, _FUNC, ##__VA_ARGS__);                    \
        assert (0);                                                     \
    } while (0)

#define print_fatal(exit_value, fmt, ...)                          \
    do {                                                                \
        fprintf(stderr, "Fatal. %s:%d: %s(): " fmt, __FILE__,           \
                __LINE__, _FUNC, ##__VA_ARGS__);                        \
        exit (exit_value);                                              \
    } while (0)

#endif // _DEBUG_H

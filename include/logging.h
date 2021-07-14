//=============================================================================
// This is free and unencumbered software released into the public domain.
//
// Anyone is free to copy, modify, publish, use, compile, sell, or
// distribute this software, either in source code form or as a compiled
// binary, for any purpose, commercial or non-commercial, and by any
// means.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
// OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.
//
// This is a simplified version of UNLICENSE. For more information,
// please refer to <http://unlicense.org/>
//=============================================================================
#ifndef __LOGGING_H_
#define __LOGGING_H_
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * @file     logging.h
 * @date     2019-04-30
 * @author   Martin
 * @brief    Log and debug related utility macros
 * Interface for:
 * - simple debug calls
 * - simple logging
 * Everything is disabled if NDEBUG is defined
 */

/**** Includes ***************************************************************/
#include <stdio.h>
#include <assert.h>

/* Use STMT to implement macros with compound statements */

#if !defined(NDEBUG)
//=============================================================================
// GENERAL LOG STUFF
// FATAL will log and stop the system
// TRACE and ERROR are functionally the same, use them to provide a different
// output, i.e. trace is typically to follow execution, error is typically
// to catch an error condition for later correction.
//
// NOTE:
// To track the system log, open a terminal on the BBB3 and use:
// - tail -f /var/log/syslog
// - tail -f -n 20 /var/log/syslog     (shows only the last 20 lines)
//=============================================================================
#define LOG_TRACE(...) do {                                                   \
    fprintf(stderr, "[TRC]%s ln:%d %s(): ",__FILE__, __LINE__, __func__);     \
    fprintf(stderr, __VA_ARGS__); } while (0)

#define LOG_ERROR(...) do {                                                   \
    fprintf(stderr, "[ERR]%s ln%d %s(): ",__FILE__, __LINE__, __func__);      \
    fprintf(stderr, __VA_ARGS__); } while (0)

#define LOG_FATAL(...) do {                                                   \
    fprintf(stderr, "[FATAL]%s ln%d %s(): ",__FILE__, __LINE__, __func__);    \
    fprintf(stderr, __VA_ARGS__);                                             \
    fflush( stderr );assert(0); } while (0)

//=============================================================================
// GENERAL DEBUG FUNCTIONALITY
//=============================================================================

// Simple macro enhancement for assert
#define ASSERT(cond) do { if (!(cond)) {                                      \
        printf( "ASSERT fail - file:%s,func:%s,ln:%d cond:%s\n",              \
        __FILE__,__func__,__LINE__,#cond );                                   \
        fflush( stdout );assert(0);                                           \
    } } while (0)

// Simple warning macro. Will not cause an assert, but will warn about
// wrong non-fatal conditions
#define WARN(cond) do { if (!(cond)) {                                       \
    printf( "WARNING condition not met - file:%s,func:%s,ln:%d cond:%s\n",   \
    __FILE__,__func__,__LINE__,#cond );                                      \
    } } while (0)

//=============================================================================

#else
    #define LOG_TRACE(msg, ...) ((void)0)
    #define LOG_ERROR(msg, ...) ((void)0)
    #define LOG_FATAL(msg, ...) ((void)0)
    #define ASSERT(cond)        ((void)0)
    #define WARN(cond)          ((void)0)
#endif // defined (NDEBUG)

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* __LOGGING_H_ */

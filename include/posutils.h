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
#ifndef _POSUTILS_H_
#define _POSUTILS_H_
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * \file     posutils.h
 * \date     2019-04-30
 * \author   Martin
 */

/**
 * \defgroup GENUTILS Generic utilities
 *
 * \brief
 * This is a collection of generic utilities. These are used inside the Kauai library.
 * The integrator is free to use them or to ignore them. There is \b NO \b REQUIREMENT to use them
 * but they may simplify certain things. Included are:
 * - pthread creation using fixed stack sizes
 * - mutex creation
 * - some enhanced assertion and warning macros
 * - a single linked list utility
 */

/**
 * \defgroup POSUTILS Posix utilities
 * \ingroup GENUTILS
 *
 * \brief
 * Some simple Posix utilities. Interface for:
 * - Thread creation
 * - Mutex creation
 */

/**** Includes ***************************************************************/
#include <pthread.h>
#include <errno.h>
#include "putimer.h"

/**** Definitions ************************************************************/

/**
 * \brief   Init all the posix utilities
 *
 * \retval  0 for success
 * \retval  Non-zero for failure
 *
 * \post    The library is initialised
 *
 * \par Side Effects
 * None
 *
 * \par Description
 * This is an idempotent call, it can be invoked multiple times. Only the first invocation will take effect
 */
#define POSUTILS_INIT {pu_thread_init(); putimer_init();}

/**
 * \brief   Exit the library
 *
 * \retval  0 for success
 * \retval  Non-zero for failure
 *
 * \pre     Any threads must be exited
 * \post    The library is cleaned up
 *
 * \par Side Effects
 * May exit all
 *
 * \par Description
 * This is an idempotent call, it can be invoked multiple times. Only the first invocation will take effect
 */
#define POSUTILS_EXIT {putimer_exit(); pu_thread_exit();}


/*===========================================================================*/
/* POSIX MUTEX FUNCTIONS                                                     */
/*===========================================================================*/
/**
 * \defgroup PMTX Posix mutex utility
 * \ingroup  POSUTILS
 *
 * \brief
 * This is a factory that creates standard pthread mutexes. It hides the complexity
 * of the creation from the user.
 *
 * \section pmtx_sect_1 Fast mutexes
 * The mutex may be created as a simple mutex for. This is the 99% case. These mutexes a used for
 * small fast critical section implementation. Semaphores should \b NOT be used for this!.
 *
 * \section pmtx_sect_2 Error mutexes
 * These are used in scenarios where it is important to know where and when a deadlock occurs. A macro is provided
 * trap deadlocks (\ref PU_MUTEX_LOCK_ERROR). This macro throws a fatal log if a deadlock is detected.
 *
 * \section pmtx_sect_4 Mutex usage
 * The factory simply creates a standard Posix pthread mutex with some constraints. All the
 * mutex calls may be used as normal, i.e.:
 * - \c pthread_mutex_lock( pthread_mutex_t* )
 * - \c pthread_mutex_unlock( pthread_mutex_t* )
 * - \c pthread_mutex_trylock( pthread_mutex_t* )
 * .
 *
 * \{
 */

/**
 * \brief Mutex types supported
 * The recommendation is to ALWAYS use the fast (default)
 *
 * \note
 * The error mutex is typically used to trap deadlocks. Use it sparingly and to trap specific debug
 * scenarios.
 */
typedef enum
{
    PU_MUTEX_TYPE_FAST,         /*!< Default (timed) fast mutex       */
    PU_MUTEX_TYPE_ERROR,        /*!< Error mutex, to trap deadlocks   */
    PU_MUTEX_TYPE_ENDDEF        /* Enum terminator                    */
}   pu_mutex_type;

/**
 * \brief   Creates (initialises) a mutex of the specified type with the constraints applied
 *
 * \param[in] pMtx   : Pointer to a valid mutex
 * \param[in] enType : Mutex type
 * \retval  0 for success
 * \retval  Non-zero for failure
 *
 * \pre     The mutex pointer is non-null
 * \post    The mutex is initialised
 *
 * \par Side Effects
 * None
 *
 * \par Description
 * Creates a standard Posix pthread mutex. An example is shown below:
 * \code
 * int              iResult;
 * pthread_mutex_t* pMtx = malloc( sizeof(pthread_mutex_t) );
 * ASSERT( pMtx );
 * if (pMtx)
 * {
 *     // create a fast mutex
 *     iResult = pu_mutex_create_type( pMtx, PU_MUTEX_TYPE_FAST );
 *     ASSERT( 0 == iResult );
 *     if (0 == iResult)
 *     {
 *         // do lots of useful things..
 *     }
 * }
 * \endcode
 */
int pu_mutex_create_type(
    pthread_mutex_t* pMtx,
    pu_mutex_type    enType );

/**
 * \param pMtx: pointer to a mutex
 *
 * \par Description
 * Locks an error checking mutex, and checks for the deadlock condition.
 *
 * \note
 * \b ONLY for use with the \ref PU_MUTEX_TYPE_ERROR mutex type
 */
#define PU_MUTEX_LOCK_ERROR(pMtx) {if (EDEADLK == pthread_mutex_lock( pMtx )) {LOG_FATAL( "MUTEX DEADLOCK (0x%zu)\n", (size_t)(pMtx) );}}

/**
 * \param pMtx: pointer to a mutex
 *
 * \par Description
 * Unlocks an error checking mutex, and checks for the case where the caller is not the "owner",
 * i.e. the owner did not originally lock the mutex. This is an unlikely scenario as mutexes are
 * normally (in well written code) hidden inside interfaces. It is included for completeness simply
 * because the error checking mutex type can detect this scenario.
 *
 * \note
 * \b ONLY for use with the \ref PU_MUTEX_TYPE_ERROR mutex type
 */
#define PU_MUTEX_UNLOCK_ERROR(pMtx) {if (EPERM == pthread_mutex_unlock( pMtx )) {LOG_FATAL("CANT UNLOCK MTX, NOT OWNER (0x%08x)\n", (size_t)(pMtx) );}}

/**
 * \}
 */

/*===========================================================================*/
/* POSIX THREAD FUNCTIONS                                                    */
/*===========================================================================*/
/**
 * \brief Posix thread utilities
 * \defgroup PTHREAD Posix thread utilities
 * \ingroup  POSUTILS
 * This is a factory that produces Posix pthreads. It handles the multiple creation
 * steps internally, and constrains various options, like:
 * - scheduling scheme (SCHED_OTHER)
 * - fixed stack sizes
 * - guards for dumb systems (like uClibc)
 * .
 *
 * \par Pthread usage
 * The factory creates a standard Posix pthread with some constraints. All the
 * thread calls (\c pthread_xxx()) may be used as normal.
 *
 * \par Thread lists
 * The factory keeps (on a per process basis) a list of all the threads. As they are created
 * they are added to the list, as they exit they are moved from the list. This list can
 * iterated through. This allows for things like debug and graceful shutdown.
 *
 * \{
 */

/**
 * \brief   The standard function type for a pthread entry point (main)
 *
 * \param[in] pArg : Argument
 * \retval  Exit value
 *
 * \pre     None
 * \pre     None
 *
 * \par Side Effects
 * None
 *
 * \par Description
 * The function type for a pthread entry point (main)
 */
typedef void* (*pu_thread_fct_t)( void* pArg );

/**
 * \brief   Creates a non-RT pthread with the constraints applied
 *
 * \param[in] fctMain     : Thread main function (entry point)
 * \param[in] pMainArg    : Argument for main
 * \param[in] uiStackSize : Stack size
 * \param[in] szName      : Thread name
 * \retval  A non-zero pthread ID indicates success
 * \retval  A zero pthread ID means failure
 *
 * \pre     The entry function (main) is non-NULL
 * \pre     The name is non-NULL
 * \post    The pthread is created.
 *
 * \par Description
 * Creates a standard pthread (SCHED_OTHER) with the passed in parameters. The stack size will be
 * rounded up to the nearest multiple of the page size, i.e. (n * 4k).
 *
 * \par Exit handling
 * The are two main ways a thread can exit. It can terminate on its own either by simply coming to
 * the end of the main loop or invoking pthread_exit(). In this case the thread is aware it is going to exit,
 * and will naturally clean up after itself. The second scenario is when the thread is terminated
 * by another thread, i.e. as a result of a call to pthread_cancel(pid). Is this case the exit handler
 * will be invoked. This allows us to do clean up during an on-demand process shutdown.
 *
 * \par Note
 * If there is no need for an exit handler, then simply set the \c fctExit parameter to NULL.
 *
 * \par Note
 * The thread name (szName) \b MUST be persistent and null terminated.. In the code the \b pointer to the
 * name will simply be copied, the string itself will not be copied (i.e. there will be no strcpy call).
 * If you use the provided macros this will not be an issue, as the entry function is string-ised into a
 * constant persistent string.
 */
pthread_t pu_thread_create(
    pu_thread_fct_t fctMain,
    void*           pMainArg,
    size_t          uiStackSize,
    const char*     szName );

/**
 * \brief   Creates an automatically named pthread with exit handler
 *
 * \param[in] mainfct_    : Thread main function (entry point)
 * \param[in] mainarg_    : Main argument
 * \param[in] exitfct_    : Thread exit handler function
 * \param[in] exitarg_    : Exit handler argument
 * \param[in] stack_size_ : Stack size
 * \retval  A non-zero pthread ID indicates success
 * \retval  A zero pthread ID means failure
 *
 * \pre     The entry function (main) is non-NULL
 * \post    The pthread is created.
 *
 * \par Description
 * This is a helper macro that creates an automatically named non-RT pthread. The name is simply
 * a string-ised form of the main entry function. All of the constraints of the underlying
 * thread creation function still apply.
 * \see pu_thread_create
 */
#define PU_THREAD_CREATE(mainfct_,mainarg_,stack_size_)    \
    pu_thread_create(                                      \
        (mainfct_),(mainarg_),                             \
        (stack_size_),                                     \
        #mainfct_)

/**
 * \brief   Init all the the thread logic
 *
 * \retval  0 for success
 * \retval  Non-zero for failure
 *
 * \post    The library is initialised
 *
 * \par Side Effects
 * None
 *
 * \par Description
 * This is an idempotent call, it can be invoked multiple times. Only the first invocation will take effect
 */
int pu_thread_init( void );

/**
 * \brief   Exit the logic
 *
 * \retval  0 for success
 * \retval  Non-zero for failure
 *
 * \pre     Any threads must be exited
 * \post    The library is cleaned up
 *
 * \par Description
 * This is an idempotent call, it can be invoked multiple times. Only the first invocation will take effect
 */
int pu_thread_exit( void );

/**
 * \}
 */

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* _POSUTILS_H_ */

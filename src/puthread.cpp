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

/**
 * @file     puthread.c
 * @brief    Implementation of the pthread utilities
 */

/**** Includes ***************************************************************/
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <malloc.h>
#include <assert.h>
#include <errno.h>
#include <syscall.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sched.h>
#include <limits.h>
#include <atomic>

#include "posutils.h"
#include "logging.h"

/**** Definitions ************************************************************/
#if defined (PUTHREAD_DEBUGGING)
    #define PUTHREAD_DEBUG LOG_TRACE
#else
    #define PUTHREAD_DEBUG(...)
#endif

/* Per thread context structure */
typedef struct pu_thread_context_tag
{
    pu_thread_fct_t fctMain;                 /* Thread main function (entry point) */
    void*           pMainArg;                /* Main argument                      */
    pthread_t       pid;                     /* Posix thread ID                    */
    pid_t           tid;                     /* Linux thread ID                    */
    const char*     szName;                  /* Thread name                        */
}   pu_thread_context_t;

#if defined(PUTHREAD_DEBUGGING)
    #include <vector>
    static std::vector <pu_thread_context_t*> vecCtxt;
#endif // defined(PUTHREAD_DEBUGGING)

#define PU_THREAD_STUPID_STACKSIZE (1024*1024)

/**** Macros ****************************************************************/

/**** Static declarations ***************************************************/
static std::atomic<int>     iIsInit{ 0 };
static pthread_mutex_t      mtxLock;
static size_t               uiPageSize   = 0;
static size_t               uiThreadCount = 0;

/**** Local function prototypes (NB Use static modifier) ********************/
static size_t               pu_thread_stacksize_fix( size_t uiStackSize );
static void*                pu_thread_entry_handler( void* pArg );
static void                 pu_thread_exit_handler( void* pArg );

/****************************************************************************/
/* LOCAL FUNCTION DEFINITIONS                                               */
/****************************************************************************/
static inline bool pu_thread_first_init( void ) {
    int iExpectedVal = 0;
    int iDesiredVal  = 1;
    return (iIsInit.compare_exchange_strong(iExpectedVal, iDesiredVal));
}
// pu_thread_first_init

static inline bool pu_thread_first_exit( void ) {
    int iExpectedVal = 1;
    int iDesiredVal  = 0;
    return (iIsInit.compare_exchange_strong(iExpectedVal, iDesiredVal));
}
// pu_thread_first_exit

static inline size_t pu_thread_stacksize_fix( size_t uiStackSize )
{
    size_t uiNewSize = uiStackSize;

    /* Less than the PTHREAD minimum, set size to the minimum + a guard page */
    if (uiNewSize < PTHREAD_STACK_MIN)
    {
        uiNewSize = (PTHREAD_STACK_MIN + uiPageSize);
    }

    /* Greater than or equal to the PTHREAD minimum
     * We round the stack size up to the next page boundary.
     * Also, we want a guard page. This comes out of the stack size, so we have to add
     * an extra page to account for it.
     * So we need to round up to the nearest TWO pages...
     */
    else
    {
        uiNewSize += ((2 * uiPageSize) - 1);
        uiNewSize /= uiPageSize;
        uiNewSize *= uiPageSize;
    }

    /* Done */
    return (uiNewSize);
}
/* pu_thread_stacksize_fix */

static void* pu_thread_entry_handler( void* pArg )
{
    pu_thread_context_t* pNode = (pu_thread_context_t*)pArg;
    void*                pReturn;

    /* Get the system thread ID */
    pNode->tid = (pid_t)syscall( SYS_gettid );

    /* Trace thread creation */
    PUTHREAD_DEBUG(
        "PU_THREAD(create): thrd=%s, tid=%d\n",
        pNode->szName,
        (int)pNode->tid );

    /* register the exit handler */
    pthread_cleanup_push( pu_thread_exit_handler, pNode );

    /* Thread real main entry */  
    pReturn = pNode->fctMain( pNode->pMainArg );
    pthread_cleanup_pop( 1 );
    
    /* pass back the actual thread return value */
    return (pReturn);
}
/* pu_thread_entry_handler */

static void pu_thread_exit_handler( void* pArg )
{
    pu_thread_context_t* pNode = (pu_thread_context_t*)pArg;
    ASSERT( pNode );
    pthread_mutex_lock( &mtxLock );
    PUTHREAD_DEBUG( "PU_THREAD(exit_handler): thrd=%s\n", pNode->szName );
#if defined(PUTHREAD_DEBUGGING)
    for (auto it = vecCtxt.begin(); it != vecCtxt.end(); ) {
        if (*it == pNode) {
            vecCtxt.erase(it);
            break;
        } else {
            ++it;
        }
    }
#endif // defined(PUTHREAD_DEBUGGING)
    free( pNode );
    pNode = NULL;
    ASSERT(uiThreadCount > 0);
    uiThreadCount--;
    pthread_mutex_unlock( &mtxLock );
}
/* pu_thread_exit_handler */

/****************************************************************************/
/* PUBLIC FUNCTION DEFINITIONS                                              */
/****************************************************************************/

/**
 * @brief   Creates a non-RT pthread with the
 *
 * @param[in] fctMain     : Thread main function (entry point)
 * @param[in] pMainArg    : Argument for main
 * @param[in] fctExit     : Thread exit handler function
 * @param[in] pExitArg    : Exit handler argument
 * @param[in] uiStackSize : Stack size
 * @param[in] szName      : Thread name
 * @retval  A non-zero pthread ID indicates success
 * @retval  A zero pthread ID means failure
 *
 * @pre     The entry function (main) is non-NULL
 * @pre     The name is non-NULL
 * @post    The pthread is created.
 *
 * @par Description
 * Creates a standard pthread (SCHED_OTHER) with the passed in parameters. The stack size will be
 * rounded up to the nearest multiple of the page size, i.e. (n * 4k).
 *
 * @par Exit handling
 * The are two main ways a thread can exit. It can terminate on its own either by simply coming to
 * the end of the main loop or invoking pthread_exit(). In this case the thread is aware it is going to exit,
 * and will naturally clean up after itself. The second scenario is when the thread is terminated
 * by another thread, i.e. as a result of a call to pthread_cancel(pid). Is this case the exit handler
 * will be invoked. This allows us to do clean up during an on-demand process shutdown.
 *
 * @par Note
 * If there is no need for an exit handler, then simply set the \c fctExit parameter to NULL.
 *
 * @par Note
 * The thread name (szName) \b MUST be persistent and null terminated.. In the code the \b pointer to the
 * name will simply be copied, the string itself will not be copied (i.e. there will be no strcpy call).
 * If you use the provided macros this will not be an issue, as the entry function is string-ised into a
 * constant persistent string.
 */
pthread_t pu_thread_create(
    pu_thread_fct_t fctMain,
    void*           pMainArg,
    size_t          uiStackSize,
    const char*     szName )
{
    pthread_attr_t       attr;
    int                  iResult = -1;
    pu_thread_context_t* pNode = NULL;
    pthread_t            iPid = (pthread_t)0;

    // Self init if not already
    if (!iIsInit) {
        iResult = pu_thread_init();
        ASSERT( 0 == iResult );
        if (0 != iResult) {
            return ((pthread_t)0);
        }
    }

    /* pre-condition */
    ASSERT( uiPageSize > 0 );
    ASSERT( fctMain );
    ASSERT( szName );
    ASSERT( uiStackSize <= PU_THREAD_STUPID_STACKSIZE);
    if ((uiPageSize > 0) && fctMain && szName && (uiStackSize <= PU_THREAD_STUPID_STACKSIZE))
    {
        iResult = pthread_attr_init( &attr );
        ASSERT( 0 == iResult );
        if (0 == iResult)
        {
            /* set stack size and guard size */
            uiStackSize = pu_thread_stacksize_fix( uiStackSize );
            iResult = pthread_attr_setstacksize( &attr, uiStackSize );
            ASSERT( 0 == iResult );

            /* GLIBC will by default set the guard size to one page whenever we set the stack size.
             * uClibC does NOT do this, so we always force the guard size
             */
            if (0 == iResult)
            {
                iResult = pthread_attr_setguardsize( &attr, uiPageSize );
                ASSERT( 0 == iResult );
            }
            if (0 == iResult)
            {
                pNode = (pu_thread_context_t*)malloc( sizeof( pu_thread_context_t ) );
                ASSERT( NULL != pNode );
                if (NULL != pNode)
                {
                    memset( pNode, 0, sizeof(pu_thread_context_t) );
                    pNode->fctMain  = fctMain;
                    pNode->pMainArg = pMainArg;

                    // simply copy the name pointer. This is constant and persistent,
                    // it does not need a separate allocation
                    pNode->szName = szName;
                    pthread_mutex_lock( &mtxLock );
                    iResult = pthread_create(
                        &(pNode->pid),
                        &attr,
                        pu_thread_entry_handler,
                        (void*)pNode );
                    ASSERT( 0 == iResult );
                    if (0 != iResult)
                    {
                        free( pNode );
                    }

                    // Store the PID, set the system thread name. This name is 15+null long,
                    // so will often cause the input name to be truncated. This means the debug name and
                    // the name in the system may be different.
                    else
                    {
                        iPid = pNode->pid;
                        char szSysName[16];
                        strncpy( szSysName, szName, 16 );
                        szSysName[15] = 0;
                        pthread_setname_np( iPid, szSysName );
                        uiThreadCount++;
#if defined(PUTHREAD_DEBUGGING)
                        vecCtxt.push_back( pNode );
#endif // defined(PUTHREAD_DEBUGGING)
                    }
                    pthread_mutex_unlock( &mtxLock );
                }
            }
            pthread_attr_destroy( &attr );
        }
    }
    if (0 == iPid)
    {
        LOG_ERROR( "PU_THREAD(create): cannot create %s\n", szName );
        ASSERT(0 != iPid);
    }

    /* Done */
    return (iPid);
}
/* pu_thread_create */

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
int pu_thread_init( void ) {
    int      iResult = 0;
    intptr_t iPageSize;

    // Atomic compare and exchange makes the init thread safe and idempotent
    PUTHREAD_DEBUG("PU_THREAD(init)\n");
    if (pu_thread_first_init()) {
        PUTHREAD_DEBUG("PU_THREAD(init): first idempotent init\n");

        // Check the page size
        if (uiPageSize == 0) {
            iPageSize = sysconf( _SC_PAGESIZE );
            ASSERT( iPageSize >= 1024 );
            if (iPageSize >= 1024) {
                /* Store for posterity.. */
                uiPageSize = (size_t)iPageSize;

                /* Initialise the mutex */
                iResult = pu_mutex_create_type( &mtxLock, PU_MUTEX_TYPE_FAST );
                ASSERT( 0 == iResult );
            }
        }

        // error? Uninit..
        if ((0 != iResult) || (uiPageSize < 1024)) {
            uiPageSize = 0;
            pu_thread_first_exit();
            iResult = -1;
        }
    }

    // Done
    return (iResult);
}
// pu_thread_init

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
int pu_thread_exit( void ) {
    PUTHREAD_DEBUG("PU_THREAD(exit)\n");
    if (pu_thread_first_exit()) {
        PUTHREAD_DEBUG("PU_THREAD(exit): first idempotent exit\n");

        // In debug mode warn if there are threads outstanding
        // not much to do in NDEBUG. Better hope like hell the process cleans up after you!!!
        PUTHREAD_DEBUG("PU_THREAD(exit): remaining threads = %ld\n", uiThreadCount);
        WARN( uiThreadCount == 0 );

#if defined(PUTHREAD_DEBUGGING)
        for (auto it = vecCtxt.begin(); it != vecCtxt.end(); ) {
            PUTHREAD_DEBUG("PU_THREAD(exit): thread remnant = %s\n", (*it)->szName );
            ++it;
        }
#endif // defined(PUTHREAD_DEBUGGING)

        pthread_mutex_destroy( &mtxLock );
        uiPageSize = 0;
    }
    return (0);
}
// pu_thread_exit


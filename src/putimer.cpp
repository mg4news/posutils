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
 * @file     putimer.cpp
 * @brief    Implementation of a timer utility built on posix condition variables
 */

/**** Includes ***************************************************************/
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "putimer.h"
#include "posutils.h"
#include "logging.h"

/**** Definitions ************************************************************/
#if defined (PUTIMER_DEBUGGING)
    #define PUTIMER_DEBUG LOG_TRACE
#else
    #define PUTIMER_DEBUG(...)
#endif

/* Handle macros */
#define PUTIMER_HND_CREATE(idx,tag) (putimer_hnd_t)((((size_t)(idx)) << 16) + tag)
#define PUTIMER_HND_GET_IDX(hnd)    (uint16_t)(((size_t)(hnd)) >> 16)
#define PUTIMER_HND_GET_TAG(hnd)    (uint16_t)(((size_t)(hnd)) &  0x000ffff)

/**
 * A simple limit is imposed for a few reasons. If the system is using too many
 * timer resources then it is possible the design needs to be re-evaluated.
 * Also, if a process is allocating but not freeing, then the resources will
 * "creep" up to the limit. This allows us to catch the issue sooner.
 */
#define PUTIMER_RES_UNITS       (32) /* NEVER EVER CHANGE */
#define PUTIMER_RES_MULTIPLIER  (4)  /* This can change   */
#define PUTIMER_MAX_RESOURCES   (PUTIMER_RES_UNITS*PUTIMER_RES_MULTIPLIER)

/**
 * Event flag used to indicate that either a timer has been added, removed or updated,
 * and that the timer queue head (i.e. next timer to wake up) may need updating
 */
#define PUTIMER_EVT_NEW_HEAD  (0x00000001)

/**
 * Timer state..
 */
typedef enum
{
    PUTIMER_STATE_IDLE,     /*!< Not active                                  */
    PUTIMER_STATE_WAITING,  /*!< Active, in timer queue, waiting for timeout */
    PUTIMER_STATE_FIRED,    /*!< Active, not in queue, waiting to call back  */
    PUTIMER_STATE_ENDDEF    /* terminator */
}   putimer_state_t;

/**
 * Timer context structure
 */
typedef struct putimer_tmr_tag
{
    /* Identification and validation.
     * We use an unsigned for the ID as it saves two op-codes per range check,
     * i.e. if signed it would be: (i >= 0) && (i < size)
     *      unsigned is          : (u < size)
     */
    uint16_t                usID;
    uint16_t                usTag;

    /* Timer data */
    putimer_callback_fct_t  pFct;
    size_t                  uiPeriodMs;
    struct timespec         tsEnd;
    putimer_type_t          enType;
    putimer_state_t         enState;
    int                     iUseAbsTime;
    void*                   pCookie;
    bool                   bLockable;

    /* Pointer for list */
    struct putimer_tmr_tag* pNext;
}   putimer_tmr_t;

/**** Macros ****************************************************************/

/**** Static declarations ***************************************************/
static pthread_mutex_t  mtxLock;
static putimer_tmr_t*   pQueue      = nullptr;
static int              iIsInit     = 0;
static int              iKillThread = 0;
static uint32_t         pTimerId[PUTIMER_RES_MULTIPLIER];
static putimer_tmr_t*   pCallList[PUTIMER_MAX_RESOURCES];
static uint16_t         usRollingTag = 0;
static pthread_t        pidTmrThread;
static pthread_mutex_t  mtxWake;
static pthread_cond_t   cndWake;
static size_t           uiAllocatedTimers = 0;
static putimer_tmr_t    pTimerList[PUTIMER_MAX_RESOURCES];

/**** Local function prototypes (NB Use static modifier) ********************/
static uint16_t putimer_alloc_id( void );
static void     putimer_free_id( uint16_t usId );
static void*    putimer_thread( void* pArg );
static int      putimer_remove(
    putimer_tmr_t* pTmr,
    int*           pWasActive,
    size_t*        pRemainingMs );
static int      putimer_add( putimer_tmr_t* pTmr );
putimer_hnd_t      putimer_create_local(
    putimer_type_t         enType,
    putimer_callback_fct_t fctCallback,
    size_t                 uiPeriodMs,
    void*                  pCookie,
    bool                  bLockable );

/****************************************************************************/
/* TIMESPEC FUNCTION DEFINITIONS                                            */
/****************************************************************************/

/* is A after B */
int timespec_is_a_after_b(
    struct timespec* pA,
    struct timespec* pB )
{
    /* test seconds first - easy */
    int iIsAfter = (pA->tv_sec > pB->tv_sec) ? 1 : 0;

    /* test case where seconds are the same */
    if ((!iIsAfter) && (pA->tv_sec == pB->tv_sec))
    {
        iIsAfter = (pA->tv_nsec > pB->tv_nsec) ? 1 : 0;
    }

    /* all cases covered */
    return (iIsAfter);
}
/* timespec_is_a_after_b */

/* subtract b from a (a is definitely after b) */
void timespec_a_sub_b(
    struct timespec* pA,
    struct timespec* pB,
    struct timespec* pRes )
{
    pRes->tv_sec = pA->tv_sec - pB->tv_sec;
    if (pA->tv_nsec >= pB->tv_nsec)
    {
        pRes->tv_nsec = (pA->tv_nsec - pB->tv_nsec);
    }
    else
    {
        pRes->tv_nsec = (1000000000 - pB->tv_nsec) + pA->tv_nsec;
        pRes->tv_sec -= 1;
    }
}
/* timespec_a_sub_b */

/* Difference between A and B in milliseconds */
size_t timespec_a_sub_b_ms(
    struct timespec* pA,
    struct timespec* pB )
{
    struct timespec tsDiff;
    size_t          lMs;
    timespec_a_sub_b( pA, pB, &tsDiff );
    lMs  = (size_t)(tsDiff.tv_sec * 1000);
    lMs += (size_t)(tsDiff.tv_nsec / 1000000);
    return (lMs);
}
/* timespec_a_sub_b_ms */

/* Difference between A and B in microseconds */
size_t timespec_a_sub_b_us(
    struct timespec* pA,
    struct timespec* pB )
{
    struct timespec tsDiff;
    size_t          lUs;
    timespec_a_sub_b( pA, pB, &tsDiff );
    lUs  = (size_t)(tsDiff.tv_sec * 1000000);
    lUs += (size_t)(tsDiff.tv_nsec / 1000);
    return (lUs);

}
/* timespec_a_sub_b_us */

/* subtract b from a (a is definitely after b) */
void timespec_add_ms( struct timespec* pTs, size_t ulMs )
{
    long lNs;

    pTs->tv_sec += (time_t)(ulMs / 1000);
    ulMs -= ((ulMs / 1000) * 1000);
    lNs = (long)(ulMs * 1000000);
    pTs->tv_nsec += lNs;
    if (pTs->tv_nsec > 1000000000)
    {
        pTs->tv_sec += 1;
        pTs->tv_nsec -= 1000000000;
    }
}
/* timespec_add_ms */

/**
 * @brief  Create a timespec = ("now" + ms) - using CLOCK_REALTIME
 *
 * @param[out] pTs : Pointer to timespec
 * @param[in]  lMs : Millisecond value
 *
 * @pre     none
 * @post    none
 *
 * @par Description
 * Populates the time spec with the current ("now") time plus the specified
 * number of milliseconds. This is typically used in "wait until" timeout calculations.
 *
 * @note
 * Uses CLOCK_REALTIME
 */
void timespec_now_plus_ms(
    struct timespec* pTs,
    size_t           ulMs )
{
    clock_gettime( CLOCK_REALTIME, pTs );
    timespec_add_ms( pTs, ulMs );
}
/* timespec_now_plus_ms */

/**
 * @brief  Create a timespec = ("now" + ms) - using CLOCK_MONOTONIC
 *
 * @param[out] pTs : Pointer to timespec
 * @param[in]  lMs : Millisecond value
 *
 * @pre     none
 * @post    none
 *
 * @par Description
 * Populates the time spec with the current ("now") time plus the specified
 * number of milliseconds. This is typically used in "wait until" timeout calculations.
 *
 * @note
 * Uses CLOCK_MONOTONIC
 */
void timespec_now_plus_ms_monotonic(
    struct timespec* pTs,
    size_t           ulMs )
{
    clock_gettime( CLOCK_MONOTONIC, pTs );
    timespec_add_ms( pTs, ulMs );
}
/* timespec_now_plus_ms_monotonic */

/**
 * @brief Converts a timespec clock base
 * @param[in] pTs : Pointer to timespec (using CLOCK_REALTIME)
 *
 * @pre     none
 * @post    none
 *
 * @par Description
 * Takes a timespec generated against CLOCK_REALTIME and converts it to a value
 * based on CLOCK_MONOTONIC.
 */
void timespec_realtime_to_monotonic( struct timespec* pTs )
{
    struct timespec stMT;
    struct timespec stRT;
    struct timespec stDiff;

    /* Get current time from both clocks */
    clock_gettime( CLOCK_MONOTONIC, &stMT );
    clock_gettime( CLOCK_REALTIME,  &stRT );
    if (timespec_is_a_after_b( &stMT, &stRT ))
    {
        timespec_a_sub_b( &stMT, &stRT, &stDiff );
        pTs->tv_sec  += stDiff.tv_sec;
        pTs->tv_nsec += stDiff.tv_nsec;
        if (pTs->tv_nsec > 1000000000)
        {
            pTs->tv_sec += 1;
            pTs->tv_nsec -= 1000000000;
        }
    }
    else
    {
        timespec_a_sub_b( &stRT, &stMT, &stDiff );
        pTs->tv_nsec -= stDiff.tv_nsec;
        if (pTs->tv_nsec < 0)
        {
            pTs->tv_nsec = 1000000000 + pTs->tv_nsec;
            pTs->tv_sec -= 1;
        }
        pTs->tv_sec  -= stDiff.tv_sec;
    }
}
/* timespec_realtime_to_monotonic */

/****************************************************************************/
/* LOCAL FUNCTION DEFINITIONS                                               */
/****************************************************************************/

/**
 * putimer_alloc_id
 *
 * param   void argument
 * retval  ID, or 0xffff;
 *
 * pre     none
 * post    none
 *
 * Description
 * Searches for a clear (0) bit in the ID array
 * This never fails since the preceding logic checks for availability.
 * But. in debug mode, we add an assert to check the logic
 */
uint16_t putimer_alloc_id( void )
{
    int      i;
    int      j;
    uint32_t uiMask;

    for (i = 0; i < PUTIMER_RES_MULTIPLIER; i++)
    {
        if (0xffffffff != pTimerId[i])
        {
            uiMask = (uint32_t)1;
            for (j = 0; j < 32; j++)
            {
                if (0 == (pTimerId[i] & uiMask))
                {
                    PUTIMER_DEBUG(
                        "ID=%d mask[%d]=0x%08x mask=0x%08x\n",
                        (j + (i * 32)),
                        i,
                        pTimerId[i],
                        uiMask );
                    pTimerId[i] |= uiMask;
                    return (uint16_t)(j + (i * 32));
                }
                uiMask <<= 1;
            }
        }
    }
    ASSERT( 0 );
    return (0xffff);
}
/* putimer_alloc_id */

/**
 * putimer_free_id
 *
 * param   void argument
 * retval  ID
 *
 * pre     none
 * post    none
 *
 * Description
 * Clears the corresponding bit in the bit array.
 */
void putimer_free_id( uint16_t usId )
{
    int     i;
    int     j;
    uint32_t uiMask;

    ASSERT( usId < PUTIMER_MAX_RESOURCES );
    i = (int)(usId / 32);
    j = (int)(usId - (i * 32));
    uiMask = 1;
    uiMask <<= j;
    pTimerId[i] &= ~(uiMask);
}
/* putimer_free_id */

/**
 * putimer_thread
 *
 * param   void argument
 * retval  none
 *
 * pre     none
 * post    none
 *
 * Description
 * manages the timeouts
 */
void* putimer_thread( void* pArg )
{
    struct timespec        tsWaitTill;
    struct timespec        tsNow;
    putimer_tmr_t*         pPrev;
    putimer_tmr_t*         pCurr;
    size_t                 uiToCall;
    size_t                 uiCalled;
    putimer_callback_fct_t pFct;
    void*                  pCookie;

    // Kill compiler warning
    (void)pArg;

    /* Do while thread is alive */
    while (!iKillThread)
    {
        /* Zero the count */
        uiToCall = 0;

        /* Take the wake lock - all queue head modification is done under this lock 
         * check for next to expire, calculate the time to sleep 
         */
        pthread_mutex_lock( &mtxWake );
        if (pQueue)
        {
            clock_gettime( CLOCK_MONOTONIC, &tsNow );
            if (timespec_is_a_after_b( &(pQueue->tsEnd), &tsNow ))
            {
                tsWaitTill = pQueue->tsEnd;
            }
            else
            {
                tsWaitTill = tsNow;
            }
            pthread_cond_timedwait( &cndWake, &mtxWake, &tsWaitTill );
        }
        else
        {
            pthread_cond_wait( &cndWake, &mtxWake );
        }

        /* We have the condition and the wake mutex, parse and modify pQueue under this mutex */
        if (!iKillThread)
        {
            /* Get the current time, then check for expiration, get the last expired timer.
             * Timers are always in order. As we find expired timers add them to the call list.
             * We remove each element from the LL and keep the pointer in an array. That way
             * we completely ignore any changes to the "next" pointer. So:
             * - get the current time
             * - mark/extract all the expired timers
             * - move the queue head to the first unexpired  timer (might be null)
             */
            clock_gettime( CLOCK_MONOTONIC, &tsNow );
            for (
                pPrev = nullptr, pCurr = pQueue, uiToCall = 0;
                (pCurr && (!timespec_is_a_after_b( &(pCurr->tsEnd), &tsNow )));
                pCurr = pCurr->pNext)
            {
                /* Move the state to "fired", and add to call list */
                pCurr->enState = PUTIMER_STATE_FIRED;
                pCallList[uiToCall++] = pCurr;
                pPrev = pCurr;
            }
            if (pPrev)
            {
                pPrev->pNext = nullptr;
                pQueue       = pCurr;
            }
        }

        /* Now release the wake lock */
        pthread_mutex_unlock( &mtxWake );

        /* thread not killed, and there are timers to notify (uiToCall > 0) */
        if ((!iKillThread) && (uiToCall > 0))
        {
            /* Lock, check for deadlock */
            PU_MUTEX_LOCK_ERROR( &mtxLock );

            /* Manage the callbacks */
            for (uiCalled = 0; uiCalled < uiToCall; uiCalled++)
            {
                /* Only call it if it has NOT been either:
                 * - stopped
                 * - rescheduled
                 * - deleted (tag is 0)
                 * So, the state must still be "fired", and the tag should be non-zero
                 */
                if ((PUTIMER_STATE_FIRED == pCallList[uiCalled]->enState) &&
                    ((pCallList[uiCalled])->usTag > 0))
                {
                    /* Always move state to idle, restart periodic */
                    pCallList[uiCalled]->enState = PUTIMER_STATE_IDLE;
                    if (PUTIMER_TYPE_PERIODIC == (pCallList[uiCalled])->enType)
                    {
                        putimer_add( pCallList[uiCalled] );
                    }

                    /* Now call:
                     * - inside the lock if lockable
                     * - else outside the lock
                     */
                    if (pCallList[uiCalled]->bLockable)
                    {
                        (pCallList[uiCalled])->pFct( (pCallList[uiCalled])->pCookie );
                    }
                    else
                    {
                        pFct    = (pCallList[uiCalled])->pFct;
                        pCookie = (pCallList[uiCalled])->pCookie;
                        if (pFct)
                        {
                            pthread_mutex_unlock( &mtxLock );
                            pFct( pCookie );
                            PU_MUTEX_LOCK_ERROR( &mtxLock );
                        }
                    }
                }

#if !defined(NDEBUG)
                /* Debug only logic to see how many timers are stopped in the transition between
                 * fired and actually firing. It is probably not serious, but it would be useful to
                 * know if it is the same timer. Then we can see if the client is using the timer
                 * in a weird way
                 */
                else
                {
                    LOG_ERROR(
                        "WEIRD TIMER USAGE!! (id=%d,tag=0x%04x,type=%d,state=%d)\n",
                        pCallList[uiCalled]->usID,
                        pCallList[uiCalled]->usTag,
                        pCallList[uiCalled]->enType,
                        pCallList[uiCalled]->enState );
                }
#endif /* !defined(NDEBUG) */
            }

            /* Done, unlock */
            pthread_mutex_unlock( &mtxLock );
        }
    }

    /* done */
    return (nullptr);
}
/* putimer_thread */

/**
 * putimer_remove
 *
 * param   pTmr
 * retval  0 if the head is unchanged
 * retval  non-zero if the head was updated
 *
 * pre     none
 * post    none
 *
 * Description
 * removes the timer from the queue
 */
int putimer_remove(
    putimer_tmr_t* pTmr,
    int*           pWasActive,
    size_t*        pRemainingMs )
{
    int             iHeadUpdated = 0;
    putimer_tmr_t*  pPrev;
    putimer_tmr_t*  pCurr;
    struct timespec tsNow;
    struct timespec tsDiff;
    bool           bInQ;

    /* Set default values */
    *pWasActive   = 0;
    *pRemainingMs = 0;

    /* Check state, and immediate go to idle. This prevents the timer from calling back */
    bInQ = (PUTIMER_STATE_WAITING == pTmr->enState);
    pTmr->enState = PUTIMER_STATE_IDLE;

    /* Yank it from the queue */
    if (bInQ)
    {
        pPrev = nullptr;
        pCurr = pQueue;
        while (pCurr)
        {
            if (pCurr == pTmr)
            {
                *pWasActive = 1;

                /* Remove it from the queue, watch to see if the head is modified.
                 * If the head is modified then we need to set a new timeout
                 */
                if (pPrev)
                {
                    pPrev->pNext = pCurr->pNext;
                }
                else
                {
                    pQueue = pCurr->pNext;
                    iHeadUpdated = 1;
                }

                /* Check if the end time is AFTER the current time. If it is then subtract the
                 * current time from the end time to get remaining
                 */
                clock_gettime( CLOCK_MONOTONIC, &tsNow );
                if (timespec_is_a_after_b( &(pCurr->tsEnd), &tsNow ))
                {
                    timespec_a_sub_b( &(pCurr->tsEnd), &tsNow, &tsDiff );
                    *pRemainingMs = (size_t)((tsDiff.tv_nsec / 1000000) + (tsDiff.tv_sec * 1000));
                }
                else
                {
                    *pRemainingMs = 0;
                }

                /* found it, nothing else to do.. */
                return (iHeadUpdated);
            }
            else
            {
                pPrev = pCurr;
                pCurr = pCurr->pNext;
            }
        }
    }
    return (iHeadUpdated);
}
/* putimer_remove */

/**
 * putimer_add
 *
 * param   pTmr
 * retval  0 if the head is unchanged
 * retval  non-zero if the head was updated
 *
 * pre     The caller holds the mtxLock mutex
 * post    none
 *
 * Description
 * Puts the timer in the queue at the right place
 */
int putimer_add( putimer_tmr_t* pTmr )
{
    putimer_tmr_t* pPrev;
    putimer_tmr_t* pCurr;
    int            iHeadUpdated = 0;

    /* check state */
    if (PUTIMER_STATE_IDLE == pTmr->enState)
    {
        pPrev = nullptr;
        pCurr = pQueue;

        /* set the end tick - if we are NOT using absolute time */
        if (pTmr->iUseAbsTime)
        {
            pTmr->iUseAbsTime = 0;
        }
        else
        {
            clock_gettime( CLOCK_MONOTONIC, &(pTmr->tsEnd) );
            timespec_add_ms( &(pTmr->tsEnd), (pTmr->uiPeriodMs) );
        }

        /* where to put it */
        while (pCurr && !(timespec_is_a_after_b( &(pCurr->tsEnd), &(pTmr->tsEnd) )))
        {
            pPrev = pCurr;
            pCurr = pCurr->pNext;
        }

        /* insert it */
        if (pPrev)
        {
            pPrev->pNext = pTmr;
        }
        else
        {
            pQueue = pTmr;
            iHeadUpdated = 1;
        }
        pTmr->pNext   = pCurr;
        pTmr->enState = PUTIMER_STATE_WAITING;
    }
    return (iHeadUpdated);
}
/* putimer_add */

/**
 * Local timer create function
 * @param   enType      :timer type
 * @param   fctCallback :function to call on expiration
 * @param   uiPeriodMs  :timeout period in ms
 * @param   pCookie     :optional user data
 * @param   bLockable   :lockable or not
 *
 * @retval  Valid handle or nullptr
 *
 * @par Description
 * Local implementation of the timer create function
 */
putimer_hnd_t putimer_create_local(
    putimer_type_t         enType,
    putimer_callback_fct_t fctCallback,
    size_t                 uiPeriodMs,
    void*                  pCookie,
    bool                  bLockable )
{
    putimer_tmr_t* pTmr;
    uint16_t       usID;
    putimer_hnd_t  hndTmr{};

    WARN( iIsInit );
    ASSERT( fctCallback );
    ASSERT(
        (PUTIMER_TYPE_SINGLESHOT == enType) ||
        (PUTIMER_TYPE_PERIODIC   == enType) );

    /* adjust timeout */
    uiPeriodMs = (uiPeriodMs < PUTIMER_MIN_TIMEOUT) ? PUTIMER_MIN_TIMEOUT : uiPeriodMs;

    /* only create it everything is OK */
    if ((iIsInit) && (fctCallback) &&
        ((PUTIMER_TYPE_SINGLESHOT == enType) || (PUTIMER_TYPE_PERIODIC   == enType)))
    {
        PU_MUTEX_LOCK_ERROR( &mtxLock );
        WARN( uiAllocatedTimers < PUTIMER_MAX_RESOURCES );
        if (uiAllocatedTimers < PUTIMER_MAX_RESOURCES)
        {
            /* allocate an ID (equals index) use that timer
             * Debug ONLY tag test for really bad logic
             */
            usID = putimer_alloc_id();
            PUTIMER_DEBUG( "creating..:id=%d, checking tag=%d\n", usID, pTimerList[usID].usTag );
            ASSERT( 0 == pTimerList[usID].usTag );
            usRollingTag++;
            if (0 == usRollingTag)
            {
                usRollingTag = 1;
            }

            /* Set the values */
            pTmr = &(pTimerList[usID]);
            pTmr->usID        = usID;
            pTmr->usTag       = usRollingTag;
            pTmr->enType      = enType;
            pTmr->pFct        = fctCallback;
            pTmr->pCookie     = pCookie;
            pTmr->uiPeriodMs  = uiPeriodMs;
            pTmr->enState     = PUTIMER_STATE_IDLE;
            pTmr->iUseAbsTime = 0;
            pTmr->pNext       = nullptr;
            pTmr->bLockable   = bLockable;
            uiAllocatedTimers++;

            /* Build the handle */
            hndTmr = PUTIMER_HND_CREATE( usID, usRollingTag );
            PUTIMER_DEBUG(
                "Created: t=%d, %s, hnd=%zx\n",
                enType,
                ((true == bLockable) ? "lockable" : "reentrant"),
                ((size_t)hndTmr) );
        }
        pthread_mutex_unlock( &mtxLock );
    }
    return (hndTmr);
}
/* putimer_create_local */

/****************************************************************************/
/* PUBLIC FUNCTION DEFINITIONS                                              */
/****************************************************************************/

/**
 * @brief   Initialises the timer framework
 *
 * @retval  0  If successful
 * @retval -1 On failure
 *
 * @pre     function is protected against multiple init
 * @post    all resources required are initialised
 *
 * @par Description
 * Called at process initialisation.
 */
int putimer_init( void )
{
    int                 iResult = 0;
    pthread_mutexattr_t mattr;
    pthread_condattr_t  cattr;

    /* simple initialisation test, could use glib atomics if ther is a concern... */
    if (!iIsInit)
    {
        /* Use an error mutex to trap deadlocks */
        iIsInit = 1;
        iResult = pu_mutex_create_type( &mtxLock, PU_MUTEX_TYPE_ERROR );
        ASSERT( 0 == iResult );

        /* condition, set to use the monotonic clock */
        if (0 == iResult)
        {
            pthread_condattr_init( &cattr );
            pthread_condattr_setclock( &cattr, CLOCK_MONOTONIC );
            iResult = pthread_cond_init( &cndWake, &cattr );
            ASSERT( 0 == iResult );
            pthread_condattr_destroy( &cattr );
        }

        /* mutex - in the condition is OK */
        if (0 == iResult)
        {
            pthread_mutexattr_init( &mattr );
            iResult = pthread_mutex_init( &mtxWake, &mattr );
            ASSERT( 0 == iResult );
            pthread_mutexattr_destroy( &mattr );
        }

        /* clear the ID array and timer list */
        memset( pTimerId, 0, PUTIMER_RES_MULTIPLIER * sizeof(uint32_t) );
        memset( pTimerList, 0, PUTIMER_MAX_RESOURCES * sizeof(putimer_tmr_t) );
        uiAllocatedTimers = 0;

        /* Create the thread */
        if (0 == iResult)
        {
            pidTmrThread = PU_THREAD_CREATE(
                putimer_thread,
                nullptr,
                (16*1024) );
            ASSERT( pidTmrThread );
            iResult = ((0 == pidTmrThread) ? -1 : 0);
        }
    }
    return (iResult);
}
/* putimer_init */

/**
 * @brief   Shuts down the timer framework
 *
 * @retval  0  If successful
 * @retval -1 On failure
 *
 * @pre     None
 * @post    All resources are closed
 * @post    All outstanding timer are destroyed.
 *
 * @par Description
 * Called on process exit
 */
int putimer_exit( void )
{
    if (iIsInit)
    {
        /* instruct thread to exit */
        iIsInit = 0;
        iKillThread = 1;
        pthread_cond_signal( &cndWake );

        /* wait for thread to exit, then kill all timer resources */
        pthread_join( pidTmrThread, nullptr );
        PU_MUTEX_LOCK_ERROR( &mtxLock );
        memset( pTimerId, 0, PUTIMER_RES_MULTIPLIER * sizeof(uint32_t) );
        memset( pTimerList, 0, PUTIMER_MAX_RESOURCES * sizeof(putimer_tmr_t) );
        uiAllocatedTimers = 0;

        /* kill the wake mutex and condition */
        pthread_mutex_destroy( &mtxWake );
        pthread_cond_destroy( &cndWake );

        /* No need to destroy the simple lock mutex */
        pthread_mutex_unlock( &mtxLock );
    }
    return (0);
}
/* putimer_exit */

/**
 * @brief   Creates a timer resource
 *
 * @param   enType      :timer type
 * @param   fctCallback :function to call on expiration
 * @param   uiPeriodMs  :timeout period in ms
 * @param   pCookie     :optional user data
 * @retval  non-nullptr    : Valid handle
 * @retval  nullptr     : Failure
 *
 * @pre     Module is initialised
 * @pre     timeout is >= PUTIMER_MIN_TIMEOUT
 * @post    Timer is created
 *
 * @par Description
 * Allocates a timer  resource
 */
putimer_hnd_t putimer_create(
    putimer_type_t     enType,
    putimer_callback_fct_t fctCallback,
    size_t             uiPeriodMs,
    void*              pCookie )
{
    /* Create non-lockable */
    return (putimer_create_local( enType, fctCallback, uiPeriodMs, pCookie, false ));
}
/* putimer_create */

/**
 * @brief   Creates a lockable timer resource against the DEFAULT timer controller class instance.
 *
 * @param   enType      :timer type
 * @param   fctCallback :function to call on expiration
 * @param   uiPeriodMs  :timeout period in ms
 * @param   pCookie     :optional user data
 * @retval  non-nullptr    : Valid handle
 * @retval  nullptr        : Failure
 *
 * @pre     Module is initialised
 * @pre     timeout is >= PUTIMER_MIN_TIMEOUT
 * @post    Timer is created
 *
 * @par Description
 * Allocates a lockable timer resource. A lockable timer is one that invokes the callback function under the same
 * lock (mutex) as the start/stop/delete/etc calls.
 *
 * @par Side Effects
 * While in the timer callback, calling any of the following will cause a \b deadlock:
 * - timer delete
 * - timer start
 */
putimer_hnd_t putimer_create_lockable(
    putimer_type_t         enType,
    putimer_callback_fct_t fctCallback,
    size_t                 uiPeriodMs,
    void*                  pCookie )
{
    /* Create lockable */
    return (putimer_create_local( enType, fctCallback, uiPeriodMs, pCookie, true ));
}
/* putimer_create_lockable */

/**
 * @brief   Delete a timer resource
 *
 * @param   hndTimer
 * @retval  0  If successful
 * @retval -1 On failure
 *
 * @pre     driver is initialised
 * @post    none
 *
 * @par Description
 * Deletes a resource
 */
int putimer_delete( putimer_hnd_t hndTimer )
{
    int            iActive;
    size_t         uiMsLeft;
    int            iUpdateQ;
    int            iRet = -1;
    putimer_tmr_t* pTmr;
    uint16_t       usIdx;
    uint16_t       usTag;

    usIdx = PUTIMER_HND_GET_IDX( hndTimer );
    WARN( usIdx < PUTIMER_MAX_RESOURCES );
    if (usIdx < PUTIMER_MAX_RESOURCES)
    {
        /* Lock, check tag. Valid (non-stale) handle, do good things */
        PU_MUTEX_LOCK_ERROR( &mtxLock );
        usTag = PUTIMER_HND_GET_TAG( hndTimer );
        if (usTag == pTimerList[usIdx].usTag)
        {
            ASSERT( 0 != uiAllocatedTimers );
            if (0 != uiAllocatedTimers)
            {
                pTmr = &(pTimerList[usIdx]);

                /* This may update pQueue, do it under wake lock */
                pthread_mutex_lock( &mtxWake );
                iUpdateQ = putimer_remove( pTmr, &iActive, &uiMsLeft );
                pthread_mutex_unlock( &mtxWake );

                /* Release the resources */
                putimer_free_id( pTmr->usID );
                pTmr->usTag = 0;
                uiAllocatedTimers--;
                if (iUpdateQ)
                {
                    pthread_cond_signal( &cndWake );
                }
                PUTIMER_DEBUG( "Deleted: hnd=%zx\n", (size_t)hndTimer );
                iRet = 0;
            }
        }
#if !defined(NDEBUG)
        /* Debug only stale timer handle notification */
        else
        {
            LOG_ERROR( "Timer handle %zx is stale!!!\n", (size_t)hndTimer );
        }
#endif /* !defined(NDEBUG) */

        /* unlock */
        pthread_mutex_unlock( &mtxLock );
    }
    return (iRet);
}
/* putimer_delete */

/**
 * @brief   Changes the timer period
 *
 * @param   hndTimer   :timer handle
 * @param   uiPeriodMs :timer period in ms
 * @retval  0  If successful
 * @retval -1  On failure
 *
 * @pre     handle is valid
 * @pre     timeout is >= PUTIMER_MIN_TIMEOUT
 * @post    none
 *
 * @par Side Effects
 * If the timer was active this call will stop it.
 *
 * @par Description
 * Changes the timer period. This will stop the timer. You need to call
 * putimer_start to re-start the timer.
 */
int putimer_set_period(
    putimer_hnd_t hndTimer,
    size_t     uiPeriodMs )
{
    int            iActive;
    size_t         uiRemainingMs;
    int            iUpdateQ;
    int            iRet = -1;
    putimer_tmr_t* pTmr;
    uint16_t       usIdx;
    uint16_t       usTag;

    usIdx = PUTIMER_HND_GET_IDX( hndTimer );
    ASSERT( usIdx < PUTIMER_MAX_RESOURCES );
    if (usIdx < PUTIMER_MAX_RESOURCES)
    {
        /* adjust timeout */
        uiPeriodMs = (uiPeriodMs < PUTIMER_MIN_TIMEOUT) ? PUTIMER_MIN_TIMEOUT : uiPeriodMs;

        /* Lock, check tag. Valid (non-stale) handle, do good things */
        PU_MUTEX_LOCK_ERROR( &mtxLock );
        usTag = PUTIMER_HND_GET_TAG( hndTimer );
        if (usTag == pTimerList[usIdx].usTag)
        {
            /* Stop timer */
            pTmr = &(pTimerList[usIdx]);
            pthread_mutex_lock( &mtxWake );
            iUpdateQ = putimer_remove( pTmr, &iActive, &uiRemainingMs );
            pthread_mutex_unlock( &mtxWake );

            pTmr->uiPeriodMs = uiPeriodMs;
            if (iUpdateQ)
            {
                pthread_cond_signal( &cndWake );
            }
            iRet = 0;
        }
#if !defined(NDEBUG)
        /* Debug only stale timer handle notification */
        else
        {
            LOG_ERROR( "Timer handle %zx is stale!!!\n", (size_t)hndTimer );
        }
#endif /* !defined(NDEBUG) */

        /* unlock */
        pthread_mutex_unlock( &mtxLock );
    }
    return (iRet);
}
/* putimer_set_period */

/**
 * @brief   Changes the timer wake time
 *
 * @param   hndTimer :timer handle
 * @param   pWake    :pointer to wake time
 * @retval  0  If successful
 * @retval -1  On failure
 *
 * @pre     handle is valid
 * @pre     timer is of type PUTIMER_TYPE_SINGLESHOT
 * @post    none
 *
 * @par Side Effects
 * If the timer was active this call will stop it.
 *
 * @par Description
 * Explicitly sets the time at which the timer should wake up. This has slightly different
 * semantics to \ref putimer_set_period, and obviously only makes sense for timers of
 * type = \ref PUTIMER_TYPE_SINGLESHOT.
 */
int putimer_set_wake_time(
    putimer_hnd_t       hndTimer,
    struct timespec* pWake )
{
    int            iActive;
    size_t         uiRemainingMs;
    int            iUpdateQ;
    int            iRet = -1;
    putimer_tmr_t* pTmr;
    uint16_t       usIdx;
    uint16_t       usTag;

    usIdx = PUTIMER_HND_GET_IDX( hndTimer );
    ASSERT( usIdx < PUTIMER_MAX_RESOURCES );
    if (usIdx < PUTIMER_MAX_RESOURCES)
    {
        /* Lock, check tag. Valid (non-stale) handle, do good things */
        PU_MUTEX_LOCK_ERROR( &mtxLock );
        usTag = PUTIMER_HND_GET_TAG( hndTimer );
        if (usTag == pTimerList[usIdx].usTag)
        {
            pTmr = &(pTimerList[usIdx]);
            ASSERT( PUTIMER_TYPE_SINGLESHOT == pTmr->enType );
            if (PUTIMER_TYPE_SINGLESHOT == pTmr->enType)
            {
                /* use absolute time for this. This will modify the queue so
                 * we have to do it under the condition lock
                 */
                pthread_mutex_lock( &mtxWake );
                iUpdateQ = putimer_remove( pTmr, &iActive, &uiRemainingMs );
                pthread_mutex_unlock( &mtxWake );

                /* Convert from monotonic to realtime */
                pTmr->tsEnd       = *pWake;
                timespec_realtime_to_monotonic( &(pTmr->tsEnd) );
                pTmr->iUseAbsTime = 1;
                if (iUpdateQ)
                {
                    pthread_cond_signal( &cndWake );
                }
                iRet = 0;
            }
        }
#if !defined(NDEBUG)
        /* Debug only stale timer handle notification */
        else
        {
            LOG_ERROR( "Timer handle %zx is stale!!!\n", (size_t)hndTimer );
        }
#endif /* !defined(NDEBUG) */

        /* unlock */
        pthread_mutex_unlock( &mtxLock );
    }
    return (iRet);
}
/* putimer_set_wake_time */

/**
 * @brief   Starts a timer.
 *
 * @param   hndTimer :timer handle
 * @retval  0  If successful
 * @retval -1  On failure
 *
 * @pre     handle is valid
 * @post    none
 *
 * @par Side Effects
 * None
 *
 * @par Description
 * Starts a timer running.
 */
int putimer_start( putimer_hnd_t hndTimer )
{
    int            iRet = -1;
    int            iUpdatedQ;
    uint16_t       usIdx;
    uint16_t       usTag;

    usIdx = PUTIMER_HND_GET_IDX( hndTimer );
    ASSERT( usIdx < PUTIMER_MAX_RESOURCES );
    if (usIdx < PUTIMER_MAX_RESOURCES)
    {
        /* Lock, check tag. Valid (non-stale) handle, do good things */
        PU_MUTEX_LOCK_ERROR( &mtxLock );
        usTag = PUTIMER_HND_GET_TAG( hndTimer );
        if (usTag == pTimerList[usIdx].usTag)
        {
            /* This modifies pQueue, do it under mtxWake lock
             * If the queue is modified, wake the timer
             */
            pthread_mutex_lock( &mtxWake );
            iUpdatedQ = putimer_add( &(pTimerList[usIdx]) );
            pthread_mutex_unlock( &mtxWake );
            if (iUpdatedQ)
            {
                pthread_cond_signal( &cndWake );
            }
            iRet = 0;
        }
#if !defined(NDEBUG)
        /* Debug only stale timer handle notification */
        else
        {
            LOG_ERROR( "Timer handle %zx is stale!!!\n", (size_t)hndTimer );
        }
#endif /* !defined(NDEBUG) */

        /* unlock */
        pthread_mutex_unlock( &mtxLock );
    }
    return (iRet);
}
/* putimer_start */

/**
 * @brief   Query if a timer is active
 *
 * @param[in]   hndTimer :timer handle
 * @param[out]  pActive  :pointer to 'active' flag
 * @retval  0  If successful
 * @retval -1  On failure
 *
 * @pre     handle is valid
 * @pre     flag pointer is valid
 * @post    none
 *
 * @par Description
 * Queries if a valid timer is running, i.e. has been started, and has not yet expired.
 * If the timer is periodic and has been started then it will be active until stop is called. If the timer
 * is a single-shot, then it will be active between the time it is started and the time it expires.
 */
int putimer_is_active(
    putimer_hnd_t hndTimer,
    bool*         pActive )
{
    int            iRet = -1;
    uint16_t       usIdx;
    uint16_t       usTag;

    usIdx = PUTIMER_HND_GET_IDX( hndTimer );
    ASSERT( usIdx < PUTIMER_MAX_RESOURCES );
    if (usIdx < PUTIMER_MAX_RESOURCES)
    {
        /* Lock, check tag. Valid (non-stale) handle, do good things */
        PU_MUTEX_LOCK_ERROR( &mtxLock );
        usTag = PUTIMER_HND_GET_TAG( hndTimer );
        if (usTag == pTimerList[usIdx].usTag)
        {
            *pActive = (PUTIMER_STATE_IDLE != pTimerList[usIdx].enState);
            iRet = 0;
        }
#if !defined(NDEBUG)
        /* Debug only stale timer handle notification */
        else
        {
            LOG_ERROR( "Timer handle %zx is stale!!!\n", (size_t)hndTimer );
        }
#endif /* !defined(NDEBUG) */

        /* unlock */
        pthread_mutex_unlock( &mtxLock );
    }
    return (iRet);
}
/* putimer_is_active */

/**
 * @brief   Stops a timer.
 *
 * @param[in]  hndTimer:timer handle
 * @param[out] pMsLeft :milliseconds left on the timer
 * @retval  0  If successful
 * @retval -1  On failure
 *
 * @pre     driver is initialised
 * @post    none
 *
 * @par Side Effects
 * None
 *
 * @par Description
 * Stops a timer running. Works out how many ms are left on the timer
 */
int putimer_stop(
    putimer_hnd_t hndTimer,
    size_t*       pMsLeft )
{
    int            iActive = 0;
    int            iRet    = -1;
    int            iUpdateQ;
    size_t         uiMsLeft;
    uint16_t       usIdx;
    uint16_t       usTag;

    usIdx = PUTIMER_HND_GET_IDX( hndTimer );
    ASSERT( usIdx < PUTIMER_MAX_RESOURCES );
    if (usIdx < PUTIMER_MAX_RESOURCES)
    {
        /* Lock, check tag. Valid (non-stale) handle, do good things */
        PU_MUTEX_LOCK_ERROR( &mtxLock );
        usTag = PUTIMER_HND_GET_TAG( hndTimer );
        if (usTag == pTimerList[usIdx].usTag)
        {
            /* This modifies pQueue, do it under mtxWake lock
             * If the queue is modified, wake the timer
             */
            pthread_mutex_lock( &mtxWake );
            uiMsLeft = 0;
            iUpdateQ = putimer_remove( &(pTimerList[usIdx]), &iActive, &uiMsLeft );
            pthread_mutex_unlock( &mtxWake );
            if (iUpdateQ)
            {
                pthread_cond_signal( &cndWake );
            }
            if (pMsLeft)
            {
                *pMsLeft = uiMsLeft;
            }
            iRet = 0;
        }

#if !defined(NDEBUG)
        /* Debug only stale timer handle notification */
        else
        {
            LOG_ERROR( "Timer handle %zx is stale!!!\n", (size_t)hndTimer );
        }
#endif /* !defined(NDEBUG) */

        /* unlock */
        pthread_mutex_unlock( &mtxLock );
    }

    /* pass back the result */
    return (iRet);
}
/* putimer_stop */


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
 * @file     putimer.h
 * @author   Martin Gibson
 * @date     Feb 16, 2011
 */
#ifndef __PUTIMER_H_
#define __PUTIMER_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * @brief Simple timer callback utility
 * @defgroup PUTIMER Simple timer callback utility
 * @ingroup  SYSUTILS
 * Provides a framework for generic single-shot and periodic timers. Each timer results
 * in a callback being invoked.
 *
 * @par Clock type
 * The timers use \c CLOCK_MONOTONIC, and so are not affected by changes in wall time.
 * There is an exception to this. The \ref putimer_set_wake_time passes in a time based on
 * \c CLOCK_REALTIME. Internally this is re-based to \c CLOCK_MONOTONIC. If the wall time is changed
 * before the timer expires, then the results for this call only may be unexpected.
 *
 * @par WARNING
 * The timer callbacks must be treated in the same fashion as interrupt handlers, i.e.
 * in the context of the callback you must do as little as possible. Simply dispatch
 * an event, update a value, change a state, etc. Do not block for a "long" time. If you
 * do so you will penalise all the other clients of the timer. In DEBUG mode there
 * will be a check, if a callback is held for too long then the code will assert with
 * the appropriate information messages.
 *
 * @{
 */

/**** Includes ***************************************************************/
#include <stddef.h>
#include <time.h>

/**** Definitions ************************************************************/

typedef void* putimer_hnd_t;

/**
 * The minimum timeout value. The timer is a general facility, not intended for rapid
 * tiny timeouts. As a result even though the timeout is millisecond accurate it is limited
 * to a minimum of 10ms.
 */
#define PUTIMER_MIN_TIMEOUT (10) /* milliseconds */

/**
 * Different timer types
 */
typedef enum
{
    PUTIMER_TYPE_SINGLESHOT,  /*!< Triggers once based on a timeout period */
    PUTIMER_TYPE_PERIODIC,    /*!< Triggers repeatedly at a set period     */
    PUTIMER_TYPE_UNDEF        /* enum terminator..                         */
}   putimer_type_t;

/**
 * @brief Timeout callback function type
 *
 * @param[in] pCookie: Cookie data
 * return None
 *
 * @par Description
 * This function type is used as the timer expiration callback
 */
typedef void (*putimer_callback_fct_t)( void* pCookie );

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
int putimer_init( void );

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
int putimer_exit( void );

/**
 * @brief   Creates a timer resource
 *
 * @param   enType      :timer type
 * @param   fctCallback :function to call on expiration
 * @param   uiPeriodMs  :timeout period in ms
 * @param   pCookie     :optional user data, can be NULL
 * @retval  non-NULL    : Valid handle
 * @retval  NULL        : Failure
 *
 * @pre     Module is initialised
 * @pre     timeout is >= PUTIMER_MIN_TIMEOUT
 * @post    Timer is created
 *
 * @par Description
 * Allocates a non-lockable timer resource. A timer of this type invokes the timer callback outside
 * of the lock (mutex) used the start/stop/delete/etc calls.
 *
 * @par Side Effects
 * With this type of timer it is possible to get a callback \b AFTER stopping the timer. This is a corner
 * case and is extremely unlikely, but invoking the callback outside of the lock means that the behaviour
 * can never be 100% deterministic. The only way to guarantee that the timer \b NEVER fires after a "stop"
 * is to use a lock-able timer (i.e. \ref putimer_create_lockable).
 *
 * @note
 * When using a non-lock-able timer, while in the timer callback it is possible to:
 * - delete a timer
 * - start/restart a timer
 *
 */
putimer_hnd_t putimer_create(
    putimer_type_t         enType,
    putimer_callback_fct_t fctCallback,
    size_t                 uiPeriodMs,
    void*                  pCookie );

/**
 * @brief   Creates a lock-able timer resource against the DEFAULT timer controller class instance.
 *
 * @param   enType      :timer type
 * @param   fctCallback :function to call on expiration
 * @param   uiPeriodMs  :timeout period in ms
 * @param   pCookie     :optional user data
 * @retval  non-NULL    : Valid handle
 * @retval  NULL        : Failure
 *
 * @pre     Module is initialised
 * @pre     timeout is >= PUTIMER_MIN_TIMEOUT
 * @post    Timer is created
 *
 * @par Description
 * Allocates a lock-able timer resource. A lock-able timer is one that invokes the callback function under the same
 * lock (mutex) as the start/stop/delete/etc calls.
 *
 * @par Side Effects
 * If you use a lock-able timer,it is guaranteed that the time will \b NEVER fire after a "stop" call has been made. It is deterministic.
 *
 * @note
 * While in the timer callback, calling any of the following will cause a \b deadlock:
 * - timer delete
 * - timer start
 * .
 * When moving from a non-lock-able timer to a lock-able timer you need to examine your callback very carefully for deadlock scenarios.
 */
putimer_hnd_t putimer_create_lockable(
    putimer_type_t         enType,
    putimer_callback_fct_t fctCallback,
    size_t                 uiPeriodMs,
    void*                  pCookie );

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
int putimer_delete( putimer_hnd_t hndTimer );

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
    size_t        uiPeriodMs );

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
 *
 * @note
 * The wake time \b MUST be generated using the CLOCK_REALTIME. Internally it will be converted
 * to a CLOCK_MONOTONIC value for use with the timer.
 */
int putimer_set_wake_time(
    putimer_hnd_t    hndTimer,
    struct timespec* Wake );

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
int putimer_start( putimer_hnd_t hndTimer );

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
    bool*         pActive );

/**
 * @brief   Stops a timer.
 *
 * @param[in]  hndTimer: Timer handle
 * @param[out] pMsLeft : Milliseconds left on the timer
 * @retval  0  If successful
 * @retval -1  On failure
 *
 * @pre     driver is initialised
 * @pre     the handle is valid
 * @post    none
 *
 * @par Description
 * Stops a timer running. Works out how many ms are left on the timer
 *
 * @note
 * The pMsLeft parameter may be NULL if the caller is not interested in the time remaining.
 */
int putimer_stop(
    putimer_hnd_t hndTimer,
    size_t*       pMsLeft );

/**
 * @brief Posix "timespec" utility functions
 * @defgroup TSPEC Posix "timespec" utility functions
 * @ingroup  PUTIMER
 * For some strange reason Posix does not provide any utilities for manipulating
 * timespec structures. This section provides a few simple functions for this.
 *
 * @{
 */

/**
 * @brief   Determines if a is after b.
 *
 * @param[in] pA : Pointer to timespec A
 * @param[in] pB : Pointer to timespec B
 * @retval  0    If A <= B
 * @retval non-0 If A > B
 *
 * @pre     none
 * @post    none
 *
 * @par Description
 * Determines if timespec A is after (later than) timespec B
 */
int timespec_is_a_after_b(
    struct timespec* pA,
    struct timespec* pB );

/**
 * @brief   Subtracts timespec B from timespec A
 *
 * @param[in] pA   : Pointer to timespec A
 * @param[in] pB   : Pointer to timespec B
 * @param[in] pRes : Pointer to result timespec
 *
 * @pre     none
 * @post    none
 *
 * @par Description
 * Subtracts timespec B from timespec A
 */
void timespec_a_sub_b(
    struct timespec* pA,
    struct timespec* pB,
    struct timespec* pRes );

/**
 * @brief   Add a millisecond value to a timespec
 *
 * @param[out] pTs  : Pointer to timespec
 * @param[in]  uiMs : Millisecond value
 *
 * @pre     none
 * @post    none
 *
 * @par Description
 * Add a millisecond value to a timespec
 */
void timespec_add_ms(
    struct timespec* pTs,
    size_t           uiMs );

/**
 * @brief  Difference between A and B in milliseconds
 *
 * @param[in] pA   : Pointer to timespec A
 * @param[in] pB   : Pointer to timespec B
 * @retval ms The millisecond value
 *
 * @pre     none
 * @post    none
 *
 * @par Description
 * Subtracts timespec B from timespec A (i.e. A-B=ms) and return the value in milliseconds.
 * Note that there is no error checking. If the difference is too large it may result in strange results.
 */
size_t timespec_a_sub_b_ms(
    struct timespec* pA,
    struct timespec* pB );

/**
 * @brief  Difference between A and B in microseconds
 *
 * @param[in] pA   : Pointer to timespec A
 * @param[in] pB   : Pointer to timespec B
 * @retval us The microsecond value
 *
 * @pre     none
 * @post    none
 *
 * @par Description
 * Subtracts timespec B from timespec A (i.e. A-B=ms) and return the value in microseconds.
 * Note that there is no error checking. If the difference is too large it may result in strange results.
 */
size_t timespec_a_sub_b_us(
    struct timespec* pA,
    struct timespec* pB );

/**
 * @brief  Create a timespec = ("now" + ms) - using CLOCK_REALTIME
 *
 * @param[out] pTs  : Pointer to timespec
 * @param[in]  uiMs : Millisecond value
 *
 * @pre     none
 * @post    none
 *
 * @par Description
 * Populates the time spec with the current ("now") time plus the specified
 * number of milliseconds. This is typically used in "wait until" timeout calculations,
 * e.g. the value of the timespec in sem_timedwait()
 *
 * @note
 * Uses CLOCK_REALTIME
 */
void timespec_now_plus_ms(
    struct timespec* pTs,
    size_t           uiMs );

/**
 * @brief  Create a timespec = ("now" + ms) - using CLOCK_MONOTONIC
 *
 * @param[out] pTs  : Pointer to timespec
 * @param[in]  uiMs : Millisecond value
 *
 * @pre     none
 * @post    none
 *
 * @par Description
 * Populates the time spec with the current ("now") time plus the specified
 * number of milliseconds. This is typically used in "wait until" timeout calculations,
 * e.g. the value of the timespec in sem_timedwait()
 *
 * @note
 * Uses CLOCK_MONOTONIC
 */
void timespec_now_plus_ms_monotonic(
    struct timespec* pTs,
    size_t           uiMs );

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
void timespec_realtime_to_monotonic( struct timespec* pTs );

/**
 * @}
 */

/**
 * @brief Get a "safe" time measurement
 * @param[out] timespec_ : timespec structure
 *
 * @return None
 *
 * @par Description
 * Gets a "safe" time based on a hardware tick. This is useful for time-stamping, and for measuring
 * periods (i.e. home many ms since last event). It uses the monotonic clock, this is guaranteed to
 * be unaffected by changes in wall time (unlike CLOCK_REALTIME).
 *
 * @par Usage example
 * @code
 * struct timespec tsTick;
 * TIME_GET_HW_TICK( tsTick );
 * @endcode
 */
#define TIME_GET_HW_TICK( timespec_ ) (clock_gettime( CLOCK_MONOTONIC, &(timespec_) ))

/**
 * @brief PTS and DTS conversion macros
 * @defgroup OPTS PTS and DTS conversion macros
 * @ingroup  PUTIMER
 * Simple efficient 90kHz to ms unit calculations. These avoid 64bit mathematics.
 *
 * @{
 */

/**
 * @}
 */

/**
 * @}
 */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __PUTIMER_H_ */

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
 * @file     pumutex.c
 * @author   Martin
 * @date     2019-05-01
 */

/**** Includes ***************************************************************/
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include "posutils.h"
#include "logging.h"

/**** Definitions ************************************************************/

/**** Macros ****************************************************************/

/**** Static declarations ***************************************************/

/**** Globals and externs ***************************************************/

/**** Local function prototypes (NB Use static modifier) ********************/

/****************************************************************************/
/* LOCAL FUNCTION DEFINITIONS                                               */
/****************************************************************************/

/****************************************************************************/
/* PUBLIC FUNCTION DEFINITIONS                                              */
/****************************************************************************/

/**
 * @brief   Creates (initialises) a mutex with the Comet constraints applied
 *
 * @param[in] pMtx      : Pointer to a valid mutex
 * @param[in] bRecursive: true or false
 * @oaram[in] enType    : The type of mutex to create
 * @retval  0 for success
 * @retval  Non-zero for failure
 *
 * @pre     The mutex pointer is non-null
 * @post    The mutex is initialised
 *
 * @par Side Effects
 * None
 *
 * @par Description
 * Creates a standard Posix pthread mutex.
 */
int pu_mutex_create_type(
    pthread_mutex_t* pMtx,
    pu_mutex_type    enType )
{
    pthread_mutexattr_t attr;
    int                 iResult = -1;

    /* pre-condition */
    ASSERT( pMtx );
    ASSERT( (enType >= PU_MUTEX_TYPE_FAST) && (enType < PU_MUTEX_TYPE_ENDDEF) );
    if ((pMtx)                         &&
        (enType >= PU_MUTEX_TYPE_FAST) &&
        (enType < PU_MUTEX_TYPE_ENDDEF) )
    {
        /* Default - fast mutex (default) */
        if (PU_MUTEX_TYPE_FAST == enType)
        {
            iResult = pthread_mutex_init( pMtx, NULL );
        }

        /* Error mutex */
        else if (PU_MUTEX_TYPE_ERROR == enType)
        {
            iResult = pthread_mutexattr_init( &attr );
            if (0 == iResult)
            {
                iResult = pthread_mutexattr_settype( &attr, PTHREAD_MUTEX_ERRORCHECK );
            }
            if (0 == iResult)
            {
                iResult = pthread_mutex_init( pMtx, &attr );
            }
            pthread_mutexattr_destroy( &attr );
        }

// Removed recursive mutexes, They are (relatively) slow and using them is bad practice.
// The use of a recursive mutex typically means one the author is not clear about the
// execution paths in their code.
// The code block remains both as a caution not to add them back in, and in the possible case
// where using a recursive in an engineering/experimental build may have discovery value..
#if 0
        /* Recursive mutex */
        else if (PU_MUTEX_TYPE_RECURSIVE == enType)
        {
            iResult = pthread_mutexattr_init( &attr );
            if (0 == iResult)
            {
                iResult = pthread_mutexattr_settype( &attr, PTHREAD_MUTEX_RECURSIVE );
            }
            if (0 == iResult)
            {
                iResult = pthread_mutex_init( pMtx, &attr );
            }
            pthread_mutexattr_destroy( &attr );
        }
#endif
    }

    /* post-condition */
    ASSERT( 0 == iResult);
    return( iResult );
}
/* pu_mutex_create_type */


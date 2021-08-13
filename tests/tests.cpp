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
 * \file     tests.cpp
 * \brief    Implementation of unit tests
 */

/**** System includes, namespaces, then local includes  *********************/
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <assert.h>
#include "posutils.h"
#include "putimer.h"

// start anonymous namespace
namespace {

/**** Definitions ************************************************************/
#define UNUSED(parameter) (void)parameter
#define BATCH_SIZE        ((int)10)

/**** Macros ****************************************************************/

/**** Local function prototypes (NB Use static modifier) ********************/
int   sighandler_install( void );
void  sighandler_handler( int signo, siginfo_t* info, void* data );
void* stub_thread(void* pArg);

/****************************************************************************/
/* LOCAL FUNCTION DEFINITIONS                                               */
/****************************************************************************/

// Callback on signal..
//
void sighandler_handler( int signo, siginfo_t* info, void* data ) {
    UNUSED(signo);
    UNUSED(info);
    UNUSED(data);
}
// sighandler_handler

// Set up and install the signal handler..
int sighandler_install( void )
{
    struct sigaction sa;
    memset( &sa, 0, sizeof(struct sigaction) );
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = sighandler_handler;
    sigfillset( &sa.sa_mask );
    return (sigaction( SIGINT, &sa, NULL ));
}

void* stub_thread(void* pArg) {
    std::cout << "Running thread: " << (size_t)pArg << std::endl;
    return (NULL);
}


} // End anonymous namespace

/****************************************************************************/
/* PUBLIC FUNCTION OR METHOD DEFINITIONS                                    */
/****************************************************************************/

/**
 * Main
 * @param argc: unused
 * @param argv: unused
 * @return 0
 * Create all the processes, resources, barriers, etc.
 */
int main( int argc, char *argv[] )
{
    UNUSED(argc);
    UNUSED(argv);

    // setup
    sighandler_install();

    // build info
    std::cout << "Posix Utilities: simple tests" << std::endl;
#if defined(__clang__)
    std::cout << "Compiled with Clang " << __clang_version__ << std::endl;
#else
    std::cout << "Compiled with GCC " << __GNUC__ << "." << __GNUC_MINOR__ << "." << __GNUC_PATCHLEVEL__ << std::endl;
#endif

    // Test multiple init
    POSUTILS_INIT;
    POSUTILS_INIT;
    POSUTILS_INIT;
    POSUTILS_INIT;

    // Create N threads
    pthread_t pThreadList[BATCH_SIZE];
    for (size_t i = 0; i < BATCH_SIZE; i++) {
        std::cout << "Creating thread: " << i << std::endl;
        pThreadList[i] = PU_THREAD_CREATE(stub_thread, (void*)i, 32*1024);
        assert(0 != pThreadList[i]);
    }

    // Wait for N threads to exit
    for (size_t i = 0; i < BATCH_SIZE; i++) {
        std::cout << "Joining thread: " << i << std::endl;
        pthread_join(pThreadList[i], NULL);
        std::cout << "Thread exited" << std::endl;
    }

    // Test multiple exit
    POSUTILS_EXIT;
    POSUTILS_EXIT;
    POSUTILS_EXIT;
    POSUTILS_EXIT;

    // Done
    return (0);
}
/* main */

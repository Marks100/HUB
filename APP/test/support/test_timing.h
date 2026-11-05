#ifndef TEST_TIMING_H
#define TEST_TIMING_H

/*! \file
*               Author: mstewart
*   \brief      Test execution timing - how long each test took
*
*   \details    Two macros over Unity's execution time feature:
*
*                   TEST_TIMING_START()  - last line of setUp()
*                   TEST_TIMING_STOP()   - first line of tearDown()
*
*               Each prints "[TIMING] <test> : <n> ms" and appends "<test>,<n>" to build/test_timings.csv (the
*               folder Ceedling runs the tests from is APP/test). The file is opened and closed around every
*               row, so nothing is lost if a later test crashes the run. Delete it to start a new series.
*
*               Both do nothing unless UNITY_INCLUDE_EXEC_TIME is defined, which sw_test_project.yml does under
*               :unity: :defines:.
*/

/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include <stdio.h>
#include "unity.h"

/***************************************************************************************************
**                              Defines                                                           **
***************************************************************************************************/
#define TEST_TIMING_CSV_PATH    "build/test_timings.csv"

#ifdef UNITY_INCLUDE_EXEC_TIME

/* Unity keeps a test's start and stop time as a plain count on Windows and as a struct timespec on Linux and macOS */
#if defined( __unix__ ) || defined( __APPLE__ )
#define TEST_TIMING_ELAPSED_MS()                                                                    \
    ( (unsigned)( ( Unity.CurrentTestStopTime.tv_sec  - Unity.CurrentTestStartTime.tv_sec ) * 1000L \
                + ( Unity.CurrentTestStopTime.tv_nsec - Unity.CurrentTestStartTime.tv_nsec ) / 1000000L ) )
#else
#define TEST_TIMING_ELAPSED_MS()    ( (unsigned)( Unity.CurrentTestStopTime - Unity.CurrentTestStartTime ) )
#endif

#define TEST_TIMING_START()    UNITY_EXEC_TIME_START()

#define TEST_TIMING_STOP()                                                                          \
    do                                                                                              \
    {                                                                                               \
        FILE*    timing_file_p;                                                                     \
        unsigned elapsed_ms;                                                                        \
                                                                                                    \
        UNITY_EXEC_TIME_STOP();                                                                     \
        elapsed_ms = TEST_TIMING_ELAPSED_MS();                                                      \
        printf( "[TIMING] %s : %u ms\n", Unity.CurrentTestName, elapsed_ms );                       \
                                                                                                    \
        timing_file_p = fopen( TEST_TIMING_CSV_PATH, "a" );                                         \
                                                                                                    \
        if( timing_file_p != NULL )                                                                 \
        {                                                                                           \
            fprintf( timing_file_p, "%s,%u\n", Unity.CurrentTestName, elapsed_ms );                 \
            fclose( timing_file_p );                                                                \
        }                                                                                           \
    } while( 0 )

#else

#define TEST_TIMING_START()    do { } while( 0 )
#define TEST_TIMING_STOP()     do { } while( 0 )

#endif /* UNITY_INCLUDE_EXEC_TIME */

#endif /* TEST_TIMING_H */

/****************************** END OF FILE *******************************************************/

#ifndef _UTIL_PARSER_H_
#define _UTIL_PARSER_H_

#include <time.h>

/*
 * Use to print elapsed time of set of timers with user-defined prefix `timer`
 */
#define TIME_PRINT(timer) std::cout << #timer ": " << timer##_elapsed << "\n" << std::flush

/*
 * Use to create initial timer with user-defined prefix `timer`

 */
#define TIME_BEGIN(timer) auto timer##_start = clock()

/*
 * Use to create final timer with user-defined prefix `timer`

 * and print time elapsed between initial and final timers.
 */
#define TIME_END(timer) do { \
        auto timer##_end = clock(); \
        double timer##_elapsed = (double) (timer##_end - timer##_start) / CLOCKS_PER_SEC; \
        TIME_PRINT(timer); \
    } while(0)

#endif

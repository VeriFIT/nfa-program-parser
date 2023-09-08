#ifndef _UTIL_PARSER_H_
#define _UTIL_PARSER_H_

/*
 * Use to print elapsed time of set of timers with user-defined prefix `timer`
 */
#define TIME_PRINT(timer) std::cout << #timer ": " << timer##_elapsed.count() << "\n" << std::flush

/*
 * Use to create initial timer with user-defined prefix `timer`

 */
#define TIME_BEGIN(timer) auto timer##_start = std::chrono::system_clock::now()

/*
 * Use to create final timer with user-defined prefix `timer`

 * and print time elapsed between initial and final timers.
 */
#define TIME_END(timer) do { \
        auto timer##_end = std::chrono::system_clock::now(); \
        std::chrono::duration<double> timer##_elapsed = timer##_end - timer##_start; \
        TIME_PRINT(timer); \
    } while(0)

#endif
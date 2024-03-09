/**
 * @file time.h
 * @brief date and time functions header.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___TIME_H
#define ___TIME_H 0

#include <types.h>

#define TIME_TIMESTAMP_START_YEAR  1970
#define TIME_SECONDS_OF_MINUTE       60
#define TIME_SECONDS_OF_HOUR       3600
#define TIME_SECONDS_OF_DAY       86400
#define TIME_SECONDS_OF_MONTH   2629743
#define TIME_SECONDS_OF_YEAR   31556926
#define TIME_DAYS_AT_YEAR           365
#define TIME_DAYS_AT_LEAP_YEAR      366

typedef uint64_t time_t;

extern uint64_t TIME_EPOCH;

typedef struct timeparsed_t {
    uint16_t year;
    uint8_t  month;
    uint8_t  day;
    uint8_t  hours;
    uint8_t  minutes;
    uint8_t  seconds;
} timeparsed_t;

time_t time(time_t* t);
time_t time_ns(time_t* t);

timeparsed_t* timeparsed(timeparsed_t* tp);

time_t timeparsed_to_time(timeparsed_t* tp);

timeparsed_t* time_to_timeparsed(time_t t);

uint64_t rdtsc(void);

#endif

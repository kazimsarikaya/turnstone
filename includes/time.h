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

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup TimeConstants Time Constants
 * @brief Defines various time-related constants for calculations.
 * @{
 */
#define TIME_TIMESTAMP_START_YEAR  1970 /**< @brief The starting year for the Unix epoch timestamp. */
#define TIME_SECONDS_OF_MINUTE       60 /**< @brief Number of seconds in a minute. */
#define TIME_SECONDS_OF_HOUR       3600 /**< @brief Number of seconds in an hour. */
#define TIME_SECONDS_OF_DAY       86400 /**< @brief Number of seconds in a day. */
#define TIME_SECONDS_OF_MONTH   2629743 /**< @brief Approximate number of seconds in a month (average). */
#define TIME_SECONDS_OF_YEAR   31556926 /**< @brief Approximate number of seconds in a year (average). */
#define TIME_DAYS_AT_YEAR           365 /**< @brief Number of days in a common year. */
#define TIME_DAYS_AT_LEAP_YEAR      366 /**< @brief Number of days in a leap year. */
/** @} */

/**
 * @typedef time_t
 * @brief Represents time in seconds since the Unix epoch (January 1, 1970, 00:00:00 UTC).
 */
typedef uint64_t time_t;

/**
 * @brief Global variable representing the system's epoch time.
 *
 * This variable is typically initialized at system boot and represents
 * the current time in seconds since the Unix epoch.
 */
extern uint64_t TIME_EPOCH;

/**
 * @struct timeparsed_t
 * @brief Represents a broken-down time structure.
 *
 * This structure holds individual components of a date and time,
 * such as year, month, day, hours, minutes, and seconds.
 */
typedef struct timeparsed_t {
    uint16_t year; /**< @brief The year (e.g., 2023). */
    uint8_t  month; /**< @brief The month (1-12). */
    uint8_t  day; /**< @brief The day of the month (1-31). */
    uint8_t  hours; /**< @brief The hour of the day (0-23). */
    uint8_t  minutes; /**< @brief The minute of the hour (0-59). */
    uint8_t  seconds; /**< @brief The second of the minute (0-59). */
} timeparsed_t;

/**
 * @brief Gets the current time in seconds since the Unix epoch.
 *
 * If `t` is not NULL, the current time value is also stored in the location
 * pointed to by `t`.
 *
 * @param t Optional pointer to a `time_t` variable to store the current time.
 * @return The current time in seconds since the Unix epoch.
 */
time_t time(time_t* t);

/**
 * @brief Gets the current time in milliseconds since the Unix epoch.
 *
 * If `t` is not NULL, the current time value is also stored in the location
 * pointed to by `t`.
 *
 * @param t Optional pointer to a `time_t` variable to store the current time.
 * @return The current time in milliseconds since the Unix epoch.
 */
time_t time_ms(time_t* t);

/**
 * @brief Gets the current time in microseconds since the Unix epoch.
 *
 * If `t` is not NULL, the current time value is also stored in the location
 * pointed to by `t`.
 *
 * @param t Optional pointer to a `time_t` variable to store the current time.
 * @return The current time in microseconds since the Unix epoch.
 */
time_t time_us(time_t* t);

/**
 * @brief Gets the current time in nanoseconds since the Unix epoch.
 *
 * If `t` is not NULL, the current time value is also stored in the location
 * pointed to by `t`.
 *
 * @param t Optional pointer to a `time_t` variable to store the current time.
 * @return The current time in nanoseconds since the Unix epoch.
 */
time_t time_ns(time_t* t);

/**
 * @brief Gets the current time broken down into a `timeparsed_t` structure.
 *
 * This function populates the provided `timeparsed_t` structure with the
 * current date and time components.
 *
 * @param tp Pointer to a `timeparsed_t` structure to be filled with the current time.
 * @return A pointer to the populated `timeparsed_t` structure on success, or NULL on failure.
 */
timeparsed_t* timeparsed(timeparsed_t* tp);

/**
 * @brief Converts a `timeparsed_t` structure to a `time_t` (seconds since epoch).
 *
 * @param tp Pointer to the `timeparsed_t` structure to convert.
 * @return The `time_t` value representing the given broken-down time.
 */
time_t timeparsed_to_time(timeparsed_t* tp);

/**
 * @brief Converts a `time_t` (seconds since epoch) to a `timeparsed_t` structure.
 *
 * This function allocates and populates a new `timeparsed_t` structure.
 * The caller is responsible for freeing the returned structure.
 *
 * @param t The `time_t` value to convert.
 * @return A pointer to a newly allocated and populated `timeparsed_t` structure on success,
 *         or NULL on failure.
 */
timeparsed_t* time_to_timeparsed(time_t t);

/**
 * @brief Formats a `time_t` value into a UTC string representation.
 *
 * The formatted string follows the "YYMMDDHHMMSSZ" format, where:
 * - YY: Last two digits of the year
 * - MM: Month (01-12)
 * - DD: Day of the month (01-31)
 * - HH: Hour (00-23)
 * - MM: Minute (00-59)
 * - SS: Second (00-59)
 * - Z: Indicates UTC time
 *
 * @param t The `time_t` value to format.
 * @param buffer Pointer to a character buffer where the formatted string will be stored.
 * @param buffer_size Size of the provided buffer. Must be at least 13 bytes to hold the full string and null terminator.
 */
void time_format_utc(time_t t, char_t* buffer, size_t buffer_size);

/**
 * @brief Formats a `time_t` value into a UTC string representation with nanosecond precision.
 *
 * The formatted string follows the "YYMMDDHHMMSSZ" format, where:
 * - YY: Last two digits of the year
 * - MM: Month (01-12)
 * - DD: Day of the month (01-31)
 * - HH: Hour (00-23)
 * - MM: Minute (00-59)
 * - SS: Second (00-59)
 * - Z: Indicates UTC time
 *
 * @param t The `time_t` value to format. It is expected to represent time in nanoseconds.
 * @param buffer Pointer to a character buffer where the formatted string will be stored.
 * @param buffer_size Size of the provided buffer. Must be at least 13 bytes to hold the full string and null terminator.
 */
void time_ns_format_utc(time_t t, char_t* buffer, size_t buffer_size);

/**
 * @brief Reads the Time Stamp Counter (TSC) of the CPU.
 *
 * This function provides access to the CPU's internal Time Stamp Counter,
 * which is a 64-bit register that counts the number of clock cycles since reset.
 * Its frequency can vary, so it's not always suitable for wall-clock time,
 * but it can be useful for high-resolution timing and profiling.
 *
 * @return The current value of the CPU's Time Stamp Counter.
 */
uint64_t rdtsc(void);

#ifdef __cplusplus
}
#endif

#endif

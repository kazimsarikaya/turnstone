/**
 * @file int_limits.h
 * @brief integer limits
 */

#ifndef __INT_LIMITS_H
#define __INT_LIMITS_H 1

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define INT8_MIN  (-128)
#define INT8_MAX  (127)

#define UINT8_MIN (0)
#define UINT8_MAX (255)

#define INT16_MIN (-32768)
#define INT16_MAX (32767)

#define UINT16_MIN (0)
#define UINT16_MAX (65535)

#define INT32_MIN (-2147483648)
#define INT32_MAX (2147483647)

#define UINT32_MIN (0)
#define UINT32_MAX (4294967295)

#define INT64_MIN (-9223372036854775808)
#define INT64_MAX (9223372036854775807)

#define UINT64_MIN (0)
#define UINT64_MAX (18446744073709551615)

#ifdef __cplusplus
}
#endif

#endif /* __INT_LIMITS_H */

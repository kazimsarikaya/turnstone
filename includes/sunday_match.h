/**
 * @file sunday_match.h
 * @brief sunday match interface
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#ifndef ___SUNDAY_MATCH_H
/*! prevent duplicate header error macro */
#define ___SUNDAY_MATCH_H 0

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

int64_t sunday_match(const uint8_t* data, const int64_t data_len, const uint8_t* pattern, const int64_t pattern_len);

#ifdef __cplusplus
}
#endif

#endif

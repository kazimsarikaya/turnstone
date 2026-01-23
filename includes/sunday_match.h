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

/**
 * @brief Searches for the first occurrence of a pattern within a data buffer using the Sunday algorithm.
 *
 * The Sunday string matching algorithm is an efficient algorithm for finding
 * the first occurrence of a pattern string within a larger text (data) string.
 * It works by aligning the pattern with the text and, upon a mismatch,
 * shifting the pattern based on the character in the text immediately
 * following the current alignment of the pattern.
 *
 * @param data Pointer to the data buffer (text) to search within.
 * @param data_len The length of the data buffer.
 * @param pattern Pointer to the pattern buffer to search for.
 * @param pattern_len The length of the pattern buffer.
 * @return The starting index of the first occurrence of the pattern in the data,
 *         or -1 if the pattern is not found.
 */
int64_t sunday_match(const uint8_t* data, const int64_t data_len, const uint8_t* pattern, const int64_t pattern_len);

#ifdef __cplusplus
}
#endif

#endif

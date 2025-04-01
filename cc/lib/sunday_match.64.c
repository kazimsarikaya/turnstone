/**
 * @file sunday_match.64.c
 * @brief sunday match interface implementation.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <sunday_match.h>

MODULE("turnstone.lib");

int64_t sunday_match(const uint8_t* data, const int64_t data_len, const uint8_t* pattern, const int64_t pattern_len) {

    if(pattern_len > data_len) {
        return -1;
    }

    int64_t skip[256];

    for(int64_t i = 0; i < 256; i++) {
        skip[i] = pattern_len + 1;
    }

    for(int64_t i = 0; i < pattern_len; i++) {
        skip[pattern[i]] = pattern_len - i;
    }

    int64_t pos = 0;

    while (pos <= data_len - pattern_len) {
        int64_t j = 0;

        while (data[pos + j] == pattern[j]) {
            j++;

            if (j == pattern_len) {
                return pos;
            }
        }

        pos += skip[data[pos + pattern_len]];
    }

    return -1;
}


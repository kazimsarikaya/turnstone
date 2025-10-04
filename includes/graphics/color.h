/**
 * @file color.h
 * @brief color header file
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */


#ifndef ___COLOR_H
#define ___COLOR_H 0

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef union color_t color_t;

union color_t {
    struct {
        uint8_t blue;
        uint8_t green;
        uint8_t red;
        uint8_t alpha;
    } __attribute__((packed));
    uint32_t color;
};

_Static_assert(sizeof(color_t) == sizeof(uint32_t), "Invalid color_t size");

#ifdef __cplusplus
}
#endif

#endif // ___COLOR_H

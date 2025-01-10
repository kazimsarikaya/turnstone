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

/*! pixel type */
typedef uint32_t pixel_t;

typedef union color_t color_t;

union color_t {
    struct {
        uint8_t alpha;
        uint8_t red;
        uint8_t green;
        uint8_t blue;
    } __attribute__((packed));
    uint32_t color;
};


#endif // ___COLOR_H

/**
 * @file text_cursor.h
 * @brief Text cursor header file.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */



#ifndef ___TEXT_CURSOR_H
#define ___TEXT_CURSOR_H 0

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

int8_t text_cursor_move(int32_t x, int32_t y);
int8_t text_cursor_move_relative(int32_t x, int32_t y);
void   text_cursor_get(int32_t* x, int32_t* y);

#ifdef __cplusplus
}
#endif

#endif // ___TEXT_CURSOR_H

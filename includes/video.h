/**
 * @file video.h
 * @brief video out interface
 */
#ifndef ___VIDEO_H
/*! prevent duplicate header error macro */
#define ___VIDEO_H 0

#include <types.h>

/*! video buffer len rowsxcolumns at text mode*/
#define  VIDEO_BUF_LEN 25 * 80

/**
 * @brief writes string to a spacial location
 * @param[in] string string will be writen
 * @param[in] x      column number
 * @param[in] y      row number
 *
 * updates cursor location and calls @ref video_print
 */
void video_print_at(char_t* string, uint8_t x, uint8_t y);

/**
 * @brief writes string to video buffer
 * @param[in] string string will be writen
 *
 * string will be writen at current cursor position and cursor will be updated.
 */
void video_print(char_t* string);

/**
 * @brief clears screen aka write space to all buffer
 */
void video_clear_screen();

#endif

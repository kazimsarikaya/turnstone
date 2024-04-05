/**
 * @file    console.c
 * @brief   EFI console driver
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include "setup.h"
#include <efi.h>
#include <memory.h>
#include <strings.h>
#include <utils.h>

/*! module name */
MODULE("turnstone.efi");

/*! EFI system table  global variable */
extern efi_system_table_t* ST;

/**
 * @brief prints string to console with efi system table's console output protocol.
 * @param[in] string string to print.
 */
void video_print(const char_t* string);

void screen_clear(void) {
    ST->console_output->clear_screen(ST->console_output);
}

void video_print(const char_t* string) {
    if(string == NULL) {
        return;
    }

    wchar_t data[] = {0, 0};

    size_t i = 0;

    while(string[i] != '\0') {
        char_t c = string[i];

        if(c == '\n') {
            data[0] = '\r';
            ST->console_output->output_string(ST->console_output, data);
        }

        data[0] = c;

        ST->console_output->output_string(ST->console_output, data);

        data[0] = 0;

        i++;
    }
}

void video_text_print(const char_t* str);
void video_text_print(const char_t* str) {
    video_print(str);
}

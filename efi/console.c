/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include "setup.h"
#include <efi.h>
#include <memory.h>
#include <strings.h>
#include <utils.h>

MODULE("turnstone.efi");

extern efi_system_table_t* ST;

void   video_print(const char_t* string);
void   video_clear_screen(void);
size_t video_printf(const char_t* fmt, ...);

void video_clear_screen(void) {
    ST->console_output->clear_screen(ST->console_output);
}

void video_print(const char_t* string)
{
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

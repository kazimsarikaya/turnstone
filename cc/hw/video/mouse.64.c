/**
 * @file mouse.64.c
 * @brief mouse mask
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */


#include <types.h>
#include <utils.h>
#include <graphics/png.h>
#include <device/mouse.h>
#include <buffer.h>
#include <cpu/task.h>


MODULE("turnstone.kernel.hw.video");

void video_text_print(const char_t* string);

extern uint8_t mouse_icon_data_start;
extern uint8_t mouse_icon_data_end;
extern buffer_t* mouse_buffer;
extern uint64_t shell_task_id;
extern uint64_t windowmanager_task_id;

mouse_move_cursor_f MOUSE_MOVE_CURSOR = NULL;

graphics_raw_image_t* mouse_get_image(void) {
    uint8_t* tga_image = (uint8_t*)&mouse_icon_data_start;
    uint64_t mouse_data_size = &mouse_icon_data_end - &mouse_icon_data_start;

    uint32_t size = mouse_data_size;

    return graphics_load_png_image(tga_image, size);
}

int8_t mouse_report(mouse_report_t * report) {
    if(!report) {
        return -1;
    }

    if(mouse_buffer) {
        buffer_append_bytes(mouse_buffer, (uint8_t*)report, sizeof(mouse_report_t));

        if(shell_task_id != 0) {
            task_set_interrupt_received(shell_task_id);
        } else if(windowmanager_task_id != 0) {
            task_set_interrupt_received(windowmanager_task_id);
        } else {
            video_text_print("no task to notify\n");
        }
    }

    return 0;
}

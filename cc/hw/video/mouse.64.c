/**
 * @file mouse.64.c
 * @brief mouse mask
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */


#include <types.h>
#include <utils.h>
#include <video.h>
#include <graphics/image.h>
#include <device/mouse.h>
#include <buffer.h>
#include <cpu/task.h>


MODULE("turnstone.kernel.hw.video");

extern uint8_t mouse_icon_data_start;
extern uint8_t mouse_icon_data_end;
extern buffer_t* mouse_buffer;
extern uint64_t shell_task_id;
extern uint64_t windowmanager_task_id;

mouse_move_cursor_f MOUSE_MOVE_CURSOR = NULL;

graphics_raw_image_t* mouse_get_image(void) {
    graphics_tga_image_t* tga_image = (graphics_tga_image_t*)&mouse_icon_data_start;
    uint64_t mouse_data_size = &mouse_icon_data_end - &mouse_icon_data_start;

    uint32_t size = mouse_data_size;

    return graphics_load_tga_image(tga_image, size);
}

int8_t mouse_report(mouse_report_t * report) {
    if(!report) {
        return -1;
    }

    if(mouse_buffer) {
        buffer_append_bytes(mouse_buffer, (uint8_t*)report, sizeof(mouse_report_t));

        if(shell_task_id != 0) {
            task_set_interrupt_received(shell_task_id);
        }

        if(windowmanager_task_id != 0) {
            task_set_interrupt_received(windowmanager_task_id);
        }
    }

    return 0;
}

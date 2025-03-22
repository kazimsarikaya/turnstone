/**
 * @file wnd_mouse.64.c
 * @brief window mouse operations
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <windowmanager/wnd_mouse.h>
#include <windowmanager/wnd_utils.h>
#include <device/mouse.h>
#include <graphics/screen.h>
#include <memory.h>
#include <logging.h>
#include <utils.h>

MODULE("turnstone.windowmanager");

static graphics_raw_image_t* wndmgr_mouse_image = NULL;
static uint32_t wndmgr_last_mouse_x = 0;
static uint32_t wndmgr_last_mouse_y = 0;
static boolean_t wndmgr_mouse_initialized = false;

extern pixel_t* VIDEO_BASE_ADDRESS;


int8_t wndmgr_mouse_init(void) {
    wndmgr_mouse_image = mouse_get_image();

    if(wndmgr_mouse_image == NULL) {
        PRINTLOG(WINDOWMANAGER, LOG_ERROR, "Failed to get mouse image\n");
        return -1;
    }

    PRINTLOG(WINDOWMANAGER, LOG_INFO, "Mouse image loaded with size %dx%d", wndmgr_mouse_image->width, wndmgr_mouse_image->height);
    PRINTLOG(WINDOWMANAGER, LOG_INFO, "Mouse image first pixel: 0x%x", wndmgr_mouse_image->data[0]);
    PRINTLOG(WINDOWMANAGER, LOG_INFO, "Mouse image last pixel: 0x%x", wndmgr_mouse_image->data[wndmgr_mouse_image->width * wndmgr_mouse_image->height - 1]);

    MOUSE_MOVE_CURSOR = wndmgr_mouse_move_cursor;
    wndmgr_mouse_initialized = true;

    return 0;
}

static void wndmgr_mouse_restore_last_background(void) {
    if(!wndmgr_mouse_initialized) {
        return;
    }

    SCREEN_FLUSH(0, 0, wndmgr_last_mouse_x, wndmgr_last_mouse_y, wndmgr_mouse_image->width, wndmgr_mouse_image->height);
}

void wndmgr_mouse_move_cursor(uint32_t x, uint32_t y) {
    if(!wndmgr_mouse_initialized) {
        return;
    }

    if(wndmgr_last_mouse_x == x && wndmgr_last_mouse_y == y) {
        return;
    }

    screen_info_t screen_info = screen_get_info();

    uint32_t scw = screen_info.width;
    uint32_t sch = screen_info.height;

    if(wndmgr_last_mouse_x >= scw || wndmgr_last_mouse_y >= sch) {
        return;
    }

    if(x >= scw || y >= sch) {
        return;
    }

    wndmgr_mouse_restore_last_background();

    wndmgr_last_mouse_x = x;
    wndmgr_last_mouse_y = y;

    for(uint32_t i = 0; i < wndmgr_mouse_image->height; i++) {
        for(uint32_t j = 0; j < wndmgr_mouse_image->width; j++) {
            uint32_t abs_x = wndmgr_last_mouse_x + j;
            uint32_t abs_y = wndmgr_last_mouse_y + i;

            if(abs_x >= scw || abs_y >= sch) {
                continue;
            }

            pixel_t* fb_pixel = &VIDEO_BASE_ADDRESS[abs_y * screen_info.pixels_per_scanline + abs_x];
            pixel_t mouse_pixel = wndmgr_mouse_image->data[i * wndmgr_mouse_image->width + j];

            if(mouse_pixel == 0) {
                continue;
            }

            *fb_pixel = mouse_pixel;

        }
    }
}

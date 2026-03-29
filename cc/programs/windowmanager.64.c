/**
 * @file windowmanager.64.c
 * @brief Window Manager implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <windowmanager.h>
#include <windowmanager/wnd_types.h>
#include <windowmanager/wnd_utils.h>
#include <windowmanager/wnd_gfx.h>
#include <windowmanager/wnd_create_destroy.h>
#include <logging.h>
#include <memory.h>
#include <utils.h>
#include <hashmap.h>
#include <cpu.h>
#include <cpu/task.h>
#include <utils.h>
#include <device/event.h>
#include <device/mouse.h>
#include <device/kbd.h>
#include <device/kbd_scancodes.h>
#include <strings.h>
#include <graphics/screen.h>
#include <graphics/font.h>
#include <graphics/softgfx.h>
#include <time.h>
#include <math_f32.h>

MODULE("turnstone.user.programs.windowmanager");

void video_text_print(const char_t* text);

static void windowmanager_handle_events(windowmanager_t* wndmgr) {
    if(buffer_get_length(kbd_buffer) == 0 && buffer_get_length(mouse_buffer) == 0) {
        return;
    }

    uint32_t font_width = 0, font_height = 0;

    font_get_font_dimension(&font_width, &font_height);

    uint64_t kbd_length   = 0;
    uint32_t kbd_ev_cnt   = 0;
    uint64_t mouse_length = 0;
    uint32_t mouse_ev_cnt = 0;

    kbd_report_t* kbd_data     = (kbd_report_t*)(void*)buffer_get_all_bytes_and_reset(kbd_buffer, &kbd_length);
    mouse_report_t* mouse_data = (mouse_report_t*)(void*)buffer_get_all_bytes_and_reset(mouse_buffer, &mouse_length);

    if(kbd_length == 0 && mouse_length == 0) {
        memory_free(kbd_data);
        memory_free(mouse_data);

        return;
    }

    if(mouse_length) {
        mouse_ev_cnt = mouse_length / sizeof(mouse_report_t);

        for(uint32_t i = 0; i < mouse_ev_cnt; i++) {

            if(mouse_data[i].wheel != 0) {
                window_event_t event = {0};
                event.window = wndmgr->current_window;

                if(mouse_data[i].wheel > 0) {
                    event.type = WINDOW_EVENT_TYPE_SCROLL_UP;
                } else {
                    event.type = WINDOW_EVENT_TYPE_SCROLL_DOWN;
                }

                windowmanager_scroll(wndmgr->current_window, &event);
            }
        }

        mouse_report_t* last = &mouse_data[mouse_ev_cnt - 1];

        if((last->buttons & MOUSE_BUTTON_LEFT) && !wndmgr->has_alert) {
            wndmgr_text_cursor_move(last->x / font_width, last->y / font_height);
        }

        wndmgr_mouse_move_cursor(wndmgr, last->x, last->y);
    }

    memory_free(mouse_data);


    if(kbd_length == 0) {
        memory_free(kbd_data);

        return;
    }

    char16_t data[4096];
    uint32_t data_idx = 0;
    data[data_idx] = NULL;

    kbd_ev_cnt = kbd_length / sizeof(kbd_report_t);

    for(uint32_t i = 0; i < kbd_ev_cnt; i++) {
        if(kbd_data[i].is_pressed) {

            if(wndmgr->has_alert && kbd_data[i].key != '\n') {
                continue;
            }

            if(kbd_data[i].is_printable) {
                if(kbd_data[i].key == '\n') {
                    window_event_t event = {0};
                    event.type   = WINDOW_EVENT_TYPE_ENTER;
                    event.window = wndmgr->current_window;
                    windowmanager_enter(wndmgr->current_window, &event);
                } else if(kbd_data[i].key == '\t') {
                    boolean_t is_reverse = false;

                    if(kbd_data[i].state.is_shift_pressed) {
                        is_reverse = true;
                    }

                    wndmgr_move_cursor_to_next_input(wndmgr->current_window, is_reverse);
                }else {
                    data[data_idx++] = kbd_data[i].key;
                }
            } else {
                if(kbd_data[i].key == KBD_SCANCODE_BACKSPACE) {
                    data[data_idx++] = '\b';
                    data[data_idx++] = ' ';
                    data[data_idx++] = '\b';
                } else if(kbd_data[i].key == KBD_SCANCODE_F2) {
                    window_t* options_window = windowmanager_create_primary_options_window();

                    if(options_window != NULL) {
                        windowmanager_insert_and_set_current_window(options_window);
                    }
                } else if(kbd_data[i].key == KBD_SCANCODE_F3) {
                    windowmanager_remove_and_set_current_window(wndmgr->current_window);
                } else if(kbd_data[i].key == KBD_SCANCODE_F4) {
                    wndmgr_mark_all_windows_dirty(wndmgr->current_window);
                } else if(kbd_data[i].key == KBD_SCANCODE_UP) {
                    wndmgr_text_cursor_move_relative(0, -1);
                } else if(kbd_data[i].key == KBD_SCANCODE_DOWN) {
                    wndmgr_text_cursor_move_relative(0, 1);
                } else if(kbd_data[i].key == KBD_SCANCODE_LEFT) {
                    wndmgr_text_cursor_move_relative(-1, 0);
                } else if(kbd_data[i].key == KBD_SCANCODE_RIGHT) {
                    wndmgr_text_cursor_move_relative(1, 0);
                } else if(kbd_data[i].key == KBD_SCANCODE_F5) {
                    window_event_t event = {0};
                    event.type   = WINDOW_EVENT_TYPE_SCROLL_LEFT;
                    event.window = wndmgr->current_window;
                    windowmanager_scroll(wndmgr->current_window, &event);
                } else if(kbd_data[i].key == KBD_SCANCODE_F6 || kbd_data[i].key == KBD_SCANCODE_PAGEUP) {
                    window_event_t event = {0};
                    event.type   = WINDOW_EVENT_TYPE_SCROLL_UP;
                    event.window = wndmgr->current_window;
                    windowmanager_scroll(wndmgr->current_window, &event);
                } else if(kbd_data[i].key == KBD_SCANCODE_F7 || kbd_data[i].key == KBD_SCANCODE_PAGEDOWN) {
                    window_event_t event = {0};
                    event.type   = WINDOW_EVENT_TYPE_SCROLL_DOWN;
                    event.window = wndmgr->current_window;
                    windowmanager_scroll(wndmgr->current_window, &event);
                } else if(kbd_data[i].key == KBD_SCANCODE_F8) {
                    window_event_t event = {0};
                    event.type   = WINDOW_EVENT_TYPE_SCROLL_RIGHT;
                    event.window = wndmgr->current_window;
                    windowmanager_scroll(wndmgr->current_window, &event);
                } else if(kbd_data[i].key == KBD_SCANCODE_PRINTSCREEN) {
                    // clipboard_send_text("hello world from turnstone os!");
                }

            }
        }
    }

    data[data_idx] = NULL;

    memory_free(kbd_data);

    const window_t* edit_area = NULL;

    if(wstrlen(data) && wndmgr_find_window_by_text_cursor(wndmgr->current_window, &edit_area)) {
        if(edit_area != NULL) {
            wndmgr_set_window_text(edit_area, data);
        }
    }
}

static int8_t windowmanager_main(void) {
    PRINTLOG(WINDOWMANAGER, LOG_INFO, "Window Manager initializing");
    task_set_interruptible();

    boolean_t test_trigangle = false;
    boolean_t print_fps      = false;

    if(!test_trigangle) {
        task_set_interrupt_receive_workaround(1000 / 5);
    }

    kbd_buffer   = buffer_new_with_capacity(NULL, 4100);
    mouse_buffer = buffer_new_with_capacity(NULL, 4096);

    windowmanager_t* wndmgr = windowmanager_get_instance();

    if(wndmgr == NULL) {
        PRINTLOG(WINDOWMANAGER, LOG_ERROR, "Failed to get windowmanager instance\n");
        return -1;
    }

    sgfx_context_t* gfx_ctx = wndmgr->gfx_ctx;

    if(wndmgr_mouse_init(wndmgr) != 0) {
        PRINTLOG(WINDOWMANAGER, LOG_ERROR, "Failed to initialize mouse\n");
    }

    if(wndmgr_font_init(wndmgr) != 0) {
        PRINTLOG(WINDOWMANAGER, LOG_ERROR, "Failed to initialize font\n");
    }

#if 0
    float32_t virtual_w = 1920.0f;
    float32_t virtual_h = 1080.0f;

    // Real screen resolution
    float32_t real_w = (float32_t)wndmgr->screen_width;
    float32_t real_h = (float32_t)wndmgr->screen_height;

    // Compute scale only if real screen is smaller
    float32_t scale_x = 1.0f;
    float32_t scale_y = 1.0f;

    if(real_w < virtual_w) {
        scale_x = real_w / virtual_w;
    } else {
        virtual_w = real_w;
    }

    if(real_h < virtual_h) {
        scale_y = real_h / virtual_h;
    } else {
        virtual_h = real_h;
    }

    // Use the smaller scale to preserve aspect ratio
    float32_t scale = math_min_f32(scale_x, scale_y);

    // Update "virtual screen" for window manager
    wndmgr->screen_width  = virtual_w;
    wndmgr->screen_height = virtual_h;

    // Save scale for SGFX
    wndmgr->scale_x = scale;
    wndmgr->scale_y = scale;

    screen_set_dimensions(virtual_w, virtual_h, virtual_w);

    PRINTLOG(WINDOWMANAGER, LOG_INFO, "Screen: real: %ux%u, virtual: %ux%u, scale: %f",
             (uint32_t)real_w, (uint32_t)real_h,
             wndmgr->screen_width, wndmgr->screen_height,
             scale);
#endif

    wndmgr->scale_x = 1.0f;
    wndmgr->scale_y = 1.0f;
    wndmgr->scale_z = 1.0f;

    wndmgr->font_width  *= wndmgr->scale_x;
    wndmgr->font_height *= wndmgr->scale_y;

    wndmgr->translate_x = 0.0f;
    wndmgr->translate_y = 0.0f;
    wndmgr->translate_z = 0.0f;

    wndmgr->rotate_angle = 0.0f;
    wndmgr->rotate_x     = 0.0f;
    wndmgr->rotate_y     = 0.0f;
    wndmgr->rotate_z     = 0.0f;

    wndmgr->current_window = windowmanager_create_greater_window();

    if(wndmgr->current_window == NULL) {
        PRINTLOG(WINDOWMANAGER, LOG_ERROR, "Failed to create main window");
        return -1;
    }

    PRINTLOG(WINDOWMANAGER, LOG_INFO, "Window Manager initialized, waiting events");

    windowmanager_set_initialized(true);

    sgfx_clear(gfx_ctx, 0.0f, 0.0f, 0.0f, 1.0f);
    sgfx_swap_buffers(gfx_ctx);

    sgfx_matrix_mode(gfx_ctx, SGFX_MATRIX_MODE_PROJECTION);
    sgfx_load_identity(gfx_ctx);
    sgfx_ortho_f32(gfx_ctx,
                   0.0f, (float32_t)wndmgr->screen_width,
                   (float32_t)wndmgr->screen_height, 0.0f,
                   -1.0f, 1.0f);

    sgfx_matrix_mode(gfx_ctx, SGFX_MATRIX_MODE_MODELVIEW);
    sgfx_load_identity(gfx_ctx);
    sgfx_scale_f32(gfx_ctx, wndmgr->scale_x, wndmgr->scale_y, wndmgr->scale_z);
    sgfx_translate_f32(gfx_ctx, wndmgr->translate_x, wndmgr->translate_y, wndmgr->translate_z);
    sgfx_rotate_f32(gfx_ctx, wndmgr->rotate_angle, wndmgr->rotate_x, wndmgr->rotate_y, wndmgr->rotate_z);

    float32_t angle = 0.0f;

    uint64_t start_time  = 0;
    uint64_t end_time    = 0;
    uint64_t clear_start = 0;
    uint64_t clear_end   = 0;
    uint64_t swap_start  = 0;
    uint64_t swap_end    = 0;

    while(windowmanager_is_initialized()) {
        start_time = time_us(NULL);

        uint64_t event_start = time_us(NULL);
        windowmanager_handle_events(wndmgr);
        uint64_t event_end = time_us(NULL);

        if(test_trigangle) {
            clear_start = time_us(NULL);
            sgfx_clear(gfx_ctx, 0.0f, 0.0f, 0.0f, 1.0f);
            clear_end = time_us(NULL);

            sgfx_matrix_mode(gfx_ctx, SGFX_MATRIX_MODE_PROJECTION);
            sgfx_load_identity(gfx_ctx);
            sgfx_ortho_f32(gfx_ctx,
                           -1.0f, 1.0f,
                           -1.0f, 1.0f,
                           -1.0f, 1.0f);

            sgfx_matrix_mode(gfx_ctx, SGFX_MATRIX_MODE_MODELVIEW);
            sgfx_load_identity(gfx_ctx);
            sgfx_rotate_f32(gfx_ctx, angle, 0.0f, 0.0f, 1.0f);
            sgfx_scale_f32(gfx_ctx, 0.5f, 0.5f, 1.0f);

            sgfx_begin(gfx_ctx, SGFX_DRAW_MODE_TRIANGLES);
            sgfx_color4_f32(gfx_ctx, 1.0f, 0.0f, 0.0f, 1.0f);
            sgfx_vertex3_f32(gfx_ctx, 0.0f, 0.5f, 0.0f);
            sgfx_color4_f32(gfx_ctx, 0.0f, 1.0f, 0.0f, 1.0f);
            sgfx_vertex3_f32(gfx_ctx, -0.5f, -0.5f, 0.0f);
            sgfx_color4_f32(gfx_ctx, 0.0f, 0.0f, 1.0f, 1.0f);
            sgfx_vertex3_f32(gfx_ctx, 0.5f, -0.5f, 0.0f);
            sgfx_end(gfx_ctx);
        } else {
            windowmanager_draw_window(wndmgr, wndmgr->current_window);
            if(print_fps) {
                video_text_print("--------------------------------\n");
            }
        }

        // Swap buffers (copies diff to framebuffer)
        swap_start = time_us(NULL);
        sgfx_swap_buffers(gfx_ctx);
        swap_end = time_us(NULL);

        end_time = time_us(NULL);

        uint64_t frame_time = end_time - start_time;

        wndmgr->previous_render_time = frame_time;

        if(print_fps) {
            if(test_trigangle) {
                char_t* fps_str = strprintf("WM: %llu us, evt: %llu us, clr: %llu us, swp: %llu us fps: %04.02f\n",
                                            frame_time,
                                            event_end - event_start,
                                            clear_end - clear_start,
                                            swap_end - swap_start,
                                            (frame_time) ? (1000000.0f / (float32_t)frame_time) : 1000000.0f
                                            );
                video_text_print(fps_str);
                memory_free(fps_str);
            } else {
                char_t* fps_str = strprintf("WM: %llu us, evt: %llu us, swp: %llu us fps: %04.02f\n",
                                            frame_time,
                                            event_end - event_start,
                                            swap_end - swap_start,
                                            (frame_time) ? (1000000.0f / (float32_t)frame_time) : 1000000.0f
                                            );
                video_text_print(fps_str);
                memory_free(fps_str);
            }
        }

        if(test_trigangle) {
            angle += 0.5f;
            if(angle >= 360.0f) {
                angle = 0.0f;
            }

            task_msleep(16); // ~60 FPS
        } else {
            task_yield_with_message_waiting();
        }
    }

    return 0;
}

uint64_t windowmanager_task_id = 0;

int8_t windowmanager_init(void) {
    windowmanager_task_id = task_create_task("windowmanager", windowmanager_main, .heap_size = 64 << 20, 2 << 20);

    if(windowmanager_task_id == -1ULL) {
        PRINTLOG(WINDOWMANAGER, LOG_ERROR, "Failed to create windowmanager task\n");
        return -1;
    }

    while(!windowmanager_is_initialized()) {
        cpu_sti();
        cpu_idle();
    }

    return 0;
}

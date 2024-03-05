/**
 * @file shell.64.c
 * @brief Shell
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <shell.h>
#include <video.h>
#include <cpu/task.h>
#include <logging.h>
#include <strings.h>
#include <acpi.h>
#include <time.h>
#include <driver/usb.h>
#include <memory/frame.h>
#include <windowmanager.h>
#include <stdbufs.h>
#include <device/mouse.h>
#include <device/kbd.h>
#include <device/kbd_scancodes.h>
#include <utils.h>

MODULE("turnstone.user.programs.shell");

int32_t shell_main(int32_t argc, char* argv[]);
int8_t  shell_process_command(buffer_t* command_buffer, buffer_t* argument_buffer);

buffer_t* shell_buffer = NULL;
buffer_t* mouse_buffer = NULL;


int8_t  shell_process_command(buffer_t* command_buffer, buffer_t* argument_buffer) {
    char_t* command = (char_t*)buffer_get_all_bytes_and_reset(command_buffer, NULL);

    if(command == NULL) {
        return -1;
    }

    char_t* arguments = (char_t*)buffer_get_all_bytes_and_reset(argument_buffer, NULL);
    UNUSED(arguments);

    int8_t res = -1;

    if(strcmp(command, "help") == 0) {
        printf("Commands:\n"
               "\thelp\t\t: prints this help\n"
               "\tclear\t\t: clears the screen\n"
               "\tpoweroff\t: powers off the system alias shutdown\n"
               "\treboot\t\t: reboots the system\n"
               "\tcolor\t\t: changes the color first argument foreground second is background in hex\n"
               "\tps\t\t: prints the current processes\n"
               "\tdate\t\t: prints the current date with time alias time\n"
               "\tusbprobe\t: probes the USB bus\n"
               "\tfree\t\t: prints the frame usage\n"
               "\twm\t\t: opens test window\n"
               );
        res = 0;
    } else if(strcmp(command, "clear") == 0) {
        video_clear_screen();
        res = 0;
    } else if(strcmp(command, "poweroff") == 0 || strcmp(command, "shutdown") == 0) {
        acpi_poweroff();
    } else if(strcmp(command, "reboot") == 0) {
        acpi_reset();
    } else if(strcmp(command, "color") == 0) {
        if(arguments == NULL) {
            printf("Usage: color <foreground> <background>\n");
            res = -1;
        } else {
            uint32_t foreground = 0;
            uint32_t background = 0;

            if(strlen(arguments) == 6) {
                foreground = atoh(arguments);
                res = 0;
            } else if(strlen(arguments) == 13 && arguments[6] == ' ') {
                arguments[6] = 0;
                foreground = atoh(arguments);
                background = atoh(arguments + 7);
                res = 0;
            } else {
                printf("Usage: color <foreground> <background>\n");
                printf("\tgiven arguments: %s %lli\n", arguments, strlen(arguments));
                res = -1;
            }

            if(res != -1) {
                printf("request foreground: %x background: %x\n", foreground, background);
                video_set_color(foreground, background);
            }
        }
    } else if(strcmp(command, "ps") == 0) {
        task_print_all();
        res = 0;
    } else if(strcmp(command, "date") == 0 || strcmp(command, "time") == 0) {
        timeparsed_t tp;
        timeparsed(&tp);

        printf("\t%04i-%02i-%02i %02i:%02i:%02i\n", tp.year, tp.month, tp.day, tp.hours, tp.minutes, tp.seconds);

        res = 0;
    } else if(strcmp(command, "usbprobe") == 0) {
        res = usb_probe_all_devices_all_ports();
    } else if(strcmp(command, "free") == 0) {
        printf("\tfree frames: 0x%llx\n\tallocated frames: 0x%llx\n\ttotal frames: 0x%llx\n",
               KERNEL_FRAME_ALLOCATOR->get_free_frame_count(KERNEL_FRAME_ALLOCATOR),
               KERNEL_FRAME_ALLOCATOR->get_allocated_frame_count(KERNEL_FRAME_ALLOCATOR),
               KERNEL_FRAME_ALLOCATOR->get_total_frame_count(KERNEL_FRAME_ALLOCATOR));
        res = 0;
    } else if(strcmp(command, "wm") == 0) {
        res = windowmanager_init();
    } else {
        printf("Unknown command: %s\n", command);
        res = -1;
    }


    memory_free(command);
    memory_free(arguments);

    return res;
}

static uint32_t shell_append_wchar_to_buffer(wchar_t src, char_t* dst, uint32_t dst_idx) {
    if(dst == NULL) {
        return NULL;
    }

    int64_t j = dst_idx;

    if(src >= 0x800) {
        dst[j++] = ((src >> 12) & 0xF) | 0xE0;
        dst[j++] = ((src >> 6) & 0x3F) | 0x80;
        dst[j++] = (src & 0x3F) | 0x80;
    } else if(src >= 0x80) {
        dst[j++] = ((src >> 6) & 0x1F) | 0xC0;
        dst[j++] = (src & 0x3F) | 0x80;
    } else {
        dst[j++] = src & 0x7F;
    }

    return j;
}

void video_text_print(const char_t* string);

int32_t shell_main(int32_t argc, char* argv[]) {
    UNUSED(argc);
    UNUSED(argv);

    task_set_interruptible();

    shell_buffer = buffer_new_with_capacity(NULL, 4100);
    mouse_buffer = buffer_new_with_capacity(NULL, 4096);
    buffer_t* command_buffer = buffer_new_with_capacity(NULL, 4096);
    buffer_t* argument_buffer = buffer_new_with_capacity(NULL, 4096);
    boolean_t first_space = false;

    while(true) {
        uint64_t kbd_length = 0;
        uint32_t kbd_ev_cnt = 0;
        uint64_t mouse_length = 0;
        uint32_t mouse_ev_cnt = 0;

        kbd_report_t* kbd_data = (kbd_report_t*)buffer_get_all_bytes_and_reset(shell_buffer, &kbd_length);
        mouse_report_t* mouse_data = (mouse_report_t*)buffer_get_all_bytes_and_reset(mouse_buffer, &mouse_length);

        if(kbd_length == 0 && mouse_length == 0) {
            memory_free(kbd_data);
            memory_free(mouse_data);
            task_set_message_waiting();
            task_yield();

            continue;
        }

        if(mouse_length) {
            mouse_ev_cnt = mouse_length / sizeof(mouse_report_t);

            if(VIDEO_MOVE_CURSOR) {
                VIDEO_MOVE_CURSOR(mouse_data[mouse_ev_cnt - 1].x, mouse_data[mouse_ev_cnt - 1].y);
            }
        }

        memory_free(mouse_data);

        if(kbd_length == 0) {
            memory_free(kbd_data);
            task_set_message_waiting();
            task_yield();

            continue;
        }

        char_t data[4096] = {0};
        uint32_t data_idx = 0;

        kbd_ev_cnt = kbd_length / sizeof(kbd_report_t);

        for(uint32_t i = 0; i < kbd_ev_cnt; i++) {
            if(kbd_data[i].is_pressed) {
                if(kbd_data[i].is_printable) {
                    data_idx = shell_append_wchar_to_buffer(kbd_data[i].key, data, data_idx);
                } else {
                    if(kbd_data[i].key == KBD_SCANCODE_BACKSPACE) {
                        data[data_idx++] = '\b';
                        data[data_idx++] = ' ';
                        data[data_idx++] = '\b';
                    }
                }
            }
        }

        data[data_idx] = NULL;

        memory_free(kbd_data);

        char_t last_char = data[4095];

        if(last_char != NULL) {
            data[4095] = NULL;
        }

        printf("%s", data);

        if(last_char != NULL) {
            printf("%c", last_char);
        }

        data[4095] = last_char;

        uint64_t idx = 0;

        while(data_idx > 0) {
            char_t c = data[idx++];
            data_idx--;

            if(c == '\n') {
                first_space = false;
                buffer_append_byte(argument_buffer, NULL);

                if(shell_process_command(command_buffer, argument_buffer) == -1) {
                    printf("Command failed\n");
                }

                printf("$ ");

                break;
            }

            if(c == '\b') {
                data_idx -= 2; // remove ' \b'
                idx += 2; // remove ' \b'

                if(first_space) {
                    buffer_seek(argument_buffer, -1, BUFFER_SEEK_DIRECTION_CURRENT);
                    buffer_append_byte(argument_buffer, NULL);
                    buffer_seek(argument_buffer, -1, BUFFER_SEEK_DIRECTION_CURRENT);
                } else {
                    buffer_seek(command_buffer, -1, BUFFER_SEEK_DIRECTION_CURRENT);
                    buffer_append_byte(command_buffer, NULL);
                    buffer_seek(command_buffer, -1, BUFFER_SEEK_DIRECTION_CURRENT);
                }

                continue;
            }

            if(first_space) {
                buffer_append_byte(argument_buffer, c);
            } else {
                if(c == ' ') {
                    first_space = true;
                    buffer_append_byte(command_buffer, NULL);
                    continue;
                }

                buffer_append_byte(command_buffer, c);
            }
        }
    }

    return 0;
}

uint64_t shell_task_id = 0;

int8_t shell_init(void) {
    shell_task_id = task_create_task(NULL, 32 << 20, 64 << 10, shell_main, 0, NULL, "shell");
    return shell_task_id == -1ULL ? -1 : 0;
}

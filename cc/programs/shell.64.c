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
#include <hypervisor/hypervisor.h>
#include <hypervisor/hypervisor_ipc.h>
#include <list.h>
#include <tosdb/tosdb_manager.h>
#include <linker.h>

MODULE("turnstone.user.programs.shell");

int32_t shell_main(int32_t argc, char* argv[]);
int8_t  shell_process_command(buffer_t* command_buffer, buffer_t* argument_buffer);

buffer_t* shell_buffer = NULL;
buffer_t* mouse_buffer = NULL;

typedef struct shell_argument_parser_t {
    char_t*  arguments;
    uint32_t idx;
} shell_argument_parser_t;

static char_t* shell_argument_parser_advance(shell_argument_parser_t* parser) {
    if(parser == NULL) {
        return NULL;
    }

    if(parser->arguments == NULL) {
        return NULL;
    }

    if(parser->arguments[parser->idx] == NULL) {
        return NULL;
    }

    uint32_t i = parser->idx;

    while(parser->arguments[i] == ' ') { // trim spaces
        i++;
    }

    char_t* start = &parser->arguments[i]; // save start

    if(start[0] == '\'' || start[0] == '\"'){
        char_t quote = start[0];
        start++;
        i++;

        while(parser->arguments[i] != quote && parser->arguments[i] != NULL) { // find end
            i++;
        }

        if(parser->arguments[i] == quote) {
            parser->arguments[i] = NULL;
            i++;
        } else {
            printf("Cannot parse argument: -%s-\n", start);
            return NULL;
        }
    } else {
        while(parser->arguments[i] != ' ' && parser->arguments[i] != NULL) { // find end
            i++;
        }

        if(parser->arguments[i] == ' ') { // replace space with null
            parser->arguments[i] = NULL;
            i++;
        }
    }

    parser->idx = i; // save new index

    return start;
}


static int8_t shell_handle_module_command(char_t* arguments) {
    shell_argument_parser_t parser = {arguments, 0};

    char_t* command = shell_argument_parser_advance(&parser);

    if(strncmp("list", command, 6) == 0) {
        linker_print_modules_at_memory();
        return 0;
    }

    uint64_t module_id = atoh(command);

    if(module_id == 0) {
        printf("cannot parse module id: -%s-\n", command);
        printf("Usage: module <list>\n");
        printf("Usage: module id <info>\n");
        return -1;
    }

    command = shell_argument_parser_advance(&parser);

    if(strncmp(command, "info", 4) == 0){
        linker_print_module_info_at_memory(module_id);
        return 0;
    }

    printf("Unknown command: %s\n", command);
    printf("Usage: module <list>\n");
    printf("Usage: module id <info>\n");

    return -1;
}

static int8_t shell_handle_tosdb_command(char_t* arguments) {
    shell_argument_parser_t parser = {arguments, 0};

    char_t* command = shell_argument_parser_advance(&parser);


    if(strncmp("close", command, 5) == 0) {
        return tosdb_manager_close();
    } else if(strncmp("init", command, 4) == 0) {
        return tosdb_manager_init();
    } else if(strncmp("clear", command, 5) == 0) {
        // clear takes a force argument
        char_t* force = shell_argument_parser_advance(&parser);

        if(strncmp(force, "force", 5) == 0) {
            return tosdb_manager_clear();
        }

        printf("Usage: tosdb clear force\n");
        return -1;
    }

    printf("Unknown command: %s\n", command);
    printf("Usage: tosdb <close|init|build_program <entry_point>>\n");

    return -1;
}

static int8_t shell_handle_vm_command(char_t* arguments) {
    shell_argument_parser_t parser = {arguments, 0};

    char_t* command = shell_argument_parser_advance(&parser);

    if(strncmp("create", command, 6) == 0) {
        char_t* entrypoint = shell_argument_parser_advance(&parser);

        if(entrypoint == NULL) {
            printf("Usage: vm create <entrypoint_name>\n");
            return -1;
        }

        printf("Creating VM with entrypoint: -%s-\n", entrypoint);

        return hypervisor_vm_create(strdup(entrypoint));
    }

    uint64_t vmid = atoh(command);

    if(vmid == 0) {
        printf("cannot parse vmid: -%s-\n", command);
        printf("Usage: vm <vmid> <command>\n");
        printf("Usage: vm create <entrypoint_name>\n");
        return -1;
    }

    command = shell_argument_parser_advance(&parser);

    if(strncmp(command, "output", 6) == 0) {

        buffer_t* buffer = task_get_task_output_buffer(vmid);

        if(!buffer) {
            printf("VM not found: 0x%llx\n", vmid);
            return -1;
        }

        uint8_t* buffer_data = buffer_get_view_at_position(buffer, 0, buffer_get_length(buffer));

        if(!buffer_data) {
            printf("VM output not found\n");
            return -1;
        }

        printf("VM 0x%llx output:\n", vmid);
        printf("%s", buffer_data);
        printf("\n");
    } else if(strncmp(command, "dump", 4) == 0) {
        list_t* vm_mq = task_get_message_queue(vmid, 0);

        if(!vm_mq) {
            printf("VM not found: 0x%llx\n", vmid);
            return -1;
        }

        hypervisor_ipc_message_t* msg = memory_malloc(sizeof(hypervisor_ipc_message_t));

        if(!msg) {
            printf("Failed to allocate memory\n");
            return -1;
        }

        msg->message_type = HYPERVISOR_IPC_MESSAGE_TYPE_DUMP;
        msg->message_data = buffer_new();

        if(!msg->message_data) {
            printf("Failed to allocate memory\n");
            memory_free(msg);
            return -1;
        }

        list_queue_push(vm_mq, msg);

        while(!msg->message_data_completed) {
            task_yield();
        }

        uint8_t* msg_data = buffer_get_all_bytes_and_destroy(msg->message_data, NULL);

        memory_free(msg);

        if(!msg_data) {
            printf("Failed to get VM state\n");
            return -1;
        }

        printf("VM 0x%llx state:\n", vmid);
        printf("%s", msg_data);
        printf("\n");

        memory_free(msg_data);

    } else if(strncmp(command, "close", 5) == 0) {
        return hypervisor_ipc_send_close(vmid);
    } else {
        printf("Unknown command: %llx -%s-\n", vmid, command);
        printf("Usage: vm <vmid> <command>\n");
        printf("Usage: vm create <entrypoint_name>\n");
        printf("\toutput\t: prints the VM output\n");
        printf("\tdump\t: dumps the VM state\n");
        printf("\tcreate\t: creates a new VM with given entrypoint name\n");
        printf("\tclose\t: closes vm\n");
        return -1;
    }

    return 0;
}


int8_t  shell_process_command(buffer_t* command_buffer, buffer_t* argument_buffer) {
    char_t* command = (char_t*)buffer_get_all_bytes_and_reset(command_buffer, NULL);

    if(command == NULL) {
        return -1;
    }

    if(strlen(command) == 0) {
        memory_free(command);
        char_t* discard = (char_t*)buffer_get_all_bytes_and_reset(argument_buffer, NULL);
        memory_free(discard);
        return 0;
    }

    char_t* orig_command = command;

    while(*command == ' ') {
        command++;
    }

    char_t* arguments = (char_t*)buffer_get_all_bytes_and_reset(argument_buffer, NULL);

    shell_argument_parser_t parser = {arguments, 0};

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
               "\tvm\t\t: vm commands\n"
               "\trdtsc\t\t: read timestamp counter\n"
               "\ttosdb\t\t: tosdb commands\n"
               "\tkill\t\t: kills a process with pid\n"
               "\tmodule\t\t: module(library) utils\n"
               "\tlog\t\t: configures the log level\n"
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
        char_t* foreground_str = shell_argument_parser_advance(&parser);
        char_t* background_str = shell_argument_parser_advance(&parser);

        if(foreground_str == NULL && background_str == NULL) {
            printf("Usage: color <foreground> [<background>]\n");
            res = -1;
        } else {
            uint32_t foreground = atoh(foreground_str);
            uint32_t background = atoh(background_str);

            video_set_color(foreground, background);
            res = 0;
        }
    } else if(strcmp(command, "ps") == 0) {
        buffer_t* buffer = buffer_new();
        task_print_all(buffer);
        char_t* buffer_data = (char_t*)buffer_get_all_bytes_and_destroy(buffer, NULL);
        printf("%s", buffer_data);
        memory_free(buffer_data);
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
               frame_get_allocator()->get_free_frame_count(frame_get_allocator()),
               frame_get_allocator()->get_allocated_frame_count(frame_get_allocator()),
               frame_get_allocator()->get_total_frame_count(frame_get_allocator()));
        res = 0;
    } else if(strcmp(command, "wm") == 0) {
        res = windowmanager_init();
    } else if(strcmp(command, "vm") == 0) {
        res = shell_handle_vm_command(arguments);
    } else if(strcmp(command, "tosdb") == 0) {
        res = shell_handle_tosdb_command(arguments);
    } else if(strcmp(command, "module") == 0) {
        res = shell_handle_module_command(arguments);
    } else if(strcmp(command, "rdtsc") == 0) {
        printf("rdtsc: 0x%llx\n", rdtsc());
        res = 0;
    } else if(strcmp(command, "kill") == 0) {
        uint64_t pid = atoh(shell_argument_parser_advance(&parser));
        char_t* force_str = shell_argument_parser_advance(&parser);
        boolean_t force = false;

        if(strncmp(force_str, "force", 5) == 0) {
            force = true;
        }

        if(pid == 0) {
            printf("Usage: kill <pid>\n");
            printf("\tgiven arguments: %s\n", arguments);
            res = -1;
        } else {
            task_kill_task(pid, force);
            res = 0;
        }
    } else if(strcmp(command, "log") == 0) {
        char_t* log_module = shell_argument_parser_advance(&parser);
        char_t* log_level = shell_argument_parser_advance(&parser);

        if(!log_module || !log_level) {
            printf("Usage: log <module> <level>\n");
            res = -1;
        } else {
            res = logging_set_level_by_string_values(log_module, log_level);
        }
    } else {
        printf("Unknown command: %s\n", command);
        res = -1;
    }


    memory_free(orig_command);
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

        if(buffer_get_length(command_buffer) == 0) {
            while(data[idx] == ' ') {
                idx++;
                data_idx--;
            }
        }

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

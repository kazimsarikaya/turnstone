/**
 * @file shell.64.c
 * @brief Shell
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <shell.h>
#include <cpu/task.h>
#include <logging.h>
#include <strings.h>
#include <acpi.h>
#include <time.h>
#include <driver/usb.h>
#include <memory/frame.h>
#include <windowmanager.h>
#include <stdbufs.h>
#include <device/event.h>
#include <device/mouse.h>
#include <device/kbd.h>
#include <device/kbd_scancodes.h>
#include <utils.h>
#include <hypervisor/hypervisor.h>
#include <hypervisor/hypervisor_ipc.h>
#include <list.h>
#include <tosdb/tosdb_manager.h>
#include <linker.h>
#include <argumentparser.h>
#include <graphics/screen.h>

MODULE("turnstone.user.programs.shell");

int32_t shell_main(int32_t argc, char* argv[]);
int8_t  shell_process_command(buffer_t* command_buffer, buffer_t* argument_buffer);

static int8_t shell_handle_module_command(char16_t* arguments) {
    argument_parser_t parser = {arguments, 0};

    char16_t* command = argument_parser_advance(&parser);

    if(wstrncmp(u"list", command, 6) == 0) {
        linker_print_modules_at_memory();
        return 0;
    }

    uint64_t module_id = watoh(command);

    if(module_id == 0) {
        printf("cannot parse module id: -%hs-\n", command);
        printf("Usage: module <list>\n");
        printf("Usage: module id <info>\n");
        return -1;
    }

    command = argument_parser_advance(&parser);

    if(wstrncmp(command, u"info", 4) == 0) {
        linker_print_module_info_at_memory(module_id);
        return 0;
    }

    printf("Unknown command: %hs\n", command);
    printf("Usage: module <list>\n");
    printf("Usage: module id <info>\n");

    return -1;
}

static int8_t shell_handle_tosdb_command(char16_t* arguments) {
    argument_parser_t parser = {arguments, 0};

    char16_t* command = argument_parser_advance(&parser);


    if(wstrncmp(u"close", command, 5) == 0) {
        return tosdb_manager_close();
    } else if(wstrncmp(u"init", command, 4) == 0) {
        return tosdb_manager_init();
    } else if(wstrncmp(u"clear", command, 5) == 0) {
        // clear takes a force argument
        char16_t* force = argument_parser_advance(&parser);

        if(wstrncmp(force, u"force", 5) == 0) {
            return tosdb_manager_clear();
        }

        printf("Usage: tosdb clear force\n");
        return -1;
    }

    printf("Unknown command: %hs\n", command);
    printf("Usage: tosdb <close|init|build_program <entry_point>>\n");

    return -1;
}

static int8_t shell_handle_vm_command(char16_t* arguments) {
    argument_parser_t parser = {arguments, 0};

    char16_t* command = argument_parser_advance(&parser);

    if(wstrncmp(u"create", command, 6) == 0) {
        char16_t* entrypoint = argument_parser_advance(&parser);

        if(entrypoint == NULL) {
            printf("Usage: vm create <entrypoint_name>\n");
            return -1;
        }

        printf("Creating VM with entrypoint: -%hs-\n", entrypoint);

        char_t* c8_entrypoint = wstr_to_str(entrypoint);

        return hypervisor_vm_create(c8_entrypoint,
                                    2 << 20,
                                    1 << 20);
    }

    uint64_t vmid = watoh(command);

    if(vmid == 0) {
        printf("cannot parse vmid: -%hs-\n", command);
        printf("Usage: vm <vmid> <command>\n");
        printf("Usage: vm create <entrypoint_name>\n");
        return -1;
    }

    command = argument_parser_advance(&parser);

    if(wstrncmp(command, u"output", 6) == 0) {

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
    } else if(wstrncmp(command, u"dump", 4) == 0) {
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

    } else if(wstrncmp(command, u"close", 5) == 0) {
        return hypervisor_ipc_send_close(vmid);
    } else {
        printf("Unknown command: %llx -%hs-\n", vmid, command);
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
    char16_t* command = (char16_t*)(void*)buffer_get_all_bytes_and_reset(command_buffer, NULL);

    if(command == NULL) {
        return -1;
    }

    if(wstrlen(command) == 0) {
        memory_free(command);
        void* discard = buffer_get_all_bytes_and_reset(argument_buffer, NULL);
        memory_free(discard);
        return 0;
    }

    char16_t* orig_command = command;

    while(*command == ' ') {
        command++;
    }

    char16_t* arguments = (char16_t*)(void*)buffer_get_all_bytes_and_reset(argument_buffer, NULL);

    argument_parser_t parser = {arguments, 0};

    int8_t res = -1;

    if(wstrcmp(command, u"help") == 0) {
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
    } else if(wstrcmp(command, u"clear") == 0) {
        screen_clear();
        res = 0;
    } else if(wstrcmp(command, u"poweroff") == 0 || wstrcmp(command, u"shutdown") == 0) {
        acpi_poweroff();
    } else if(wstrcmp(command, u"reboot") == 0) {
        acpi_reset();
    } else if(wstrcmp(command, u"color") == 0) {
        char16_t* foreground_str = argument_parser_advance(&parser);
        char16_t* background_str = argument_parser_advance(&parser);

        if(foreground_str == NULL && background_str == NULL) {
            printf("Usage: color <foreground> [<background>]\n");
            res = -1;
        } else {
            uint32_t foreground = watoh(foreground_str);
            uint32_t background = watoh(background_str);

            screen_set_color((color_t){.color = foreground}, (color_t){.color = background});
            res = 0;
        }
    } else if(wstrcmp(command, u"ps") == 0) {
        buffer_t* buffer = buffer_new();
        task_print_all(buffer);
        char_t* buffer_data = (char_t*)buffer_get_all_bytes_and_destroy(buffer, NULL);
        printf("%s", buffer_data);
        memory_free(buffer_data);
        res = 0;
    } else if(wstrcmp(command, u"date") == 0 || wstrcmp(command, u"time") == 0) {
        timeparsed_t tp;
        timeparsed(&tp);

        printf("\t%04i-%02i-%02i %02i:%02i:%02i\n", tp.year, tp.month, tp.day, tp.hours, tp.minutes, tp.seconds);

        res = 0;
    } else if(wstrcmp(command, u"usbreset") == 0) {
        res = usb_reset_all_devices_all_ports();
    } else if(wstrcmp(command, u"free") == 0) {
        printf("\tfree frames: 0x%llx\n\tallocated frames: 0x%llx\n\ttotal frames: 0x%llx\n",
               frame_get_allocator()->get_free_frame_count(frame_get_allocator()),
               frame_get_allocator()->get_allocated_frame_count(frame_get_allocator()),
               frame_get_allocator()->get_total_frame_count(frame_get_allocator()));
        res = 0;
    } else if(wstrcmp(command, u"wm") == 0) {
        res = windowmanager_init();
    } else if(wstrcmp(command, u"vm") == 0) {
        res = shell_handle_vm_command(arguments);
    } else if(wstrcmp(command, u"tosdb") == 0) {
        res = shell_handle_tosdb_command(arguments);
    } else if(wstrcmp(command, u"module") == 0) {
        res = shell_handle_module_command(arguments);
    } else if(wstrcmp(command, u"rdtsc") == 0) {
        printf("rdtsc: 0x%llx\n", rdtsc());
        res = 0;
    } else if(wstrcmp(command, u"kill") == 0) {
        uint64_t pid        = watoh(argument_parser_advance(&parser));
        char16_t* force_str = argument_parser_advance(&parser);
        boolean_t force     = false;

        if(wstrncmp(force_str, u"force", 5) == 0) {
            force = true;
        }

        if(pid == 0) {
            printf("Usage: kill <pid>\n");
            printf("\tgiven arguments: %hs\n", arguments);
            res = -1;
        } else {
            task_kill_task(pid, force);
            res = 0;
        }
    } else if(wstrcmp(command, u"log") == 0) {
        char16_t* log_module = argument_parser_advance(&parser);
        char16_t* log_level  = argument_parser_advance(&parser);

        if(!log_module || !log_level) {
            printf("Usage: log <module> <level>\n");
            res = -1;
        } else {
            char_t* c8_log_module = wstr_to_str(log_module);
            char_t* c8_log_level  = wstr_to_str(log_level);
            res = logging_set_level_by_string_values(c8_log_module, c8_log_level);
            memory_free(c8_log_module);
            memory_free(c8_log_level);
        }
    } else {
        printf("Unknown command: %hs\n", command);
        res = -1;
    }


    memory_free(orig_command);
    memory_free(arguments);

    return res;
}

void video_text_print(const char_t* string);

int32_t shell_main(int32_t argc, char* argv[]) {
    UNUSED(argc);
    UNUSED(argv);

    task_set_interruptible();

    kbd_buffer   = buffer_new_with_capacity(NULL, 4100);
    mouse_buffer = buffer_new_with_capacity(NULL, 4096);
    buffer_t* command_buffer  = buffer_new_with_capacity(NULL, 4096);
    buffer_t* argument_buffer = buffer_new_with_capacity(NULL, 4096);
    boolean_t first_space     = false;

    while(true) {
        uint64_t kbd_length   = 0;
        uint32_t kbd_ev_cnt   = 0;
        uint64_t mouse_length = 0;
        uint32_t mouse_ev_cnt = 0;

        kbd_report_t* kbd_data     = (kbd_report_t*)(void*)buffer_get_all_bytes_and_reset(kbd_buffer, &kbd_length);
        mouse_report_t* mouse_data = (mouse_report_t*)(void*)buffer_get_all_bytes_and_reset(mouse_buffer, &mouse_length);

        if(kbd_length == 0 && mouse_length == 0) {
            memory_free(kbd_data);
            memory_free(mouse_data);
            task_yield_with_message_waiting();

            continue;
        }

        if(mouse_length) {
            mouse_ev_cnt = mouse_length / sizeof(mouse_report_t);
            UNUSED(mouse_ev_cnt);
        }

        memory_free(mouse_data);

        if(kbd_length == 0) {
            memory_free(kbd_data);
            task_yield_with_message_waiting();

            continue;
        }

        char16_t data[4096];
        uint32_t data_idx = 0;
        data[data_idx] = NULL;

        kbd_ev_cnt = kbd_length / sizeof(kbd_report_t);

        for(uint32_t i = 0; i < kbd_ev_cnt; i++) {
            if(kbd_data[i].is_pressed) {
                if(kbd_data[i].is_printable) {
                    data[data_idx++] = kbd_data[i].key;
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

        char16_t last_char = data[4095];

        if(last_char != NULL) {
            data[4095] = NULL;
        }

        printf("%hs", data);

        if(last_char != NULL) {
            printf("%hc", last_char);
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
            char16_t c = data[idx++];
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
                idx      += 2; // remove ' \b'

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

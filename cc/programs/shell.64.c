/**
 * @file shell.64.c
 * @brief Shell
 */

#include <shell.h>
#include <cpu/task.h>
#include <video.h>
#include <strings.h>
#include <acpi.h>

MODULE("turnstone.user.programs.shell");

int32_t shell_main(int32_t argc, char* argv[]);
int8_t  shell_process_command(buffer_t command_buffer, buffer_t argument_buffer);

buffer_t shell_buffer = NULL;


int8_t  shell_process_command(buffer_t command_buffer, buffer_t argument_buffer) {
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
               );
        res = 0;
    } else if(strcmp(command, "clear") == 0) {
        video_clear_screen();
        res = 0;
    } else if(strcmp(command, "poweroff") == 0 || strcmp(command, "shutdown") == 0) {
        acpi_poweroff();
    } else if(strcmp(command, "reboot") == 0) {
        acpi_reset();
    }


    memory_free(command);
    memory_free(arguments);

    return res;
}

int32_t shell_main(int32_t argc, char* argv[]) {
    UNUSED(argc);
    UNUSED(argv);

    shell_buffer = buffer_new_with_capacity(NULL, 4096);
    buffer_t command_buffer = buffer_new_with_capacity(NULL, 4096);
    buffer_t argument_buffer = buffer_new_with_capacity(NULL, 4096);
    boolean_t first_space = false;

    while(true) {
        uint64_t length = 0;

        uint8_t* data = buffer_get_all_bytes_and_reset(shell_buffer, &length);

        if(length == 0) {
            memory_free(data);
            task_yield();

            continue;
        }

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

        while(length > 0) {
            char_t c = data[idx++];
            length--;

            if(c == '\n') {
                first_space = false;
                buffer_append_byte(argument_buffer, NULL);

                if(shell_process_command(command_buffer, argument_buffer) == -1) {
                    printf("Command not found\n");
                }

                printf("$ ");

                break;
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

        memory_free(data);
    }

    return 0;
}









int8_t shell_init(void) {
    return task_create_task(NULL, 2 << 20, 64 << 10, shell_main, 0, NULL, "shell") == -1ULL ? -1 : 0;
}

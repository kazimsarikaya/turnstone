/**
 * @file vm_test_program.64.c
 * @brief VM Test Program
 *
 * This is a sample program that will run in the VM.
 * It has minimal dependencies and is used to test the VM.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */


#include <types.h>
#include <cpu.h>
#include <ports.h>
#include <buffer.h>

MODULE("turnstone.user.programs.vm_test_program");

_Noreturn void vm_test_program_main(void);

static void vm_test_program_print(const char* str) {
    while(*str != '\0') {
        outb(0x3f8, *str);
        str++;
    }
}

static void __attribute__((format(printf, 1, 2))) vm_test_program_printf(const char* fstr, ...){
    buffer_t* buffer = buffer_new();

    va_list args;
    va_start(args, fstr);

    buffer_vprintf(buffer, fstr, args);

    va_end(args);

    const uint8_t* out = buffer_get_all_bytes_and_destroy(buffer, NULL);

    vm_test_program_print((const char*)out);

}

_Noreturn static void vm_test_program_halt(void) {
    cpu_hlt();
}

_Noreturn void vm_test_program_main(void) {
    memory_heap_t* heap = memory_create_heap_simple(0, 0);

    if(heap == NULL) {
        vm_test_program_print("Failed to create heap\n");
        vm_test_program_halt();
    }

    memory_set_default_heap(heap);

    vm_test_program_printf("Hello, World!\n");
    vm_test_program_printf("This is a test program for the VM\n");
    vm_test_program_printf("Now halting...\n");


    vm_test_program_halt();
}

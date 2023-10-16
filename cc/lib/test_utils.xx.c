/**
 * @file test_utils.xx.c
 * @brief Test utilities.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <tests.h>
#include <ports.h>

MODULE("turnstone.lib");

void test_init_output(void) {
    init_serial(COM2);
}

void test_print(char_t* data) {
    size_t i = 0;
    while(data[i] != NULL) {
        write_serial(COM2, data[i]);
        i++;
    }
}

void test_exit_qemu(uint32_t exit_code) {
    outl(QEMU_ISA_DEBUG_EXIT, exit_code);
}

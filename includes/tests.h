/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___TESTS_H
#define ___TESTS_H 0

#include <types.h>
#include <ports.h>

#define QEMU_ISA_DEBUG_EXIT  0xf4

#define _TEST_PASTE(a, b) a ## b
#define TEST_PASTE(a, b) _TEST_PASTE(a, b)

#define _TEST_STR_PASTE(a, b) a#b
#define TEST_STR_PASTE(a, b) _TEST_STR_PASTE(a, b)

#define TEST_FUNC(module, group, test_name) \
        __attribute__((section(TEST_STR_PASTE(".text.__test_"#module "_"#group "_"#test_name "_", __LINE__)))) \
        int8_t TEST_PASTE(__test_ ## module ## _ ## group ## _ ## test_name ## _, __LINE__)(size_t test_no)

typedef int8_t (* test_func)(size_t test_no);

void test_init_output(void);
void test_print(char_t* data);
void test_exit_qemu(uint32_t exit_code);

#endif

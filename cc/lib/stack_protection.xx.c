/**
 * @file stack_protection.xx.c
 * @brief stack protection implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <types.h>
#include <video.h>
#include <cpu.h>

#define STACK_PROTECTION_CHECK_GUARD 0xE85093A9FABF3890

size_t __stack_chk_guard = STACK_PROTECTION_CHECK_GUARD;

__attribute__((noreturn)) void __stack_chk_fail(void);

__attribute__((noreturn)) void __stack_chk_fail(void) {
    PRINTLOG(KERNEL, LOG_PANIC, "Stack smashed. Halting");
    cpu_hlt();
}

/**
 * @file assert.64.c
 * @brief Assertion library for 64-bit systems.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */


#include <assert.h>
#include <logging.h>
#include <cpu.h>

MODULE("turnstone.lib.assert")

#if ___TESTMODE == 1
extern _Noreturn void abort(void);
#endif


_Noreturn void __assert(const char_t * expr, const char_t * file, size_t line, const char_t * func) {
    printf("Assertion failed at %s:%lli in %s\n", file, line, func);
    printf("Expression %s\n", expr);

#if ___TESTMODE == 1
    abort();
#else
    cpu_hlt();
#endif

}

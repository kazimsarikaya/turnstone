/**
 * @file assert.h
 * @brief Assertion library
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */



#ifndef ___ASSERT_H
#define ___ASSERT_H

#include <types.h>

#define assert(expr) ((void)((expr) || (__assert(#expr, __FILE__, __LINE__, __func__), 0)))

_Noreturn void __assert(const char_t * expr, const char_t * file, size_t line, const char_t * func);

#endif // ___ASSERT_H

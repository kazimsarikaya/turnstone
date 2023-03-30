/**
 * @file backtrace.h
 * @brief printing backtrace (stack trace)
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#ifndef ___BACKTRACE_H
#define ___BACKTRACE_H 0

#include <types.h>

typedef struct stackframe_t {
    struct stackframe_t* previous;
    uint64_t             rip;
} stackframe_t;

stackframe_t* backtrace_get_stackframe(void);
void          backtrace_print(stackframe_t* frame);
void          backtrace(void);

#endif

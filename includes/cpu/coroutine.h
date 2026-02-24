/**
 * @file coroutine.h
 * @brief coroutine methods
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___COROUTINE_H
/*! prevent duplicate header error macro */
#define ___COROUTINE_H 0


#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void * (*coroutine_function_f)(void* arg);

typedef struct coroutine_wait_handle_t coroutine_wait_handle_t;

int8_t   coroutine_init(void);
void     coroutine_deinit(void);
uint64_t coroutine_get_id(void);
void     coroutine_yield(void);
void*    coroutine_await(coroutine_wait_handle_t* co_wh);
void     coroutine_msleep(uint64_t ms);
void     coroutine_loop(void);

typedef struct coroutine_go_args_t {
    coroutine_function_f      function;
    void*                     arg;
    size_t                    stack_size;
    coroutine_wait_handle_t** wait_handle;
} coroutine_go_args_t;

void coroutine_go_internal(coroutine_go_args_t args);

#define coroutine_go(f, ...) { \
            _Pragma("GCC diagnostic push") \
            _Pragma("GCC diagnostic ignored \"-Woverride-init\"") \
            coroutine_go_internal((coroutine_go_args_t){ .arg = nullptr, .stack_size = 16 << 10, .wait_handle=NULL, .function = (f) ,##__VA_ARGS__}); \
            _Pragma("GCC diagnostic pop") \
            }


#ifdef __cplusplus
}
#endif

#endif

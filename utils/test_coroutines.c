/**
 * @file test_coroutines.c
 * @brief test application for coroutines.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#define RAMSIZE 0x8000000
#include "setup.h"
#include <cpu/cpu_registers.h>
#include <list.h>
#include <assert.h>
#include <cpu/coroutine.h>

int32_t main(void);

static void* coroutine_example_function(void* arg) {
    int32_t limit = (int32_t)(uintptr_t)arg;
    printf("Coroutine %llu started with limit: %d\n", coroutine_get_id(), limit);
    int32_t count = 0;
    while(count < limit) {
        printf("Coroutine %llu count: %d\n", coroutine_get_id(), count);
        count++;
        coroutine_yield();
    }

    if(coroutine_get_id() & 1) {
        printf("Coroutine %llu going to sleep for %llu ms\n", coroutine_get_id(), 1000 * coroutine_get_id());
        coroutine_msleep(1000 * coroutine_get_id()); // sleep for 100 ms to test sleeping functionality
        printf("Coroutine %llu woke up from sleep\n", coroutine_get_id());
    }

    return (void*)(uintptr_t)(limit); // return some result to test waiting for result functionality
}


int32_t main(void) {
    printf("Starting coroutine test application\n");
    if(coroutine_init() != 0) {
        print_error("Failed to initialize coroutines");
        return -1;
    }

    time_t start_time = time_ns(NULL);

    coroutine_wait_handle_t* wait_handle = NULL;
    coroutine_go(coroutine_example_function, .arg = (void*)(uintptr_t)7, .wait_handle = &wait_handle);

    coroutine_go(coroutine_example_function, .arg = (void*)(uintptr_t)9);

    coroutine_go(coroutine_example_function, (void*)(uintptr_t)5, .stack_size = 2 << 20);

    int32_t result = (int32_t)(uintptr_t)coroutine_await(wait_handle);
    printf("Coroutine with wait handle finished with result: %d\n", result);

    coroutine_go(coroutine_example_function, (void*)(uintptr_t)14);

    coroutine_go(coroutine_example_function, (void*)(uintptr_t)3);

    coroutine_loop();

    coroutine_deinit();

    uint64_t elapsed_time_ms = (time_ns(NULL) - start_time) / 1000000; // convert ns to ms
    printf("Coroutine test application finished in %llu ms\n", elapsed_time_ms);

    print_success("Coroutine test application finished successfully");

    return 0;
}

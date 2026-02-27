/**
 * @file coroutine.64.c
 * @brief coroutine methods
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <cpu/coroutine.h>
#include <cpu/cpu_registers.h>
#include <list.h>
#include <logging.h>
#include <assert.h>
#include <time.h>
#include <cpu.h>
#include <cpu/task.h>

MODULE("turnstone.kernel.cpu.coroutine");


__attribute__((naked, no_stack_protector, noinline))
static void coroutine_save_registers(cpu_registers_t* registers) {
    __asm__ __volatile__ (
        "mov %%rax, %[rax]\n"
        "mov %%rbx, %[rbx]\n"
        "mov %%rcx, %[rcx]\n"
        "mov %%rdx, %[rdx]\n"
        "mov %%r8,  %[r8]\n"
        "mov %%r9,  %[r9]\n"
        "mov %%r10, %[r10]\n"
        "mov %%r11, %[r11]\n"
        "mov %%r12, %[r12]\n"
        "mov %%r13, %[r13]\n"
        "mov %%r14, %[r14]\n"
        "mov %%r15, %[r15]\n"
        "mov %%rdi, %[rdi]\n"
        "mov %%rsi, %[rsi]\n"
        "mov %%rbp, %[rbp]\n"
        "push %%rbx\n"
        "push %%rax\n"
        "push %%rdx\n"
        "mov %[xsave_mask_lo], %%eax\n"
        "mov %[xsave_mask_hi], %%edx\n"
        "lea %[avx512f], %%rbx\n"
        "xsave (%%rbx)\n"
        "pop %%rdx\n"
        "pop %%rax\n"
        "pop %%rbx\n"
        "push %%rax\n"
        "pushfq\n"
        "mov (%%rsp), %%rax\n"
        "mov %%rax, %[rflags]\n"
        "popfq\n"
        "pop %%rax\n"
        "mov %%rsp, %[rsp]\n"
        "retq\n"
        : :
        [rax]    "m" (registers->rax),
        [rbx]    "m" (registers->rbx),
        [rcx]    "m" (registers->rcx),
        [rdx]    "m" (registers->rdx),
        [r8]     "m" (registers->r8),
        [r9]     "m" (registers->r9),
        [r10]    "m" (registers->r10),
        [r11]    "m" (registers->r11),
        [r12]    "m" (registers->r12),
        [r13]    "m" (registers->r13),
        [r14]    "m" (registers->r14),
        [r15]    "m" (registers->r15),
        [rdi]    "m" (registers->rdi),
        [rsi]    "m" (registers->rsi),
        [rbp]    "m" (registers->rbp),
        [avx512f]    "m" (registers->avx512f),
        [rflags] "m" (registers->rflags),
        [rsp]    "m" (registers->rsp),
        [cr3]     "m" (registers->cr3),
        [xsave_mask_lo] "m" (registers->xsave_mask_lo),
        [xsave_mask_hi] "m" (registers->xsave_mask_hi)
        );
}

__attribute__((naked, no_stack_protector, noinline))
static void coroutine_load_registers(cpu_registers_t* registers) {
    __asm__ __volatile__ (
        "mov %[rcx],  %%rcx\n"
        "mov %[r8],  %%r8\n"
        "mov %[r9],  %%r9\n"
        "mov %[r10],  %%r10\n"
        "mov %[r11],  %%r11\n"
        "mov %[r12],  %%r12\n"
        "mov %[r13],  %%r13\n"
        "mov %[r14], %%r14\n"
        "mov %[r15], %%r15\n"
        "mov %[rsi], %%rsi\n"
        "mov %[rbp], %%rbp\n"
        "lea %[avx512f], %%rbx\n"
        "mov %[xsave_mask_lo], %%eax\n"
        "mov %[xsave_mask_hi], %%edx\n"
        "xrstor (%%rbx)\n"
        "mov %[rflags], %%rax\n"
        "push %%rax\n"
        "popfq\n"
        "mov %[rax],  %%rax\n"
        "mov %[rbx],  %%rbx\n"
        "mov %[rdx],  %%rdx\n"
        "mov %[rsp], %%rsp\n"
        "mov %[rdi], %%rdi\n"
        "retq\n"
        : :
        [rax]     "m" (registers->rax),
        [rbx]     "m" (registers->rbx),
        [rcx]     "m" (registers->rcx),
        [rdx]     "m" (registers->rdx),
        [r8]      "m" (registers->r8),
        [r9]      "m" (registers->r9),
        [r10]     "m" (registers->r10),
        [r11]     "m" (registers->r11),
        [r12]     "m" (registers->r12),
        [r13]     "m" (registers->r13),
        [r14]     "m" (registers->r14),
        [r15]     "m" (registers->r15),
        [rdi]     "m" (registers->rdi),
        [rsi]     "m" (registers->rsi),
        [rbp]     "m" (registers->rbp),
        [avx512f]     "m" (registers->avx512f),
        [rflags]  "m" (registers->rflags),
        [rsp]     "m" (registers->rsp),
        [cr3]     "m" (registers->cr3),
        [xsave_mask_lo] "m" (registers->xsave_mask_lo),
        [xsave_mask_hi] "m" (registers->xsave_mask_hi)
        );
}

typedef struct coroutine_t {
    uint64_t             id;
    boolean_t            finished;
    boolean_t            wait_for_result;
    uint64_t             yield_count;
    uint64_t             wake_at; // the ns value to wake the coroutine, this is used for sleeping coroutines, if 0 then not sleeping
    cpu_registers_t*     registers;
    coroutine_function_f function;
    void*                arg;
    void*                result;
    uint8_t*             stack;
} coroutine_t;

_Thread_local boolean_t coroutine_initialized = false;

_Thread_local uint64_t coroutine_id_counter = 0;

_Thread_local list_t* coroutine_list                    = NULL;
_Thread_local list_t* coroutine_sleeping_list           = NULL; // list of sleeping coroutines, sorted by wake_at
_Thread_local list_t* coroutine_finished_list           = NULL; // list of finished coroutines, sorted by yield_count to ensure they are cleaned up in order of finishing
_Thread_local list_t* coroutine_waiting_for_result_list = NULL; // list of coroutines waiting for result

_Thread_local uint64_t coroutine_xsave_mask = 0;
_Thread_local uint32_t coroutine_mxcsr_mask = 0;

_Thread_local coroutine_t* coroutine_current = NULL;
_Thread_local coroutine_t* coroutine_main    = NULL;

static int8_t coroutine_sleep_comparator(const void* item1, const void* item2) {
    const coroutine_t* c1 = (const coroutine_t*)item1;
    const coroutine_t* c2 = (const coroutine_t*)item2;

    if(c1->wake_at < c2->wake_at) {
        return -1;
    } else if(c1->wake_at > c2->wake_at) {
        return 1;
    }

    return 0;
}

static int8_t coroutine_yield_comparator(const void* item1, const void* item2) {
    const coroutine_t* c1 = (const coroutine_t*)item1;
    const coroutine_t* c2 = (const coroutine_t*)item2;

    if(c1->yield_count < c2->yield_count) {
        return -1;
    } else if(c1->yield_count > c2->yield_count) {
        return 1;
    }

    return 0;
}

static boolean_t coroutine_has_not_finished() {
    if(!coroutine_initialized) {
        PRINTLOG(TASKING, LOG_ERROR, "Coroutines not initialized");
        return false;
    }

    while(list_size(coroutine_finished_list) > 0) {
        coroutine_t* coroutine = (coroutine_t*)list_queue_pop(coroutine_finished_list);
        memory_free(coroutine->registers);
        memory_free(coroutine->stack);
        memory_free(coroutine);
    }

    return list_size(coroutine_list) > 0 || list_size(coroutine_sleeping_list) > 0;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
void coroutine_yield(void) {
    if(!coroutine_initialized) {
        PRINTLOG(TASKING, LOG_ERROR, "Coroutines not initialized");
        return;
    }

    coroutine_save_registers(coroutine_current->registers);

    if(coroutine_current != coroutine_main) {
        if(coroutine_current->wake_at > 0) {
            if(list_sortedlist_insert(coroutine_sleeping_list, coroutine_current) == -1ULL) {
                PRINTLOG(TASKING, LOG_ERROR, "Failed to add coroutine to sleeping list");
            }
        } else {
            if(list_sortedlist_insert(coroutine_list, coroutine_current) == -1ULL) {
                PRINTLOG(TASKING, LOG_ERROR, "Failed to add coroutine to list");
            }
        }
    }

    // Find the next active coroutine to switch to
    coroutine_t* next = NULL;

    if(list_size(coroutine_sleeping_list) > 0) {
        const coroutine_t* sleeping_coroutine = list_get_data_at_position(coroutine_sleeping_list, 0);
        uint64_t current_time                 = time_ns(NULL);

        if(sleeping_coroutine->wake_at <= current_time) {
            list_delete_at_position(coroutine_sleeping_list, 0);
            // reset wake_at to 0 to indicate not sleeping
            next          = (coroutine_t*)sleeping_coroutine;
            next->wake_at = 0;
        }
    }

    if(!next && list_size(coroutine_list) > 0) {
        coroutine_t* c = (coroutine_t*)list_get_data_at_position(coroutine_list, 0);
        if(c->yield_count < coroutine_current->yield_count) {
            next = c;
            list_delete_at_position(coroutine_list, 0);
        }
    }

    if(!next) {
        next = coroutine_main;
    }

    coroutine_current = next;

    coroutine_current->yield_count++;

    coroutine_load_registers(coroutine_current->registers);
    asm volatile ("" ::: "memory"); // prevent compiler reordering
}
#pragma GCC diagnostic pop

static void coroutine_end() {
    if(!coroutine_initialized) {
        PRINTLOG(TASKING, LOG_ERROR, "Coroutines not initialized");
        return;
    }

    coroutine_current->result = coroutine_current->function(coroutine_current->arg);

    memory_free((void*)coroutine_current->registers);
    coroutine_current->registers = NULL;
    // we cannot free the coroutine here because of its stack, we will free it in the cleanup function at coroutine_cleanup_finished

    if(coroutine_current->wait_for_result) {
        if(list_sortedlist_insert(coroutine_waiting_for_result_list, coroutine_current) == -1ULL) {
            PRINTLOG(TASKING, LOG_ERROR, "Failed to add coroutine to waiting for result list");
        }
    } else {
        if(list_list_insert(coroutine_finished_list, coroutine_current) == -1ULL) {
            PRINTLOG(TASKING, LOG_ERROR, "Failed to add coroutine to finished list");
        }
    }

    coroutine_current->finished = true;

    coroutine_current = coroutine_main; // switch back to main coroutine before yielding to ensure we don't switch to a finished coroutine
    coroutine_load_registers(coroutine_main->registers); // switch back to main coroutine after finishing
    asm volatile ("" ::: "memory"); // prevent compiler reordering
}

void* coroutine_await(coroutine_wait_handle_t* co_wh) {
    if(!coroutine_initialized) {
        PRINTLOG(TASKING, LOG_ERROR, "Coroutines not initialized");
        return NULL;
    }

    coroutine_t* co = (coroutine_t*)co_wh;

    if(!co) {
        PRINTLOG(TASKING, LOG_ERROR, "Coroutine to await cannot be null");
        return NULL;
    }

    if(!co->wait_for_result) {
        PRINTLOG(TASKING, LOG_ERROR, "Coroutine to await must be created with wait_for_result=true");
        return NULL;
    }

    while(!co->finished) {
        coroutine_yield();
    }

    void* result = co->result;

    coroutine_t* tmp = (coroutine_t*)list_list_delete(coroutine_waiting_for_result_list, co);

    assert(tmp == co && "Failed to find coroutine in waiting for result list");

    if(list_list_insert(coroutine_finished_list, co) == -1ULL) {
        PRINTLOG(TASKING, LOG_ERROR, "Failed to add coroutine to finished list");
    }

    return result;
}

void coroutine_msleep(uint64_t ms) {
    if(!coroutine_initialized) {
        PRINTLOG(TASKING, LOG_ERROR, "Coroutines not initialized");
        return;
    }

    uint64_t wake_at = time_ns(NULL) + ms * 1000000; // convert ms to ns

    coroutine_current->wake_at = wake_at;

    coroutine_yield();
}

uint64_t coroutine_get_id(void) {
    if(!coroutine_initialized) {
        PRINTLOG(TASKING, LOG_ERROR, "Coroutines not initialized");
        return 0;
    }

    return coroutine_current->id;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
int8_t coroutine_go_internal(coroutine_go_args_t args) {
    if(!coroutine_initialized) {
        PRINTLOG(TASKING, LOG_ERROR, "Coroutines not initialized");
        return -1;
    }

    if(!args.function) {
        PRINTLOG(TASKING, LOG_ERROR, "Coroutine function cannot be null");
        return -1;
    }

    if(args.stack_size < 0x1000) {
        PRINTLOG(TASKING, LOG_ERROR, "Coroutine stack size must be at least 0x1000");
        return -1;
    }

    coroutine_t* co = memory_malloc(sizeof(coroutine_t));
    if(!co) {
        PRINTLOG(TASKING, LOG_ERROR, "Failed to allocate memory for coroutine");
        return -1;
    }

    co->registers = memory_malloc_ext(NULL, sizeof(cpu_registers_t), 0x40);
    if(!co->registers) {
        PRINTLOG(TASKING, LOG_ERROR, "Failed to allocate memory for coroutine registers");
        memory_free(co);
        return -1;
    }

    co->stack = memory_malloc_ext(NULL, args.stack_size, 0x1000);
    if(!co->stack) {
        PRINTLOG(TASKING, LOG_ERROR, "Failed to allocate memory for coroutine stack");
        memory_free(co->registers);
        memory_free(co);
        return -1;
    }

    co->id              = ++coroutine_id_counter;
    co->finished        = false;
    co->function        = args.function;
    co->arg             = args.arg;
    co->wait_for_result = args.wait_handle != NULL;
    co->yield_count     = coroutine_current->yield_count; // set initial yield count to current coroutine's yield count to ensure fair scheduling

    co->registers->xsave_mask_lo = (uint32_t)(coroutine_xsave_mask & 0xFFFFFFFF);
    co->registers->xsave_mask_hi = (uint32_t)(coroutine_xsave_mask >> 32);

    *(uint16_t*)&co->registers->avx512f[0]  = 0x37F;
    *(uint32_t*)&co->registers->avx512f[24] = 0x1F80 & coroutine_mxcsr_mask;

    uint64_t rbp = (uint64_t)co->stack;
    rbp               += args.stack_size - 16; // safeguard for return address, we will set it to a function that ends the coroutine, this should never be returned to
    co->registers->rbp = rbp;
    co->registers->rsp = rbp - 16; // reserve space for return address defined below


    uint64_t* stack = (uint64_t*)rbp;
    stack[-1] = 0; // safeguard forces page fault if we ever return from the coroutine function, this should never happen
    stack[-2] = (uint64_t)coroutine_end; // entry_point;

    if(list_sortedlist_insert(coroutine_list, co) == -1ULL) {
        PRINTLOG(TASKING, LOG_ERROR, "Failed to add coroutine to list");
        memory_free(co->stack);
        memory_free(co->registers);
        memory_free(co);
        return -1;
    }

    if(co->wait_for_result) {
        *(args.wait_handle) = (coroutine_wait_handle_t*)co;
    }

    coroutine_yield(); // yield to start the coroutine

    return 0;
}
#pragma GCC diagnostic pop

int8_t coroutine_init(void) {
    if(coroutine_initialized) {
        PRINTLOG(TASKING, LOG_ERROR, "Coroutines already initialized");
        return -1;
    }

    cpu_cpuid_regs_t query = {0};
    cpu_cpuid_regs_t result;

    query.eax = 0xd;

    cpu_cpuid(query, &result);

    coroutine_xsave_mask = ((uint64_t)result.edx << 32) | result.eax;

    cpu_registers_t tmp_registers;
    memory_memclean(&tmp_registers, sizeof(cpu_registers_t));

    coroutine_save_registers(&tmp_registers);

    coroutine_mxcsr_mask = *(uint32_t*)&tmp_registers.avx512f[28];

    coroutine_list = list_create_sortedlist(coroutine_yield_comparator);

    if(!coroutine_list) {
        PRINTLOG(TASKING, LOG_ERROR, "Failed to create coroutine list");
        return -1;
    }

    coroutine_sleeping_list = list_create_sortedlist(coroutine_sleep_comparator);

    if(!coroutine_sleeping_list) {
        PRINTLOG(TASKING, LOG_ERROR, "Failed to create coroutine sleeping list");
        list_destroy(coroutine_list);
        return -1;
    }

    coroutine_finished_list = list_create_list();

    if(!coroutine_finished_list) {
        PRINTLOG(TASKING, LOG_ERROR, "Failed to create coroutine finished list");
        list_destroy(coroutine_list);
        list_destroy(coroutine_sleeping_list);
        return -1;
    }

    coroutine_waiting_for_result_list = list_create_list();
    if(!coroutine_waiting_for_result_list) {
        PRINTLOG(TASKING, LOG_ERROR, "Failed to create coroutine waiting for result list");
        list_destroy(coroutine_list);
        list_destroy(coroutine_sleeping_list);
        list_destroy(coroutine_finished_list);
        return -1;
    }

    coroutine_t* main_co = memory_malloc(sizeof(coroutine_t));
    if(!main_co) {
        PRINTLOG(TASKING, LOG_ERROR, "Failed to allocate memory for main coroutine");
        return -1;
    }

    main_co->registers = memory_malloc_ext(NULL, sizeof(cpu_registers_t), 0x40);
    if(!main_co->registers) {
        PRINTLOG(TASKING, LOG_ERROR, "Failed to allocate memory for main coroutine registers");
        memory_free(main_co);
        list_destroy(coroutine_list);
        return -1;
    }

    coroutine_main    = main_co;
    coroutine_current = main_co;

    coroutine_initialized = true;

    coroutine_save_registers(coroutine_main->registers);
    coroutine_load_registers(coroutine_main->registers);

    return 0;
}

void coroutine_deinit(void) {
    if(!coroutine_initialized) {
        PRINTLOG(TASKING, LOG_ERROR, "Coroutines not initialized");
        return;
    }

    while(list_size(coroutine_list) > 0) {
        coroutine_t* co = (coroutine_t*)list_queue_pop(coroutine_list);
        memory_free(co->registers);
        memory_free(co->stack);
        memory_free(co);
    }
    list_destroy(coroutine_list);
    coroutine_list = NULL;

    list_destroy(coroutine_sleeping_list);
    coroutine_sleeping_list = NULL;

    list_destroy(coroutine_finished_list);
    coroutine_finished_list = NULL;

    while(list_size(coroutine_waiting_for_result_list) > 0) {
        coroutine_t* co = (coroutine_t*)list_queue_pop(coroutine_waiting_for_result_list);
        memory_free(co->registers);
        memory_free(co->stack);
        memory_free(co);
    }
    list_destroy(coroutine_waiting_for_result_list);

    memory_free(coroutine_main->registers);
    memory_free(coroutine_main);
    coroutine_main    = NULL;
    coroutine_current = NULL;

    coroutine_initialized = false;
}

#ifdef ___TESTMODE
void usleep(uint64_t us);
#endif

void coroutine_loop(void) {
    if(!coroutine_initialized) {
        PRINTLOG(TASKING, LOG_ERROR, "Coroutines not initialized");
        return;
    }

    while(coroutine_has_not_finished()) {
        coroutine_yield();
        if(list_size(coroutine_list) == 0 &&
           list_size(coroutine_finished_list) == 0 &&
           list_size(coroutine_sleeping_list) > 0) {
            const coroutine_t* sleeping_coroutine = list_get_data_at_position(coroutine_sleeping_list, 0);
            uint64_t current_time                 = time_ns(NULL);

            if(sleeping_coroutine->wake_at > current_time) {
                uint64_t sleep_time_us = (sleeping_coroutine->wake_at - current_time) / 1000; // convert ns to us for usleep
#ifdef ___TESTMODE
                usleep(sleep_time_us);
#else
                task_msleep(sleep_time_us / 1000); // convert us to ms for task_msleep
#endif
            }
        }
    }
}

/**
 * @file task.64.c
 * @brief cpu task methods
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#include <apic.h>
#include <cpu.h>
#include <cpu/cpu_state.h>
#include <cpu/interrupt.h>
#include <cpu/task.h>
#include <cpu/crx.h>
#include <cpu/sync.h>
#include <cpu/smp.h>
#include <memory/paging.h>
#include <memory/frame.h>
#include <list.h>
#include <time.h>
#include <time/timer.h>
#include <logging.h>
#include <systeminfo.h>
#include <linker.h>
#include <utils.h>
#include <hashmap.h>
#include <stdbufs.h>
#include <hypervisor/hypervisor_vmx_ops.h>
#include <hypervisor/hypervisor_vm.h>
#include <hypervisor/hypervisor_vmx_macros.h>
#include <strings.h>
#include <spool.h>

MODULE("turnstone.kernel.cpu.task");

void video_text_print(const char_t* str);

typedef task_t * (*memory_current_task_getter_f)(void);
void memory_set_current_task_getter(memory_current_task_getter_f getter);

typedef task_t * (*lock_current_task_getter_f)(void);
extern lock_current_task_getter_f lock_get_current_task_getter;

typedef void (*lock_task_yielder_f)(void);
extern lock_task_yielder_f lock_task_yielder;

typedef buffer_t * (*stdbuf_task_buffer_getter_f)(void);
extern stdbuf_task_buffer_getter_f stdbufs_task_get_input_buffer;
extern stdbuf_task_buffer_getter_f stdbufs_task_get_output_buffer;
extern stdbuf_task_buffer_getter_f stdbufs_task_get_error_buffer;

typedef void (*future_task_wait_toggler_f)(uint64_t task_id);
extern future_task_wait_toggler_f future_task_wait_toggler_func;

extern buffer_t* stdbufs_default_input_buffer;
extern buffer_t* stdbufs_default_output_buffer;
extern buffer_t* stdbufs_default_error_buffer;

hashmap_t* task_map = NULL;

static uint64_t task_next_task_id     = 0;
static lock_t* task_next_task_id_lock = NULL;

static memory_heap_t* task_map_heap                 = NULL;
static memory_heap_t** task_queue_and_cleanup_heaps = NULL;
static list_t** task_queues                         = NULL;
static list_t** task_sleep_queues; ///< task sleep lists
static list_t** task_wait_queues; ///< task wait lists
static list_t** task_cleanup_queues = NULL;
static uint64_t task_xsave_mask     = 0;
static uint32_t task_mxcsr_mask     = 0;

static uint64_t task_max_tick_count_limit = 0;

extern int8_t kmain64(void);

uint64_t task_get_task_xsave_mask(void) {
    return task_xsave_mask;
}

uint32_t task_get_task_mxcsr_mask(void) {
    return task_mxcsr_mask;
}


static int8_t task_sleep_queue_comparator(const void* item1, const void* item2) {
    const task_t* t1 = (const task_t*)item1;
    const task_t* t2 = (const task_t*)item2;

    if(t1->wake_tick < t2->wake_tick) {
        return -1;
    } else if(t1->wake_tick > t2->wake_tick) {
        return 1;
    }

    return 0;
}

task_t* task_get_current_task(void){
    return (task_t*)cpu_state->current_task;
}

int8_t task_broadcast_parked_but_not_myself(void) {
    uint64_t cpu_state_size = SYSTEM_INFO->gs_page_size / SYSTEM_INFO->cpu_count;

    for(uint32_t i = 0; i < SYSTEM_INFO->cpu_count; i++) {
        cpu_state_t* other_cpu_state = (cpu_state_t*)(void*)(SYSTEM_INFO->gs_page_address_base + i * cpu_state_size);

        if(other_cpu_state->local_apic_id == cpu_state->local_apic_id) {
            continue;
        }

        other_cpu_state->parked = true;
    }

    return 0;
}

int8_t task_wait_for_cpus_in_parked_but_not_myself(void) {
    uint64_t cpu_state_size = SYSTEM_INFO->gs_page_size / SYSTEM_INFO->cpu_count;

    while(true) {
        bool all_parked = true;

        for(uint32_t i = 0; i < SYSTEM_INFO->cpu_count; i++) {
            cpu_state_t* other_cpu_state = (cpu_state_t*)(void*)(SYSTEM_INFO->gs_page_address_base + i * cpu_state_size);

            if(other_cpu_state->local_apic_id == cpu_state->local_apic_id) {
                continue;
            }

            if(!other_cpu_state->in_parked_state) {
                all_parked = false;
                break;
            }
        }

        if(all_parked) {
            break;
        }

        cpu_idle();
    }

    return 0;
}

int8_t task_wake_up(task_t* task) {
    if(task == NULL) {
        return -1;
    }

    task->state       = TASK_STATE_SUSPENDED;
    task->attributes &= ~TASK_ATTRIBUTE_WAKEUP_FROM_ACPI_SLEEP;
    task->wake_tick   = 0; // immediate wake up

    list_t* task_queue = task_sleep_queues[cpu_state->local_apic_id];

    if(task_queue == NULL) {
        return -1;
    }

    if(list_sortedlist_insert(task_queue, task) == -1ULL) {
        return -1;
    }

    return 0;
}

__attribute__((naked, no_stack_protector, noinline))
static void task_save_registers(cpu_registers_t* registers) {
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
        "mov %%cr3, %%rax\n"
        "mov %%rax, %[cr3]\n"
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
static void task_load_registers(cpu_registers_t* registers) {
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
        "btr $0x9, %%rax\n" // clear interrupt flag
        "push %%rax\n"
        "popfq\n"
        "mov %%cr3, %%rax\n"
        "cmp %[cr3], %%rax\n"
        "je 1f\n"
        "mov %[cr3], %%rax\n"
        "mov %%rax, %%cr3\n"
        "1:\n"
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

static void task_cleanup_task(task_t* task) {
    if(!task) {
        return;
    }

    uint64_t task_task_id = task->task_id;

    PRINTLOG(TASKING, LOG_DEBUG, "cleaning up task with id 0x%llx", task_task_id);

    if(task->vm) {
        hypervisor_vm_destroy(task->vm);
    }

    frame_allocator_t* fa = frame_get_allocator();

    memory_free_ext(task->creator_heap, (void*)task->task_name);

    hashmap_delete(task_map, (void*)task->task_id);

    if(!task->is_stack_protected) {
        uint64_t stack_va = (uint64_t)task->stack;
        uint64_t stack_fa = MEMORY_PAGING_GET_FA_FOR_RESERVED_VA(stack_va);

        uint64_t stack_size       = task->stack_size;
        uint64_t stack_frames_cnt = stack_size / FRAME_SIZE;

        memory_memclean(task->stack, stack_size);

        frame_t stack_frames = {.frame_address = stack_fa, .frame_count = stack_frames_cnt};

        PRINTLOG(TASKING, LOG_TRACE, "stack frames 0x%llx with count 0x%llx releasing for task %s", stack_fa, stack_frames_cnt, task->task_name);

        if(memory_paging_delete_va_for_frame_ext(task->page_table, stack_va, &stack_frames) != 0 ) {
            PRINTLOG(TASKING, LOG_ERROR, "cannot remove pages for stack at va 0x%llx", stack_va);

            cpu_hlt();
        }

        if(fa->release_frame(fa, &stack_frames) != 0) {
            PRINTLOG(TASKING, LOG_ERROR, "cannot release stack with frames at 0x%llx with count 0x%llx", stack_fa, stack_frames_cnt);

            cpu_hlt();
        }

        PRINTLOG(TASKING, LOG_TRACE, "stack frames 0x%llx with count 0x%llx released for task %s", stack_fa, stack_frames_cnt, task->task_name);
    } else {
        memory_memclean(task->stack, task->stack_size);
    }

    if(task->heap != memory_get_default_heap() && task->heap != task_map_heap) {
        uint64_t heap_va = (uint64_t)task->heap;
        uint64_t heap_fa = MEMORY_PAGING_GET_FA_FOR_RESERVED_VA(heap_va);

        uint64_t heap_size       = task->heap_size;
        uint64_t heap_frames_cnt = heap_size / FRAME_SIZE;

        memory_memclean(task->heap, heap_size);

        frame_t heap_frames = {.frame_address = heap_fa, .frame_count = heap_frames_cnt};

        PRINTLOG(TASKING, LOG_TRACE, "releasing heap frames 0x%llx with count 0x%llx for task %s", heap_fa, heap_frames_cnt, task->task_name);

        if(memory_paging_delete_va_for_frame_ext(task->page_table, heap_va, &heap_frames) != 0 ) {
            PRINTLOG(TASKING, LOG_ERROR, "cannot remove pages for heap at va 0x%llx", heap_va);

            if(task->page_table) {
                PRINTLOG(TASKING, LOG_ERROR, "page table 0x%p", task->page_table->page_table);
            }

            cpu_hlt();
        }

        if(fa->release_frame(fa, &heap_frames) != 0) {
            PRINTLOG(TASKING, LOG_ERROR, "cannot release heap with frames at 0x%llx with count 0x%llx", heap_fa, heap_frames_cnt);

            cpu_hlt();
        }

        PRINTLOG(TASKING, LOG_TRACE, "heap frames 0x%llx with count 0x%llx released for task %s", heap_fa, heap_frames_cnt, task->task_name);
    }

    if(task->allocated_frames) {
        for(uint64_t i = 0; i < list_size(task->allocated_frames); i++) {
            frame_t* frm = (frame_t*)list_get_data_at_position(task->allocated_frames, i);

            if(frm) {
                uint64_t frm_va = frm->frame_address;

                if(frm->type == FRAME_TYPE_RESERVED) {
                    frm_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(frm_va);
                }

                uint64_t frm_count = frm->frame_count;
                uint64_t frm_type  = frm->type;
                uint64_t frm_fa    = frm->frame_address;

                PRINTLOG(TASKING, LOG_TRACE, "allocated frame 0x%llx (0x%llx) with count 0x%llx and type 0x%llx releasing for task %s",
                         frm_va, frm_fa, frm_count, frm_type, task->task_name);

                if(memory_paging_delete_va_for_frame_ext(task->page_table, frm_va, frm) != 0 ) {
                    PRINTLOG(TASKING, LOG_ERROR, "cannot remove pages for allocated frame at va 0x%llx",
                             frm_va);

                    if(task->page_table) {
                        PRINTLOG(TASKING, LOG_ERROR, "page table 0x%p", task->page_table->page_table);
                    }

                    cpu_hlt();
                }

                if(frm->frame_address) {
                    if(fa->release_frame(fa, frm) != 0) {
                        PRINTLOG(TASKING, LOG_ERROR, "cannot release allocated frame at 0x%llx with count 0x%llx",
                                 frm->frame_address, frm->frame_count);
                        cpu_hlt();
                    }
                }

                PRINTLOG(TASKING, LOG_TRACE, "allocated frame 0x%llx (0x%llx) with count 0x%llx and type 0x%llx released for task %s",
                         frm_va, frm_fa, frm_count, frm_type, task->task_name);
            }
        }

        list_destroy(task->allocated_frames);
    }

    memory_paging_destroy_userspace_table(task->userspace_page_table);

    memory_free_ext(task->creator_heap, task->registers);
    memory_free_ext(task->creator_heap, task);

    PRINTLOG(TASKING, LOG_DEBUG, "task with id 0x%llx cleaned up", task_task_id);
}

static int8_t task_cleaner_task(void){
    while(true) {
        while(list_size(cpu_state->task_cleanup_queue)) {
            task_t* task = (task_t*)list_queue_peek(cpu_state->task_cleanup_queue);
            task_cleanup_task(task);
            list_queue_pop(cpu_state->task_cleanup_queue);
        }

        task_yield();
    }

    PRINTLOG(TASKING, LOG_ERROR, "task cleaner task exiting unexpectedly");

    return 0;
}

static task_t* task_find_next_task(void) {
    task_t* tmp_task = NULL;

    if(cpu_state->current_task->attributes & TASK_ATTRIBUTE_NO_PREEMPTION) {
        return cpu_state->current_task;
    }

    if(!tmp_task && list_size(cpu_state->task_sleep_queue)) {
        for(uint64_t i = 0; i < list_size(cpu_state->task_sleep_queue); i++) {
            task_t* t = (task_t*)list_get_data_at_position(cpu_state->task_sleep_queue, i);

            if(t->state == TASK_STATE_ENDED) {
                tmp_task = (task_t*)list_delete_at_position(cpu_state->task_sleep_queue, i);
                break;
            }
        }

        if(!tmp_task && list_size(cpu_state->task_sleep_queue)) {
            task_t* t = (task_t*)list_get_data_at_position(cpu_state->task_sleep_queue, 0);

            if(t->wake_tick < cpu_state->tick_count) {
                tmp_task = (task_t*)list_delete_at_position(cpu_state->task_sleep_queue, 0);
            }
        }
    }

    if(!tmp_task && list_size(cpu_state->task_wait_queue)) {
        uint64_t found_index = -1;

        for(uint64_t i = 0; i < list_size(cpu_state->task_wait_queue); i++) {
            task_t* t = (task_t*)list_get_data_at_position(cpu_state->task_wait_queue, i);

            if(t->state == TASK_STATE_FUTURE_WAITING) {
                continue;
            } else if(t->attributes & TASK_ATTRIBUTE_INTERRUPTIBLE) {
                if(t->state == TASK_STATE_INTERRUPT_RECEIVED) {
                    found_index = i;
                    break;
                }


                if(t->interrupt_receive_workaround) {
                    uint64_t current_tick = rdtsc();

                    if((t->last_tick_count + t->interrupt_receive_workaround_max_tick_count) < current_tick) {
                        found_index = i;
                        break;
                    }
                }

            } else if(t->state == TASK_STATE_MESSAGE_WAITING) {
                if(t->message_queues) {
                    for(uint64_t q_idx = 0; q_idx < list_size(t->message_queues); q_idx++) {
                        list_t* q = (list_t*)list_get_data_at_position(t->message_queues, q_idx);

                        if(list_size(q)) {
                            found_index = i;
                            break;
                        }
                    }
                }

                if(t->message_waiting_max_tick_count) {
                    uint64_t current_tick = rdtsc();

                    if((t->last_tick_count + t->message_waiting_max_tick_count) < current_tick) {
                        found_index = i;
                    }
                }

                if(found_index != i && t->custom_has_message_func) {
                    if(t->custom_has_message_func(t->custom_has_message_func_args)) {
                        found_index = i;
                        break;
                    }
                }

            } else if(t->state == TASK_STATE_ENDED) {
                found_index = i;
                break;
            } else { // wait status cleared task
                if(t->state != TASK_STATE_SUSPENDED) {
                    video_text_print("task_find_next_task: task state is not suspended: 0x");
                    char_t buf[16] = {0};
                    utoh_with_buffer(buf, t->state);
                    video_text_print(buf);
                    video_text_print("\n");
                }

                found_index = i;
                break;
            }
        }

        if(found_index != -1ULL) {
            tmp_task = (task_t*)list_delete_at_position(cpu_state->task_wait_queue, found_index);
        }
    }

    static boolean_t cleaner_task_alternate_flag = false;

    if(cleaner_task_alternate_flag) {
        cleaner_task_alternate_flag = false;

        if(!tmp_task && list_size(cpu_state->task_queue)) {
            tmp_task = (task_t*)list_queue_pop(cpu_state->task_queue);
        }

        if(!tmp_task && list_size(cpu_state->task_cleanup_queue)) {
            tmp_task = cpu_state->cleaner_task;
        }
    } else {
        cleaner_task_alternate_flag = true;

        if(!tmp_task && list_size(cpu_state->task_cleanup_queue)) {
            tmp_task = cpu_state->cleaner_task;
        }

        if(!tmp_task && list_size(cpu_state->task_queue)) {
            tmp_task = (task_t*)list_queue_pop(cpu_state->task_queue);
        }
    }

    if(!tmp_task) {
        tmp_task = (task_t*)cpu_state->idle_task;
    }

    if(!tmp_task->registers) {
        video_text_print("task_find_next_task: task registers null : 0x");
        char_t buf[32] = {0};
        utoh_with_buffer(buf, (uintptr_t)tmp_task);
        video_text_print(buf);
        video_text_print("\n");
        tmp_task = (task_t*)cpu_state->idle_task;
    }

    if(tmp_task->state == TASK_STATE_ENDED) {
        list_queue_push(cpu_state->task_cleanup_queue, tmp_task);
        tmp_task = (task_t*)cpu_state->cleaner_task;
    }

    return tmp_task;
}

void task_task_switch_set_parameters(boolean_t need_eoi) {
    cpu_state->task_switch_paramters_need_eoi = need_eoi;
}

void task_task_switch_exit(void) {
    if(cpu_state->task_switch_paramters_need_eoi) {
        cpu_state->task_switch_paramters_need_eoi = false;
        apic_eoi();
    }
}

static char_t task_switch_task_id_buf[100] = {0};

__attribute__((noinline))
static boolean_t task_is_speacial_task(task_t* task) {
    return task == cpu_state->idle_task ||
           task == cpu_state->cleaner_task ||
           task->attributes & TASK_ATTRIBUTE_NO_PREEMPTION ||
           task->attributes & TASK_ATTRIBUTE_ACPI_SLEEP_TASK ||
           task->attributes & TASK_ATTRIBUTE_WAKEUP_FROM_ACPI_SLEEP;
}

__attribute__((no_stack_protector))
void task_switch_task(void) {
    task_t* current_task = cpu_state->current_task;

    uint64_t current_tick = rdtsc();

    if(!cpu_state->parked &&
       current_task != cpu_state->idle_task &&
       current_task->state == TASK_STATE_RUNNING &&
       (current_tick - current_task->last_tick_count) < task_max_tick_count_limit &&
       current_tick > current_task->last_tick_count) {

        return;
    }

    if(current_task->vmcs_physical_address) {
        if(cpu_get_type() == CPU_TYPE_INTEL) {
            if(vmx_vmclear(current_task->vmcs_physical_address) != 0) {
                utoh_with_buffer(task_switch_task_id_buf, current_task->task_id);
                video_text_print("vmclear failed for task 0x");
                video_text_print(task_switch_task_id_buf);
                video_text_print("\n");
                return;
            }
        } else if(cpu_get_type() == CPU_TYPE_AMD) {

        }
    }

    task_save_registers(current_task->registers);

    if(current_task->state == TASK_STATE_RUNNING) {
        current_task->state = TASK_STATE_SUSPENDED;
    }

    boolean_t special_task = task_is_speacial_task(current_task);

    if(!special_task) {
        switch(current_task->state) {
        case TASK_STATE_SUSPENDED:
        case TASK_STATE_STARTING:
            list_queue_push(cpu_state->task_queue, current_task);
            break;
        case TASK_STATE_ENDED:
            list_queue_push(cpu_state->task_cleanup_queue, current_task);
            break;
        case TASK_STATE_SLEEPING:
            list_sortedlist_insert(cpu_state->task_sleep_queue, current_task);
            break;
        default:
            list_queue_push(cpu_state->task_wait_queue, current_task);
            break;
        }
    }

    while(cpu_state->parked) {
        cpu_state->in_parked_state = true;
        asm volatile ("wbinvd" ::: "memory"); // ensure all memory operations are completed before checking parked state again
        cpu_hlt();
    }

    current_task                  = task_find_next_task();
    current_task->last_tick_count = rdtsc();
    current_task->task_switch_count++;

    switch(current_task->state) {
    case TASK_STATE_CREATED:
        current_task->state = TASK_STATE_STARTING;
        break;
    default:
        current_task->state = TASK_STATE_RUNNING;
        break;
    }

    cpu_state->current_task = current_task;

    if(current_task->vmcs_physical_address) {
        if(cpu_get_type() == CPU_TYPE_INTEL) {
            if(vmx_vmptrld(current_task->vmcs_physical_address) != 0) {
                utoh_with_buffer(task_switch_task_id_buf, current_task->task_id);
                video_text_print("vmptrld failed for task 0x");
                video_text_print(task_switch_task_id_buf);
                video_text_print("\n");
                return;
            }

            vmx_write(VMX_HOST_FS_BASE, cpu_read_fs_base());
            vmx_write(VMX_HOST_GS_BASE, cpu_read_gs_base());
        } else if(cpu_get_type() == CPU_TYPE_AMD) {

        }
    }

    task_load_registers(current_task->registers);

    asm volatile ("" ::: "memory"); // prevent compiler jmp directly to the task_load_registers
}

void task_exit(int32_t exit_code) {
    task_t* current_task = task_get_current_task();

    if(!current_task) {
        PRINTLOG(TASKING, LOG_ERROR, "current task not found");

        return;
    }

    current_task->exit_code = exit_code;
    task_end_task();
}

void task_end_task(void) {
    task_t* current_task = cpu_state->current_task;

    if(current_task == NULL) {
        PRINTLOG(TASKING, LOG_ERROR, "no current task");
        cpu_hlt();
    }

    typedef int64_t (*entry_point_f)(uint64_t, void**);
    entry_point_f entry_point = current_task->entry_point;

    int64_t ret = -1;


    if(current_task->state == TASK_STATE_STARTING) {
        task_task_switch_exit();
        cpu_sti();

        if(!entry_point) {
            PRINTLOG(TASKING, LOG_ERROR, "no entry point for task 0x%llx", current_task->task_id);
        } else {

            PRINTLOG(TASKING, LOG_INFO, "starting task %s with pid 0x%llx on cpu 0x%llx",
                     current_task->task_name, current_task->task_id, cpu_state->local_apic_id);
            ret = entry_point(current_task->arguments_count, current_task->arguments);
        }
    } else if(current_task->state == TASK_STATE_RUNNING) {
        ret = current_task->exit_code;
    } else {
        PRINTLOG(TASKING, LOG_WARNING, "ending task %s with pid 0x%llx on cpu 0x%llx that is not in starting state but in state 0x%x",
                 current_task->task_name, current_task->task_id, cpu_state->local_apic_id, current_task->state);
        ret = current_task->exit_code;
    }

    logging_level_t log_level = LOG_INFO;

    if(ret != 0) {
        log_level = LOG_ERROR;
    }

    PRINTLOG(TASKING, log_level, "ending task 0x%llx return code 0x%llx on cpu 0x%llx",
             current_task->task_id, ret, cpu_state->local_apic_id);

    current_task->state = TASK_STATE_ENDED;

    task_yield();
}

void task_kill_task(uint64_t task_id, boolean_t force) {
    task_t* task = (task_t*)hashmap_get(task_map, (void*)task_id);

    if(task == NULL) {
        PRINTLOG(TASKING, LOG_WARNING, "task 0x%llx not found", task_id);

        return;
    }

    if(task->state == TASK_STATE_ENDED) {
        if(force) {
            task_cleanup_task(task);
        }

        return;
    }

    PRINTLOG(TASKING, LOG_INFO, "killing task 0x%llx (0x%p) state: %d", task->task_id, task, task->state);

    task->state = TASK_STATE_ENDED;
}

uint64_t task_create_task(memory_heap_t* heap, uint64_t heap_size, uint64_t stack_size, void* entry_point, uint64_t args_cnt, void** args, const char_t* task_name) {
    heap = task_map_heap; // override heap

    task_t* new_task = memory_malloc_ext(heap, sizeof(task_t), 0x40);

    if(new_task == NULL) {
        return -1;
    }

    new_task->creator_heap = heap;

    cpu_registers_t* registers = memory_malloc_ext(heap, sizeof(cpu_registers_t), 0x40);

    if(registers == NULL) {
        memory_free_ext(heap, new_task);

        return -1;
    }

    frame_allocator_t* fa = frame_get_allocator();

    frame_t* stack_frames;
    uint64_t stack_frames_cnt = (stack_size + FRAME_SIZE - 1) / FRAME_SIZE;
    stack_size = stack_frames_cnt * FRAME_SIZE;

    if(fa->allocate_frame_by_count(fa, stack_frames_cnt, FRAME_ALLOCATION_TYPE_USED | FRAME_ALLOCATION_TYPE_BLOCK, &stack_frames, NULL) != 0) {
        PRINTLOG(TASKING, LOG_ERROR, "cannot allocate stack with frame count 0x%llx", stack_frames_cnt);
        memory_free_ext(heap, new_task);
        memory_free_ext(heap, registers);

        return -1;
    }

    frame_t* heap_frames;
    uint64_t heap_frames_cnt = (heap_size + FRAME_SIZE - 1) / FRAME_SIZE;
    heap_size = heap_frames_cnt * FRAME_SIZE;

    if(fa->allocate_frame_by_count(fa, heap_frames_cnt, FRAME_ALLOCATION_TYPE_USED | FRAME_ALLOCATION_TYPE_BLOCK, &heap_frames, NULL) != 0) {
        PRINTLOG(TASKING, LOG_ERROR, "cannot allocate heap with frame count 0x%llx", heap_frames_cnt);

        if(fa->release_frame(fa, stack_frames) != 0) {
            PRINTLOG(TASKING, LOG_ERROR, "cannot release stack with frames at 0x%llx with count 0x%llx", stack_frames->frame_address, stack_frames->frame_count);

            cpu_hlt();
        }

        memory_free_ext(heap, new_task);
        memory_free_ext(heap, registers);

        return -1;
    }

    uint64_t stack_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(stack_frames->frame_address);

    if(memory_paging_add_va_for_frame(stack_va, stack_frames, MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
        PRINTLOG(TASKING, LOG_ERROR, "cannot add stack va 0x%llx for frame at 0x%llx with count 0x%llx", stack_va, stack_frames->frame_address, stack_frames->frame_count);

        cpu_hlt();
    }

    memory_memclean((void*)stack_va, stack_size);

    uint64_t heap_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(heap_frames->frame_address);

    if(memory_paging_add_va_for_frame(heap_va, heap_frames, MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
        PRINTLOG(TASKING, LOG_ERROR, "cannot add heap va 0x%llx for frame at 0x%llx with count 0x%llx", heap_va, heap_frames->frame_address, heap_frames->frame_count);

        cpu_hlt();
    }

    memory_heap_t* task_heap = NULL;

    if(heap_size >= (16 << 20)) {
        task_heap = memory_create_heap_hash(heap_va, heap_va + heap_size);
    } else {
        task_heap = memory_create_heap_simple(heap_va, heap_va + heap_size);
    }


    lock_acquire(task_next_task_id_lock);
    uint64_t new_task_id = task_next_task_id++;
    lock_release(task_next_task_id_lock);

    task_heap->task_id = new_task_id;

    new_task->heap        = task_heap;
    new_task->heap_size   = heap_size;
    new_task->task_id     = new_task_id;
    new_task->state       = TASK_STATE_CREATED;
    new_task->entry_point = entry_point;
    new_task->page_table  = memory_paging_get_table();
    new_task->registers   = registers;
    new_task->stack_size  = stack_size;
    new_task->stack       = (void*)stack_va;
    new_task->task_name   = strdup_at_heap(task_map_heap, task_name);

    new_task->arguments_count = args_cnt;
    new_task->arguments       = args;

    registers->rflags = 0x002;

    uint64_t cr3_fa = (uint64_t)new_task->page_table->page_table;
    cr3_fa = MEMORY_PAGING_GET_FA_FOR_RESERVED_VA(cr3_fa);

    registers->cr3 = cr3_fa;

    registers->xsave_mask_lo = task_xsave_mask & 0xFFFFFFFF;
    registers->xsave_mask_hi = task_xsave_mask >> 32;

    *(uint16_t*)(void*)&registers->avx512f[0]  = 0x37F;
    *(uint32_t*)(void*)&registers->avx512f[24] = 0x1F80 & task_mxcsr_mask;

    uint64_t rbp = (uint64_t)new_task->stack;
    rbp           += stack_size - 16;
    registers->rbp = rbp;
    registers->rsp = rbp - 16; // 24 is for last return address, entry point and end task


    uint64_t* stack = (uint64_t*)rbp;
    stack[-1] = 0;
    stack[-2] = (uint64_t)task_end_task; // entry_point;

    memory_heap_t* sheap = spool_get_heap();

    new_task->input_buffer  = buffer_create_with_heap(sheap, 0x1000);
    new_task->output_buffer = buffer_create_with_heap(sheap, 0x1000);
    new_task->error_buffer  = buffer_create_with_heap(sheap, 0x1000);

    spool_add(new_task->task_name, 3, new_task->input_buffer, new_task->output_buffer, new_task->error_buffer);

    PRINTLOG(TASKING, LOG_INFO, "scheduling new task %s 0x%llx 0x%p stack at 0x%llx-0x%llx heap at 0x%p[0x%llx]",
             new_task->task_name, new_task->task_id, new_task, registers->rsp, registers->rbp, new_task->heap, new_task->heap_size);

    uint64_t cpu_count    = apic_get_ap_count() + 1;
    size_t min_queue_size = -1;
    list_t* min_queue     = NULL;

    for(uint64_t i = 0; i < cpu_count; i++) {
        size_t task_count = list_size(task_queues[i]) + list_size(task_sleep_queues[i]) + list_size(task_wait_queues[i]);

        if(task_count < min_queue_size) {
            min_queue_size   = task_count;
            min_queue        = task_queues[i];
            new_task->cpu_id = i;
        }
    }

    hashmap_put(task_map, (void*)new_task->task_id, new_task);
    list_stack_push(min_queue, new_task);


    PRINTLOG(TASKING, LOG_INFO, "task %s 0x%llx added to task queue on cpu 0x%llx", new_task->task_name, new_task->task_id, new_task->cpu_id);

    return new_task->task_id;
}

static void task_idle_task(void) {
    cpu_cpuid_regs_t query = {0};
    query.eax = 0x1;
    cpu_cpuid_regs_t answer = {0};

    cpu_cpuid(query, &answer);

    boolean_t monitor_mwait_supported = (answer.ecx >> 3) & 0x1;

    query.eax = 0x5;
    query.ecx = 0x0;
    query.edx = 0x0;
    query.ebx = 0x0;
    cpu_cpuid(query, &answer);

    boolean_t monitor_mwait_extended_supported = (answer.ecx >> 1) & 0x1;

    register uint64_t data asm ("r11") = cpu_read_gs_base();

    if(monitor_mwait_supported && monitor_mwait_extended_supported) {
        asm volatile (
            "xor %rcx, %rcx\n"
            "xor %rdx, %rdx\n");
        while(true) {
            asm volatile (
                "mov %0, %%rax\n"
                "monitor\n"
                "mov $1, %%rcx\n"
                "mov $1, %%rax\n"
                "mwait\n"
                :
                : "r" (data)
                :
                "rax", "rdx", "memory"
                );
        }
    } else {
        while(true) {
            asm volatile ("sti\nhlt\n");
        }
    }

}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
static int8_t task_create_idle_task(void) {
    program_header_t* kernel = (program_header_t*)SYSTEM_INFO->program_header_virtual_start;
    memory_heap_t* heap      = task_map_heap;

    task_t* new_task = memory_malloc_ext(heap, sizeof(task_t), 0x40);

    if(new_task == NULL) {
        return -1;
    }

    new_task->creator_heap = heap;
    new_task->heap         = heap;
    new_task->heap_size    = kernel->program_heap_size;

    cpu_registers_t* registers = memory_malloc_ext(heap, sizeof(cpu_registers_t), 0x40);

    if(registers == NULL) {
        memory_free_ext(heap, new_task);

        return -1;
    }

    frame_allocator_t* fa = frame_get_allocator();

    uint64_t stack_size = 128 << 10;
    frame_t* stack_frames;
    uint64_t stack_frames_cnt = (stack_size + FRAME_SIZE - 1) / FRAME_SIZE;
    stack_size = stack_frames_cnt * FRAME_SIZE;

    if(fa->allocate_frame_by_count(fa, stack_frames_cnt, FRAME_ALLOCATION_TYPE_USED | FRAME_ALLOCATION_TYPE_BLOCK, &stack_frames, NULL) != 0) {
        PRINTLOG(TASKING, LOG_ERROR, "cannot allocate stack with frame count 0x%llx", stack_frames_cnt);
        memory_free_ext(heap, new_task);
        memory_free_ext(heap, registers);

        return -1;
    }

    uint64_t stack_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(stack_frames->frame_address);

    if(memory_paging_add_va_for_frame(stack_va, stack_frames, MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
        PRINTLOG(TASKING, LOG_ERROR, "cannot add stack va 0x%llx for frame at 0x%llx with count 0x%llx", stack_va, stack_frames->frame_address, stack_frames->frame_count);

        cpu_hlt();
    }
    new_task->task_id = cpu_state->local_apic_id + 1;
    new_task->cpu_id  = cpu_state->local_apic_id;

    new_task->state       = TASK_STATE_CREATED;
    new_task->entry_point = task_idle_task;
    new_task->page_table  = memory_paging_get_table();
    new_task->registers   = registers;
    new_task->stack_size  = stack_size;
    new_task->stack       = (void*)stack_va;

    char_t* tmp_task_name = strprintf("%s-%lli", "idle", cpu_state->local_apic_id);

    new_task->task_name = strdup_at_heap(heap, tmp_task_name);

    memory_free(tmp_task_name);

    registers->rflags = 0x002;

    uint64_t cr3_fa = (uint64_t)new_task->page_table->page_table;
    cr3_fa = MEMORY_PAGING_GET_FA_FOR_RESERVED_VA(cr3_fa);

    registers->cr3 = cr3_fa;

    registers->xsave_mask_lo = task_xsave_mask & 0xFFFFFFFF;
    registers->xsave_mask_hi = task_xsave_mask >> 32;

    *(uint16_t*)(void*)&registers->avx512f[0]  = 0x37F;
    *(uint32_t*)(void*)&registers->avx512f[24] = 0x1F80 & task_mxcsr_mask;

    uint64_t rbp = (uint64_t)new_task->stack;
    rbp           += stack_size - 16;
    registers->rbp = rbp;
    registers->rsp = rbp - 16;

    uint64_t* stack = (uint64_t*)rbp;
    stack[-1] = 0;
    stack[-2] = (uint64_t)task_end_task;

    cpu_state->idle_task = new_task;

    memory_heap_t* sheap = spool_get_heap();

    new_task->output_buffer = buffer_create_with_heap(sheap, 0x1000);
    new_task->error_buffer  = buffer_create_with_heap(sheap, 0x1000);

    spool_add(new_task->task_name, 2, new_task->output_buffer, new_task->error_buffer);

    hashmap_put(task_map, (void*)new_task->task_id, new_task);

    PRINTLOG(TASKING, LOG_INFO, "created idle task %s 0x%llx 0x%p stack at 0x%llx-0x%llx on cpu 0x%llx", new_task->task_name, new_task->task_id, new_task, registers->rsp, registers->rbp, new_task->cpu_id);

    return 0;
}
#pragma GCC diagnostic pop

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
static int8_t task_create_cleaner_task(void) {
    program_header_t* kernel = (program_header_t*)SYSTEM_INFO->program_header_virtual_start;
    memory_heap_t* heap      = task_map_heap;

    task_t* new_task = memory_malloc_ext(heap, sizeof(task_t), 0x40);

    if(new_task == NULL) {
        return -1;
    }

    new_task->creator_heap = heap;
    new_task->heap         = heap;
    new_task->heap_size    = kernel->program_heap_size;

    cpu_registers_t* registers = memory_malloc_ext(heap, sizeof(cpu_registers_t), 0x40);

    if(registers == NULL) {
        memory_free_ext(heap, new_task);

        return -1;
    }

    frame_allocator_t* fa = frame_get_allocator();

    uint64_t stack_size = 2 << 20;
    frame_t* stack_frames;
    uint64_t stack_frames_cnt = (stack_size + FRAME_SIZE - 1) / FRAME_SIZE;
    stack_size = stack_frames_cnt * FRAME_SIZE;

    if(fa->allocate_frame_by_count(fa, stack_frames_cnt, FRAME_ALLOCATION_TYPE_USED | FRAME_ALLOCATION_TYPE_BLOCK, &stack_frames, NULL) != 0) {
        PRINTLOG(TASKING, LOG_ERROR, "cannot allocate stack with frame count 0x%llx", stack_frames_cnt);
        memory_free_ext(heap, new_task);
        memory_free_ext(heap, registers);

        return -1;
    }

    uint64_t stack_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(stack_frames->frame_address);

    if(memory_paging_add_va_for_frame(stack_va, stack_frames, MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
        PRINTLOG(TASKING, LOG_ERROR, "cannot add stack va 0x%llx for frame at 0x%llx with count 0x%llx", stack_va, stack_frames->frame_address, stack_frames->frame_count);

        cpu_hlt();
    }
    new_task->task_id = cpu_state->local_apic_id + 1 + apic_get_ap_count() + 1;
    new_task->cpu_id  = cpu_state->local_apic_id;

    new_task->state       = TASK_STATE_CREATED;
    new_task->entry_point = task_cleaner_task;
    new_task->page_table  = memory_paging_get_table();
    new_task->registers   = registers;
    new_task->stack_size  = stack_size;
    new_task->stack       = (void*)stack_va;

    char_t* tmp_task_name = strprintf("%s-%lli", "task-cleaner", cpu_state->local_apic_id);

    new_task->task_name = strdup_at_heap(heap, tmp_task_name);

    memory_free(tmp_task_name);

    registers->rflags = 0x002;

    uint64_t cr3_fa = (uint64_t)new_task->page_table->page_table;
    cr3_fa = MEMORY_PAGING_GET_FA_FOR_RESERVED_VA(cr3_fa);

    registers->cr3 = cr3_fa;

    registers->xsave_mask_lo = task_xsave_mask & 0xFFFFFFFF;
    registers->xsave_mask_hi = task_xsave_mask >> 32;

    *(uint16_t*)(void*)&registers->avx512f[0]  = 0x37F;
    *(uint32_t*)(void*)&registers->avx512f[24] = 0x1F80 & task_mxcsr_mask;

    uint64_t rbp = (uint64_t)new_task->stack;
    rbp           += stack_size - 16;
    registers->rbp = rbp;
    registers->rsp = rbp - 16;

    uint64_t* stack = (uint64_t*)rbp;
    stack[-1] = 0;
    stack[-2] = (uint64_t)task_end_task;

    cpu_state->cleaner_task = new_task;

    memory_heap_t* sheap = spool_get_heap();

    new_task->output_buffer = buffer_create_with_heap(sheap, 0x1000);
    new_task->error_buffer  = buffer_create_with_heap(sheap, 0x1000);

    spool_add(new_task->task_name, 2, new_task->output_buffer, new_task->error_buffer);

    hashmap_put(task_map, (void*)new_task->task_id, new_task);

    PRINTLOG(TASKING, LOG_INFO, "created cleaner task %s 0x%llx 0x%p stack at 0x%llx-0x%llx on cpu 0x%llx", new_task->task_name, new_task->task_id, new_task, registers->rsp, registers->rbp, new_task->cpu_id);

    return 0;
}
#pragma GCC diagnostic pop

void task_yield(void) {
    if(!cpu_state->tasking_enabled) {
        return;
    }

    cpu_cli();
    task_task_switch_set_parameters(false);
    task_switch_task();
    task_task_switch_exit();
    cpu_sti();
}

static int8_t task_task_switch_isr(interrupt_frame_ext_t* frame) {
    UNUSED(frame);

    task_task_switch_set_parameters(true);
    task_switch_task();
    task_task_switch_exit();

    return 0;
}

void task_remove_task_after_fault(uint64_t task_id) {
    if(task_id == 0) {
        PRINTLOG(TASKING, LOG_ERROR, "task_remove_task_after_fault: task id is 0");
        return;
    }

    task_t* task = (task_t*)hashmap_get(task_map, (void*)task_id);

    if(task == NULL) {
        PRINTLOG(TASKING, LOG_ERROR, "task_remove_task_after_fault: task 0x%llx not found", task_id);
        return;
    }

    PRINTLOG(TASKING, LOG_WARNING, "task_remove_task_after_fault: task 0x%llx", task_id);

    task->state = TASK_STATE_ENDED;

    if(task->vmcs_physical_address) {
        if(cpu_get_type() == CPU_TYPE_INTEL) {
            if(vmx_vmclear(task->vmcs_physical_address) != 0) {
                PRINTLOG(TASKING, LOG_ERROR, "vmclear failed for task 0x%llx", task->task_id);
            }
        }  else if(cpu_get_type() == CPU_TYPE_AMD) {

        }
    }

    if(cpu_state->current_task && cpu_state->current_task == task) {
        // if current task caused the fault, it is obious,
        // we should push it to cleanup queue
        // otherwise, it will be dangling.

        if(task->registers) {
            task->registers->rax = -1; // set return value to -1 to indicate fault
        }

        task->exit_code = -1;

        list_queue_push(cpu_state->task_cleanup_queue, task);
    }

    task_t* current_task = (task_t*)cpu_state->idle_task;

    current_task->last_tick_count = rdtsc();
    current_task->task_switch_count++;

    switch(current_task->state) {
    case TASK_STATE_CREATED:
        current_task->state = TASK_STATE_STARTING;
        break;
    default:
        current_task->state = TASK_STATE_RUNNING;
        break;
    }

    cpu_state->current_task = current_task;

    if(current_task->vmcs_physical_address) {
        if(cpu_get_type() == CPU_TYPE_INTEL) {
            if(vmx_vmptrld(current_task->vmcs_physical_address) != 0) {
                utoh_with_buffer(task_switch_task_id_buf, current_task->task_id);
                video_text_print("vmptrld failed for task 0x");
                video_text_print(task_switch_task_id_buf);
                video_text_print("\n");
                return;
            }

            vmx_write(VMX_HOST_FS_BASE, cpu_read_fs_base());
            vmx_write(VMX_HOST_GS_BASE, cpu_read_gs_base());
        } else if(cpu_get_type() == CPU_TYPE_AMD) {

        }
    }

    // idle task always opens interrupts and we dont need EOI because if we are there,
    // it is a fault interrupt which does not require EOI.

    task_load_registers(current_task->registers);

    asm volatile ("" ::: "memory"); // prevent compiler jmp directly to the task_load_registers
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
int8_t task_init_tasking_ext(memory_heap_t* heap) {
    PRINTLOG(TASKING, LOG_INFO, "tasking system initialization started");

    uint32_t apic_id = cpu_state->local_apic_id;

    task_max_tick_count_limit = TASK_MAX_TICK_COUNT * time_timer_get_rdtsc_delta();

    frame_allocator_t* fa = frame_get_allocator();

    descriptor_register_t gdtr = descriptor_get_gdt_register();
    descriptor_gdt_t* gdts     = (descriptor_gdt_t*)gdtr.base;

    descriptor_tss_t* d_tss = (descriptor_tss_t*)&gdts[3];

    size_t tmp_selector   = (size_t)d_tss - (size_t)gdts;
    uint16_t tss_selector = (uint16_t)tmp_selector;

    PRINTLOG(TASKING, LOG_TRACE, "tss selector 0x%x",  tss_selector);

    program_header_t* kernel = (program_header_t*)SYSTEM_INFO->program_header_virtual_start;
    uint64_t stack_size      = kernel->program_stack_size;
    uint64_t stack_top       = kernel->program_stack_virtual_address;

    PRINTLOG(TASKING, LOG_INFO, "stack top 0x%llx size 0x%llx", stack_top, stack_size);

    uint64_t frame_count = 10 * stack_size / FRAME_SIZE;

    frame_t* stack_frames = NULL;

    if(fa->allocate_frame_by_count(fa, frame_count, FRAME_ALLOCATION_TYPE_BLOCK, &stack_frames, NULL) != 0) {
        PRINTLOG(TASKING, LOG_FATAL, "cannot allocate stack frames of count 0x%llx", frame_count);

        return -1;
    }

    uint64_t stack_bottom = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(stack_frames->frame_address);

    uint64_t tss_size = sizeof(tss_t);
    tss_size += 0x1000 - (tss_size % 0x1000);

    frame_t* tss_fa = NULL;

    if(fa->allocate_frame_by_count(fa,
                                   tss_size / FRAME_SIZE,
                                   FRAME_ALLOCATION_TYPE_BLOCK,
                                   &tss_fa, NULL) != 0) {
        PRINTLOG(KERNEL, LOG_FATAL, "cannot allocate frames for tss");

        return -1;
    }

    uint64_t tss_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(tss_fa->frame_address);

    if(memory_paging_add_va_for_frame(tss_va, tss_fa, MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
        PRINTLOG(KERNEL, LOG_ERROR, "cannot add va for tss frame");

        fa->release_frame(fa, tss_fa);
        fa->release_frame(fa, stack_frames);

        return -1;
    }

    memory_memclean((void*)tss_va, tss_size);

    tss_t* tss = (tss_t*)tss_va;

    cpu_state->tss_va       = tss_va;
    cpu_state->tss_size     = tss_size;
    cpu_state->tss_selector = tss_selector;

    if(memory_paging_add_va_for_frame(stack_bottom, stack_frames, MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
        PRINTLOG(TASKING, LOG_FATAL, "cannot add stack va 0x%llx for frame at 0x%llx with count 0x%llx", stack_bottom, stack_frames->frame_address, stack_frames->frame_count);

        return -1;
    }

    memory_memclean((void*)stack_bottom, frame_count * FRAME_SIZE);

    PRINTLOG(TASKING, LOG_TRACE, "for tasking frames 0x%llx with count 0x%llx mapped to 0x%llx",  stack_frames->frame_address, stack_frames->frame_count, stack_bottom);

    tss->ist7 = stack_bottom + stack_size - 0x10;
    tss->ist6 = tss->ist7  + stack_size;
    tss->ist5 = tss->ist6  + stack_size;
    tss->ist4 = tss->ist5  + stack_size;
    tss->ist3 = tss->ist4  + stack_size;
    tss->ist2 = tss->ist3  + stack_size;
    tss->ist1 = tss->ist2  + stack_size;
    tss->rsp2 = tss->ist1  + stack_size;
    tss->rsp1 = tss->rsp2  + stack_size;
    tss->rsp0 = tss->rsp1  + stack_size;

    uint32_t cpu_count = apic_get_ap_count() + 1;

    PRINTLOG(TASKING, LOG_INFO, "cpu count 0x%x", cpu_count);

    task_queue_and_cleanup_heaps = memory_malloc_ext(heap, sizeof(memory_heap_t*) * cpu_count, 0x0);
    task_queues                  = memory_malloc_ext(heap, sizeof(list_t*) * cpu_count, 0x0);
    task_sleep_queues            = memory_malloc_ext(heap, sizeof(list_t*) * cpu_count, 0x0);
    task_wait_queues             = memory_malloc_ext(heap, sizeof(list_t*) * cpu_count, 0x0);
    task_cleanup_queues          = memory_malloc_ext(heap, sizeof(list_t*) * cpu_count, 0x0);


    for(uint32_t i = 0; i < cpu_count; i++) {
        frame_t* task_related_heap_frames = NULL;

        if(fa->allocate_frame_by_count(fa, 0x1000, FRAME_ALLOCATION_TYPE_USED | FRAME_ALLOCATION_TYPE_BLOCK, &task_related_heap_frames, NULL) != 0) {
            PRINTLOG(TASKING, LOG_FATAL, "cannot allocate task related heap frames of count 0x200");

            return -1;
        }

        uint64_t task_related_heap_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(task_related_heap_frames->frame_address);

        if(memory_paging_add_va_for_frame(task_related_heap_va, task_related_heap_frames, MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
            PRINTLOG(TASKING, LOG_FATAL, "cannot add task related heap va 0x%llx for frame at 0x%llx with count 0x%llx", task_related_heap_va, task_related_heap_frames->frame_address, task_related_heap_frames->frame_count);

            return -1;
        }

        memory_heap_t* task_related_heap = memory_create_heap_simple(task_related_heap_va,
                                                                     task_related_heap_va + 0x1000 * FRAME_SIZE);

        if(task_related_heap == NULL) {
            PRINTLOG(TASKING, LOG_FATAL, "cannot create task related heap");

            return -1;
        }

        PRINTLOG(TASKING, LOG_DEBUG, "cpu 0x%x task related heap 0x%p", i, task_related_heap);

        task_queue_and_cleanup_heaps[i] = task_related_heap;

        task_queues[i] = list_create_queue_with_heap(task_related_heap);

        if(task_queues[i] == NULL) {
            PRINTLOG(TASKING, LOG_FATAL, "cannot create task queue");

            return -1;
        }

        PRINTLOG(TASKING, LOG_DEBUG, "cpu 0x%x task queue 0x%p", i, task_queues[i]);

        task_sleep_queues[i] = list_create_sortedlist_with_heap(task_related_heap, &task_sleep_queue_comparator);

        if(task_sleep_queues[i] == NULL) {
            PRINTLOG(TASKING, LOG_FATAL, "cannot create task sleep queue");

            return -1;
        }

        PRINTLOG(TASKING, LOG_DEBUG, "cpu 0x%x task sleep queue 0x%p", i, task_sleep_queues[i]);

        task_wait_queues[i] = list_create_queue_with_heap(task_related_heap);

        if(task_wait_queues[i] == NULL) {
            PRINTLOG(TASKING, LOG_FATAL, "cannot create task wait queue");

            return -1;
        }

        PRINTLOG(TASKING, LOG_DEBUG, "cpu 0x%x task wait queue 0x%p", i, task_wait_queues[i]);

        task_cleanup_queues[i] = list_create_queue_with_heap(task_related_heap);

        if(task_cleanup_queues[i] == NULL) {
            PRINTLOG(TASKING, LOG_FATAL, "cannot create task cleanup queue");

            return -1;
        }

        PRINTLOG(TASKING, LOG_DEBUG, "cpu 0x%x task cleanup queue 0x%p", i, task_cleanup_queues[i]);
    }

    {
        frame_t* task_related_heap_frames = NULL;

        if(fa->allocate_frame_by_count(fa, 0x1000, FRAME_ALLOCATION_TYPE_USED | FRAME_ALLOCATION_TYPE_BLOCK, &task_related_heap_frames, NULL) != 0) {
            PRINTLOG(TASKING, LOG_FATAL, "cannot allocate task related heap frames of count 0x200");

            return -1;
        }

        uint64_t task_related_heap_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(task_related_heap_frames->frame_address);

        if(memory_paging_add_va_for_frame(task_related_heap_va, task_related_heap_frames, MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
            PRINTLOG(TASKING, LOG_FATAL, "cannot add task related heap va 0x%llx for frame at 0x%llx with count 0x%llx", task_related_heap_va, task_related_heap_frames->frame_address, task_related_heap_frames->frame_count);

            return -1;
        }

        memory_heap_t* task_related_heap = memory_create_heap_simple(task_related_heap_va,
                                                                     task_related_heap_va + 0x1000 * FRAME_SIZE);

        if(task_related_heap == NULL) {
            PRINTLOG(TASKING, LOG_FATAL, "cannot create task related heap");

            return -1;
        }

        task_map_heap = task_related_heap;

        PRINTLOG(TASKING, LOG_DEBUG, "task map heap 0x%p", task_map_heap);
    }


    cpu_state->task_queue         = task_queues[0];
    cpu_state->task_sleep_queue   = task_sleep_queues[0];
    cpu_state->task_wait_queue    = task_wait_queues[0];
    cpu_state->task_cleanup_queue = task_cleanup_queues[0];

    interrupt_irq_set_handler(0xde, &task_task_switch_isr);

    task_t* kernel_task = memory_malloc_ext(task_map_heap, sizeof(task_t), 0x40);

    if(kernel_task == NULL) {
        PRINTLOG(TASKING, LOG_FATAL, "cannot allocate memory for kernel task");

        return -1;
    }

    kernel_task->creator_heap = task_map_heap;
    kernel_task->heap         = heap;
    kernel_task->heap_size    = kernel->program_heap_size;
    kernel_task->task_id      = cpu_count * 2 + 1; // 1 for idle task and 1 for cleaner task for each cpu
    kernel_task->state        = TASK_STATE_RUNNING;
    kernel_task->entry_point  = kmain64;
    kernel_task->page_table   = memory_paging_get_table();
    kernel_task->registers    = memory_malloc_ext(task_map_heap, sizeof(cpu_registers_t), 0x40);

    if(kernel_task->registers == NULL) {
        PRINTLOG(TASKING, LOG_FATAL, "cannot allocate memory for kernel task fx registers");

        return -1;
    }

    char_t* tmp_task_name = strprintf("%s-%d", "kernel-init", apic_id);
    kernel_task->task_name = strdup_at_heap(task_map_heap, tmp_task_name);
    memory_free(tmp_task_name);

    kernel_task->stack         = (void*)(stack_top);
    kernel_task->stack_size    = stack_size;
    kernel_task->input_buffer  = stdbufs_default_input_buffer;
    kernel_task->output_buffer = stdbufs_default_output_buffer;
    kernel_task->error_buffer  = stdbufs_default_error_buffer;

    cpu_cpuid_regs_t query = {0};
    cpu_cpuid_regs_t result;

    query.eax = 0xd;

    cpu_cpuid(query, &result);

    task_xsave_mask = ((uint64_t)result.edx << 32) | result.eax;

    kernel_task->registers->xsave_mask_lo = result.eax;
    kernel_task->registers->xsave_mask_hi = result.edx;

    PRINTLOG(TASKING, LOG_INFO, "xsave mask 0x%llx", task_xsave_mask);

    // get mxcsr
    task_save_registers(kernel_task->registers);

    task_mxcsr_mask = *(uint32_t*)(void*)&kernel_task->registers->avx512f[28];

    PRINTLOG(TASKING, LOG_INFO, "mxcsr mask 0x%x", task_mxcsr_mask);

    task_next_task_id      = kernel_task->task_id + 1;
    task_next_task_id_lock = lock_create();

    task_map = hashmap_integer_with_heap(task_map_heap, 128);

    if(task_map == NULL) {
        PRINTLOG(TASKING, LOG_FATAL, "cannot allocate task map");

        return -1;
    }

    hashmap_put(task_map, (void*)kernel_task->task_id, kernel_task);

    uint32_t tss_limit = sizeof(tss_t) - 1;
    DESCRIPTOR_BUILD_TSS_SEG(d_tss, (size_t)tss, tss_limit, DPL_KERNEL);

    PRINTLOG(TASKING, LOG_TRACE, "task register loading with tss 0x%p limit 0x%x", tss, tss_limit);

    __asm__ __volatile__ (
        "cli\n"
        "ltr %0\n"
        "sti\n"
        : : "r" (tss_selector)
        );

    interrupt_ist_redirect_main_interrupts(7);
    interrupt_ist_redirect_interrupt(0xd, 6);
    interrupt_ist_redirect_interrupt(0xe, 5);

    cpu_state->current_task = kernel_task;

    if(task_create_idle_task() != 0) {
        PRINTLOG(TASKING, LOG_FATAL, "cannot create idle task");

        return -1;
    }

    if(task_create_cleaner_task() != 0) {
        PRINTLOG(TASKING, LOG_FATAL, "cannot create cleaner task");

        return -1;
    }

    PRINTLOG(TASKING, LOG_INFO, "tasking system initialization ended, kernel task address 0x%p lapic id %d", kernel_task, apic_id);

    memory_set_current_task_getter(&task_get_current_task);

    lock_get_current_task_getter = &task_get_current_task;
    lock_task_yielder            = &task_yield;

    stdbufs_task_get_input_buffer  = &task_get_input_buffer;
    stdbufs_task_get_output_buffer = &task_get_output_buffer;
    stdbufs_task_get_error_buffer  = &task_get_error_buffer;

    future_task_wait_toggler_func = &task_toggle_wait_for_future;

    cpu_state->tasking_enabled = true;
    cpu_state->parked          = false;
    cpu_state->in_parked_state = false;

    return 0;
}
#pragma GCC diagnostic pop

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
int8_t task_set_current_and_idle_task(void* entry_point, uint64_t stack_base, uint64_t stack_size) {
    memory_heap_t* heap      = task_map_heap;
    program_header_t* kernel = (program_header_t*)SYSTEM_INFO->program_header_virtual_start;
    smp_data_t* smp_data     = (smp_data_t*)0x9000;

    uint32_t apic_id = cpu_state->local_apic_id;

    if(task_queues[apic_id] == NULL || task_cleanup_queues[apic_id] == NULL ||
       task_sleep_queues[apic_id] == NULL || task_wait_queues[apic_id] == NULL) {
        PRINTLOG(TASKING, LOG_FATAL, "task queues for apic id %d are null", apic_id);

        return -1;
    }

    cpu_state->task_queue         = task_queues[apic_id];
    cpu_state->task_sleep_queue   = task_sleep_queues[apic_id];
    cpu_state->task_wait_queue    = task_wait_queues[apic_id];
    cpu_state->task_cleanup_queue = task_cleanup_queues[apic_id];
    cpu_state->local_apic_id      = apic_id;

    task_t* current_task = memory_malloc_ext(heap, sizeof(task_t), 0x40);

    if(current_task == NULL) {
        PRINTLOG(TASKING, LOG_FATAL, "cannot allocate memory for kernel task");

        return -1;
    }

    lock_acquire(task_next_task_id_lock);
    current_task->task_id = task_next_task_id++;
    lock_release(task_next_task_id_lock);

    current_task->cpu_id = apic_id;

    char_t* tmp_task_name = NULL;

    if(!smp_data->is_for_wakeup) {
        tmp_task_name = strprintf("%s-%d", "kernel-init", apic_id);
    } else {
        tmp_task_name = strprintf("%s-%lli-%d", "kernel-wakeup", smp_data->wakeup_count, apic_id);
    }

    current_task->task_name = strdup_at_heap(heap, tmp_task_name);

    memory_free(tmp_task_name);

    current_task->creator_heap = heap;
    current_task->heap         = heap;
    current_task->heap_size    = kernel->program_heap_size;
    current_task->state        = TASK_STATE_RUNNING;
    current_task->entry_point  = entry_point;
    current_task->page_table   = memory_paging_get_table();
    current_task->registers    = memory_malloc_ext(heap, sizeof(cpu_registers_t), 0x40);

    if(current_task->registers == NULL) {
        memory_free_ext(heap, current_task);
        PRINTLOG(TASKING, LOG_FATAL, "cannot allocate memory for kernel task fx registers");

        return -1;
    }

    current_task->registers->xsave_mask_lo = task_xsave_mask & 0xFFFFFFFF;
    current_task->registers->xsave_mask_hi = task_xsave_mask >> 32;

    *(uint16_t*)(void*)&current_task->registers->avx512f[0]  = 0x37F;
    *(uint32_t*)(void*)&current_task->registers->avx512f[24] = 0x1F80 & task_mxcsr_mask;

    memory_heap_t* sheap = spool_get_heap();

    current_task->input_buffer  = buffer_create_with_heap(sheap, 0x1000);
    current_task->output_buffer = buffer_create_with_heap(sheap, 0x1000);
    current_task->error_buffer  = buffer_create_with_heap(sheap, 0x1000);

    spool_add(current_task->task_name, 3, current_task->input_buffer, current_task->output_buffer, current_task->error_buffer);

    current_task->is_stack_protected = true;
    current_task->stack              = (void*)stack_base;
    current_task->stack_size         = stack_size;

    task_save_registers(current_task->registers);

    cpu_state->current_task = current_task;

    hashmap_put(task_map, (void*)current_task->task_id, current_task);

    if(smp_data->is_for_wakeup) {
        cpu_state->tasking_enabled = true;
        cpu_state->parked          = false;
        cpu_state->in_parked_state = false;

        return 0;
    }

    if(task_create_idle_task() != 0) {
        PRINTLOG(TASKING, LOG_FATAL, "cannot create idle task");

        return -1;
    }

    if(task_create_cleaner_task() != 0) {
        PRINTLOG(TASKING, LOG_FATAL, "cannot create cleaner task");

        return -1;
    }

    cpu_state->tasking_enabled = true;

    return 0;
}
#pragma GCC diagnostic pop

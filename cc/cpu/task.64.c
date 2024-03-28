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
#include <memory/paging.h>
#include <memory/frame.h>
#include <list.h>
#include <time/timer.h>
#include <logging.h>
#include <systeminfo.h>
#include <linker.h>
#include <utils.h>
#include <map.h>
#include <stdbufs.h>
#include <hypervisor/hypervisor_vmxops.h>
#include <hypervisor/hypervisor_vm.h>
#include <hypervisor/hypervisor_macros.h>
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

uint64_t task_next_task_id = 0;
lock_t* task_next_task_id_lock = NULL;

volatile boolean_t task_tasking_initialized = false;

list_t** task_queues = NULL;
list_t** task_cleanup_queues = NULL;
map_t task_map = NULL;
uint32_t task_mxcsr_mask = 0;

extern int8_t kmain64(void);

int8_t                                          task_task_switch_isr(interrupt_frame_ext_t* frame);
__attribute__((naked, no_stack_protector)) void task_save_registers(task_registers_t* registers);
__attribute__((naked, no_stack_protector)) void task_load_registers(task_registers_t* registers);
void                                            task_cleanup(void);
task_t*                                         task_find_next_task(void);


void   task_idle_task(void);
int8_t task_create_idle_task(void);

extern boolean_t local_apic_id_is_valid;
extern volatile cpu_state_t __seg_gs * cpu_state;

lock_t * task_find_next_task_lock = NULL;

task_t* task_get_current_task(void){
    if(!task_tasking_initialized) {
        return NULL;
    }

    return (task_t*)cpu_state->current_task;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
int8_t task_init_tasking_ext(memory_heap_t* heap) {
    PRINTLOG(TASKING, LOG_INFO, "tasking system initialization started");

    uint32_t apic_id = apic_get_local_apic_id();


    frame_t* kernel_gs_frames = NULL;

    if(frame_get_allocator()->allocate_frame_by_count(frame_get_allocator(), 4, FRAME_ALLOCATION_TYPE_RESERVED | FRAME_ALLOCATION_TYPE_BLOCK, &kernel_gs_frames, NULL) != 0) {
        PRINTLOG(TASKING, LOG_FATAL, "cannot allocate stack frames of count 4");

        return -1;
    }

    uint64_t kernel_gs_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(kernel_gs_frames->frame_address);

    memory_paging_add_va_for_frame(kernel_gs_va, kernel_gs_frames, MEMORY_PAGING_PAGE_TYPE_NOEXEC);

    memory_memclean((void*)kernel_gs_va, 0x4000);

    uint64_t old_gs_base = cpu_read_msr(CPU_MSR_IA32_GS_BASE);

    PRINTLOG(TASKING, LOG_TRACE, "old gs base 0x%llx new gs base 0x%llx", old_gs_base, kernel_gs_va);

    cpu_write_msr(CPU_MSR_IA32_GS_BASE, kernel_gs_va);

    cpu_state_t* current_cpu_state = (cpu_state_t*)kernel_gs_va;
    current_cpu_state->local_apic_id = apic_id;
    local_apic_id_is_valid = true;


    descriptor_gdt_t* gdts = (descriptor_gdt_t*)GDT_REGISTER->base;

    descriptor_tss_t* d_tss = (descriptor_tss_t*)&gdts[3];

    size_t tmp_selector = (size_t)d_tss - (size_t)gdts;
    uint16_t tss_selector = (uint16_t)tmp_selector;

    PRINTLOG(TASKING, LOG_TRACE, "tss selector 0x%x",  tss_selector);



    program_header_t* kernel = (program_header_t*)SYSTEM_INFO->program_header_virtual_start;
    uint64_t stack_size = kernel->program_stack_size;
    uint64_t stack_top = kernel->program_stack_virtual_address;


    uint64_t frame_count = 10 * stack_size / FRAME_SIZE;

    frame_t* stack_frames = NULL;

    if(frame_get_allocator()->allocate_frame_by_count(frame_get_allocator(), frame_count, FRAME_ALLOCATION_TYPE_RESERVED | FRAME_ALLOCATION_TYPE_BLOCK, &stack_frames, NULL) != 0) {
        PRINTLOG(TASKING, LOG_FATAL, "cannot allocate stack frames of count 0x%llx", frame_count);

        return -1;
    }

    uint64_t stack_bottom = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(stack_frames->frame_address);

    tss_t* tss = memory_malloc_ext(heap, sizeof(tss_t), 0x1000);

    if(tss == NULL) {
        PRINTLOG(TASKING, LOG_FATAL, "cannot allocate memory for tss");

        return -1;
    }

    memory_paging_add_va_for_frame(stack_bottom, stack_frames, MEMORY_PAGING_PAGE_TYPE_NOEXEC);

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

    task_queues = memory_malloc_ext(heap, sizeof(list_t*) * cpu_count, 0x0);
    task_cleanup_queues = memory_malloc_ext(heap, sizeof(list_t*) * cpu_count, 0x0);

    task_queues[0] = list_create_queue_with_heap(heap);
    task_cleanup_queues[0] = list_create_queue_with_heap(heap);

    current_cpu_state->task_queue = task_queues[0];
    current_cpu_state->task_cleanup_queue = task_cleanup_queues[0];

    interrupt_irq_set_handler(0xde, &task_task_switch_isr);

    task_t* kernel_task = memory_malloc_ext(heap, sizeof(task_t), 0x0);

    if(kernel_task == NULL) {
        PRINTLOG(TASKING, LOG_FATAL, "cannot allocate memory for kernel task");

        return -1;
    }

    kernel_task->creator_heap = heap;
    kernel_task->heap = heap;
    kernel_task->heap_size = kernel->program_heap_size;
    kernel_task->task_id = cpu_count + 1;
    kernel_task->state = TASK_STATE_CREATED;
    kernel_task->entry_point = kmain64;
    kernel_task->page_table = memory_paging_get_table();
    kernel_task->registers = memory_malloc_ext(heap, sizeof(task_registers_t), 0x10);

    if(kernel_task->registers == NULL) {
        PRINTLOG(TASKING, LOG_FATAL, "cannot allocate memory for kernel task fx registers");

        return -1;
    }

    kernel_task->stack = (void*)(stack_top + stack_size);
    kernel_task->stack_size = stack_size;
    kernel_task->task_name = strdup_at_heap(heap, "kernel");
    kernel_task->input_buffer = stdbufs_default_input_buffer;
    kernel_task->output_buffer = stdbufs_default_output_buffer;
    kernel_task->error_buffer = stdbufs_default_error_buffer;

    // get mxcsr
    task_save_registers(kernel_task->registers);

    task_mxcsr_mask = *(uint32_t*)&kernel_task->registers->sse[28];

    PRINTLOG(TASKING, LOG_INFO, "mxcsr mask 0x%x", task_mxcsr_mask);

    task_next_task_id = kernel_task->task_id + 1;
    task_next_task_id_lock = lock_create();

    task_map = map_integer();

    if(task_map == NULL) {
        PRINTLOG(TASKING, LOG_FATAL, "cannot allocate task map");

        return -1;
    }

    map_insert(task_map, (void*)kernel_task->task_id, kernel_task);

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
    // interrupt_ist_redirect_interrupt(0x20, 1);

    cpu_state->current_task = kernel_task;

    if(task_create_idle_task() != 0) {
        PRINTLOG(TASKING, LOG_FATAL, "cannot create idle task");

        return -1;
    }

    task_find_next_task_lock = lock_create();
    PRINTLOG(TASKING, LOG_INFO, "tnt lock 0x%p", task_find_next_task_lock);

    if(task_find_next_task_lock == NULL) {
        PRINTLOG(TASKING, LOG_FATAL, "cannot create task find next task lock");

        return -1;
    }

    PRINTLOG(TASKING, LOG_INFO, "tasking system initialization ended, kernel task address 0x%p lapic id %d", kernel_task, apic_id);

    memory_set_current_task_getter(&task_get_current_task);

    lock_get_current_task_getter = &task_get_current_task;
    lock_task_yielder = &task_yield;

    stdbufs_task_get_input_buffer = &task_get_input_buffer;
    stdbufs_task_get_output_buffer = &task_get_output_buffer;
    stdbufs_task_get_error_buffer = &task_get_error_buffer;

    future_task_wait_toggler_func = &task_toggle_wait_for_future;

    task_tasking_initialized = true;

    return 0;
}
#pragma GCC diagnostic pop

int8_t task_set_current_and_idle_task(void* entry_point, uint64_t stack_base, uint64_t stack_size) {
    memory_heap_t* heap = memory_get_default_heap();
    program_header_t* kernel = (program_header_t*)SYSTEM_INFO->program_header_virtual_start;

    uint32_t apic_id = apic_get_local_apic_id();

    task_queues[apic_id] = list_create_queue_with_heap(heap);
    task_cleanup_queues[apic_id] = list_create_queue_with_heap(heap);

    if(task_queues[apic_id] == NULL || task_cleanup_queues[apic_id] == NULL) {
        list_destroy(task_queues[apic_id]);
        list_destroy(task_cleanup_queues[apic_id]);
        PRINTLOG(TASKING, LOG_FATAL, "cannot create task queue for apic id %d", apic_id);

        return -1;
    }

    cpu_state->task_queue = task_queues[apic_id];
    cpu_state->task_cleanup_queue = task_cleanup_queues[apic_id];

    task_t* current_task = memory_malloc_ext(heap, sizeof(task_t), 0x0);

    if(current_task == NULL) {
        PRINTLOG(TASKING, LOG_FATAL, "cannot allocate memory for kernel task");

        return -1;
    }

    lock_acquire(task_next_task_id_lock);
    current_task->task_id = task_next_task_id++;
    lock_release(task_next_task_id_lock);

    current_task->cpu_id = apic_id;

    char_t* tmp_task_name = sprintf("%s-%d", "kernel-init-%s", current_task->cpu_id);

    current_task->task_name = strdup_at_heap(heap, tmp_task_name);

    memory_free(tmp_task_name);

    current_task->creator_heap = heap;
    current_task->heap = heap;
    current_task->heap_size = kernel->program_heap_size;
    current_task->state = TASK_STATE_RUNNING;
    current_task->entry_point = entry_point;
    current_task->page_table = memory_paging_get_table();
    current_task->registers = memory_malloc_ext(heap, sizeof(task_registers_t), 0x10);

    if(current_task->registers == NULL) {
        memory_free_ext(heap, current_task);
        PRINTLOG(TASKING, LOG_FATAL, "cannot allocate memory for kernel task fx registers");

        return -1;
    }

    memory_heap_t* sheap = spool_get_heap();

    current_task->input_buffer = buffer_create_with_heap(sheap, 0x1000);
    current_task->output_buffer = buffer_create_with_heap(sheap, 0x1000);
    current_task->error_buffer = buffer_create_with_heap(sheap, 0x1000);

    spool_add(current_task->task_name, 3, current_task->input_buffer, current_task->output_buffer, current_task->error_buffer);

    current_task->stack = (void*)stack_base;
    current_task->stack_size = stack_size;
    current_task->task_name = strdup_at_heap(heap, "kernel");

    task_save_registers(current_task->registers);

    cpu_state->current_task = current_task;

    lock_acquire(task_find_next_task_lock);
    map_insert(task_map, (void*)current_task->task_id, current_task);
    lock_release(task_find_next_task_lock);

    if(task_create_idle_task() != 0) {
        PRINTLOG(TASKING, LOG_FATAL, "cannot create idle task");

        return -1;
    }

    return 0;
}

__attribute__((naked, no_stack_protector)) void task_save_registers(task_registers_t* registers) {
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
        "lea %[sse], %%rbx\n"
        "fxsave (%%rbx)\n"
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
        [sse]    "m" (registers->sse),
        [rflags] "m" (registers->rflags),
        [rsp]    "m" (registers->rsp),
        [cr3]     "m" (registers->cr3)
        );
}

__attribute__((naked, no_stack_protector)) void task_load_registers(task_registers_t* registers) {
    __asm__ __volatile__ (
        "mov %[rbx],  %%rbx\n"
        "mov %[rcx],  %%rcx\n"
        "mov %[rdx],  %%rdx\n"
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
        "lea %[sse], %%rax\n"
        "fxrstor (%%rax)\n"
        "mov %[rflags], %%rax\n"
        "push %%rax\n"
        "popfq\n"
        "mov %%cr3, %%rax\n"
        "cmp %[cr3], %%rax\n"
        "je 1f\n"
        "mov %[cr3], %%rax\n"
        "mov %%rax, %%cr3\n"
        "1:\n"
        "mov %[rax],  %%rax\n"
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
        [sse]     "m" (registers->sse),
        [rflags]  "m" (registers->rflags),
        [rsp]     "m" (registers->rsp),
        [cr3]     "m" (registers->cr3)
        );
}

static void task_cleanup_task(task_t* task) {
    if(!task) {
        return;
    }

    if(task->vm) {
        hypervisor_vm_destroy(task->vm);
    }

    memory_free_ext(task->heap, (void*)task->task_name);

    map_delete(task_map, (void*)task->task_id);

    if(task->heap != memory_get_default_heap()) {
        uint64_t stack_va = (uint64_t)task->stack;
        uint64_t stack_fa = MEMORY_PAGING_GET_FA_FOR_RESERVED_VA(stack_va);

        uint64_t stack_size = task->stack_size;
        uint64_t stack_frames_cnt = stack_size / FRAME_SIZE;

        memory_memclean(task->stack, stack_size);

        frame_t stack_frames = {stack_fa, stack_frames_cnt, FRAME_TYPE_USED, FRAME_ALLOCATION_TYPE_USED | FRAME_ALLOCATION_TYPE_BLOCK};

        if(memory_paging_delete_va_for_frame_ext(task->page_table, stack_va, &stack_frames) != 0 ) {
            PRINTLOG(TASKING, LOG_ERROR, "cannot remove pages for stack at va 0x%llx", stack_va);

            cpu_hlt();
        }

        if(frame_get_allocator()->release_frame(frame_get_allocator(), &stack_frames) != 0) {
            PRINTLOG(TASKING, LOG_ERROR, "cannot release stack with frames at 0x%llx with count 0x%llx", stack_fa, stack_frames_cnt);

            cpu_hlt();
        }

        uint64_t heap_va = (uint64_t)task->heap;
        uint64_t heap_fa = MEMORY_PAGING_GET_FA_FOR_RESERVED_VA(heap_va);

        uint64_t heap_size = task->heap_size;
        uint64_t heap_frames_cnt = heap_size / FRAME_SIZE;

        memory_memclean(task->heap, heap_size);

        frame_t heap_frames = {heap_fa, heap_frames_cnt, FRAME_TYPE_USED, FRAME_ALLOCATION_TYPE_USED | FRAME_ALLOCATION_TYPE_BLOCK};

        if(memory_paging_delete_va_for_frame_ext(task->page_table, heap_va, &heap_frames) != 0 ) {
            PRINTLOG(TASKING, LOG_ERROR, "cannot remove pages for heap at va 0x%llx", heap_va);

            if(task->page_table) {
                PRINTLOG(TASKING, LOG_ERROR, "page table 0x%p", task->page_table->page_table);
            }

            cpu_hlt();
        }

        if(frame_get_allocator()->release_frame(frame_get_allocator(), &heap_frames) != 0) {
            PRINTLOG(TASKING, LOG_ERROR, "cannot release heap with frames at 0x%llx with count 0x%llx", stack_fa, heap_frames_cnt);

            cpu_hlt();
        }
    }

    memory_free_ext(task->creator_heap, task->registers);
    memory_free_ext(task->creator_heap, task);
}

void task_cleanup(void){
    lock_acquire(task_find_next_task_lock);
    while(list_size(cpu_state->task_cleanup_queue)) {
        task_t* task = (task_t*)list_queue_pop(cpu_state->task_cleanup_queue);
        task_cleanup_task(task);
    }
    lock_release(task_find_next_task_lock);
}

#if 0
boolean_t task_idle_check_need_yield(void) {
    boolean_t need_yield = false;

    for(uint64_t i = 0; i < list_size(task_queue); i++) {
        task_t* t = (task_t*)list_get_data_at_position(task_queue, i);

        if(!t) {
            continue;
        }

        if(t->state == TASK_STATE_ENDED) {
            continue;
        }

        if(t->message_waiting) {
            if(t->interruptible && t->interrupt_received) {
                // t->interrupt_received = false;
                need_yield = true;
                break;
            }

            if(t->message_queues) {
                for(uint64_t q_idx = 0; q_idx < list_size(t->message_queues); q_idx++) {
                    list_t* q = (list_t*)list_get_data_at_position(t->message_queues, q_idx);

                    if(list_size(q)) {
                        // t->message_waiting = false;
                        need_yield = true;
                        break;
                    }
                }
            }
        } else if(t->sleeping) {
            if(t->wake_tick < time_timer_get_tick_count()) {
                // t->sleeping = false;
                need_yield = true;
                break;
            }
        } else if(t->wait_for_future) {
            break;
        }
    }

    return need_yield;
}
#endif

task_t* task_find_next_task(void) {
    task_t* tmp_task = NULL;

    // lock_acquire(task_find_next_task_lock);

    uint64_t found_index = -1;

    for(uint64_t i = 0; i < list_size(cpu_state->task_queue); i++) {
        task_t* t = (task_t*)list_get_data_at_position(cpu_state->task_queue, i);

        if(t->state == TASK_STATE_ENDED) {
            found_index = i;
            break; // trick to remove ended task from queue
        }

        if(t->wait_for_future) {
            continue;
        } else if(t->sleeping) {
            if(t->wake_tick < time_timer_get_tick_count()) {
                t->sleeping = false;
                found_index = i;
                break;
            }
        } else if(t->message_waiting) {
            if(t->interruptible) {
                if(t->interrupt_received) {
                    t->interrupt_received = false;
                    t->message_waiting = false;
                    found_index = i;
                    break;
                }
            }

            if(t->message_queues) {
                for(uint64_t q_idx = 0; q_idx < list_size(t->message_queues); q_idx++) {
                    list_t* q = (list_t*)list_get_data_at_position(t->message_queues, q_idx);

                    if(list_size(q)) {
                        t->message_waiting = false;
                        found_index = i;
                        break;
                    }
                }
            }
        } else {
            found_index = i;
            break;
        }
    }

    if(found_index != -1ULL) {
        tmp_task = (task_t*)list_delete_at_position(cpu_state->task_queue, found_index);

        // need check it is ended?
        if(tmp_task->state == TASK_STATE_ENDED) {
            // it is already task clear queue so find next
            list_queue_push(cpu_state->task_cleanup_queue, tmp_task);
            tmp_task = (task_t*)cpu_state->idle_task;
        }
    } else {
        tmp_task = (task_t*)cpu_state->idle_task;
    }

    if(!tmp_task) {
        video_text_print("no task found. why?");
        tmp_task = (task_t*)cpu_state->idle_task;
    }

    // lock_release(task_find_next_task_lock);


    // PRINTLOG(TASKING, LOG_WARNING, "task 0x%llx selected for execution. queue size %lli", tmp_task->task_id, list_size(task_queue));

    return tmp_task;
}

void task_task_switch_set_parameters(boolean_t need_eoi, boolean_t need_sti) {
    if(!cpu_state->task_switch_paramters_need_eoi) {
        cpu_state->task_switch_paramters_need_eoi = need_eoi;
    }

    if(!cpu_state->task_switch_paramters_need_sti) {
        cpu_state->task_switch_paramters_need_sti = need_sti;
    }
}

static __attribute__((noinline)) void task_task_switch_exit(void) {
    if(cpu_state->task_switch_paramters_need_eoi) {
        cpu_state->task_switch_paramters_need_eoi = false;
        apic_eoi();
    }

    if(cpu_state->task_switch_paramters_need_sti) {
        cpu_state->task_switch_paramters_need_sti = false;
        cpu_sti();
    }
}

static char_t task_switch_task_id_buf[100] = {0};

__attribute__((no_stack_protector)) void task_switch_task(void) {
    task_t* current_task = cpu_state->current_task;

    if(current_task != cpu_state->idle_task &&
       current_task->state != TASK_STATE_ENDED &&
       !current_task->message_waiting &&
       !current_task->sleeping &&
       (time_timer_get_tick_count() - current_task->last_tick_count) < TASK_MAX_TICK_COUNT &&
       time_timer_get_tick_count() > current_task->last_tick_count) {

        task_task_switch_exit();

        return;
    }

    if(current_task->vmcs_physical_address) {
        if(vmclear(current_task->vmcs_physical_address) != 0) {
            utoh_with_buffer(task_switch_task_id_buf, current_task->task_id);
            video_text_print("vmclear failed for task 0x");
            video_text_print(task_switch_task_id_buf);
            video_text_print("\n");
            return;
        }
    }

    task_save_registers(current_task->registers);

    switch(current_task->state) {
    case TASK_STATE_CREATED:
    case TASK_STATE_ENDED:
        break;
    default:
        current_task->state = TASK_STATE_SUSPENDED;
        break;
    }

    if(current_task != cpu_state->idle_task) {
        list_queue_push(cpu_state->task_queue, current_task);
    }

    if(current_task == cpu_state->idle_task && list_size(cpu_state->task_cleanup_queue) > 0) {
        task_cleanup();
    }

    current_task = task_find_next_task();
    current_task->last_tick_count = time_timer_get_tick_count();
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
        if(vmptrld(current_task->vmcs_physical_address) != 0) {
            utoh_with_buffer(task_switch_task_id_buf, current_task->task_id);
            video_text_print("vmclear failed for task 0x");
            video_text_print(task_switch_task_id_buf);
            video_text_print("\n");
        }

        vmx_write(VMX_HOST_FS_BASE, cpu_read_fs_base());
        vmx_write(VMX_HOST_GS_BASE, cpu_read_gs_base());
    }

    task_load_registers(current_task->registers);

    task_task_switch_exit();
}

void task_end_task(void) {
    task_t* current_task = cpu_state->current_task;

    if(current_task == NULL) {
        PRINTLOG(TASKING, LOG_ERROR, "no current task");
        cpu_hlt();
    }

    typedef int64_t (*entry_point_f)(uint64_t, void**);
    entry_point_f entry_point = current_task->entry_point;

    int64_t ret = 0;

    if(current_task->state == TASK_STATE_STARTING && entry_point != NULL) {
        task_task_switch_exit();
        PRINTLOG(TASKING, LOG_INFO, "starting task %s with pid 0x%llx", current_task->task_name, current_task->task_id);
        ret = entry_point(current_task->arguments_count, current_task->arguments);
    }

    PRINTLOG(TASKING, LOG_INFO, "ending task 0x%llx return code 0x%llx", current_task->task_id, ret);

    if(current_task->vmcs_physical_address) {
        if(vmclear(current_task->vmcs_physical_address) != 0) {
            PRINTLOG(TASKING, LOG_ERROR, "vmclear failed for task 0x%llx", current_task->task_id);
        }
    }

    current_task->message_waiting = false;
    current_task->interruptible = false;
    current_task->wait_for_future = false;

    current_task->state = TASK_STATE_ENDED;

// list_queue_push(task_cleaner_queue, current_task);

// cpu_state->current_task =  NULL;

// video_text_print("task ended");

    task_yield();
}

void task_kill_task(uint64_t task_id, boolean_t force) {
    task_t* task = (task_t*)map_get(task_map, (void*)task_id);

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

    if(task->vmcs_physical_address) {
        if(vmclear(task->vmcs_physical_address) != 0) {
            PRINTLOG(TASKING, LOG_ERROR, "vmclear failed for task 0x%llx", task->task_id);
        }
    }

    task->message_waiting = false;
    task->interruptible = false;
    task->wait_for_future = false;

    task->state = TASK_STATE_ENDED;


    PRINTLOG(TASKING, LOG_INFO, "task 0x%llx will be ended", task->task_id);
}

uint64_t task_create_task(memory_heap_t* heap, uint64_t heap_size, uint64_t stack_size, void* entry_point, uint64_t args_cnt, void** args, const char_t* task_name) {

    task_t* new_task = memory_malloc_ext(heap, sizeof(task_t), 0x0);

    if(new_task == NULL) {
        return -1;
    }

    new_task->creator_heap = heap;

    task_registers_t* registers = memory_malloc_ext(heap, sizeof(task_registers_t), 0x10);

    if(registers == NULL) {
        memory_free_ext(heap, new_task);

        return -1;
    }


    frame_t* stack_frames;
    uint64_t stack_frames_cnt = (stack_size + FRAME_SIZE - 1) / FRAME_SIZE;
    stack_size = stack_frames_cnt * FRAME_SIZE;

    if(frame_get_allocator()->allocate_frame_by_count(frame_get_allocator(), stack_frames_cnt, FRAME_ALLOCATION_TYPE_USED | FRAME_ALLOCATION_TYPE_BLOCK, &stack_frames, NULL) != 0) {
        PRINTLOG(TASKING, LOG_ERROR, "cannot allocate stack with frame count 0x%llx", stack_frames_cnt);
        memory_free_ext(heap, new_task);
        memory_free_ext(heap, registers);

        return -1;
    }

    frame_t* heap_frames;
    uint64_t heap_frames_cnt = (heap_size + FRAME_SIZE - 1) / FRAME_SIZE;
    heap_size = heap_frames_cnt * FRAME_SIZE;

    if(frame_get_allocator()->allocate_frame_by_count(frame_get_allocator(), heap_frames_cnt, FRAME_ALLOCATION_TYPE_USED | FRAME_ALLOCATION_TYPE_BLOCK, &heap_frames, NULL) != 0) {
        PRINTLOG(TASKING, LOG_ERROR, "cannot allocate heap with frame count 0x%llx", heap_frames_cnt);

        if(frame_get_allocator()->release_frame(frame_get_allocator(), stack_frames) != 0) {
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

    if(heap_size > (16 << 20)) {
        task_heap = memory_create_heap_hash(heap_va, heap_va + heap_size);
    } else {
        task_heap = memory_create_heap_simple(heap_va, heap_va + heap_size);
    }


    lock_acquire(task_next_task_id_lock);
    uint64_t new_task_id = task_next_task_id++;
    lock_release(task_next_task_id_lock);

    task_heap->task_id = new_task_id;

    new_task->heap = task_heap;
    new_task->heap_size = heap_size;
    new_task->task_id = new_task_id;
    new_task->state = TASK_STATE_CREATED;
    new_task->entry_point = entry_point;
    new_task->page_table = memory_paging_get_table();
    new_task->registers = registers;
    new_task->stack_size = stack_size;
    new_task->stack = (void*)stack_va;
    new_task->task_name = strdup_at_heap(task_heap, task_name);

    new_task->arguments_count = args_cnt;
    new_task->arguments = args;

    registers->rflags = 0x002;

    uint64_t cr3_fa = (uint64_t)new_task->page_table->page_table;
    cr3_fa = MEMORY_PAGING_GET_FA_FOR_RESERVED_VA(cr3_fa);

    registers->cr3 = cr3_fa;

    *(uint16_t*)&registers->sse[0] = 0x37F;
    *(uint32_t*)&registers->sse[24] = 0x1F80 & task_mxcsr_mask;

    uint64_t rbp = (uint64_t)new_task->stack;
    rbp += stack_size - 16;
    registers->rbp = rbp;
    registers->rsp = rbp - 16; // 24 is for last return address, entry point and end task


    uint64_t* stack = (uint64_t*)rbp;
    stack[-1] = 0; // (uint64_t)task_end_task;
    stack[-2] = (uint64_t)task_end_task; // entry_point;
    // stack[-3] = (uint64_t)task_task_switch_exit;

    memory_heap_t* sheap = spool_get_heap();

    new_task->input_buffer = buffer_create_with_heap(sheap, 0x1000);
    new_task->output_buffer = buffer_create_with_heap(sheap, 0x1000);
    new_task->error_buffer = buffer_create_with_heap(sheap, 0x1000);

    spool_add(new_task->task_name, 3, new_task->input_buffer, new_task->output_buffer, new_task->error_buffer);

    PRINTLOG(TASKING, LOG_INFO, "scheduling new task %s 0x%llx 0x%p stack at 0x%llx-0x%llx heap at 0x%p[0x%llx]",
             new_task->task_name, new_task->task_id, new_task, registers->rsp, registers->rbp, new_task->heap, new_task->heap_size);

    uint64_t cpu_count = apic_get_ap_count() + 1;
    size_t min_queue_size = -1;
    list_t* min_queue = NULL;

    for(uint64_t i = 0; i < cpu_count; i++) {
        list_t* queue = task_queues[i];

        if(list_size(queue) < min_queue_size) {
            min_queue_size = list_size(queue);
            min_queue = queue;
            new_task->cpu_id = i;
        }
    }

    lock_acquire(task_find_next_task_lock);
    cpu_cli();
    map_insert(task_map, (void*)new_task->task_id, new_task);
    list_stack_push(min_queue, new_task);
    cpu_sti();
    lock_release(task_find_next_task_lock);


    PRINTLOG(TASKING, LOG_INFO, "task %s 0x%llx added to task queue on cpu 0x%llx", new_task->task_name, new_task->task_id, new_task->cpu_id);

    return new_task->task_id;
}

void task_idle_task(void) {
    cpu_cpuid_regs_t query = {0};
    query.eax = 0x1;
    cpu_cpuid_regs_t answer = {0};

    cpu_cpuid(query, &answer);

    PRINTLOG(TASKING, LOG_TRACE, "monitor/mwait query 0x%x", answer.ecx);

    boolean_t monitor_mwait_supported = (answer.ecx >> 3) & 0x1;

    query.eax = 0x5;
    query.ecx = 0x0;
    query.edx = 0x0;
    query.ebx = 0x0;
    cpu_cpuid(query, &answer);

    PRINTLOG(TASKING, LOG_TRACE, "monitor/mwait extended query 0x%x", answer.ecx);

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
int8_t task_create_idle_task(void) {
    program_header_t* kernel = (program_header_t*)SYSTEM_INFO->program_header_virtual_start;
    memory_heap_t* heap = memory_get_default_heap();

    task_t* new_task = memory_malloc_ext(heap, sizeof(task_t), 0x0);

    if(new_task == NULL) {
        return -1;
    }

    new_task->creator_heap = heap;
    new_task->heap = heap;
    new_task->heap_size = kernel->program_heap_size;

    task_registers_t* registers = memory_malloc_ext(heap, sizeof(task_registers_t), 0x10);

    if(registers == NULL) {
        memory_free_ext(heap, new_task);

        return -1;
    }

    uint64_t stack_size = 0x1000;


    frame_t* stack_frames;
    uint64_t stack_frames_cnt = (stack_size + FRAME_SIZE - 1) / FRAME_SIZE;
    stack_size = stack_frames_cnt * FRAME_SIZE;

    if(frame_get_allocator()->allocate_frame_by_count(frame_get_allocator(), stack_frames_cnt, FRAME_ALLOCATION_TYPE_USED | FRAME_ALLOCATION_TYPE_BLOCK, &stack_frames, NULL) != 0) {
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
    new_task->task_id = apic_get_local_apic_id() + 1;
    new_task->cpu_id = apic_get_local_apic_id();

    new_task->state = TASK_STATE_CREATED;
    new_task->entry_point = task_idle_task;
    new_task->page_table = memory_paging_get_table();
    new_task->registers = registers;
    new_task->stack_size = stack_size;
    new_task->stack = (void*)stack_va;

    char_t* tmp_task_name = sprintf("%s-%d", "idle", new_task->task_id);

    new_task->task_name = strdup_at_heap(heap, tmp_task_name);

    memory_free(tmp_task_name);

    registers->rflags = 0x002;

    uint64_t cr3_fa = (uint64_t)new_task->page_table->page_table;
    cr3_fa = MEMORY_PAGING_GET_FA_FOR_RESERVED_VA(cr3_fa);

    registers->cr3 = cr3_fa;

    *(uint16_t*)&registers->sse[0] = 0x37F;
    *(uint32_t*)&registers->sse[24] = 0x1F80 & task_mxcsr_mask;

    uint64_t rbp = (uint64_t)new_task->stack;
    rbp += stack_size - 16;
    registers->rbp = rbp;
    registers->rsp = rbp - 16;

    uint64_t* stack = (uint64_t*)rbp;
    stack[-1] = 0; // (uint64_t)task_end_task;
    stack[-2] = (uint64_t)task_end_task;
    // stack[-2] = (uint64_t)new_task->entry_point;
    // stack[-3] = (uint64_t)task_task_switch_exit;

    cpu_state->idle_task = new_task;

    memory_heap_t* sheap = spool_get_heap();

    new_task->output_buffer = buffer_create_with_heap(sheap, 0x1000);
    new_task->error_buffer = buffer_create_with_heap(sheap, 0x1000);

    spool_add(new_task->task_name, 2, new_task->output_buffer, new_task->error_buffer);

    lock_acquire(task_find_next_task_lock);
    map_insert(task_map, (void*)new_task->task_id, new_task);
    lock_release(task_find_next_task_lock);

    return 0;
}
#pragma GCC diagnostic pop

void task_yield(void) {
    cpu_cli();
    task_task_switch_set_parameters(false, true);
    task_switch_task();

    // asm volatile ("int $0xfe\n");
}

int8_t task_task_switch_isr(interrupt_frame_ext_t* frame) {
    UNUSED(frame);

    task_task_switch_set_parameters(true, false);
    task_switch_task();

    return 0;
}

void task_remove_task_after_fault(uint64_t task_id) {
    task_t* task = (task_t*)map_get(task_map, (void*)task_id);

    task->state = TASK_STATE_ENDED;

    if(task->vmcs_physical_address) {
        if(vmclear(task->vmcs_physical_address) != 0) {
            PRINTLOG(TASKING, LOG_ERROR, "vmclear failed for task 0x%llx", task->task_id);
        }
    }

    task->message_waiting = false;
    task->interruptible = false;
    task->wait_for_future = false;

    task_t* current_task = (task_t*)cpu_state->idle_task;
    current_task->last_tick_count = time_timer_get_tick_count();
    current_task->task_switch_count++;

    cpu_state->current_task = current_task;
    current_task->state = TASK_STATE_RUNNING;

    task_load_registers(current_task->registers);

    cpu_sti();
}

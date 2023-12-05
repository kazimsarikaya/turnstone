/**
 * @file task.64.c
 * @brief cpu task methods
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#include <apic.h>
#include <cpu.h>
#include <cpu/interrupt.h>
#include <cpu/task.h>
#include <memory/paging.h>
#include <memory/frame.h>
#include <linkedlist.h>
#include <time/timer.h>
#include <logging.h>
#include <systeminfo.h>
#include <linker.h>
#include <utils.h>
#include <map.h>
#include <stdbufs.h>

MODULE("turnstone.kernel.cpu.task");

uint64_t task_id = 0;

task_t* current_task = NULL;

linkedlist_t* task_queue = NULL;
linkedlist_t* task_cleaner_queue = NULL;
map_t task_map = NULL;
uint32_t task_mxcsr_mask = 0;

extern int8_t kmain64(void);

int8_t                                          task_task_switch_isr(interrupt_frame_ext_t* frame);
__attribute__((naked, no_stack_protector)) void task_save_registers(task_t* task);
__attribute__((naked, no_stack_protector)) void task_load_registers(task_t* task);
void                                            task_cleanup(void);
task_t*                                         task_find_next_task(void);
void                                            task_end_task(void);

extern buffer_t* stdbufs_default_input_buffer;
extern buffer_t* stdbufs_default_output_buffer;
extern buffer_t* stdbufs_default_error_buffer;

task_t* task_get_current_task(void){
    return current_task;
}

int8_t task_init_tasking_ext(memory_heap_t* heap) {
    PRINTLOG(TASKING, LOG_INFO, "tasking system initialization started");

    uint64_t rsp;

    __asm__ __volatile__ ("mov %%rsp, %0\n" : : "m" (rsp));

    descriptor_gdt_t* gdts = (descriptor_gdt_t*)GDT_REGISTER->base;

    descriptor_tss_t* d_tss = (descriptor_tss_t*)&gdts[3];

    size_t tmp_selector = (size_t)d_tss - (size_t)gdts;
    uint16_t tss_selector = (uint16_t)tmp_selector;

    PRINTLOG(TASKING, LOG_TRACE, "tss selector 0x%x",  tss_selector);



    program_header_t* kernel = (program_header_t*)SYSTEM_INFO->program_header_virtual_start;
    uint64_t stack_size = kernel->program_stack_size;
    uint64_t stack_top = kernel->program_stack_virtual_address;


    uint64_t stack_bottom = stack_top - 9 * stack_size;

    uint64_t frame_count = 9 * stack_size / FRAME_SIZE;

    frame_t* stack_frames = NULL;

    if(KERNEL_FRAME_ALLOCATOR->allocate_frame_by_count(KERNEL_FRAME_ALLOCATOR, frame_count, FRAME_ALLOCATION_TYPE_RESERVED | FRAME_ALLOCATION_TYPE_BLOCK, &stack_frames, NULL) != 0) {
        PRINTLOG(TASKING, LOG_FATAL, "cannot allocate stack frames of count 0x%llx", frame_count);

        return -1;
    }

    tss_t* tss = memory_malloc_ext(heap, sizeof(tss_t), 0x1000);

    if(tss == NULL) {
        PRINTLOG(TASKING, LOG_FATAL, "cannot allocate memory for tss");

        return -1;
    }

    memory_paging_add_va_for_frame(stack_bottom, stack_frames, MEMORY_PAGING_PAGE_TYPE_NOEXEC);

    PRINTLOG(TASKING, LOG_TRACE, "for tasking frames 0x%llx with count 0x%llx mapped to 0x%llx",  stack_frames->frame_address, stack_frames->frame_count, stack_bottom);

    tss->ist7 = stack_bottom + stack_size;
    tss->ist6 = tss->ist7  + stack_size;
    tss->ist5 = tss->ist6  + stack_size;
    tss->ist4 = tss->ist5  + stack_size;
    tss->ist3 = tss->ist4  + stack_size;
    tss->ist2 = tss->ist3  + stack_size;
    tss->ist1 = tss->ist2  + stack_size;
    tss->rsp2 = tss->ist1  + stack_size;
    tss->rsp1 = tss->rsp2  + stack_size;
    tss->rsp0 = rsp;

    task_queue = linkedlist_create_queue_with_heap(heap);
    task_cleaner_queue = linkedlist_create_queue_with_heap(heap);

    interrupt_irq_set_handler(0x60, &task_task_switch_isr);

    current_task = memory_malloc_ext(heap, sizeof(task_t), 0x0);
    current_task->creator_heap = heap;
    current_task->heap = heap;
    current_task->task_id = TASK_KERNEL_TASK_ID;
    current_task->state = TASK_STATE_CREATED;
    current_task->entry_point = kmain64;
    current_task->page_table = memory_paging_get_table();
    current_task->fx_registers = memory_malloc_ext(heap, sizeof(uint8_t) * 512, 0x10);
    current_task->stack = (void*)(stack_top + stack_size);
    current_task->stack_size = stack_size;
    current_task->task_name = "kernel";
    current_task->input_buffer = stdbufs_default_input_buffer;
    current_task->output_buffer = stdbufs_default_output_buffer;
    current_task->error_buffer = stdbufs_default_error_buffer;

    // get mxcsr by fxsave
    asm volatile ("mov %0, %%rax\nfxsave (%%rax)\n" : "=m" (current_task->fx_registers) : : "rax", "memory");

    task_mxcsr_mask = *(uint32_t*)&current_task->fx_registers[28];

    PRINTLOG(TASKING, LOG_INFO, "mxcsr mask 0x%x", task_mxcsr_mask);

    task_id = current_task->task_id + 1;

    task_map = map_integer();

    if(task_map == NULL) {
        PRINTLOG(TASKING, LOG_FATAL, "cannot allocate task map");

        return -1;
    }

    map_insert(task_map, (void*)current_task->task_id, current_task);

    uint32_t tss_limit = sizeof(tss_t) - 1;
    DESCRIPTOR_BUILD_TSS_SEG(d_tss, (size_t)tss, tss_limit, DPL_KERNEL);

    PRINTLOG(TASKING, LOG_TRACE, "task register loading with tss 0x%p limit 0x%x", tss, tss_limit);

    __asm__ __volatile__ (
        "cli\n"
        "ltr %0\n"
        "sti\n"
        : : "r" (tss_selector)
        );

    interrupt_redirect_main_interrupts(7);

    PRINTLOG(TASKING, LOG_INFO, "tasking system initialization ended, kernel task address 0x%p", current_task);

    return 0;
}

__attribute__((naked, no_stack_protector)) void task_save_registers(task_t* task) {
    __asm__ __volatile__ (
        "mov %%rax, %0\n"
        "mov %%rbx, %1\n"
        "mov %%rcx, %2\n"
        "mov %%rdx, %3\n"
        "mov %%r8,  %4\n"
        "mov %%r9,  %5\n"
        "mov %%r10, %6\n"
        "mov %%r11, %7\n"
        "mov %%r12, %8\n"
        "mov %%r13, %9\n"
        "mov %%r14, %10\n"
        "mov %%r15, %11\n"
        "mov %%rdi, %12\n"
        "mov %%rsi, %13\n"
        "mov %%rbp, %14\n"
        "push %%rbx\n"
        "mov %15, %%rbx\n"
        "fxsave (%%rbx)\n"
        "pop %%rbx\n"
        "push %%rax\n"
        "pushfq\n"
        "mov (%%rsp), %%rax\n"
        "mov %%rax, %16\n"
        "popfq\n"
        "pop %%rax\n"
        "mov %%rsp, %17\n"
        "movq %19, %18\n"
        "retq\n"
        : :
        "m" (task->rax),
        "m" (task->rbx),
        "m" (task->rcx),
        "m" (task->rdx),
        "m" (task->r8),
        "m" (task->r9),
        "m" (task->r10),
        "m" (task->r11),
        "m" (task->r12),
        "m" (task->r13),
        "m" (task->r14),
        "m" (task->r15),
        "m" (task->rdi),
        "m" (task->rsi),
        "m" (task->rbp),
        "m" (task->fx_registers),
        "m" (task->rflags),
        "m" (task->rsp),
        "m" (task->state),
        "I" (TASK_STATE_SUSPENDED)
        );
}

__attribute__((naked, no_stack_protector)) void task_load_registers(task_t* task) {
    __asm__ __volatile__ (
        "mov %0,  %%rax\n"
        "mov %1,  %%rbx\n"
        "mov %2,  %%rcx\n"
        "mov %3,  %%rdx\n"
        "mov %4,  %%r8\n"
        "mov %5,  %%r9\n"
        "mov %6,  %%r10\n"
        "mov %7,  %%r11\n"
        "mov %8,  %%r12\n"
        "mov %9,  %%r13\n"
        "mov %10, %%r14\n"
        "mov %11, %%r15\n"
        "mov %13, %%rsi\n"
        "mov %14, %%rbp\n"
        "push %%rbx\n"
        "mov %15, %%rbx\n"
        "fxrstor (%%rbx)\n"
        "pop %%rbx\n"
        "push %%rax\n"
        "mov %16, %%rax\n"
        "push %%rax\n"
        "popfq\n"
        "pop %%rax\n"
        "mov %17, %%rsp\n"
        "push %%rax\n"
        "mov %18, %%ax\n"
        "cmp %19, %%ax\n"
        "pop %%rax\n"
        "movq %20, %18\n"
        "mov %12, %%rdi\n"
        "retq\n"
        : :
        "m" (task->rax),
        "m" (task->rbx),
        "m" (task->rcx),
        "m" (task->rdx),
        "m" (task->r8),
        "m" (task->r9),
        "m" (task->r10),
        "m" (task->r11),
        "m" (task->r12),
        "m" (task->r13),
        "m" (task->r14),
        "m" (task->r15),
        "m" (task->rdi),
        "m" (task->rsi),
        "m" (task->rbp),
        "m" (task->fx_registers),
        "m" (task->rflags),
        "m" (task->rsp),
        "m" (task->state),
        "I" (TASK_STATE_CREATED),
        "I" (TASK_STATE_RUNNING)
        );
}

void task_cleanup(void){
    while(linkedlist_size(task_cleaner_queue)) {
        task_t* tmp = (task_t*)linkedlist_queue_pop(task_cleaner_queue);

        map_delete(task_map, (void*)tmp->task_id);

        uint64_t stack_va = (uint64_t)tmp->stack;
        uint64_t stack_fa = MEMORY_PAGING_GET_FA_FOR_RESERVED_VA(stack_va);

        uint64_t stack_size = tmp->stack_size;
        uint64_t stack_frames_cnt = stack_size / FRAME_SIZE;

        memory_memclean(tmp->stack, stack_size);

        frame_t stack_frames = {stack_fa, stack_frames_cnt, FRAME_TYPE_USED, FRAME_ALLOCATION_TYPE_USED | FRAME_ALLOCATION_TYPE_BLOCK};

        if(memory_paging_delete_va_for_frame_ext(tmp->page_table, stack_va, &stack_frames) != 0 ) {
            PRINTLOG(TASKING, LOG_ERROR, "cannot remove pages for stack at va 0x%llx", stack_va);

            cpu_hlt();
        }

        if(KERNEL_FRAME_ALLOCATOR->release_frame(KERNEL_FRAME_ALLOCATOR, &stack_frames) != 0) {
            PRINTLOG(TASKING, LOG_ERROR, "cannot release stack with frames at 0x%llx with count 0x%llx", stack_fa, stack_frames_cnt);

            cpu_hlt();
        }

        uint64_t heap_va = (uint64_t)tmp->stack;
        uint64_t heap_fa = MEMORY_PAGING_GET_FA_FOR_RESERVED_VA(heap_va);

        uint64_t heap_size = tmp->heap_size;
        uint64_t heap_frames_cnt = heap_size / FRAME_SIZE;

        memory_memclean(tmp->heap, heap_size);

        frame_t heap_frames = {heap_fa, heap_frames_cnt, FRAME_TYPE_USED, FRAME_ALLOCATION_TYPE_USED | FRAME_ALLOCATION_TYPE_BLOCK};

        if(memory_paging_delete_va_for_frame_ext(tmp->page_table, heap_va, &heap_frames) != 0 ) {
            PRINTLOG(TASKING, LOG_ERROR, "cannot remove pages for heap at va 0x%llx", heap_va);

            cpu_hlt();
        }

        if(KERNEL_FRAME_ALLOCATOR->release_frame(KERNEL_FRAME_ALLOCATOR, &heap_frames) != 0) {
            PRINTLOG(TASKING, LOG_ERROR, "cannot release heap with frames at 0x%llx with count 0x%llx", stack_fa, heap_frames_cnt);

            cpu_hlt();
        }

        memory_free_ext(tmp->creator_heap, tmp->fx_registers);
        memory_free_ext(tmp->creator_heap, tmp);
    }
}

boolean_t task_idle_check_need_yield(void) {
    cpu_cli();

    boolean_t need_yield = false;

    for(uint64_t i = 0; i < linkedlist_size(task_queue); i++) {
        task_t* t = (task_t*)linkedlist_get_data_at_position(task_queue, i);

        if(t) {
            if(t->message_waiting && t->message_queues) {
                for(uint64_t q_idx = 0; q_idx < linkedlist_size(t->message_queues); q_idx++) {
                    linkedlist_t* q = (linkedlist_t*)linkedlist_get_data_at_position(t->message_queues, q_idx);

                    if(linkedlist_size(q)) {
                        t->message_waiting = false;
                        need_yield = true;
                    }
                }
            } else if(t->sleeping) {
                if(t->wake_tick < time_timer_get_tick_count()) {
                    t->sleeping = false;
                    need_yield = true;
                }
            }
        }
    }

    cpu_sti();

    return need_yield;
}

task_t* task_find_next_task(void) {
    task_t* tmp_task = NULL;

    while(1) {
        tmp_task = (task_t*)linkedlist_queue_pop(task_queue);

        if(tmp_task->sleeping) {

            if(tmp_task->wake_tick < time_timer_get_tick_count()) {
                tmp_task->sleeping = false;
                break;
            }

            linkedlist_queue_push(task_queue, tmp_task);
        } else if(tmp_task->message_waiting) {


            if(tmp_task->interruptible) {
                if(tmp_task->interrupt_received) {
                    tmp_task->interrupt_received = false;
                    tmp_task->message_waiting = false;

                    break;
                }
            } else if(tmp_task->message_queues) {

                for(uint64_t q_idx = 0; q_idx < linkedlist_size(tmp_task->message_queues); q_idx++) {
                    const linkedlist_t* q = (linkedlist_t*)linkedlist_get_data_at_position(tmp_task->message_queues, q_idx);

                    if(q) {
                        if(linkedlist_size(q)) {
                            tmp_task->message_waiting = false;
                            break;
                        }

                    } else {
                        PRINTLOG(TASKING, LOG_ERROR, "task 0x%lli has null queue in message queues, removing task", tmp_task->task_id);
                        linkedlist_queue_push(task_cleaner_queue, tmp_task);

                        continue;
                    }

                }

            } else {
                PRINTLOG(TASKING, LOG_ERROR, "task 0x%lli in message waiting state without message queues, removing task", tmp_task->task_id);
                linkedlist_queue_push(task_cleaner_queue, tmp_task);

                continue;
            }

            linkedlist_queue_push(task_queue, tmp_task);
        } else {
            break;
        }

    }

    // PRINTLOG(TASKING, LOG_WARNING, "task 0x%llx selected for execution", tmp_task->task_id);

    return tmp_task;
}

boolean_t task_switch_paramters_need_eoi = false;
boolean_t task_switch_paramters_need_sti = false;

void task_task_switch_set_parameters(boolean_t need_eoi, boolean_t need_sti) {
    task_switch_paramters_need_eoi = need_eoi;
    task_switch_paramters_need_sti = need_sti;
}


static inline void task_switch_task_exit_prep(void) {
    if(task_switch_paramters_need_eoi) {
        apic_eoi();
    }

    if(task_switch_paramters_need_sti) {
        cpu_sti();
    }
}

__attribute__((no_stack_protector)) void task_switch_task(void) {
    if(current_task == NULL || task_queue == NULL || linkedlist_size(task_queue) == 0) {
        task_switch_task_exit_prep();

        return;
    }

    if((time_timer_get_tick_count() - current_task->last_tick_count) < TASK_MAX_TICK_COUNT && time_timer_get_tick_count() > current_task->last_tick_count && !current_task->message_waiting && !current_task->sleeping) {

        task_switch_task_exit_prep();

        return;
    }

    task_save_registers(current_task);
    linkedlist_queue_push(task_queue, current_task);

    if(current_task->task_id == TASK_KERNEL_TASK_ID) {
        task_cleanup();
    }

    current_task = task_find_next_task();
    current_task->last_tick_count = time_timer_get_tick_count();
    task_load_registers(current_task);

    task_switch_task_exit_prep();
}

void task_end_task(void) {
    cpu_cli();
    PRINTLOG(TASKING, LOG_TRACE, "ending task 0x%lli", current_task->task_id);

    linkedlist_queue_push(task_cleaner_queue, current_task);

    PRINTLOG(TASKING, LOG_TRACE, "task 0x%lli added to cleaning queue", current_task->task_id);

    current_task = NULL;

    cpu_sti();

    __asm__ __volatile__ ("int $0x80\n");
}


void task_add_message_queue(linkedlist_t* queue){
    cpu_cli();
    if(current_task->message_queues == NULL) {
        current_task->message_queues = linkedlist_create_list();
    }

    linkedlist_list_insert(current_task->message_queues, queue);

    cpu_sti();
}

void task_set_message_waiting(void){
    cpu_cli();

    current_task->message_waiting = 1;

    cpu_sti();
}

uint64_t task_create_task(memory_heap_t* heap, uint64_t heap_size, uint64_t stack_size, void* entry_point, uint64_t args_cnt, void** args, const char_t* task_name) {

    task_t* new_task = memory_malloc_ext(heap, sizeof(task_t), 0x0);

    if(new_task == NULL) {
        return -1;
    }

    new_task->creator_heap = heap;

    uint8_t* fx_registers = memory_malloc_ext(heap, sizeof(uint8_t) * 512, 0x10);

    if(fx_registers == NULL) {
        memory_free_ext(heap, new_task);

        return -1;
    }


    frame_t* stack_frames;
    uint64_t stack_frames_cnt = (stack_size + FRAME_SIZE - 1) / FRAME_SIZE;
    stack_size = stack_frames_cnt * FRAME_SIZE;

    if(KERNEL_FRAME_ALLOCATOR->allocate_frame_by_count(KERNEL_FRAME_ALLOCATOR, stack_frames_cnt, FRAME_ALLOCATION_TYPE_USED | FRAME_ALLOCATION_TYPE_BLOCK, &stack_frames, NULL) != 0) {
        PRINTLOG(TASKING, LOG_ERROR, "cannot allocate stack with frame count 0x%llx", stack_frames_cnt);
        memory_free_ext(heap, new_task);
        memory_free_ext(heap, fx_registers);

        return -1;
    }

    frame_t* heap_frames;
    uint64_t heap_frames_cnt = (heap_size + FRAME_SIZE - 1) / FRAME_SIZE;
    heap_size = heap_frames_cnt * FRAME_SIZE;

    if(KERNEL_FRAME_ALLOCATOR->allocate_frame_by_count(KERNEL_FRAME_ALLOCATOR, heap_frames_cnt, FRAME_ALLOCATION_TYPE_USED | FRAME_ALLOCATION_TYPE_BLOCK, &heap_frames, NULL) != 0) {
        PRINTLOG(TASKING, LOG_ERROR, "cannot allocate heap with frame count 0x%llx", heap_frames_cnt);

        if(KERNEL_FRAME_ALLOCATOR->release_frame(KERNEL_FRAME_ALLOCATOR, stack_frames) != 0) {
            PRINTLOG(TASKING, LOG_ERROR, "cannot release stack with frames at 0x%llx with count 0x%llx", stack_frames->frame_address, stack_frames->frame_count);

            cpu_hlt();
        }

        memory_free_ext(heap, new_task);
        memory_free_ext(heap, fx_registers);

        return -1;
    }

    uint64_t stack_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(stack_frames->frame_address);

    if(memory_paging_add_va_for_frame(stack_va, stack_frames, MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
        PRINTLOG(TASKING, LOG_ERROR, "cannot add stack va 0x%llx for frame at 0x%llx with count 0x%llx", stack_va, stack_frames->frame_address, stack_frames->frame_count);

        cpu_hlt();
    }

    uint64_t heap_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(heap_frames->frame_address);

    if(memory_paging_add_va_for_frame(heap_va, heap_frames, MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
        PRINTLOG(TASKING, LOG_ERROR, "cannot add heap va 0x%llx for frame at 0x%llx with count 0x%llx", heap_va, heap_frames->frame_address, heap_frames->frame_count);

        cpu_hlt();
    }

    memory_heap_t* task_heap = memory_create_heap_simple(heap_va, heap_va + heap_size);

    cpu_cli();

    uint64_t new_task_id = task_id++;

    cpu_sti();

    task_heap->task_id = new_task_id;

    new_task->heap = task_heap;
    new_task->heap_size = heap_size;
    new_task->task_id = new_task_id;
    new_task->state = TASK_STATE_CREATED;
    new_task->entry_point = entry_point;
    new_task->page_table = memory_paging_get_table();
    new_task->fx_registers = fx_registers;
    new_task->rflags = 0x202;
    new_task->stack_size = stack_size;
    new_task->stack = (void*)stack_va;
    new_task->task_name = task_name;

    *(uint16_t*)&new_task->fx_registers[0] = 0x37F;
    *(uint32_t*)&new_task->fx_registers[24] = 0x1F80 & task_mxcsr_mask;

    uint64_t rbp = (uint64_t)new_task->stack;
    rbp += stack_size - 16;
    new_task->rbp = rbp;
    new_task->rsp = rbp - 24;

    new_task->rdi = args_cnt;
    new_task->rsi = (uint64_t)args;

    uint64_t* stack = (uint64_t*)rbp;
    stack[-1] = (uint64_t)task_end_task;
    stack[-2] = (uint64_t)entry_point;
    stack[-3] = (uint64_t)apic_eoi;


    new_task->input_buffer = buffer_create_with_heap(task_heap, 0x1000);
    new_task->output_buffer = buffer_create_with_heap(task_heap, 0x1000);
    new_task->error_buffer = buffer_create_with_heap(task_heap, 0x1000);

    PRINTLOG(TASKING, LOG_INFO, "scheduling new task %s 0x%llx 0x%p stack at 0x%llx-0x%llx heap at 0x%p[0x%llx]",
             new_task->task_name, new_task->task_id, new_task, new_task->rsp, new_task->rbp, new_task->heap, new_task->heap_size);

    cpu_cli();

    linkedlist_stack_push(task_queue, new_task);
    map_insert(task_map, (void*)new_task->task_id, new_task);

    cpu_sti();

    return new_task->task_id;
}

void task_yield(void) {
    if(linkedlist_size(task_queue)) { // prevent unneccessary interrupt
        // __asm__ __volatile__ ("int $0x80\n");
        cpu_cli();
        task_task_switch_set_parameters(false, true);
        task_switch_task();
    }
}

int8_t task_task_switch_isr(interrupt_frame_ext_t* frame) {
    UNUSED(frame);

    task_task_switch_set_parameters(true, false);
    task_switch_task();

    return 0;
}


uint64_t task_get_id(void) {
    uint64_t id = TASK_KERNEL_TASK_ID;

    cpu_cli();

    if(current_task) {
        id = current_task->task_id;
    }

    cpu_sti();

    return id;
}

void task_current_task_sleep(uint64_t wake_tick) {
    cpu_cli();

    if(current_task) {
        current_task->wake_tick = wake_tick;
        current_task->sleeping = true;
        task_yield();
    } else {
        cpu_sti();
    }
}

void task_clear_message_waiting(uint64_t tid) {
    task_t* task = (task_t*)map_get(task_map, (void*)tid);

    if(task) {
        task->message_waiting = false;
    } else {
        PRINTLOG(TASKING, LOG_ERROR, "task not found 0x%llx", tid);
    }
}

void task_set_interrupt_received(uint64_t tid) {
    task_t* task = (task_t*)map_get(task_map, (void*)tid);

    if(task) {
        task->interrupt_received = true;
    } else {
        PRINTLOG(TASKING, LOG_ERROR, "task not found 0x%llx", tid);
    }
}

void task_set_interruptible(void) {
    cpu_cli();

    if(current_task) {
        current_task->interruptible = true;
    }

    cpu_sti();
}

void task_print_all(void) {
    iterator_t* it = map_create_iterator(task_map);

    while(it->end_of_iterator(it) != 0) {
        const task_t* task = it->get_item(it);

        printf("\ttask %s 0x%llx 0x%p stack at 0x%llx-0x%llx heap at 0x%p[0x%llx]\n",
               task->task_name, task->task_id, task, task->rsp, task->rbp, task->heap, task->heap_size);

        it = it->next(it);
    }

    it->destroy(it);
}

buffer_t* task_get_input_buffer(void) {
    buffer_t* buffer = NULL;

    cpu_cli();

    if(current_task) {
        buffer = current_task->input_buffer;
    }

    cpu_sti();

    return buffer;
}

buffer_t* task_get_output_buffer(void) {
    buffer_t* buffer = NULL;

    cpu_cli();

    if(current_task) {
        buffer = current_task->output_buffer;
    }

    cpu_sti();

    return buffer;
}

buffer_t* task_get_error_buffer(void) {
    buffer_t* buffer = NULL;

    cpu_cli();

    if(current_task) {
        buffer = current_task->error_buffer;
    }

    cpu_sti();

    return buffer;
}

int8_t task_set_input_buffer(buffer_t* buffer) {
    if(!buffer) {
        return -1;
    }

    cpu_cli();

    if(!current_task) {
        cpu_sti();

        return -1;
    }

    current_task->input_buffer = buffer;
    cpu_sti();

    return 0;
}

int8_t task_set_output_buffer(buffer_t * buffer) {
    if(!buffer) {
        return -1;
    }

    cpu_cli();

    if(!current_task) {
        cpu_sti();

        return -1;
    }

    current_task->output_buffer = buffer;
    cpu_sti();

    return 0;
}

int8_t task_set_error_buffer(buffer_t * buffer) {
    if(!buffer) {
        return -1;
    }

    cpu_cli();

    if(!current_task) {
        cpu_sti();

        return -1;
    }

    current_task->error_buffer = buffer;
    cpu_sti();

    return 0;
}

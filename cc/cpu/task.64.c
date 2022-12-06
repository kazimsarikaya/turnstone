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
#include <video.h>
#include <systeminfo.h>
#include <linker.h>
#include <utils.h>

uint64_t task_id = 0;

task_t* current_task = NULL;

linkedlist_t task_queue = NULL;
linkedlist_t task_cleaner_queue = NULL;

extern int8_t kmain64();

int8_t task_task_switch_isr(interrupt_frame_t* frame, uint8_t intnum);

extern uint64_t __stack_top;

task_t* task_get_current_task(){
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



	program_header_t* kernel = (program_header_t*)SYSTEM_INFO->kernel_start;
	uint64_t stack_size = kernel->section_locations[LINKER_SECTION_TYPE_STACK].section_size;
	uint64_t stack_top = kernel->section_locations[LINKER_SECTION_TYPE_STACK].section_start;


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
	current_task->heap = heap;
	current_task->task_id = TASK_KERNEL_TASK_ID;
	current_task->state = TASK_STATE_CREATED;
	current_task->entry_point = kmain64;
	current_task->page_table = memory_paging_get_table();
	current_task->fx_registers = memory_malloc_ext(heap, sizeof(uint8_t) * 512, 0x10);

	task_id = current_task->task_id + 1;

	uint32_t tss_limit = sizeof(tss_t) - 1;
	DESCRIPTOR_BUILD_TSS_SEG(d_tss, (size_t)tss, tss_limit, DPL_KERNEL);

	PRINTLOG(TASKING, LOG_TRACE, "task register loading with tss 0x%p limit 0x%x", tss, tss_limit);

	__asm__ __volatile__ (
		"cli\n"
		"ltr %0\n"
		"sti\n"
		: : "r" (tss_selector)
		);

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
		"mov %19, %18\n"
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
		"mov %20, %18\n"
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

void task_cleanup(){
	while(linkedlist_size(task_cleaner_queue)) {
		task_t* tmp = (task_t*)linkedlist_queue_pop(task_cleaner_queue);
		memory_free_ext(tmp->heap, tmp->stack);
		memory_free_ext(tmp->heap, tmp->fx_registers);
		memory_free_ext(tmp->heap, tmp);
	}
}

task_t* task_find_next_task() {
	task_t* tmp_task = NULL;

	while(1) {
		tmp_task = linkedlist_queue_pop(task_queue);

		if(tmp_task->message_waiting) {
			if(tmp_task->message_queues) {

				for(uint64_t q_idx = 0; q_idx < linkedlist_size(tmp_task->message_queues); q_idx++) {
					linkedlist_t q = linkedlist_get_data_at_position(tmp_task->message_queues, q_idx);

					if(q) {
						if(linkedlist_size(q)) {
							tmp_task->message_waiting = 0;
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

	return tmp_task;
}

__attribute__((no_stack_protector)) void task_switch_task(boolean_t need_eoi) {
	if(task_queue == NULL) {
		return;
	}

	if(linkedlist_size(task_queue) == 0) {

		if(need_eoi) {
			apic_eoi();
		}

		return;
	}

	if(current_task != NULL) {
		if((time_timer_get_tick_count() - current_task->last_tick_count) < TASK_MAX_TICK_COUNT && time_timer_get_tick_count() > current_task->last_tick_count && !current_task->message_waiting) {

			if(need_eoi) {
				apic_eoi();
			}

			return;
		}

		task_save_registers(current_task);
		linkedlist_queue_push(task_queue, current_task);

		if(current_task->task_id == TASK_KERNEL_TASK_ID) {
			task_cleanup();
		}
	}

	current_task = task_find_next_task();
	current_task->last_tick_count = time_timer_get_tick_count();
	task_load_registers(current_task);

	if(need_eoi) {
		apic_eoi();
	}
}

void task_end_task() {
	cpu_cli();
	PRINTLOG(TASKING, LOG_TRACE, "ending task 0x%lli", current_task->task_id);

	linkedlist_queue_push(task_cleaner_queue, current_task);

	PRINTLOG(TASKING, LOG_TRACE, "task 0x%lli added to cleaning queue", current_task->task_id);

	current_task = NULL;

	cpu_sti();

	__asm__ __volatile__ ("int $0x80\n");
}


void task_add_message_queue(linkedlist_t queue){
	if(current_task->message_queues == NULL) {
		current_task->message_queues = linkedlist_create_list();
	}

	linkedlist_list_insert(current_task->message_queues, queue);
}

void task_set_message_waiting(){
	current_task->message_waiting = 1;
}

int8_t task_create_task(memory_heap_t* heap, uint64_t stack_size, void* entry_point) {

	frame_t* stack_frames;
	uint64_t stack_frames_cnt = (stack_size + FRAME_SIZE - 1) / FRAME_SIZE;
	stack_size = stack_frames_cnt * FRAME_SIZE;

	if(KERNEL_FRAME_ALLOCATOR->allocate_frame_by_count(KERNEL_FRAME_ALLOCATOR, stack_frames_cnt, FRAME_ALLOCATION_TYPE_USED | FRAME_ALLOCATION_TYPE_BLOCK, &stack_frames, NULL) != 0) {
		PRINTLOG(TASKING, LOG_TRACE, "cannot allocate stack with frame count 0x%llx", stack_frames_cnt);

		return -1;
	}

	uint64_t stack_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(stack_frames->frame_address);

	memory_paging_add_va_for_frame(stack_va, stack_frames, MEMORY_PAGING_PAGE_TYPE_NOEXEC);

	task_t* new_task = memory_malloc_ext(heap, sizeof(task_t), 0x0);
	new_task->heap = heap;
	new_task->task_id = task_id++;
	new_task->state = TASK_STATE_CREATED;
	new_task->entry_point = entry_point;
	new_task->page_table = memory_paging_get_table();
	new_task->fx_registers = memory_malloc_ext(heap, sizeof(uint8_t) * 512, 0x10);
	new_task->rflags = 0x202;
	new_task->stack_size = stack_size;
	new_task->stack = (void*)stack_va;

	uint64_t rbp = (uint64_t)new_task->stack;
	rbp += stack_size - 16;
	new_task->rbp = rbp;
	new_task->rsp = rbp - 24;

	uint64_t* stack = (uint64_t*)rbp;
	stack[-1] = (uint64_t)task_end_task;
	stack[-2] = (uint64_t)entry_point;
	stack[-3] = (uint64_t)apic_eoi;

	cpu_cli();

	PRINTLOG(TASKING, LOG_INFO, "scheduling new task 0x%llx 0x%p stack at 0x%llx-0x%llx", new_task->task_id, new_task, new_task->rsp, new_task->rbp);

	linkedlist_stack_push(task_queue, new_task);

	cpu_sti();

	return 0;
}

void task_yield() {
	if(linkedlist_size(task_queue)) { // prevent unneccessary interrupt
		//	__asm__ __volatile__ ("int $0x80\n");
		cpu_cli();
		task_switch_task(false);
		cpu_sti();
	}
}

int8_t task_task_switch_isr(interrupt_frame_t* frame, uint8_t intnum) {
	UNUSED(frame);
	UNUSED(intnum);

	task_switch_task(true);

	return 0;
}


uint64_t task_get_id() {
	if(current_task) {
		return current_task->task_id;
	} else {
		return TASK_KERNEL_TASK_ID;
	}

}

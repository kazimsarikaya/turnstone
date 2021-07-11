/**
 * @file task.64.c
 * @brief cpu task methods
 */
#include <apic.h>
#include <cpu.h>
#include <cpu/interrupt.h>
#include <cpu/task.h>
#include <memory/paging.h>
#include <linkedlist.h>
#include <time/timer.h>
#include <video.h>

uint64_t task_id = 1;

task_t* current_task = NULL;

linkedlist_t task_queue = NULL;
linkedlist_t task_cleaner_queue = NULL;

extern int8_t kmain64();

extern uint64_t __stack_top;

task_t* task_get_current_task(){
	return current_task;
}

void task_task_switch_isr(interrupt_frame_t* frame, uint16_t intnum);

int8_t task_init_tasking_ext(memory_heap_t* heap) {
	uint64_t rsp;

	__asm__ __volatile__ ("mov %%rsp, %0\n" : : "m" (rsp));

	descriptor_gdt_t* gdts = (descriptor_gdt_t*)GDT_REGISTER.base;

	descriptor_tss_t* d_tss = (descriptor_tss_t*)&gdts[3];

	size_t tmp_selector = (size_t)d_tss - (size_t)gdts;
	uint16_t tss_selector = (uint16_t)tmp_selector;

	printf("TASK: Info selector 0x%x\n",  tss_selector);

	tss_t* tss = memory_malloc_ext(heap, sizeof(tss_t), 0x1000);

	if(tss == NULL) {
		return -1;
	}

	memory_paging_add_page(TASK_TSS_STACK_START, TASK_TSS_STACK_START, MEMORY_PAGING_PAGE_TYPE_2M);

	uint64_t kernel_rsp = TASK_TSS_STACK_START + TASK_TSS_STACK_STEP;

	tss->rsp0 = kernel_rsp + TASK_TSS_STACK_STEP;
	tss->rsp1 = tss->rsp0  + TASK_TSS_STACK_STEP;
	tss->rsp2 = tss->rsp1  + TASK_TSS_STACK_STEP;
	tss->ist1 = tss->rsp2  + TASK_TSS_STACK_STEP;
	tss->ist2 = tss->ist1  + TASK_TSS_STACK_STEP;
	tss->ist3 = tss->ist2  + TASK_TSS_STACK_STEP;
	tss->ist4 = tss->ist3  + TASK_TSS_STACK_STEP;
	tss->ist5 = tss->ist4  + TASK_TSS_STACK_STEP;
	tss->ist6 = tss->ist5  + TASK_TSS_STACK_STEP;
	tss->ist7 = tss->ist6  + TASK_TSS_STACK_STEP;

	task_queue = linkedlist_create_queue_with_heap(heap);
	task_cleaner_queue = linkedlist_create_queue_with_heap(heap);

	interrupt_irq_set_handler(0x60, task_task_switch_isr);

	current_task = memory_malloc_ext(heap, sizeof(task_t), 0x0);
	current_task->heap = heap;
	current_task->task_id = 0;
	current_task->state = TASK_STATE_CREATED;
	current_task->entry_point = kmain64;
	current_task->page_table = memory_paging_get_table();
	current_task->fx_registers = memory_malloc_ext(heap, sizeof(uint8_t) * 512, 0x0);

	uint32_t tss_limit = sizeof(tss_t) - 1;
	DESCRIPTOR_BUILD_TSS_SEG(d_tss, (size_t)tss, tss_limit, DPL_KERNEL);

	uint64_t* old_rbp = (uint64_t*)&__stack_top;
	uint64_t* old_rsp = (uint64_t*)rsp;
	uint64_t* new_rbp = (uint64_t*)kernel_rsp;
	uint64_t* new_rsp = new_rbp - (old_rbp - old_rsp);
	kernel_rsp = (uint64_t)new_rsp;

	printf("TASK: move stack 0x%p-0x%p to 0x%p-0x%p\n", old_rsp, old_rbp, new_rsp, new_rbp);
	printf("TASK: kernel task is at 0x%p\n", current_task);

	while(old_rsp < old_rbp) {
		*new_rsp = *old_rsp;
		new_rsp++;
		old_rsp++;
	}

	new_rbp[-2] = (uint64_t)new_rbp;

	printf("TASK: Info task register loading with tss 0x%p limit 0x%x\n", tss, tss_limit);

	__asm__ __volatile__ (
		"cli\n"
		"ltr %0\n"
		"mov %1, %%rsp\n"
		"sti\n"
		: : "r" (tss_selector), "m" (kernel_rsp)
		);

	return 0;
}

void task_save_registers(task_t* task) {
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
		"m" (task->rsp)
		);
}

void task_load_registers(task_t* task) {
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
		"mov 0x8(%%rsp), %%rax\n"
		"cmp $0xdeadc0de, %%eax\n"
		"jne label1%=\n"
		"pop %%rax\n"
		"add $0x8, %%rsp\n"
		"jmp label2%=\n"
		"label1%=: pop %%rax\n"
		"add $0x10, %%rsp\n"
		"label2%=: mov %12, %%rdi\n"
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
		"m" (task->rsp)
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

void task_switch_task() {
	if(task_queue == NULL) {
		return;
	}

	if(linkedlist_size(task_queue) == 0) {
		return;
	}

	if(current_task != NULL) {
		if((time_timer_get_tick_count() - current_task->last_tick_count) < TASK_MAX_TICK_COUNT) {
			return;
		}

		task_save_registers(current_task);
		linkedlist_queue_push(task_queue, current_task);

		if(current_task->task_id == 0) {
			task_cleanup();
		}
	}

	current_task = linkedlist_queue_pop(task_queue);
	current_task->last_tick_count = time_timer_get_tick_count();
	task_load_registers(current_task);
}

void task_end_task() {
	cpu_cli();
	printf("TASK: Debug ending task 0x%li\n", current_task->task_id);

	linkedlist_queue_push(task_cleaner_queue, current_task);

	current_task = NULL;

	cpu_sti();

	__asm__ __volatile__ ("int $0x80\n");
}

void task_create_task(memory_heap_t* heap, uint64_t stack_size, void* entry_point) {
	cpu_cli();

	task_t* new_task = memory_malloc_ext(heap, sizeof(task_t), 0x0);
	new_task->heap = heap;
	new_task->task_id = task_id++;
	new_task->state = TASK_STATE_CREATED;
	new_task->entry_point = entry_point;
	new_task->page_table = memory_paging_get_table();
	new_task->fx_registers = memory_malloc_ext(heap, sizeof(uint8_t) * 512, 0x0);
	new_task->stack = memory_malloc_ext(heap, sizeof(uint8_t) * stack_size, 0x0);
	new_task->rflags = 0x202;

	uint64_t rbp = (uint64_t)new_task->stack;
	rbp += stack_size - 16;
	new_task->rbp = rbp;
	new_task->rsp = rbp - 32;

	uint64_t* stack = (uint64_t*)rbp;
	stack[-1] = (uint64_t)task_end_task;
	stack[-2] = (uint64_t)entry_point;
	stack[-3] = (uint64_t)apic_eoi;
	stack[-4] = 0xdeadc0de;

	printf("TASK: scheduling new task 0x%lx 0x%p stack at 0x%lx-0x%lx\n", new_task->task_id, new_task, new_task->rsp, new_task->rbp);

	linkedlist_stack_push(task_queue, new_task);

	cpu_sti();
}

void task_yield() {
	__asm__ __volatile__ ("int $0x80\n");
}

void task_task_switch_isr(interrupt_frame_t* frame, uint16_t intnum) {
	UNUSED(frame);
	UNUSED(intnum);

	task_switch_task();

	cpu_sti();
}


uint64_t task_get_id() {
	return current_task->task_id;
}

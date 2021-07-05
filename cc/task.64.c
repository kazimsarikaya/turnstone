/**
 * @file task.64.c
 * @brief cpu task methods
 */
#include <cpu/task.h>
#include <memory/paging.h>
#include <video.h>

task_t* current_task = NULL;

extern int8_t kmain64();

extern uint64_t __stack_top;

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


	current_task = memory_malloc_ext(heap, sizeof(task_t), 0x0);
	current_task->pid = 0;
	current_task->entry_point = kmain64;
	current_task->page_table = memory_paging_get_table();
	current_task->register_store->fx_registers = memory_malloc_ext(heap, sizeof(uint8_t) * 512, 0x0);

	video_printf("ct 0x%p ctrs 0x%p ctrsfx 0x%p\n", current_task, current_task->register_store->fx_registers );

	uint32_t tss_limit = sizeof(tss_t) - 1;
	DESCRIPTOR_BUILD_TSS_SEG(d_tss, (size_t)tss, tss_limit, DPL_KERNEL);

	uint64_t* old_rbp = (uint64_t*)&__stack_top;
	uint64_t* old_rsp = (uint64_t*)rsp;
	uint64_t* new_rbp = (uint64_t*)kernel_rsp;
	uint64_t* new_rsp = new_rbp - (old_rbp - old_rsp);
	kernel_rsp = (uint64_t)new_rsp;
	current_task->rsp = kernel_rsp;

	printf("TASK: move stack 0x%p-0x%p to 0x%p-0x%p\n", old_rsp, old_rbp, new_rsp, new_rbp);

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


void task_switch_task() {
	if(current_task == NULL) {
		return;
	}

	//__asm__ __volatile__ ("1: jmp 1b\n");
}

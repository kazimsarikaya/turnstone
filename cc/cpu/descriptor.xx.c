/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <types.h>
#include <cpu.h>
#include <cpu/descriptor.h>
#include <cpu/task.h>
#include <memory.h>
#include <memory/frame.h>
#include <systeminfo.h>
#include <linker.h>
#include <systeminfo.h>

MODULE("turnstone.kernel.cpu.descriptor");

descriptor_register_t* GDT_REGISTER = NULL;
descriptor_register_t* IDT_REGISTER = NULL;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
uint8_t descriptor_build_gdt_register(){
    uint16_t gdt_size = sizeof(descriptor_gdt_t) * 7;
    descriptor_gdt_t* gdts = memory_malloc(gdt_size);
    if(gdts == NULL) {
        return -1;
    }
    DESCRIPTOR_BUILD_GDT_NULL_SEG(gdts[0]);
    DESCRIPTOR_BUILD_GDT_CODE_SEG(gdts[1], DPL_KERNEL);
    DESCRIPTOR_BUILD_GDT_DATA_SEG(gdts[2], DPL_KERNEL);
    DESCRIPTOR_BUILD_GDT_DATA_SEG(gdts[5], DPL_USER);
    DESCRIPTOR_BUILD_GDT_CODE_SEG(gdts[6], DPL_USER);

    if(GDT_REGISTER) {
        memory_free(GDT_REGISTER);
    }

    GDT_REGISTER = memory_malloc(sizeof(descriptor_register_t));
    if(GDT_REGISTER == NULL) {
        return -1;
    }

    GDT_REGISTER->limit = gdt_size - 1;
    GDT_REGISTER->base = (size_t)gdts;

    __asm__ __volatile__ ("lgdt (%%rax)\n"
                          "push $0x08\n"
                          "lea fix_gdt_jmp%=(%%rip),%%rax\n"
                          "push %%rax\n"
                          "lretq\n"
                          "fix_gdt_jmp%=:"
                          "mov $0x10, %%rax\n"
                          "mov %%ax, %%ss\n"
                          : : "a" (GDT_REGISTER));
    return 0;
}
#pragma GCC diagnostic pop

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
uint8_t descriptor_build_ap_descriptors_register(void){

    uint64_t rsp = 0;

    __asm__ __volatile__ ("mov %%rsp, %0\n" : : "m" (rsp));

    uint16_t gdt_size = sizeof(descriptor_gdt_t) * 7;
    descriptor_gdt_t* gdts = memory_malloc(gdt_size);
    if(gdts == NULL) {
        return -1;
    }
    DESCRIPTOR_BUILD_GDT_NULL_SEG(gdts[0]);
    DESCRIPTOR_BUILD_GDT_CODE_SEG(gdts[1], DPL_KERNEL);
    DESCRIPTOR_BUILD_GDT_DATA_SEG(gdts[2], DPL_KERNEL);
    DESCRIPTOR_BUILD_GDT_DATA_SEG(gdts[5], DPL_USER);
    DESCRIPTOR_BUILD_GDT_CODE_SEG(gdts[6], DPL_USER);

    descriptor_register_t* gdtr = memory_malloc(sizeof(descriptor_register_t));

    if(gdtr == NULL) {
        PRINTLOG(KERNEL, LOG_FATAL, "cannot allocate memory for gdtr");

        return -1;
    }

    gdtr->limit = gdt_size - 1;
    gdtr->base = (size_t)gdts;

    __asm__ __volatile__ ("lgdt (%%rax)\n"
                          "push $0x08\n"
                          "lea fix_gdt_jmp%=(%%rip),%%rax\n"
                          "push %%rax\n"
                          "lretq\n"
                          "fix_gdt_jmp%=:"
                          "mov $0x10, %%rax\n"
                          "mov %%ax, %%ss\n"
                          : : "a" (gdtr));


    program_header_t* kernel = (program_header_t*)SYSTEM_INFO->kernel_start;
    uint64_t stack_size = kernel->section_locations[LINKER_SECTION_TYPE_STACK].section_size;


    uint64_t frame_count = 9 * stack_size / FRAME_SIZE;

    frame_t* stack_frames = NULL;

    if(KERNEL_FRAME_ALLOCATOR->allocate_frame_by_count(KERNEL_FRAME_ALLOCATOR,
                                                       frame_count,
                                                       FRAME_ALLOCATION_TYPE_RESERVED | FRAME_ALLOCATION_TYPE_BLOCK,
                                                       &stack_frames,
                                                       NULL) != 0) {
        PRINTLOG(KERNEL, LOG_FATAL, "cannot allocate stack frames of count 0x%llx", frame_count);

        return -1;
    }

    uint64_t stack_bottom = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(stack_frames->frame_address);

    memory_paging_add_va_for_frame(stack_bottom, stack_frames, MEMORY_PAGING_PAGE_TYPE_NOEXEC);


    tss_t* tss = memory_malloc_ext(NULL, sizeof(tss_t), 0x1000);

    if(tss == NULL) {
        PRINTLOG(KERNEL, LOG_FATAL, "cannot allocate memory for tss");

        return -1;
    }

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


    descriptor_tss_t* d_tss = (descriptor_tss_t*)&gdts[3];

    size_t tmp_selector = (size_t)d_tss - (size_t)gdts;
    uint16_t tss_selector = (uint16_t)tmp_selector;

    uint32_t tss_limit = sizeof(tss_t) - 1;
    DESCRIPTOR_BUILD_TSS_SEG(d_tss, (size_t)tss, tss_limit, DPL_KERNEL);

    __asm__ __volatile__ (
        "ltr %0\n"
        : : "r" (tss_selector)
        );

    return 0;
}
#pragma GCC diagnostic pop

uint8_t descriptor_build_idt_register(){
    uint16_t idt_size = sizeof(descriptor_idt_t) * 256;

    if(IDT_REGISTER) {
        memory_free(IDT_REGISTER);
    }

    IDT_REGISTER = memory_malloc(sizeof(descriptor_register_t));
    if(IDT_REGISTER == NULL) {
        return -1;
    }

    if(SYSTEM_INFO->remapped == 0) {
        frame_t idt_frame = {IDT_BASE_ADDRESS, 1, FRAME_TYPE_RESERVED, 0};
        KERNEL_FRAME_ALLOCATOR->allocate_frame(KERNEL_FRAME_ALLOCATOR, &idt_frame);
    }

    IDT_REGISTER->limit = idt_size - 1;
    IDT_REGISTER->base = IDT_BASE_ADDRESS;

    __asm__ __volatile__ ("lidt (%%rax)\n" : : "a" (IDT_REGISTER));

    return 0;
}

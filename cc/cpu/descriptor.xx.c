/**
 * @file descriptor.xx.c
 * @brief CPU descriptor implementations such as GDT, IDT.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <types.h>
#include <cpu.h>
#include <cpu/descriptor.h>
#include <cpu/task.h>
#include <memory.h>
#include <memory/frame.h>
#include <memory/paging.h>
#include <systeminfo.h>
#include <linker.h>
#include <systeminfo.h>
#include <logging.h>

MODULE("turnstone.kernel.cpu.descriptor");

int8_t descriptor_build_gdt_register(void){
    frame_allocator_t* fa = frame_get_allocator();

    if(!fa) {
        PRINTLOG(KERNEL, LOG_ERROR, "frame allocator is null");

        return -1;
    }

    uint16_t gdt_size = sizeof(descriptor_gdt_t) * 7;

    uint64_t gdt_fa_size = gdt_size + (FRAME_SIZE - (gdt_size % FRAME_SIZE));

    frame_t* gdt_fa = NULL;

    if(fa->allocate_frame_by_count(fa,
                                   gdt_fa_size / FRAME_SIZE,
                                   FRAME_ALLOCATION_TYPE_RESERVED | FRAME_ALLOCATION_TYPE_BLOCK,
                                   &gdt_fa, NULL) != 0) {
        PRINTLOG(KERNEL, LOG_FATAL, "cannot allocate frames for gdt");

        return -1;
    }

    PRINTLOG(KERNEL, LOG_DEBUG, "gdt frames allocated at 0x%llx with size 0x%llx", gdt_fa->frame_address, gdt_fa_size);

    uint64_t gdt_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(gdt_fa->frame_address);

    memory_paging_add_va_for_frame(gdt_va, gdt_fa, MEMORY_PAGING_PAGE_TYPE_NOEXEC);

    memory_memclean((void*)gdt_va, gdt_fa_size);

    descriptor_gdt_t* gdts = (descriptor_gdt_t*)gdt_va;


    DESCRIPTOR_BUILD_GDT_NULL_SEG(gdts[0]);
    DESCRIPTOR_BUILD_GDT_CODE_SEG(gdts[1], DPL_KERNEL);
    DESCRIPTOR_BUILD_GDT_DATA_SEG(gdts[2], DPL_KERNEL);
    // because of sysretq, user code segment selector
    // must be 0x30 and user data segment selector must be 0x28,
    // msr_star's upper 16 bits (48:63) are sysretq's cs selector.
    // from that value cs = sysretq_cs + 0x16, ss = sysretq_cs + 0x08.
    // so they are in reverse order in gdt,
    // otherwise sysretq will cause general protection fault when
    // returning to user space
    DESCRIPTOR_BUILD_GDT_DATA_SEG(gdts[5], DPL_USER);
    DESCRIPTOR_BUILD_GDT_CODE_SEG(gdts[6], DPL_USER);

    descriptor_register_t gdtr = {
        .limit = gdt_size - 1,
        .base  = (size_t)gdts
    };

    PRINTLOG(KERNEL, LOG_DEBUG, "gdt register limit: 0x%04x base: 0x%p", gdtr.limit, (void*)gdtr.base);

    asm volatile ("lgdt %0\n"
                  "push $0x08\n"
                  "lea fix_gdt_jmp%=(%%rip),%%rax\n"
                  "push %%rax\n"
                  "lretq\n"
                  "fix_gdt_jmp%=:"
                  "mov $0x10, %%rax\n"
                  "mov %%ax, %%ss\n"
                  : : "m" (gdtr));
    return 0;
}

descriptor_register_t descriptor_get_gdt_register(void) {
    descriptor_register_t gdtr;

    asm volatile ("sgdt %0" : "=m" (gdtr));

    return gdtr;
}

int8_t descriptor_build_ap_descriptors_register(uint64_t* gdt_fa_location,
                                                uint64_t* out_gdt_size,
                                                uint64_t* tss_fa_location,
                                                uint64_t* out_tss_size,
                                                uint64_t* stack_bottom_fa_location,
                                                uint64_t* out_stack_size) {
    if(gdt_fa_location == NULL || out_gdt_size == NULL ||
       tss_fa_location == NULL || out_tss_size == NULL ||
       stack_bottom_fa_location == NULL || out_stack_size == NULL) {
        PRINTLOG(KERNEL, LOG_ERROR, "invalid argument");

        return -1;
    }

    frame_allocator_t* fa = frame_get_allocator();

    uint16_t gdt_size = sizeof(descriptor_gdt_t) * 7;

    uint64_t gdt_fa_size = gdt_size + (FRAME_SIZE - (gdt_size % FRAME_SIZE));

    frame_t* gdt_fa = NULL;

    if(fa->allocate_frame_by_count(fa,
                                   gdt_fa_size / FRAME_SIZE,
                                   FRAME_ALLOCATION_TYPE_RESERVED | FRAME_ALLOCATION_TYPE_BLOCK,
                                   &gdt_fa, NULL) != 0) {
        PRINTLOG(KERNEL, LOG_FATAL, "cannot allocate frames for gdt");

        return -1;
    }

    uint64_t gdt_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(gdt_fa->frame_address);

    memory_paging_add_va_for_frame(gdt_va, gdt_fa, MEMORY_PAGING_PAGE_TYPE_NOEXEC);

    memory_memclean((void*)gdt_va, gdt_fa_size);

    descriptor_gdt_t* gdts = (descriptor_gdt_t*)gdt_va;

    *out_gdt_size    = gdt_fa_size;
    *gdt_fa_location = gdt_fa->frame_address;


    DESCRIPTOR_BUILD_GDT_NULL_SEG(gdts[0]);
    DESCRIPTOR_BUILD_GDT_CODE_SEG(gdts[1], DPL_KERNEL);
    DESCRIPTOR_BUILD_GDT_DATA_SEG(gdts[2], DPL_KERNEL);
    // because of sysretq, user code segment selector
    // must be 0x30 and user data segment selector must be 0x28,
    // msr_star's upper 16 bits (48:63) are sysretq's cs selector.
    // from that value cs = sysretq_cs + 0x16, ss = sysretq_cs + 0x08.
    // so they are in reverse order in gdt,
    // otherwise sysretq will cause general protection fault when
    // returning to user space
    DESCRIPTOR_BUILD_GDT_DATA_SEG(gdts[5], DPL_USER);
    DESCRIPTOR_BUILD_GDT_CODE_SEG(gdts[6], DPL_USER);

    descriptor_register_t gdtr = {
        .limit = gdt_size - 1,
        .base  = (size_t)gdts
    };

    asm volatile ("lgdt (%%rax)\n"
                  "push $0x08\n"
                  "lea fix_gdt_jmp%=(%%rip),%%rax\n"
                  "push %%rax\n"
                  "lretq\n"
                  "fix_gdt_jmp%=:"
                  "mov $0x10, %%rax\n"
                  "mov %%ax, %%ss\n"
                  : : "a" (&gdtr));


    program_header_t* kernel = (program_header_t*)SYSTEM_INFO->program_header_virtual_start;
    uint64_t stack_size      = kernel->program_stack_size;


    uint64_t frame_count = 10 * stack_size / FRAME_SIZE;

    frame_t* stack_frames = NULL;

    if(fa->allocate_frame_by_count(fa,
                                   frame_count,
                                   FRAME_ALLOCATION_TYPE_RESERVED | FRAME_ALLOCATION_TYPE_BLOCK,
                                   &stack_frames,
                                   NULL) != 0) {
        PRINTLOG(KERNEL, LOG_FATAL, "cannot allocate stack frames of count 0x%llx", frame_count);

        return -1;
    }

    uint64_t stack_bottom = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(stack_frames->frame_address);

    memory_paging_add_va_for_frame(stack_bottom, stack_frames, MEMORY_PAGING_PAGE_TYPE_NOEXEC);

    memory_memclean((void*)stack_bottom, frame_count * FRAME_SIZE);

    *stack_bottom_fa_location = stack_frames->frame_address;
    *out_stack_size           = frame_count * FRAME_SIZE;

    uint64_t tss_size = sizeof(tss_t);
    tss_size += 0x1000 - (tss_size % 0x1000);

    frame_t* tss_fa = NULL;

    if(fa->allocate_frame_by_count(fa,
                                   tss_size / FRAME_SIZE,
                                   FRAME_ALLOCATION_TYPE_RESERVED | FRAME_ALLOCATION_TYPE_BLOCK,
                                   &tss_fa, NULL) != 0) {
        PRINTLOG(KERNEL, LOG_FATAL, "cannot allocate frames for tss");

        return -1;
    }

    uint64_t tss_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(tss_fa->frame_address);

    memory_paging_add_va_for_frame(tss_va, tss_fa, MEMORY_PAGING_PAGE_TYPE_NOEXEC);

    memory_memclean((void*)tss_va, tss_size);

    tss_t* tss = (tss_t*)tss_va;

    *out_tss_size    = tss_size;
    *tss_fa_location = tss_fa->frame_address;


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


    descriptor_tss_t* d_tss = (descriptor_tss_t*)&gdts[3];

    size_t tmp_selector   = (size_t)d_tss - (size_t)gdts;
    uint16_t tss_selector = (uint16_t)tmp_selector;

    uint32_t tss_limit = sizeof(tss_t) - 1;
    DESCRIPTOR_BUILD_TSS_SEG(d_tss, (size_t)tss, tss_limit, DPL_KERNEL);

    asm volatile (
        "ltr %0\n"
        : : "r" (tss_selector)
        );

    interrupt_ist_redirect_main_interrupts(7);
    interrupt_ist_redirect_interrupt(0xd, 6);
    interrupt_ist_redirect_interrupt(0xe, 5);

    // the functions above will open the interrupts,
    // but we don't want that before ap booting,
    // so we will close them here
    cpu_cli();

    return 0;
}

int8_t descriptor_build_idt_register(void){
    frame_allocator_t* fa = frame_get_allocator();

    if(!fa) {
        PRINTLOG(KERNEL, LOG_ERROR, "frame allocator is null");

        return -1;
    }

    uint16_t idt_size = sizeof(descriptor_idt_t) * 256;

    frame_t idt_frame = {IDT_BASE_ADDRESS, (idt_size + FRAME_SIZE - 1) / FRAME_SIZE, FRAME_TYPE_RESERVED, 0};

    PRINTLOG(KERNEL, LOG_DEBUG, "idt frame address: 0x%llx count 0x%llx", idt_frame.frame_address, idt_frame.frame_count);

    if(fa->allocate_frame(fa, &idt_frame) != 0) {
        return -1;
    }

    if(memory_paging_add_va_for_frame(idt_frame.frame_address, &idt_frame, MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
        return -1;
    }

    memory_memclean((void*)IDT_BASE_ADDRESS, idt_size);

    descriptor_register_t idt_register = {
        .limit = idt_size - 1,
        .base  = IDT_BASE_ADDRESS
    };

    PRINTLOG(KERNEL, LOG_DEBUG, "idt register limit: 0x%04x base: 0x%p", idt_register.limit, (void*)idt_register.base);

    asm volatile ("lidt %0\n" : : "m" (idt_register));

    return 0;
}

descriptor_register_t descriptor_get_idt_register(void) {
    descriptor_register_t idtr;

    asm volatile ("sidt %0" : "=m" (idtr));

    return idtr;
}


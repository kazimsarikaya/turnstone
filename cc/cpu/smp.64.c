/**
 * @file smp.64.c
 * @brief SMP implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 **/

#include <cpu/smp.h>

#include <apic.h>
#include <acpi.h>
#include <logging.h>
#include <memory.h>
#include <memory/paging.h>
#include <memory/frame.h>
#include <cpu.h>
#include <cpu/crx.h>
#include <cpu/descriptor.h>
#include <cpu/interrupt.h>
#include <cpu/syscall.h>

MODULE("turnstone.kernel.cpu");

int8_t  smp_init_cpu(uint8_t cpu_id);
int32_t smp_ap_boot(uint8_t cpu_id);

const uint8_t trampoline_code[] = {
    0xea, 0x05, 0x80, 0x00, 0x00, // 8000: jmp $0x0:$0x8005
    0xfa, // 8005: cli
    0xfc, // 8006: cld
    0x8c, 0xc8, // 8007: mov %cs, %ax
    0x8e, 0xd8, // 8009: mov %ax, %ds
    0x8e, 0xc0, // 800b: mov %ax, %es
    0x8e, 0xd0, // 800d: mov %ax, %ss
    0x66, 0xbb, 0x00, 0xb0, 0x00, 0x00, // 800f: mov $0xb000, %ebx
    0x66, 0xb8, 0x03, 0xc0, 0x00, 0x00, // 8015: mov $0xc003, %eax
    0x67, 0x66, 0x89, 0x03, // 801b: mov %eax, (%ebx)
    0x66, 0xbb, 0x00, 0xc0, 0x00, 0x00, // 801f: mov $0xc000, %ebx
    0x66, 0xb8, 0x03, 0xd0, 0x00, 0x00, // 8025: mov $0xd003, %eax
    0x67, 0x66, 0x89, 0x03, // 802b: mov %eax, (%ebx)
    0x66, 0xbb, 0x00, 0xd0, 0x00, 0x00, // 802f: mov $0xd000, %ebx
    0x66, 0xb8, 0x83, 0x00, 0x00, 0x00, // 8035: mov $0x83, %eax
    0x67, 0x66, 0x89, 0x03, 0x66, // 803b: mov %eax, (%ebx)
    0xb8, 0x00, 0xb0, 0x00, 0x00, // 803f: mov $0xb000, %eax
    0x0f, 0x22, 0xd8, // 8045: mov %eax, %cr3
    0x66, 0xb8, 0xa0, 0x00, 0x00, 0x00, // 8048: mov $0xa0, %eax
    0x0f, 0x22, 0xe0, // 804e: mov %eax, %cr4
    0x66, 0xb9, 0x80, 0x00, 0x00, 0xc0, // 8051: mov $0xc0000080, %ecx
    0x0f, 0x32, // 8057: rdmsr
    0x66, 0x0d, 0x00, 0x09, 0x00, 0x00, // 8059: or $0x900, %eax
    0x0f, 0x30, // 805f: wrmsr
    0x0f, 0x01, 0x1e, 0x10, 0x81, // 8061: lidt $0x8110
    0x0f, 0x01, 0x16, 0x08, 0x81, // 8066: lgdt $0x8108
    0x66, 0xb8, 0x01, 0x00, 0x00, 0x80, // 806b: mov $0x80000001, %eax
    0x0f, 0x22, 0xc0, // 8071: mov %eax, %cr0
    0xea, 0x80, 0x80, 0x08, 0x00, // 8074: jmp $0x08:$0x8080
    0x0f, 0x1f, 0x80, 0x00, 0x00, 0x00, 0x00, // nop
    0x66, 0x31, 0xc0, // 8080: xor %eax, %eax
    0x8e, 0xd8, // 8083:mov %ax, %ds
    0x8e, 0xc0, // 8085: mov %ax, %es
    0x8e, 0xd0, // 8087: mov %ax, %ss
    0x8e, 0xe0, // 8089: mov %ax, %fs
    0x8e, 0xe8, // 808b: mov %ax, %gs
    0xb8, 0x01, 0x00, 0x00, 0x00, // 808d: mov $0x1, %eax
    0x0f, 0xa2, // 8092: cpuid
    0xb1, 0x18, // 8094: mov $0x18, %cl
    0x48, 0xd3, 0xeb, // 8096: shr %cl, %rbx
    0x48, 0x81, 0xe3, 0xff, 0x00, 0x00, 0x00, // 8099: and $0xff, %rbx
    0x48, 0x89, 0xda, // 80a0: mov %rbx, %rdx
    0x48, 0x89, 0xdf, // 80a3: mov %rbx, %rdi
    0x48, 0xc7, 0xc3, 0x00, 0x90, 0x00, 0x00, // 80a6: mov $0x9000, %rbx
    0x48, 0x8b, 0x43, 0x08, // 80ad: mov 0x8(%rbx), %rax
    0x48, 0x8b, 0x0b, // 80b1: mov (%rbx), %rcx
    0x48, 0xf7, 0xe2, // 80b4: mul %rdx
    0x48, 0x01, 0xc1, // 80b7: add %rax, %rcx
    0x48, 0x83, 0xe9, 0x10, // 80ba: sub $0x10, %rcx
    0x48, 0x89, 0xcc, // 80be: mov %rcx, %rsp
    0x48, 0x8b, 0x43, 0x10, // 80c1: mov 0x10(%rbx), %rax
    0x0f, 0x22, 0xc0, // 80c5: mov %rax, %cr0
    0x48, 0x8b, 0x43, 0x18, // 80c8: mov 0x18(%rbx), %rax
    0x0f, 0x22, 0xd8, // 80cc: mov %rax, %cr3
    0x48, 0x8b, 0x43, 0x20, // 80cf: mov 0x20(%rbx), %rax
    0x0f, 0x22, 0xe0, // 80d3: mov %rax, %cr4
    0x48, 0x8b, 0x43, 0x28, // 80d6: mov 0x28(%rbx), %rax
    0x48, 0xc7, 0xc0, 0x00, 0x00, 0x00, 0x00, // 80da: mov $0x0, %rax
    0xff, 0xd0, // 80e1: callq *%rax
    0xf4, // 80e3: hlt
    0xeb, 0xfd, // 80e4: jmp 0x80e3
    0x00, 0x00, // 80e6: nop
    0x00, 0x0f, 0x1f, 0x80, 0x00, 0x00, // 80e8: nop
    0x00, 0x00, // 80ee: nop
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 80f0: gdt null
    0x00, 0x00, 0x00, 0x00, 0x00, 0x99, 0x20, 0x00, // 80f8: gdt code
    0x00, 0x00, 0x00, 0x00, 0x00, 0x90, 0x00, 0x00, // 8100: gdt data
    0x17, 0x00, 0xf0, 0x80, 0x00, 0x00, // 8108: gdtr
    0x66, 0x90, // 810e: nop padding
    0xff, 0x0f, 0x00, 0x00, 0x10, 0x00, // 8110: idtr
    0x00, 0xe0, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 8116: nop padding
};


int8_t smp_init_cpu(uint8_t cpu_id) {
    PRINTLOG(APIC, LOG_INFO, "SMP: Initialising CPU %d", cpu_id);

    apic_send_init(cpu_id);

    apic_send_sipi(cpu_id, 0x08);

    apic_send_sipi(cpu_id, 0x08);

    return 0;
}


int8_t smp_init(void) {
    PRINTLOG(APIC, LOG_INFO, "SMP Initialisation");

    uint32_t local_apic_id = apic_get_local_apic_id();

    PRINTLOG(APIC, LOG_INFO, "SMP: Bootstrap APIC ID: 0x%x", local_apic_id);
    uint8_t ap_cpu_count = apic_get_ap_count();

    if(ap_cpu_count == 0) {
        PRINTLOG(APIC, LOG_INFO, "SMP: No APs found");
        PRINTLOG(APIC, LOG_INFO, "SMP: No need to initialise SMP");

        return 0;
    }

    acpi_sdt_header_t* madt = acpi_get_table(ACPI_CONTEXT->xrsdp_desc, "APIC");

    if (madt == NULL) {
        PRINTLOG(APIC, LOG_ERROR, "SMP: MADT not found");
        return -1;
    }

    memory_paging_add_page(0x8000, 0x8000, MEMORY_PAGING_PAGE_TYPE_4K);

    uint8_t * trampoline = (uint8_t*)0x8000;

    memory_paging_add_page(0x9000, 0x9000, MEMORY_PAGING_PAGE_TYPE_4K);

    smp_data_t* smp_data = (smp_data_t*)0x9000;

    memory_memcopy(trampoline_code, trampoline, sizeof(trampoline_code));

    uint32_t* trampoline_call_addr = (uint32_t*)(trampoline + 0xdd);

    *trampoline_call_addr = (uint32_t)((uint64_t)smp_ap_boot);

    linkedlist_t* apic_entries = acpi_get_apic_table_entries(madt);

    frame_t* stack_frames;
    uint64_t stack_frames_cnt = 16 * ap_cpu_count;
    uint64_t stack_size = 16 * FRAME_SIZE;

    if(KERNEL_FRAME_ALLOCATOR->allocate_frame_by_count(KERNEL_FRAME_ALLOCATOR, stack_frames_cnt, FRAME_ALLOCATION_TYPE_USED | FRAME_ALLOCATION_TYPE_BLOCK, &stack_frames, NULL) != 0) {
        PRINTLOG(APIC, LOG_ERROR, "SMP: Failed to allocate stack frames");
        return -1;
    }

    uint64_t stack_frames_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(stack_frames->frame_address);

    if(memory_paging_add_va_for_frame(stack_frames_va, stack_frames, MEMORY_PAGING_PAGE_TYPE_4K) != 0) {
        PRINTLOG(APIC, LOG_ERROR, "SMP: Failed to map stack frames");
        return -1;
    }

    smp_data->stack_base = stack_frames_va;
    smp_data->stack_size = stack_size;
    smp_data->cr0 = cpu_read_cr0();
    smp_data->cr3 =  MEMORY_PAGING_GET_FA_FOR_RESERVED_VA(memory_paging_get_table()->page_table);
    smp_data->cr4 = cpu_read_cr4();
    smp_data->idt = IDT_REGISTER;

    iterator_t* iter = linkedlist_iterator_create(apic_entries);

    while(iter->end_of_iterator(iter) != 0) {
        const acpi_table_madt_entry_t* e = iter->get_item(iter);

        if(e->info.type == ACPI_MADT_ENTRY_TYPE_PROCESSOR_LOCAL_APIC) {
            uint8_t apic_id = e->processor_local_apic.apic_id;

            PRINTLOG(APIC, LOG_INFO, "SMP: Found APIC ID: %d local apic? %i", apic_id, apic_id == local_apic_id);

            if (apic_id != local_apic_id) {
                smp_init_cpu(apic_id);
            }
        }

        iter = iter->next(iter);
    }

    iter->destroy(iter);


    return 0;
}

int32_t smp_ap_boot(uint8_t cpu_id) {
    cpu_cli();

    apic_enable_lapic();

    uint32_t local_apic_id = apic_get_local_apic_id();

    PRINTLOG(APIC, LOG_INFO, "SMP: AP %i Booting local apic id %i", cpu_id, local_apic_id);

    if(local_apic_id != cpu_id) {
        PRINTLOG(APIC, LOG_ERROR, "SMP: AP cpu id %i mismatch local apic id %i", cpu_id, local_apic_id);
        cpu_hlt();

        return -1;
    }

    if(descriptor_build_ap_descriptors_register() != 0) {
        PRINTLOG(APIC, LOG_ERROR, "SMP: AP %i Failed to build descriptors", cpu_id);
        cpu_hlt();

        return -1;
    }

    apic_configure_lapic();

    cpu_sti();

    char_t* test_data = memory_malloc(sizeof(char_t*) * 32);

    if(test_data == NULL) {
        PRINTLOG(APIC, LOG_ERROR, "SMP: AP %i Failed to allocate test data", cpu_id);
        return -1;
    }

    memory_memcopy("hello world", test_data, 11);

    test_data[11] = ' ';
    test_data[12] = '0' + cpu_id;

    PRINTLOG(APIC, LOG_INFO, "SMP: AP %i Test Data: %s", cpu_id, test_data);

    uint64_t msr_efer = cpu_read_msr(CPU_MSR_EFER);
    msr_efer |= 1;
    cpu_write_msr(CPU_MSR_EFER, msr_efer);

    uint64_t msr_star = 0x00200008ULL << 32;
    cpu_write_msr(CPU_MSR_STAR, msr_star);

    uint64_t msr_lstar = (uint64_t)syscall_handler;
    cpu_write_msr(CPU_MSR_LSTAR, msr_lstar);

    PRINTLOG(APIC, LOG_INFO, "SMP: AP %i syscall handler: %llx", cpu_id, msr_lstar);

    uint64_t msr_fmask = 0x200;
    cpu_write_msr(CPU_MSR_FMASK, msr_fmask);


    frame_t* user_code_frames = NULL;

    if(KERNEL_FRAME_ALLOCATOR->allocate_frame_by_count(KERNEL_FRAME_ALLOCATOR,
                                                       2,
                                                       FRAME_ALLOCATION_TYPE_BLOCK,
                                                       &user_code_frames,
                                                       NULL) != 0) {
        PRINTLOG(KERNEL, LOG_FATAL, "cannot allocate user code frame");

        cpu_hlt();
    }

    uint64_t user_code_fa = user_code_frames->frame_address;
    uint64_t user_code_va = 8ULL << 40 | user_code_fa;
    uint64_t user_stack_fa = user_code_fa + 0x1000;
    uint64_t user_stack_va = 8ULL << 40 | user_stack_fa;

    memory_paging_add_page_ext(NULL,
                               user_code_va, user_code_fa,
                               MEMORY_PAGING_PAGE_TYPE_4K);

    memory_paging_add_page_ext(NULL,
                               user_stack_va, user_stack_fa,
                               MEMORY_PAGING_PAGE_TYPE_4K | MEMORY_PAGING_PAGE_TYPE_USER_ACCESSIBLE | MEMORY_PAGING_PAGE_TYPE_NOEXEC);

    memory_memclean((uint8_t*)user_code_va, FRAME_SIZE);
    memory_memclean((uint8_t*)user_stack_va, FRAME_SIZE);

    uint8_t* user_code = (uint8_t*)user_code_va;
    /*
     * user space example code
     * byte array of this assembly
     * start:
     *    mov $1, %rax
     *    syscallq
     *    jmp start
     */
    user_code[0] = 0x48;
    user_code[1] = 0xc7;
    user_code[2] = 0xc0;
    user_code[3] = 0x00;
    user_code[4] = 0x00;
    user_code[5] = 0x00;
    user_code[6] = 0x00;
    user_code[7] = 0x0f;
    user_code[8] = 0x05;
    user_code[9] = 0xeb;
    user_code[10] = 0xf5;

    memory_paging_toggle_attributes(user_code_va, MEMORY_PAGING_PAGE_TYPE_READONLY);
    memory_paging_set_user_accessible(user_code_va);
    memory_paging_clear_page(user_code_va, MEMORY_PAGING_CLEAR_TYPE_DIRTY | MEMORY_PAGING_CLEAR_TYPE_ACCESSED);

    PRINTLOG(APIC, LOG_INFO, "SMP: AP %i Booted", cpu_id);

    PRINTLOG(APIC, LOG_INFO, "SMP: AP %i Jumping to user code code at 0x%llx stack at 0x%llx", cpu_id, user_code_va, user_stack_va);

    // jump user mode with sysretq
    asm volatile (
        "xor %%rbp, %%rbp\n"
        "mov %%rax, %%rsp\n"
        "mov $0x200, %%r11\n"
        "sysretq\n"
        : : "a" (user_stack_va + 0x1000 - 0x10), "c" (user_code_va)
        );


    while(true) {
        cpu_idle();
    }

    return 0;
}

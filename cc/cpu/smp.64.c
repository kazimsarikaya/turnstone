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
#include <video.h>
#include <memory.h>
#include <memory/paging.h>
#include <memory/frame.h>
#include <cpu.h>
#include <cpu/crx.h>
#include <cpu/descriptor.h>
#include <cpu/interrupt.h>


typedef struct smp_data_t {
    uint64_t               stack_base;
    uint64_t               stack_size;
    cpu_reg_cr0_t          cr0;
    memory_page_table_t*   cr3;
    cpu_reg_cr4_t          cr4;
    descriptor_register_t* idt;
} smp_data_t;


int8_t  smp_init_cpu(uint8_t cpu_id);
int32_t smp_ap_boot(uint8_t cpu_id);

uint8_t trampoline_code[] = {
    0xea, 0x05, 0x80, 0x00, 0x00,             // 8000: jmp $0x0:$0x8005
    0xfa,                                     // 8005: cli
    0xfc,                                     // 8006: cld
    0x8c, 0xc8,                               // 8007: mov %cs, %ax
    0x8e, 0xd8,                               // 8009: mov %ax, %ds
    0x8e, 0xc0,                               // 800b: mov %ax, %es
    0x8e, 0xd0,                               // 800d: mov %ax, %ss
    0x66, 0xbb, 0x00, 0xb0, 0x00, 0x00,       // 800f: mov $0xb000, %ebx
    0x66, 0xb8, 0x03, 0xc0, 0x00, 0x00,       // 8015: mov $0xc003, %eax
    0x67, 0x66, 0x89, 0x03,                   // 801b: mov %eax, (%ebx)
    0x66, 0xbb, 0x00, 0xc0, 0x00, 0x00,       // 801f: mov $0xc000, %ebx
    0x66, 0xb8, 0x03, 0xd0, 0x00, 0x00,       // 8025: mov $0xd003, %eax
    0x67, 0x66, 0x89, 0x03,                   // 802b: mov %eax, (%ebx)
    0x66, 0xbb, 0x00, 0xd0, 0x00, 0x00,       // 802f: mov $0xd000, %ebx
    0x66, 0xb8, 0x83, 0x00, 0x00, 0x00,       // 8035: mov $0x83, %eax
    0x67, 0x66, 0x89, 0x03, 0x66,             // 803b: mov %eax, (%ebx)
    0xb8, 0x00, 0xb0, 0x00, 0x00,             // 803f: mov $0xb000, %eax
    0x0f, 0x22, 0xd8,                         // 8045: mov %eax, %cr3
    0x66, 0xb8, 0xa0, 0x00, 0x00, 0x00,       // 8048: mov $0xa0, %eax
    0x0f, 0x22, 0xe0,                         // 804e: mov %eax, %cr4
    0x66, 0xb9, 0x80, 0x00, 0x00, 0xc0,       // 8051: mov $0xc0000080, %ecx
    0x0f, 0x32,                               // 8057: rdmsr
    0x66, 0x0d, 0x00, 0x09, 0x00, 0x00,       // 8059: or $0x900, %eax
    0x0f, 0x30,                               // 805f: wrmsr
    0x0f, 0x01, 0x1e, 0x10, 0x81,             // 8061: lidt $0x8110
    0x0f, 0x01, 0x16, 0x08, 0x81,             // 8066: lgdt $0x8108
    0x66, 0xb8, 0x01, 0x00, 0x00, 0x80,       // 806b: mov $0x80000001, %eax
    0x0f, 0x22, 0xc0,                         // 8071: mov %eax, %cr0
    0xea, 0x80, 0x80, 0x08, 0x00,             // 8074: jmp $0x08:$0x8080
    0x0f, 0x1f, 0x80, 0x00, 0x00, 0x00, 0x00, // nop
    0x66, 0x31, 0xc0,                         // 8080: xor %eax, %eax
    0x8e, 0xd8,                               // 8083:mov %ax, %ds
    0x8e, 0xc0,                               // 8085: mov %ax, %es
    0x8e, 0xd0,                               // 8087: mov %ax, %ss
    0x8e, 0xe0,                               // 8089: mov %ax, %fs
    0x8e, 0xe8,                               // 808b: mov %ax, %gs
    0xb8, 0x01, 0x00, 0x00, 0x00,             // 808d: mov $0x1, %eax
    0x0f, 0xa2,                               // 8092: cpuid
    0xb1, 0x18,                               // 8094: mov $0x18, %cl
    0x48, 0xd3, 0xeb,                         // 8096: shr %cl, %rbx
    0x48, 0x81, 0xe3, 0xff, 0x00, 0x00, 0x00, // 8099: and $0xff, %rbx
    0x48, 0x89, 0xda,                         // 80a0: mov %rbx, %rdx
    0x48, 0x89, 0xdf,                         // 80a3: mov %rbx, %rdi
    0x48, 0xc7, 0xc3, 0x00, 0x90, 0x00, 0x00, // 80a6: mov $0x9000, %rbx
    0x48, 0x8b, 0x43, 0x08,                   // 80ad: mov 0x8(%rbx), %rax
    0x48, 0x8b, 0x0b,                         // 80b1: mov (%rbx), %rcx
    0x48, 0xf7, 0xe2,                         // 80b4: mul %rdx
    0x48, 0x01, 0xc1,                         // 80b7: add %rax, %rcx
    0x48, 0x83, 0xe9, 0x10,                   // 80ba: sub $0x10, %rcx
    0x48, 0x89, 0xcc,                         // 80be: mov %rcx, %rsp
    0x48, 0x8b, 0x43, 0x10,                   // 80c1: mov 0x10(%rbx), %rax
    0x0f, 0x22, 0xc0,                         // 80c5: mov %rax, %cr0
    0x48, 0x8b, 0x43, 0x18,                   // 80c8: mov 0x18(%rbx), %rax
    0x0f, 0x22, 0xd8,                         // 80cc: mov %rax, %cr3
    0x48, 0x8b, 0x43, 0x20,                   // 80cf: mov 0x20(%rbx), %rax
    0x0f, 0x22, 0xe0,                         // 80d3: mov %rax, %cr4
    0x48, 0x8b, 0x43, 0x28,                   // 80d6: mov 0x28(%rbx), %rax
    0x48, 0xc7, 0xc0, 0x00, 0x00, 0x00, 0x00, // 80da: mov $0x0, %rax
    0xff, 0xd0,                               // 80e1: callq *%rax
    0xf4,                                     // 80e3: hlt
    0xeb, 0xfd,                               // 80e4: jmp 0x80e3
    0x00, 0x00,                               // 80e6: nop
    0x00, 0x0f, 0x1f, 0x80, 0x00, 0x00,       // 80e8: nop
    0x00, 0x00,                               // 80ee: nop
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 80f0: gdt null
    0x00, 0x00, 0x00, 0x00, 0x00, 0x99, 0x20, 0x00, // 80f8: gdt code
    0x00, 0x00, 0x00, 0x00, 0x00, 0x90, 0x00, 0x00, // 8100: gdt data
    0x17, 0x00, 0xf0, 0x80, 0x00, 0x00,             // 8108: gdtr
    0x66, 0x90,                                     // 810e: nop padding
    0xff, 0x0f, 0x00, 0x00, 0x10, 0x00,             // 8110: idtr
    0x00, 0xe0, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 8116: nop padding
};


int8_t smp_init_cpu(uint8_t cpu_id) {
    PRINTLOG(APIC, LOG_INFO, "SMP: Initialising CPU %d", cpu_id);

    apic_send_ini(cpu_id);

    apic_send_sipi(cpu_id, 0x08);

    apic_send_sipi(cpu_id, 0x08);

    return 0;
}


int8_t smp_init(void) {
    PRINTLOG(APIC, LOG_INFO, "SMP Initialisation");

    uint8_t local_apic_id = apic_get_local_apic_id();
    uint8_t ap_cpu_count = 0;

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

    linkedlist_t apic_entries = acpi_get_apic_table_entries(madt);

    iterator_t* iter = linkedlist_iterator_create(apic_entries);

    while(iter->end_of_iterator(iter) != 0) {
        const acpi_table_madt_entry_t* e = iter->get_item(iter);

        if(e->info.type == ACPI_MADT_ENTRY_TYPE_PROCESSOR_LOCAL_APIC) {
            uint8_t apic_id = e->processor_local_apic.apic_id;

            if (apic_id != local_apic_id) {
                ap_cpu_count++;
            }
        }

        iter = iter->next(iter);
    }

    iter->destroy(iter);

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
    smp_data->cr3 =  MEMORY_PAGING_GET_FA_FOR_RESERVED_VA(memory_paging_get_table());
    smp_data->cr4 = cpu_read_cr4();
    smp_data->idt = IDT_REGISTER;

    iter = linkedlist_iterator_create(apic_entries);

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

extern uint64_t lapic_addr;

int32_t smp_ap_boot(uint8_t cpu_id) {
    UNUSED(cpu_id);

    PRINTLOG(APIC, LOG_INFO, "SMP: AP %i Booting", cpu_id);

    apic_register_spurious_interrupt_t* ar_si = (apic_register_spurious_interrupt_t*)(lapic_addr + APIC_REGISTER_OFFSET_SPURIOUS_INTERRUPT);
    ar_si->vector = 0x0f;
    ar_si->apic_software_enable = 1;

    uint32_t* tmp = (uint32_t*)(lapic_addr + APIC_REGISTER_OFFSET_SPURIOUS_INTERRUPT);
    *tmp = 0x10f;


    lapic_init_timer();

    uint32_t* lapic_lint0 = (uint32_t*)(lapic_addr + APIC_REGISTER_OFFSET_LINT0_LVT);
    uint32_t* lapic_lint1 = (uint32_t*)(lapic_addr + APIC_REGISTER_OFFSET_LINT1_LVT);

    *lapic_lint0 = APIC_ICR_DELIVERY_MODE_EXTERNAL_INT;
    *lapic_lint1 = APIC_ICR_DELIVERY_MODE_NMI;


    cpu_sti();


    PRINTLOG(APIC, LOG_INFO, "SMP: AP %i Booted", cpu_id);

    while(true) {
        //asm volatile ("hlt");
        cpu_idle();
    }

    return 0;
}

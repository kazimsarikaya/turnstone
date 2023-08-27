/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <types.h>
#include <helloworld.h>
#include <video.h>
#include <memory.h>
#include <memory/paging.h>
#include <systeminfo.h>
#include <strings.h>
#include <cpu/interrupt.h>
#include <acpi.h>
#include <acpi/aml.h>
#include <pci.h>
#include <iterator.h>
#include <ports.h>
#include <apic.h>
#include <linkedlist.h>
#include <cpu.h>
#include <cpu/crx.h>
#include <cpu/smp.h>
#include <utils.h>
#include <device/kbd.h>
#include <cpu/task.h>
#include <linker.h>
#include <driver/ahci.h>
#include <driver/nvme.h>
#include <random.h>
#include <memory/frame.h>
#include <time/timer.h>
#include <network.h>
#include <crc.h>
#include <device/hpet.h>

MODULE("turnstone.kernel.programs.kmain");

int8_t                         kmain64(size_t entry_point);
__attribute__((noreturn)) void ___kstart64(system_info_t* sysinfo);

__attribute__((noreturn)) void  ___kstart64(system_info_t* sysinfo) {
    cpu_cli();

    SYSTEM_INFO = sysinfo;

    outb(0xa1, 0xff);
    outb(0x21, 0xff);
    cpu_nop();
    cpu_nop();

    cpu_clear_segments();

    uint64_t kernel_start = SYSTEM_INFO->kernel_start;

    program_header_t* kernel = (program_header_t*)kernel_start;

    uint64_t stack_base = 0;

    if(!SYSTEM_INFO->remapped) {
        stack_base = kernel_start;
    }

    uint64_t stack_top = stack_base + kernel->section_locations[LINKER_SECTION_TYPE_STACK].section_start + kernel->section_locations[LINKER_SECTION_TYPE_STACK].section_size;
    cpu_set_and_clear_stack(stack_top);

    cpu_enable_sse();

    cpu_cld();

    int8_t res = 0;

#ifndef ___TESTMODE
    res = kmain64((size_t)&___kstart64);
#else
    res = kmain64_test((size_t)&___kstart64);
#endif

    if(res != 0) {
        cpu_hlt();
    } else {
        while(1) {
            if(task_idle_check_need_yield()) {
                task_yield();
            } else {
                cpu_idle();
            }
        }
    }
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
int8_t kmain64(size_t entry_point) {
    srand(0x123456789);
    crc32_init_table();

    memory_heap_t* heap = memory_create_heap_hash(0, 0);

    if(heap == NULL) {
        PRINTLOG(KERNEL, LOG_FATAL, "creating heap");
        return -1;
    }

    memory_set_default_heap(heap);

    video_init();
    video_clear_screen();

    PRINTLOG(KERNEL, LOG_INFO, "Initializing stage 3 with remapped kernel? %lli", SYSTEM_INFO->remapped);
    PRINTLOG(KERNEL, LOG_INFO, "new heap created at 0x%p", heap);
    PRINTLOG(KERNEL, LOG_INFO, "Entry point of kernel is 0x%llx", entry_point);

    video_frame_buffer_t* new_vfb = memory_malloc(sizeof(video_frame_buffer_t));

    if(new_vfb == NULL) {
        return -1;
    }

    memory_memcopy(SYSTEM_INFO->frame_buffer, new_vfb, sizeof(video_frame_buffer_t));

    uint8_t* new_mmap_data = memory_malloc(SYSTEM_INFO->mmap_size);

    if(new_mmap_data == NULL) {
        return -1;
    }

    memory_memcopy(SYSTEM_INFO->mmap_data, new_mmap_data, SYSTEM_INFO->mmap_size);

    uint8_t* new_reserved_mmap_data = NULL;

    if(SYSTEM_INFO->reserved_mmap_size) {
        PRINTLOG(KERNEL, LOG_DEBUG, "reserved mmap size is %lli", SYSTEM_INFO->reserved_mmap_size);
        new_reserved_mmap_data = memory_malloc(SYSTEM_INFO->reserved_mmap_size);

        if(new_reserved_mmap_data == NULL) {
            return -1;
        }

        memory_memcopy(SYSTEM_INFO->reserved_mmap_data, new_reserved_mmap_data, SYSTEM_INFO->reserved_mmap_size);
    }

    system_info_t* new_system_info = memory_malloc(sizeof(system_info_t));

    if(new_system_info == NULL) {
        return -1;
    }

    memory_memcopy(SYSTEM_INFO, new_system_info, sizeof(system_info_t));

    new_system_info->frame_buffer = new_vfb;
    new_system_info->mmap_data = new_mmap_data;
    new_system_info->reserved_mmap_data = new_reserved_mmap_data;
    new_system_info->heap = heap;

    SYSTEM_INFO = new_system_info;

    frame_allocator_t* fa = frame_allocator_new_ext(heap);

    if(fa) {
        PRINTLOG(KERNEL, LOG_DEBUG, "frame allocator created");
        KERNEL_FRAME_ALLOCATOR = fa;

        frame_t kernel_frames = {SYSTEM_INFO->kernel_start, SYSTEM_INFO->kernel_4k_frame_count, FRAME_TYPE_USED, 0};

        if(fa->allocate_frame(fa, &kernel_frames) != 0) {
            PRINTLOG(KERNEL, LOG_PANIC, "cannot allocate kernel frames");
            cpu_hlt();
        }

        frame_t kernel_default_heap_frames = {SYSTEM_INFO->kernel_default_heap_start, SYSTEM_INFO->kernel_default_heap_4k_frame_count, FRAME_TYPE_USED, 0};

        if(fa->allocate_frame(fa, &kernel_default_heap_frames) != 0) {
            PRINTLOG(KERNEL, LOG_PANIC, "cannot allocate kernel default heap frames");
            cpu_hlt();
        }

        if(!SYSTEM_INFO->remapped) {

            frame_t page_table_helper_frame = {SYSTEM_INFO->page_table_helper_frame, 4, FRAME_TYPE_RESERVED, 0};

            if(fa->allocate_frame(fa, &page_table_helper_frame) != 0) {
                PRINTLOG(KERNEL, LOG_PANIC, "cannot allocate page table helper frame");
                frame_allocator_print(fa);
                cpu_hlt();
            }
        }

    } else {
        PRINTLOG(KERNEL, LOG_PANIC, "cannot allocate frame allocator. Halting...");
        cpu_hlt();
    }

    if(descriptor_build_idt_register() != 0) {
        PRINTLOG(KERNEL, LOG_PANIC, "Can not build idt. Halting...");
        cpu_hlt();
    } else {
        PRINTLOG(KERNEL, LOG_DEBUG, "Default idt builded");
    }

    PRINTLOG(KERNEL, LOG_DEBUG, "Initializing interrupts");

    if(interrupt_init() != 0) {
        PRINTLOG(KERNEL, LOG_PANIC, "cannot init interrupts");

        return -1;
    }

    PRINTLOG(KERNEL, LOG_DEBUG, "interrupts initialized");

    if(descriptor_build_gdt_register() != 0) {
        PRINTLOG(KERNEL, LOG_PANIC, "Can not build gdt");
        return -1;
    } else {
        PRINTLOG(KERNEL, LOG_DEBUG, "Default gdt builded");
    }

    memory_page_table_t* p4 = memory_paging_build_table();
    if( p4 == NULL) {
        PRINTLOG(KERNEL, LOG_PANIC, "Can not build default page table. Halting");
        cpu_hlt();
    } else {
        PRINTLOG(KERNEL, LOG_DEBUG, "Default page table builded at 0x%p", p4);
        memory_paging_switch_table(p4);

        SYSTEM_INFO->my_page_table = 1;

        video_refresh_frame_buffer_address();
        PRINTLOG(KERNEL, LOG_DEBUG, "Default page table switched to 0x%p", p4);
    }

    if(SYSTEM_INFO->remapped == 0) {
        linker_remap_kernel();
    }

    PRINTLOG(KERNEL, LOG_DEBUG, "vfb address 0x%p", SYSTEM_INFO->frame_buffer);
    PRINTLOG(KERNEL, LOG_DEBUG, "Frame buffer at 0x%llx and size 0x%016llx", SYSTEM_INFO->frame_buffer->virtual_base_address, SYSTEM_INFO->frame_buffer->buffer_size);
    PRINTLOG(KERNEL, LOG_DEBUG, "Screen resultion %ix%i", SYSTEM_INFO->frame_buffer->width, SYSTEM_INFO->frame_buffer->height);

    if(SYSTEM_INFO->acpi_table) {
        PRINTLOG(ACPI, LOG_DEBUG, "acpi rsdp table version: %lli address 0x%p", SYSTEM_INFO->acpi_version, SYSTEM_INFO->acpi_table);
    }

    const char_t* test_data = "çok güzel bir kış ayı İĞÜŞÖÇ ığüşöç";
    printf("address 0x%p %s\n", test_data, test_data);

    printf("random data 0x%x\n", rand());

    KERNEL_FRAME_ALLOCATOR->cleanup(KERNEL_FRAME_ALLOCATOR);

    LOGBLOCK(FRAMEALLOCATOR, LOG_DEBUG){
        frame_allocator_print(KERNEL_FRAME_ALLOCATOR);
    }

    frame_allocator_map_page_of_acpi_code_data_frames(KERNEL_FRAME_ALLOCATOR);

    acpi_xrsdp_descriptor_t* desc = acpi_find_xrsdp();

    if(desc == NULL) {
        PRINTLOG(KERNEL, LOG_FATAL, "acpi header not found or incorrect checksum. Halting...");
        cpu_hlt();
    }

    if(acpi_setup(desc) != 0) {
        PRINTLOG(KERNEL, LOG_FATAL, "acpi setup failed. Halting");
        cpu_hlt();
    }

    if(pci_setup(NULL) != 0) {
        PRINTLOG(KERNEL, LOG_FATAL, "pci setup failed. Halting");
        cpu_hlt();
    }

    if(apic_setup(ACPI_CONTEXT->xrsdp_desc) != 0) {
        PRINTLOG(KERNEL, LOG_FATAL, "apic setup failed. Halting");
        cpu_hlt();
    }

    if(acpi_setup_events() != 0) {
        PRINTLOG(KERNEL, LOG_FATAL, "cannot setup acpi events");
        cpu_hlt();
    }

    if(task_init_tasking_ext(heap) != 0) {
        PRINTLOG(KERNEL, LOG_FATAL, "cannot init tasking. Halting...");
        cpu_hlt();
    }

    PRINTLOG(KERNEL, LOG_INFO, "tasking initialized");

    if(hpet_init() != 0) {
        PRINTLOG(KERNEL, LOG_FATAL, "cannot init hpet. Halting...");
        cpu_hlt();
    }

    if(video_display_init(NULL, PCI_CONTEXT->display_controllers) != 0) {
        PRINTLOG(KERNEL, LOG_FATAL, "cannot init video display. Halting...");
        cpu_hlt();
    }

    if(smp_init() != 0) {
        PRINTLOG(KERNEL, LOG_FATAL, "cannot init smp. Halting...");
        cpu_hlt();
    }

    PRINTLOG(KERNEL, LOG_INFO, "Initializing ahci and nvme");
    int8_t sata_port_cnt = ahci_init(heap, PCI_CONTEXT->sata_controllers);
    int8_t nvme_port_cnt = nvme_init(heap, PCI_CONTEXT->nvme_controllers);

    if(sata_port_cnt == -1) {
        PRINTLOG(KERNEL, LOG_FATAL, "cannot init ahci. Halting...");
        cpu_hlt();
    }

    if(nvme_port_cnt == -1) {
        PRINTLOG(KERNEL, LOG_FATAL, "cannot init nvme. Halting...");
        cpu_hlt();
    }

    if(network_init() != 0) {
        PRINTLOG(KERNEL, LOG_FATAL, "cannot init network. Halting...");
        cpu_hlt();
    }

    PRINTLOG(KERNEL, LOG_INFO, "sata port count is %i", sata_port_cnt);

    ahci_sata_disk_t* sd = (ahci_sata_disk_t*)ahci_get_disk_by_id(0);

    if(sd) {
        PRINTLOG(KERNEL, LOG_DEBUG, "try to read sata disk 0x%p", sd);
        disk_t* sata0 = gpt_get_or_create_gpt_disk(ahci_disk_impl_open(sd));

        if(sata0) {
            PRINTLOG(KERNEL, LOG_INFO, "sata disk size 0x%llx", sata0->disk.get_size((disk_or_partition_t*)sata0));

            disk_partition_context_t* part_ctx;

            part_ctx = sata0->get_partition_context(sata0, 0);
            PRINTLOG(KERNEL, LOG_INFO, "part 0 start lba 0x%llx end lba 0x%llx", part_ctx->start_lba, part_ctx->end_lba);
            memory_free(part_ctx);

            part_ctx = sata0->get_partition_context(sata0, 1);
            PRINTLOG(KERNEL, LOG_INFO, "part 1 start lba 0x%llx end lba 0x%llx", part_ctx->start_lba, part_ctx->end_lba);
            memory_free(part_ctx);

            sata0->disk.flush((disk_or_partition_t*)sata0);
        } else {
            PRINTLOG(KERNEL, LOG_INFO, "sata0 is empty");
        }

    } else {
        PRINTLOG(KERNEL, LOG_WARNING, "sata disk 0 not found");
    }

    PRINTLOG(KERNEL, LOG_INFO, "nvme port count is %i", nvme_port_cnt);

    nvme_disk_t* nd = (nvme_disk_t*)nvme_get_disk_by_id(0);

    if(nd) {
        PRINTLOG(KERNEL, LOG_DEBUG, "try to read nvme disk 0x%p", nd);
        disk_t* nvme0 = gpt_get_or_create_gpt_disk(nvme_disk_impl_open(nd));

        if(nvme0) {
            PRINTLOG(KERNEL, LOG_INFO, "nvme disk size 0x%llx", nvme0->disk.get_size((disk_or_partition_t*)nvme0));

            disk_partition_context_t* part_ctx;

            part_ctx = nvme0->get_partition_context(nvme0, 0);
            PRINTLOG(KERNEL, LOG_INFO, "part 0 start lba 0x%llx end lba 0x%llx", part_ctx->start_lba, part_ctx->end_lba);
            memory_free(part_ctx);

            part_ctx = nvme0->get_partition_context(nvme0, 1);
            PRINTLOG(KERNEL, LOG_INFO, "part 1 start lba 0x%llx end lba 0x%llx", part_ctx->start_lba, part_ctx->end_lba);
            memory_free(part_ctx);

            nvme0->disk.flush((disk_or_partition_t*)nvme0);
        } else {
            PRINTLOG(KERNEL, LOG_INFO, "nvme0 is empty");
        }

    } else {
        PRINTLOG(KERNEL, LOG_WARNING, "nvme disk 0 not found");
    }

    if(kbd_init() != 0) {
        PRINTLOG(KERNEL, LOG_FATAL, "cannot init keyboard. Halting...");
        cpu_hlt();
    }

    PRINTLOG(KERNEL, LOG_INFO, "rdrand %i", cpu_check_rdrand());

    PRINTLOG(KERNEL, LOG_INFO, "system table %p %li %li", SYSTEM_INFO->efi_system_table, sizeof(efi_system_table_t), sizeof(efi_table_header_t));

    wchar_t* var_name_boot_current = char_to_wchar("BootCurrent");
    efi_guid_t var_global = EFI_GLOBAL_VARIABLE;
    uint32_t var_attrs = 0;
    uint64_t buffer_size = sizeof(uint16_t);
    uint16_t boot_order_idx;

    efi_runtime_services_t * RS = SYSTEM_INFO->efi_system_table->runtime_services;

    efi_status_t res = RS->get_variable(var_name_boot_current, &var_global, &var_attrs, &buffer_size, &boot_order_idx);

    if(res != EFI_SUCCESS) {
        PRINTLOG(KERNEL, LOG_WARNING, "cannot get BootCurrent variable");
    } else {
        PRINTLOG(KERNEL, LOG_INFO, "BootCurrent %i", boot_order_idx);
    }

    PRINTLOG(KERNEL, LOG_INFO, "all services is up... :)");

    return 0;
}
#pragma GCC diagnostic pop

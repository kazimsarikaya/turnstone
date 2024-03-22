/**
 * @file kmain.64.c
 * @brief kernel main functions.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <types.h>
#include <helloworld.h>
#include <logging.h>
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
#include <list.h>
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
#include <time.h>
#include <network.h>
#include <crc.h>
#include <device/hpet.h>
#include <windowmanager.h>
#include <driver/usb.h>
#include <driver/usb_mass_storage_disk.h>
#include <stdbufs.h>
#include <backtrace.h>
#include <debug.h>
#include <cpu/syscall.h>
#include <hypervisor/hypervisor.h>
#include <tosdb/tosdb_manager.h>

MODULE("turnstone.kernel.programs.kmain");

int8_t                         kmain64(size_t entry_point);
__attribute__((noreturn)) void ___kstart64(system_info_t* sysinfo);

#if 0
// code for generating linker trampoline. not neccecery for now.
void trampoline_code(system_info_t* sysinfo);
void trampoline_code(system_info_t* sysinfo) {
    program_header_t* kernel = (program_header_t*)sysinfo->program_header_physical_start;
    memory_page_table_context_t* kernel_page_table = (memory_page_table_context_t*)kernel->page_table_context_address;

    uint64_t stack_top = kernel->program_stack_virtual_address + kernel->program_stack_size;
    cpu_set_and_clear_stack(stack_top);

    asm volatile ("mov %0, %%cr3" : : "r" (kernel_page_table->page_table) : "memory");
    asm volatile ("call *%0" : : "r" (kernel->program_entry) : "memory");
}
#endif

volatile boolean_t kmain64_completed = false;

__attribute__((noreturn)) void  ___kstart64(system_info_t* sysinfo) {
    cpu_cli();

    SYSTEM_INFO = sysinfo;

    outb(0xa1, 0xff);
    outb(0x21, 0xff);
    cpu_nop();
    cpu_nop();

    cpu_clear_segments();

    cpu_enable_sse();

    cpu_cld();

    int8_t res = 0;

#ifndef ___TESTMODE
    res = kmain64((size_t)&___kstart64);
#else
    res = kmain64_test((size_t)&___kstart64);
#endif

    kmain64_completed = true;

    if(res != 0) {
        PRINTLOG(KERNEL, LOG_FATAL, "kmain64 returned with error %i", res);
        while(true){
            cpu_hlt();
        }
    }

    task_end_task();

    while(true){ // should not reach here
        cpu_hlt();
    }
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
int8_t kmain64(size_t entry_point) {
    crc32_init_table();

    memory_heap_t* heap = memory_create_heap_hash(0, 0);

    if(heap == NULL) {
        PRINTLOG(KERNEL, LOG_FATAL, "creating heap");
        return -1;
    }

    memory_set_default_heap(heap);

    srand(SYSTEM_INFO->random_seed);

    stdbufs_init_buffers(video_print);

    video_init();
    video_clear_screen();

    PRINTLOG(KERNEL, LOG_INFO, "Initializing TURNSTONE OS");
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

    system_info_t* new_system_info = memory_malloc(sizeof(system_info_t));

    if(new_system_info == NULL) {
        return -1;
    }

    memory_memcopy(SYSTEM_INFO, new_system_info, sizeof(system_info_t));

    new_system_info->frame_buffer = new_vfb;
    new_system_info->mmap_data = new_mmap_data;

    SYSTEM_INFO = new_system_info;

    PRINTLOG(KERNEL, LOG_DEBUG, "new system info created at 0x%p", SYSTEM_INFO);

    if(backtrace_init() != 0) {
        PRINTLOG(KERNEL, LOG_FATAL, "cannot init backtrace. Halting...");
        cpu_hlt();
    }

    linker_build_modules_at_memory();

    if(debug_init() != 0) {
        PRINTLOG(KERNEL, LOG_FATAL, "cannot init debug. Halting...");
        cpu_hlt();
    }

    PRINTLOG(KERNEL, LOG_DEBUG, "frame allocator is initializing");

    frame_allocator_t* fa = frame_allocator_new_ext(heap);

    if(fa) {
        PRINTLOG(KERNEL, LOG_DEBUG, "frame allocator created");
        frame_set_allocator(fa);

        program_header_t* kernel = (program_header_t*)SYSTEM_INFO->program_header_virtual_start;

        PRINTLOG(KERNEL, LOG_DEBUG, "kernel program header is at 0x%p", kernel);

        frame_t kernel_frames = {SYSTEM_INFO->program_header_physical_start, kernel->total_size / FRAME_SIZE, FRAME_TYPE_USED, 0};

        if(fa->allocate_frame(fa, &kernel_frames) != 0) {
            PRINTLOG(KERNEL, LOG_PANIC, "cannot allocate kernel frames");
            cpu_hlt();
        }

        PRINTLOG(KERNEL, LOG_DEBUG, "kernel frames allocated");

        frame_t kernel_heap_frames = {kernel->program_heap_physical_address, kernel->program_heap_size / FRAME_SIZE, FRAME_TYPE_USED, 0};

        if(fa->allocate_frame(fa, &kernel_heap_frames) != 0) {
            PRINTLOG(KERNEL, LOG_PANIC, "cannot allocate kernel default heap frames");
            cpu_hlt();
        }

        PRINTLOG(KERNEL, LOG_DEBUG, "kernel heap frames allocated");

        frame_t kernel_stack_frames = {kernel->program_stack_physical_address, kernel->program_stack_size / FRAME_SIZE, FRAME_TYPE_USED, 0};

        if(fa->allocate_frame(fa, &kernel_stack_frames) != 0) {
            PRINTLOG(KERNEL, LOG_PANIC, "cannot allocate kernel default stack frames");
            cpu_hlt();
        }

        PRINTLOG(KERNEL, LOG_DEBUG, "kernel stack frames allocated");

        if(memory_paging_reserve_current_page_table_frames() != 0) {
            PRINTLOG(KERNEL, LOG_PANIC, "cannot reserve current page table frames");
            cpu_hlt();
        }

        if(SYSTEM_INFO->boot_type == SYSTEM_INFO_BOOT_TYPE_PXE) {
            PRINTLOG(KERNEL, LOG_DEBUG, "boot type is pxe, need to allocate system used frames");

            frame_t tosdb_frames = {SYSTEM_INFO->pxe_tosdb_address, SYSTEM_INFO->pxe_tosdb_size / FRAME_SIZE, FRAME_TYPE_USED, 0};

            if(fa->allocate_frame(fa, &tosdb_frames) != 0) {
                PRINTLOG(KERNEL, LOG_PANIC, "cannot allocate tosdb frames");
                cpu_hlt();
            }
        }

        PRINTLOG(KERNEL, LOG_DEBUG, "system used frames allocated");
    } else {
        PRINTLOG(KERNEL, LOG_PANIC, "cannot allocate frame allocator. Halting...");
        cpu_hlt();
    }

    PRINTLOG(KERNEL, LOG_DEBUG, "frame allocator initialized");

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

    PRINTLOG(KERNEL, LOG_DEBUG, "vfb address 0x%p", SYSTEM_INFO->frame_buffer);
    PRINTLOG(KERNEL, LOG_DEBUG, "Frame buffer at 0x%llx and size 0x%016llx", SYSTEM_INFO->frame_buffer->virtual_base_address, SYSTEM_INFO->frame_buffer->buffer_size);
    PRINTLOG(KERNEL, LOG_DEBUG, "Screen resultion %ix%i", SYSTEM_INFO->frame_buffer->width, SYSTEM_INFO->frame_buffer->height);

    if(SYSTEM_INFO->acpi_table) {
        PRINTLOG(ACPI, LOG_DEBUG, "acpi rsdp table version: %lli address 0x%p", SYSTEM_INFO->acpi_version, SYSTEM_INFO->acpi_table);
    }

    const char_t* test_data = "çok güzel bir kış ayı İĞÜŞÖÇ ığüşöç";
    printf("address 0x%p %s\n", test_data, test_data);

    printf("random data 0x%x\n", rand());

    frame_get_allocator()->cleanup(frame_get_allocator());

    LOGBLOCK(FRAMEALLOCATOR, LOG_DEBUG){
        frame_allocator_print(frame_get_allocator());
    }

    PRINTLOG(KERNEL, LOG_DEBUG, "acpi is initializing");

    frame_allocator_map_page_of_acpi_code_data_frames(frame_get_allocator());

    acpi_xrsdp_descriptor_t* desc = acpi_find_xrsdp();

    if(desc == NULL) {
        PRINTLOG(KERNEL, LOG_FATAL, "acpi header not found or incorrect checksum. Halting...");
        cpu_hlt();
    }

    PRINTLOG(KERNEL, LOG_DEBUG, "acpi header found at 0x%p, acpi data will initialized", desc);

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

    PRINTLOG(KERNEL, LOG_DEBUG, "acpi is initialized");

    PRINTLOG(KERNEL, LOG_DEBUG, "tasking is initializing");

    syscall_init();

    if(hypervisor_init() != 0) {
        PRINTLOG(KERNEL, LOG_ERROR, "cannot init hypervisor.");
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

    if(video_display_init(NULL, pci_get_context()->display_controllers) != 0) {
        PRINTLOG(KERNEL, LOG_FATAL, "cannot init video display. Halting...");
        cpu_hlt();
    }

    if(smp_init() != 0) {
        PRINTLOG(KERNEL, LOG_FATAL, "cannot init smp. Halting...");
        cpu_hlt();
    }

    PRINTLOG(KERNEL, LOG_INFO, "Initializing usb");
    if(usb_init() != 0) {
        PRINTLOG(KERNEL, LOG_FATAL, "cannot init usb. Halting...");
        cpu_hlt();
    }

    if(tosdb_manager_init() != 0) {
        PRINTLOG(KERNEL, LOG_FATAL, "cannot init tosdb manager. Halting...");
        cpu_hlt();
    }

    if(network_init() != 0) {
        PRINTLOG(KERNEL, LOG_FATAL, "cannot init network. Halting...");
        cpu_hlt();
    }

    if(usb_mass_storage_get_disk_count()) {
        usb_driver_t* usb_ms = usb_mass_storage_get_disk_by_id(0);

        if(usb_ms) {
            disk_t* usb0 = gpt_get_or_create_gpt_disk(usb_mass_storage_disk_impl_open(usb_ms, 0));

            if(usb0) {
                PRINTLOG(KERNEL, LOG_INFO, "usb disk size 0x%llx", usb0->disk.get_size((disk_or_partition_t*)usb0));

                disk_partition_context_t* part_ctx;

                part_ctx = usb0->get_partition_context(usb0, 0);
                PRINTLOG(KERNEL, LOG_INFO, "part 0 start lba 0x%llx end lba 0x%llx", part_ctx->start_lba, part_ctx->end_lba);
                memory_free(part_ctx);

                part_ctx = usb0->get_partition_context(usb0, 1);
                PRINTLOG(KERNEL, LOG_INFO, "part 1 start lba 0x%llx end lba 0x%llx", part_ctx->start_lba, part_ctx->end_lba);
                memory_free(part_ctx);

                usb0->disk.flush((disk_or_partition_t*)usb0);
            } else {
                PRINTLOG(KERNEL, LOG_INFO, "usb0 is empty");
            }
        } else {
            PRINTLOG(KERNEL, LOG_WARNING, "usb mass storage disk 0 not found");
        }
    }

    if(windowmanager_init() != 0) {
        PRINTLOG(KERNEL, LOG_FATAL, "cannot init window manager. Halting...");
        cpu_hlt();
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

    PRINTLOG(KERNEL, LOG_INFO, "smbios version 0x%llx smbios data address 0x%p", SYSTEM_INFO->smbios_version, SYSTEM_INFO->smbios_table);

    PRINTLOG(KERNEL, LOG_INFO, "current time %lli", time_ns(NULL));

    PRINTLOG(KERNEL, LOG_INFO, "all services is up... :)");

    return 0;
}
#pragma GCC diagnostic pop

/**
 * @file vmedu.64.c
 * @brief VM EDU PCI(e) Mapping Test Program
 *
 * This is a sample program that will run in the VM.
 * It has minimal dependencies and is used to test the VM.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */


#include <types.h>
#include <hypervisor/hypervisor_guestlib.h>
#include <cpu.h>
#include <ports.h>
#include <buffer.h>
#include <stdbufs.h>
#include <pci.h>
#include <memory/paging.h>
#include <driver/edu.h>
#include <strings.h>

MODULE("turnstone.user.programs.vmedu");

_Noreturn void vmedu(void);

static volatile edu_t* edu = NULL;

static volatile boolean_t isr_done = false;

static void edu_isr(interrupt_frame_ext_t* frame) {
    UNUSED(frame);
    printf("EDU ISR 0x%x\n", edu->interrupt_status);
    edu->interrupt_acknowledge = edu->interrupt_status;
    isr_done = true;
    printf("EDU ISR done 0x%x\n", edu->interrupt_status);
    vm_guest_apic_eoi();
}

static uint64_t edu_timer_tick = 0;
static volatile uint64_t edu_timer_isr_test_end = 0;

static void edu_timer_isr(interrupt_frame_ext_t* frame) {
    UNUSED(frame);

    edu_timer_tick++;

    if(edu_timer_tick == 1000) {
        printf("EDU Timer ISR\n");
        edu_timer_tick = 0;
        edu_timer_isr_test_end++;
    }

    vm_guest_apic_eoi();
}

_Noreturn void vmedu(void) {
    vm_guest_print("VM EDU Passthrough Test Program\n");
    uint64_t heap_base = 4ULL << 40;
    uint64_t heap_size = 16ULL << 20;
    memory_heap_t* heap = memory_create_heap_simple(heap_base, heap_base + heap_size);

    if(heap == NULL) {
        vm_guest_print("Failed to create heap\n");
        vm_guest_halt();
    }

    memory_set_default_heap(heap);

    stdbufs_init_buffers(vm_guest_print);

    printf("base init done\n");

    vm_guest_enable_timer(edu_timer_isr, 1000, 1);

    uint64_t hpa = vm_guest_get_host_physical_address((uint64_t)heap);

    printf("Heap HPA: 0x%llx\n", hpa);

    if(hpa == -1ULL) {
        printf("hpa failed\n");
        vm_guest_exit(-1);
    }

    uint64_t edu_pci_va = vm_guest_attach_pci_dev(0, 0, 5, 0);

    printf("EDU PCI VA: 0x%llx\n", edu_pci_va);

    if(edu_pci_va == -1ULL) {
        printf("pci attach failed\n");
        vm_guest_exit(-1);
    }

    pci_generic_device_t* edu_pci_dev = (pci_generic_device_t*)edu_pci_va;

    printf("getting bar address\n");
    uint64_t edu_pci_bar0_addr = pci_get_bar_address(edu_pci_dev, 0);
    printf("EDU PCI BAR0 Address: 0x%llx\n", edu_pci_bar0_addr);

    uint64_t edu_pci_bar0_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(edu_pci_bar0_addr);

    printf("EDU PCI BAR0 VA: 0x%llx\n", edu_pci_bar0_va);

    edu = (edu_t*)edu_pci_bar0_va;

    printf("setting up interrupt\n");
    vm_guest_attach_interrupt(edu_pci_dev, VM_GUEST_INTERRUPT_TYPE_MSI, 0, edu_isr);
    printf("interrupt setup finished\n");

    printf("edu identification 0x%x\n", edu->identification);

    edu->factorial_computation = 5;
    edu->status = 1;

    while(edu->status & 1) {
        asm volatile ("pause");
    }

    printf("Factorial of 5 is: %d\n", edu->factorial_computation);

    char_t* source = memory_malloc_ext(NULL, 0x1000, 0x1000);
    char_t* dest = memory_malloc_ext(NULL, 0x1000, 0x1000);

    uint64_t source_hpa = vm_guest_get_host_physical_address((uint64_t)source);
    uint64_t dest_hpa = vm_guest_get_host_physical_address((uint64_t)dest);

    printf("Source HPA: 0x%llx\n", source_hpa);
    printf("Dest HPA: 0x%llx\n", dest_hpa);

    const char_t* str = "this is a test of dma transfer";

    strcopy(str, source);

    printf("DMA transfer starting\n");
    edu->dma_source = source_hpa;
    edu->dma_destination = EDU_DMA_TRANSFER_ADDRESS;
    edu->dma_length = strlen(str) + 1;
    edu->dma_command = 1;

    while(edu->dma_command & 1) {
        asm volatile ("pause");
    }

    edu->dma_source = EDU_DMA_TRANSFER_ADDRESS;
    edu->dma_destination = dest_hpa;
    edu->dma_length = strlen(str) + 1;
    edu->dma_command = 3;

    while(edu->dma_command & 1) {
        asm volatile ("pause");
    }

    printf("DMA transfer complete\n");

    printf("Source: %s\n", source);
    printf("Destination: %s\n", dest);

    printf("src and dest match: %d\n", strcmp(source, dest) == 0);

    memory_free(source);
    memory_free(dest);

    cpu_sti();

    uint32_t interrupt_status = edu->interrupt_status;

    printf("interrupt status before: 0x%x\n", interrupt_status);

    isr_done = false;

    edu->interrupt_raise = 0x5A5A5A5A;

    while(!isr_done) {
        cpu_idle();
    }

    interrupt_status = edu->interrupt_status;

    printf("interrupt status after: 0x%x\n", interrupt_status);

    if(interrupt_status) {
        printf("interrupt not lowered\n");
        vm_guest_exit(-1);
    }

    interrupt_status = edu->interrupt_status;

    printf("interrupt status before: 0x%x\n", interrupt_status);

    isr_done = false;

    edu->interrupt_raise = 0x12345678;

    while(!isr_done) {
        cpu_idle();
    }

    interrupt_status = edu->interrupt_status;

    printf("interrupt status after: 0x%x\n", interrupt_status);

    if(interrupt_status) {
        printf("interrupt not lowered\n");
        vm_guest_exit(-1);
    }

    while(edu_timer_isr_test_end < 10) {
        cpu_idle();
    }

    printf("Test program complete\n");

    vm_guest_exit(0);

}

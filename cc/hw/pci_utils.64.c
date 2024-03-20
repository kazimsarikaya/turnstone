/**
 * @file pci_utils.64.c
 * @brief PCI utility functions for 64-bit mode
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <types.h>
#include <pci.h>
#include <logging.h>
#include <memory/paging.h>
#include <memory/frame.h>
#include <utils.h>
#include <ports.h>
#include <apic.h>

MODULE("turnstone.kernel.hw.pci.utils");

int8_t pci_msix_configure(pci_generic_device_t* pci_gen_dev, pci_capability_msix_t* msix_cap) {

    msix_cap->enable = 1;
    msix_cap->function_mask = 0;

    PRINTLOG(PCI, LOG_TRACE, "device has msix cap enabled %i fmask %i", msix_cap->enable, msix_cap->function_mask);
    PRINTLOG(PCI, LOG_TRACE, "msix bir %i tables offset 0x%x  size 0x%x", msix_cap->bir, msix_cap->table_offset, msix_cap->table_size + 1);
    PRINTLOG(PCI, LOG_TRACE, "msix pending bit bir %i tables offset 0x%x", msix_cap->pending_bit_bir, msix_cap->pending_bit_offset);

    uint64_t bar_fa = 0;
    uint64_t bar_size = 0;
    uint64_t bar_va = 0;

    bar_fa = pci_get_bar_address(pci_gen_dev, msix_cap->bir);
    bar_size = pci_get_bar_size(pci_gen_dev, msix_cap->bir);
    bar_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(bar_fa);

    frame_t* bar_frames = frame_get_allocator()->get_reserved_frames_of_address(frame_get_allocator(), (void*)bar_fa);

    uint64_t bar_frm_cnt = (bar_size + FRAME_SIZE - 1) / FRAME_SIZE;
    frame_t bar_req_frm = {bar_fa, bar_frm_cnt, FRAME_TYPE_RESERVED, 0};

    if(bar_frames == NULL) {
        PRINTLOG(PCI, LOG_TRACE, "cannot find reserved frames for 0x%llx and try to reserve", bar_fa);

        if(frame_get_allocator()->allocate_frame(frame_get_allocator(), &bar_req_frm) != 0) {
            PRINTLOG(PCI, LOG_ERROR, "cannot allocate frame");

            return -1;
        }
    }

    memory_paging_add_va_for_frame(bar_va, &bar_req_frm, MEMORY_PAGING_PAGE_TYPE_NOEXEC);

    if(msix_cap->bir != msix_cap->pending_bit_bir) {
        bar_fa = pci_get_bar_address(pci_gen_dev, msix_cap->pending_bit_bir);
        bar_size = pci_get_bar_size(pci_gen_dev, msix_cap->pending_bit_bir);
        bar_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(bar_fa);

        bar_frames = frame_get_allocator()->get_reserved_frames_of_address(frame_get_allocator(), (void*)bar_fa);

        bar_frm_cnt = (bar_size + FRAME_SIZE - 1) / FRAME_SIZE;
        bar_req_frm.frame_address = bar_fa;
        bar_req_frm.frame_count = bar_frm_cnt;

        if(bar_frames == NULL) {
            PRINTLOG(PCI, LOG_TRACE, "cannot find reserved frames for 0x%llx and try to reserve", bar_fa);

            if(frame_get_allocator()->allocate_frame(frame_get_allocator(), &bar_req_frm) != 0) {
                PRINTLOG(PCI, LOG_ERROR, "cannot allocate frame");

                return -1;
            }
        }

        memory_paging_add_va_for_frame(bar_va, &bar_req_frm, MEMORY_PAGING_PAGE_TYPE_NOEXEC);
    }

    return 0;
}

uint8_t pci_msix_set_isr(pci_generic_device_t* pci_dev, pci_capability_msix_t* msix_cap, uint16_t msix_vector, interrupt_irq isr) {
    uint64_t msix_table_address = pci_get_bar_address(pci_dev, msix_cap->bir);

    msix_table_address += (msix_cap->table_offset << 3);

    pci_capability_msix_table_t* msix_table = (pci_capability_msix_table_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(msix_table_address);

    uint32_t msg_addr = 0xFEE00000;
    uint32_t apic_id = apic_get_local_apic_id();
    apic_id <<= 12;
    msg_addr |= apic_id;

    uint8_t intnum = 0;

    if(!msix_table->entries[msix_vector].message_data) {
        intnum = interrupt_get_next_empty_interrupt();
        msix_table->entries[msix_vector].message_data = intnum;
    } else {
        intnum = msix_table->entries[msix_vector].message_data;
    }

    msix_table->entries[msix_vector].message_address = msg_addr;
    msix_table->entries[msix_vector].masked = 0;

    uint8_t isrnum = intnum - INTERRUPT_IRQ_BASE;
    interrupt_irq_set_handler(isrnum, isr);

    PRINTLOG(PCI, LOG_TRACE, "msixcap %p intnum 0x%x isrnum 0x%x", msix_cap, intnum, isrnum);
    PRINTLOG(PCI, LOG_TRACE, "msix table %p offset %x vector 0x%x int 0x%02x", msix_table, msix_cap->table_offset, msix_vector,  msix_table->entries[msix_vector].message_data);

    return isrnum;
}

uint8_t pci_msix_update_lapic(pci_generic_device_t* pci_dev, pci_capability_msix_t* msix_cap, uint16_t msix_vector) {
    uint64_t msix_table_address = pci_get_bar_address(pci_dev, msix_cap->bir);

    msix_table_address += (msix_cap->table_offset << 3);

    pci_capability_msix_table_t* msix_table = (pci_capability_msix_table_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(msix_table_address);

    uint32_t msg_addr = msix_table->entries[msix_vector].message_address;
    msg_addr &= 0xFFF00FFF;
    uint32_t apic_id = apic_get_local_apic_id();
    apic_id <<= 12;
    msg_addr |= apic_id;

    msix_table->entries[msix_vector].message_address = msg_addr;

    return 0;
}

int8_t pci_msix_clear_pending_bit(pci_generic_device_t* pci_dev, pci_capability_msix_t* msix_cap, uint16_t msix_vector) {
    uint64_t msix_pendind_bit_table_address = pci_get_bar_address(pci_dev, msix_cap->pending_bit_bir);

    msix_pendind_bit_table_address += (msix_cap->pending_bit_offset << 3);

    PRINTLOG(PCI, LOG_TRACE, "msix pending bit table address 0x%llx", msix_pendind_bit_table_address);

    uint64_t* pending_bit_table = (uint64_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(msix_pendind_bit_table_address);

    uint32_t tbl_idx = msix_vector / 64;
    uint32_t bit_idx = msix_vector % 64;

    uint64_t* pending_bit_table_entry = pending_bit_table + tbl_idx;

    // clear pending bit
    *pending_bit_table_entry &= ~(1ULL << bit_idx);

    PRINTLOG(PCI, LOG_TRACE, "pending bit cleared %i 0x%llx",  msix_vector, *pending_bit_table_entry);

    return 0;
}

uint64_t pci_get_bar_size(pci_generic_device_t* pci_dev, uint8_t bar_no){
    uint64_t old_address = pci_get_bar_address(pci_dev, bar_no);
    uint64_t mask = -1ULL;
    pci_set_bar_address(pci_dev, bar_no, mask);

    uint64_t size = pci_get_bar_address(pci_dev, bar_no);

    pci_set_bar_address(pci_dev, bar_no, old_address);

    size = ~size + 1;

    pci_bar_register_t* bar = &pci_dev->bar0;
    bar += bar_no;

    if(bar->memory_space_bar.type != 2) {
        size &= 0xFFFFFFFF;
    }

    PRINTLOG(PCI, LOG_TRACE, "bar %i size 0x%llx",  bar_no, size);

    return size;
}

uint64_t pci_get_bar_address(pci_generic_device_t* pci_dev, uint8_t bar_no){
    pci_bar_register_t* bar = &pci_dev->bar0;
    bar += bar_no;

    uint64_t bar_fa = 0;

    if(bar->bar_type.type == 0) {
        bar_fa = (uint64_t)bar->memory_space_bar.base_address;
        bar_fa <<= 4;

        if(bar->memory_space_bar.type == 2) {
            bar++;
            uint64_t tmp = (uint64_t)(*((uint32_t*)bar));
            bar_fa = tmp << 32 | bar_fa;
        }

    } else {
        bar_fa = (uint64_t)bar->io_space_bar.base_address;
        bar_fa <<= 2;
    }

    PRINTLOG(PCI, LOG_TRACE, "get bar %i address 0x%llx",  bar_no, bar_fa);

    return bar_fa;
}

int8_t pci_set_bar_address(pci_generic_device_t* pci_dev, uint8_t bar_no, uint64_t bar_fa){
    pci_bar_register_t* bar = &pci_dev->bar0;
    bar += bar_no;

    PRINTLOG(PCI, LOG_TRACE, "set bar %i address 0x%llx",  bar_no, bar_fa);

    if(bar->bar_type.type == 0) {
        bar->memory_space_bar.base_address = (bar_fa >> 4) & 0xFFFFFFF;

        if(bar->memory_space_bar.type == 2) {
            bar++;

            (*(uint32_t*)bar) = bar_fa >> 32;
        }

    } else {
        bar->io_space_bar.base_address = (bar_fa >> 2) & 0xFFFFFFF;
    }

    return 0;
}

void pci_disable_interrupt(pci_generic_device_t* pci_dev) {
    if(pci_dev == NULL) {
        return;
    }

    uint64_t dest = ((uint64_t)pci_dev) + 4;

    uint32_t value = read_memio(dest, 32);
    PRINTLOG(PCI, LOG_TRACE, "interrupt disable 0x%p 0x%llx: 0x%x 0x%x", pci_dev, dest, value, value | (1 << 10));
    value |= (1 << 10);
    write_memio(dest, 32, value);

    for(int32_t i = 0; i < 1000; i++) {
        asm volatile ("pause");

        value = read_memio(dest, 32);

        if((value & (1 << 10)) == (1 << 10)) {
            break;
        }
    }

    if((value & (1 << 10)) == 0) {
        PRINTLOG(PCI, LOG_WARNING, "interrupt disable failed for 0x%p 0x%llx: 0x%x", pci_dev, dest, value);
    }
}

void pci_enable_interrupt(pci_generic_device_t* pci_dev) {
    if(pci_dev == NULL) {
        return;
    }

    uint64_t dest = ((uint64_t)pci_dev) + 4;

    // only change bit 10
    uint32_t value = read_memio(dest, 32);
    PRINTLOG(PCI, LOG_TRACE, "interrupt enable 0x%p 0x%llx: 0x%x 0x%x", pci_dev, dest, value, value & ~(1 << 10));
    value &= ~(1 << 10);
    write_memio(((uint64_t)pci_dev) + 4, 32, value);

    for(int32_t i = 0; i < 1000; i++) {
        asm volatile ("pause");

        value = read_memio(dest, 32);

        if((value & (1 << 10)) == 0) {
            break;
        }
    }

    if((value & (1 << 10)) != 0) {
        PRINTLOG(PCI, LOG_ERROR, "interrupt enable failed for 0x%p 0x%llx: 0x%x", pci_dev, dest, value);
    }
}

int8_t pci_io_port_write_data(uint32_t address, uint32_t data, uint8_t bc) {
    outl(address, PCI_IO_PORT_CONFIG);

    switch (bc) {
    case 1:
        outb(PCI_IO_PORT_DATA, data & 0xFF);
        break;
    case 2:
        outw(PCI_IO_PORT_DATA, data & 0xFFFF);
        break;
    case 4:
        outl(PCI_IO_PORT_DATA, data);
        break;
    }

    return 0;
}

uint32_t pci_io_port_read_data(uint32_t address, uint8_t bc) {
    outl(address, PCI_IO_PORT_CONFIG);

    uint32_t res = 0;

    switch (bc) {
    case 1:
        res = inb(PCI_IO_PORT_DATA);
        break;
    case 2:
        res = inw(PCI_IO_PORT_DATA);
        break;
    case 4:
        res = inl(PCI_IO_PORT_DATA);
        break;
    }

    return res;
}

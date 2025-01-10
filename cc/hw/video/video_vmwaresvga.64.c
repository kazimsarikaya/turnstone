/**
 * @file video_vmwaresvga.64.c
 * @brief VMware SVGA video driver
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */


#include <driver/video_vmwaresvga.h>
#include <memory/frame.h>
#include <memory/paging.h>
#include <ports.h>
#include <apic.h>
#include <cpu/interrupt.h>
#include <efi.h>
#include <systeminfo.h>
#include <driver/video_edid.h>
#include <cpu.h>
#include <logging.h>
#include <graphics/screen.h>

MODULE("turnstone.kernel.hw.video.vmwaresvga2");

vmware_svga2_t* vmware_svga2_active = NULL;

int8_t vmware_svga2_isr(interrupt_frame_ext_t* frame);
void   vmware_svga2_display_flush(uint32_t scanout, uint64_t offset, uint32_t x, uint32_t y, uint32_t width, uint32_t height);

int8_t vmware_svga2_isr(interrupt_frame_ext_t* frame) {
    UNUSED(frame);

    PRINTLOG(VMWARESVGA, LOG_DEBUG, "ISR");

    return 0;
}


static inline void vmware_svga2_write_reg(vmware_svga2_t* vmware_svga2, uint32_t index, uint32_t value) {
    outl(vmware_svga2->io_bar_addr + VMWARE_SVGA2_INDEX_PORT, index);
    outl(vmware_svga2->io_bar_addr + VMWARE_SVGA2_VALUE_PORT, value);
}

static inline uint32_t vmware_svga2_read_reg(vmware_svga2_t* vmware_svga2, uint32_t index) {
    outl(vmware_svga2->io_bar_addr + VMWARE_SVGA2_INDEX_PORT, index);

    return inl(vmware_svga2->io_bar_addr + VMWARE_SVGA2_VALUE_PORT);
}

uint32_t fill_color = 0x00000000;

void vmware_svga2_display_flush(uint32_t scanout, uint64_t offset, uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    UNUSED(scanout);
    UNUSED(offset);

    volatile uint32_t* fifo = (volatile uint32_t*) vmware_svga2_active->fifo_bar_addr_va;

    uint32_t next_cmd = fifo[VMWARE_SVGA2_FIFO_REG_NEXT_CMD];
    uint32_t stop = fifo[VMWARE_SVGA2_FIFO_REG_STOP];
    uint32_t min = fifo[VMWARE_SVGA2_FIFO_REG_MIN];
    uint32_t max = fifo[VMWARE_SVGA2_FIFO_REG_MAX];

    if(next_cmd + 5 * sizeof(uint32_t) > max) {
        do {
            vmware_svga2_write_reg(vmware_svga2_active, VMWARE_SVGA2_REG_SYNC, true);
            while(vmware_svga2_read_reg(vmware_svga2_active, VMWARE_SVGA2_REG_BUSY));

            next_cmd = fifo[VMWARE_SVGA2_FIFO_REG_NEXT_CMD];
            stop = fifo[VMWARE_SVGA2_FIFO_REG_STOP];
        } while(next_cmd != stop);

        vmware_svga2_write_reg(vmware_svga2_active, VMWARE_SVGA2_REG_ENABLE, false);
        fifo[VMWARE_SVGA2_FIFO_REG_NEXT_CMD] = min;
        fifo[VMWARE_SVGA2_FIFO_REG_STOP] = min;
        vmware_svga2_write_reg(vmware_svga2_active, VMWARE_SVGA2_REG_ENABLE, true);
    }

    volatile uint32_t* cmd = (volatile uint32_t*)fifo + fifo[VMWARE_SVGA2_FIFO_REG_NEXT_CMD] / sizeof(uint32_t);
    cmd[0] = VMWARE_SVGA2_CMD_UPDATE;
    cmd[1] = x;
    cmd[2] = y;
    cmd[3] = width;
    cmd[4] = height;

    fifo[VMWARE_SVGA2_FIFO_REG_NEXT_CMD] += 5 * sizeof(uint32_t);

    vmware_svga2_write_reg(vmware_svga2_active, VMWARE_SVGA2_REG_SYNC, true);
    while(vmware_svga2_read_reg(vmware_svga2_active, VMWARE_SVGA2_REG_BUSY));
}



int8_t vmware_svga2_init(memory_heap_t* heap, const pci_dev_t * dev) {
    UNUSED(heap);

    // logging_module_levels[VMWARESVGA] = LOG_TRACE;

    PRINTLOG(VMWARESVGA, LOG_DEBUG, "vmware svga2 initializing");

    efi_boot_services_t* BS = SYSTEM_INFO->efi_system_table->boot_services;

    efi_guid_t edid_guid = EFI_EDID_ACTIVE_PROTOCOL_GUID;

    efi_edid_discovered_protocol_t* edid_protocol = NULL;

    efi_status_t es = BS->locate_protocol(&edid_guid, NULL, (void**) &edid_protocol);

    if(es != EFI_SUCCESS && es != EFI_NOT_FOUND) {
        PRINTLOG(VMWARESVGA, LOG_ERROR, "Failed to locate EDID protocol: %llx", es);

        return -1;
    }

    pci_generic_device_t* pci_dev = (pci_generic_device_t*) dev->pci_header;

    uint16_t io_bar_addr = pci_get_bar_address(pci_dev, 0);

    uint64_t fb_bar_addr_fa = pci_get_bar_address(pci_dev, 1);
    uint64_t fb_bar_addr_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(fb_bar_addr_fa);
    uint64_t fb_bar_size = pci_get_bar_size(pci_dev, 1);
    uint64_t fb_bar_frm_cnt = (fb_bar_size + FRAME_SIZE - 1) / FRAME_SIZE;

    uint64_t fifo_bar_addr_fa = pci_get_bar_address(pci_dev, 2);
    uint64_t fifo_bar_addr_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(fifo_bar_addr_fa);
    uint64_t fifo_bar_size = pci_get_bar_size(pci_dev, 2);
    uint64_t fifo_bar_frm_cnt = (fifo_bar_size + FRAME_SIZE - 1) / FRAME_SIZE;


    frame_t fb_bar_frm = {
        .frame_address = fb_bar_addr_fa,
        .frame_count = fb_bar_frm_cnt,
        .type = FRAME_TYPE_RESERVED,
    };

    frame_get_allocator()->allocate_frame(frame_get_allocator(), &fb_bar_frm);
    memory_paging_add_va_for_frame(fb_bar_addr_va, &fb_bar_frm, MEMORY_PAGING_PAGE_TYPE_NOEXEC);

    frame_t fifo_bar_frm = {
        .frame_address = fifo_bar_addr_fa,
        .frame_count = fifo_bar_frm_cnt,
        .type = FRAME_TYPE_RESERVED,
    };

    frame_get_allocator()->allocate_frame(frame_get_allocator(), &fifo_bar_frm);
    memory_paging_add_va_for_frame(fifo_bar_addr_va, &fifo_bar_frm, MEMORY_PAGING_PAGE_TYPE_NOEXEC);


    vmware_svga2_t* vmware_svga2 = memory_malloc_ext(heap, sizeof(vmware_svga2_t), 0);

    if(!vmware_svga2) {
        PRINTLOG(VMWARESVGA, LOG_ERROR, "Failed to allocate memory for vmware_svga2_t");

        return -1;
    }

    vmware_svga2_active = vmware_svga2;

    vmware_svga2->io_bar_addr = io_bar_addr;
    vmware_svga2->fb_bar_addr_fa = fb_bar_addr_fa;
    vmware_svga2->fb_bar_addr_va = fb_bar_addr_va;
    vmware_svga2->fb_bar_size = fb_bar_size;
    vmware_svga2->fb_bar_frm_cnt = fb_bar_frm_cnt;
    vmware_svga2->fifo_bar_addr_fa = fifo_bar_addr_fa;
    vmware_svga2->fifo_bar_addr_va = fifo_bar_addr_va;
    vmware_svga2->fifo_bar_size = fifo_bar_size;
    vmware_svga2->fifo_bar_frm_cnt = fifo_bar_frm_cnt;


    vmware_svga2->version_id = VMWARE_SVGA2_ID_2;

    do {
        vmware_svga2_write_reg(vmware_svga2, VMWARE_SVGA2_REG_ID, vmware_svga2->version_id);

        if(vmware_svga2_read_reg(vmware_svga2, VMWARE_SVGA2_REG_ID) == vmware_svga2->version_id) {
            break;
        } else {
            vmware_svga2->version_id--;
        }

    } while(vmware_svga2_read_reg(vmware_svga2, VMWARE_SVGA2_REG_ID) >= VMWARE_SVGA2_ID_0);

    vmware_svga2->capabilities = vmware_svga2_read_reg(vmware_svga2, VMWARE_SVGA2_REG_CAPABILITIES);

    PRINTLOG(VMWARESVGA, LOG_DEBUG, "Version ID: 0x%llx", vmware_svga2->version_id);
    PRINTLOG(VMWARESVGA, LOG_DEBUG, "Capabilities: 0x%x", vmware_svga2->capabilities);

    if(vmware_svga2->capabilities & VMWARE_SVGA2_CAPABILITY_IRQMASK) {
        uint8_t irq = pci_dev->interrupt_line;
        irq = apic_get_irq_override(irq);

        interrupt_irq_set_handler(irq, vmware_svga2_isr);

        outl(vmware_svga2->io_bar_addr + VMWARE_SVGA2_REG_IRQMASK, 0);

        apic_ioapic_enable_irq(irq);
    }

    if(es != EFI_NOT_FOUND) {
        PRINTLOG(VMWARESVGA, LOG_INFO, "EDID protocol located. size: %d", edid_protocol->size_of_edid);
        video_edid_get_max_resolution(edid_protocol->edid, &vmware_svga2->screen_height, &vmware_svga2->screen_width);
    } else {
        vmware_svga2->screen_height = vmware_svga2_read_reg(vmware_svga2, VMWARE_SVGA2_REG_MAX_HEIGHT);
        vmware_svga2->screen_width = vmware_svga2_read_reg(vmware_svga2, VMWARE_SVGA2_REG_MAX_WIDTH);
        vmware_svga2->screen_height = vmware_svga2_read_reg(vmware_svga2, VMWARE_SVGA2_REG_HEIGHT);
        vmware_svga2->screen_width = vmware_svga2_read_reg(vmware_svga2, VMWARE_SVGA2_REG_WIDTH);

    }

    vmware_svga2_write_reg(vmware_svga2, VMWARE_SVGA2_REG_WIDTH, vmware_svga2->screen_width);
    vmware_svga2_write_reg(vmware_svga2, VMWARE_SVGA2_REG_HEIGHT, vmware_svga2->screen_height);

    PRINTLOG(VMWARESVGA, LOG_TRACE, "Max resolution: %dx%d", vmware_svga2->screen_width, vmware_svga2->screen_height);
    PRINTLOG(VMWARESVGA, LOG_TRACE, "Framebuffer address: 0x%x", vmware_svga2_read_reg(vmware_svga2, VMWARE_SVGA2_REG_FB_START));
    PRINTLOG(VMWARESVGA, LOG_TRACE, "Framebuffer size: 0x%x", vmware_svga2_read_reg(vmware_svga2, VMWARE_SVGA2_REG_FB_SIZE));
    PRINTLOG(VMWARESVGA, LOG_TRACE, "Framebuffer offset: 0x%x", vmware_svga2_read_reg(vmware_svga2, VMWARE_SVGA2_REG_FB_OFFSET));

    volatile uint32_t* fifo = (volatile uint32_t*) vmware_svga2->fifo_bar_addr_va;

    fifo[VMWARE_SVGA2_FIFO_REG_MIN] = VMWARE_SVGA2_FIFO_REG_NUM_REGS * sizeof(uint32_t);
    fifo[VMWARE_SVGA2_FIFO_REG_MAX] = vmware_svga2->fifo_bar_size;
    fifo[VMWARE_SVGA2_FIFO_REG_NEXT_CMD] = fifo[VMWARE_SVGA2_FIFO_REG_MIN];
    fifo[VMWARE_SVGA2_FIFO_REG_STOP] = fifo[VMWARE_SVGA2_FIFO_REG_MIN];

    vmware_svga2->fifo_capabilities = fifo[VMWARE_SVGA2_FIFO_REG_CAPABILITIES];

    PRINTLOG(VMWARESVGA, LOG_DEBUG, "FIFO capabilities: 0x%x", vmware_svga2->fifo_capabilities);

    vmware_svga2_write_reg(vmware_svga2, VMWARE_SVGA2_REG_BITS_PER_PIXEL, 32);

    vmware_svga2_write_reg(vmware_svga2, VMWARE_SVGA2_REG_ENABLE, true);
    vmware_svga2_write_reg(vmware_svga2, VMWARE_SVGA2_REG_CONFIG_DONE, true);

    if(vmware_svga2->capabilities & VMWARE_SVGA2_CAPABILITY_IRQMASK) {
        vmware_svga2_write_reg(vmware_svga2, VMWARE_SVGA2_REG_IRQMASK, VMWARE_SVGA2_IRQFLAG_ANY_FENCE);

        // send fence

        vmware_svga2_write_reg(vmware_svga2, VMWARE_SVGA2_REG_SYNC, true);
        while(vmware_svga2_read_reg(vmware_svga2, VMWARE_SVGA2_REG_BUSY));

        vmware_svga2_write_reg(vmware_svga2, VMWARE_SVGA2_REG_IRQMASK, 0);

        // test irq for fence completion
    }

    video_fb_refresh_frame_buffer_address();
    SCREEN_FLUSH = vmware_svga2_display_flush;
    SCREEN_FLUSH(0, 0, 0, 0, vmware_svga2->screen_width, vmware_svga2->screen_height);

    PRINTLOG(VMWARESVGA, LOG_DEBUG, "vmware svga2 initialized");

    return 0;
}

/**
 * @file video_qemuvga.64.c
 * @brief qemu vga video driver implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <driver/video_qemuvga.h>
#include <driver/video_edid.h>
#include <driver/video_fb.h>
#include <logging.h>
#include <memory/frame.h>
#include <memory/paging.h>
#include <graphics/screen.h>
#include <graphics/text_cursor.h>
#include <device/mmio.h>
#include <systeminfo.h>
#include <cpu/sync.h>

MODULE("turnstone.kernel.hw.video.qemuvga");

typedef struct qemuvga_device_t {
    uint8_t* mmio_data;
    int32_t  max_width;
    int32_t  max_height;
} qemuvga_device_t;

static qemuvga_device_t* qemuvga_device = NULL;
extern lock_t* video_lock;

int8_t video_qemu_vga_init(memory_heap_t* heap, const pci_dev_t* device){
    pci_generic_device_t* pci_dev = (pci_generic_device_t*) device->pci_header;

    PRINTLOG(VIDEO, LOG_INFO, "Initializing QEMU VGA Device");

    // FIXME: locking video_lock is not enough, opening tracing at frameallocator
    // causes page faults. find a better way to handle this.

    lock_acquire(video_lock);

    uint64_t fb_bar_addr_fa = pci_get_bar_address(pci_dev, 0);
    uint64_t fb_bar_addr_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(fb_bar_addr_fa);
    uint64_t fb_bar_size    = pci_get_bar_size(pci_dev, 0);
    uint64_t fb_bar_frm_cnt = (fb_bar_size + FRAME_SIZE - 1) / FRAME_SIZE;

    screen_info_t screen_info = screen_get_info();

    frame_t old_fb_frm = {
        .frame_address = SYSTEM_INFO->frame_buffer->physical_base_address,
        .frame_count   = (screen_info.pixels_per_scanline * screen_info.height * sizeof(color_t) + FRAME_SIZE - 1) / FRAME_SIZE,
        .type          = FRAME_TYPE_RESERVED,
    };

    memory_paging_delete_va_for_frame(SYSTEM_INFO->frame_buffer->virtual_base_address, &old_fb_frm);

    frame_t fb_bar_frm = {
        .frame_address = fb_bar_addr_fa,
        .frame_count   = fb_bar_frm_cnt,
        .type          = FRAME_TYPE_RESERVED,
    };

    memory_paging_add_va_for_frame(fb_bar_addr_va, &fb_bar_frm,
                                   MEMORY_PAGING_PAGE_TYPE_NOEXEC);

    lock_release(video_lock);

    uint64_t mmio_bar_addr_fa = pci_get_bar_address(pci_dev, 2);
    uint64_t mmio_bar_addr_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(mmio_bar_addr_fa);
    uint64_t mmio_bar_size    = pci_get_bar_size(pci_dev, 2);
    uint64_t mmio_bar_frm_cnt = (mmio_bar_size + FRAME_SIZE - 1) / FRAME_SIZE;

    frame_t mmio_bar_frm = {
        .frame_address = mmio_bar_addr_fa,
        .frame_count   = mmio_bar_frm_cnt,
        .type          = FRAME_TYPE_RESERVED,
    };

    frame_allocator_t* fa = frame_get_allocator();

    if(fa->get_reserved_frames_of_address(fa, (void*)mmio_bar_addr_fa) == NULL) {
        PRINTLOG(VIDEO, LOG_INFO, "QEMU VGA MMIO BAR not reserved, reserving");
        frame_get_allocator()->allocate_frame(frame_get_allocator(), &mmio_bar_frm);
    }

    memory_paging_add_va_for_frame(mmio_bar_addr_va, &mmio_bar_frm,
                                   MEMORY_PAGING_PAGE_TYPE_NOEXEC |
                                   MEMORY_PAGING_PAGE_TYPE_DISABLE_CACHE |
                                   MEMORY_PAGING_PAGE_TYPE_WRITE_THROUGH);


    qemuvga_device = memory_malloc_ext(heap, sizeof(qemuvga_device_t), 0);
    if(qemuvga_device == NULL) {
        PRINTLOG(VIDEO, LOG_ERROR, "Failed to allocate memory for qemuvga_device");

        return -1;
    }

    qemuvga_device->mmio_data = (uint8_t*) mmio_bar_addr_va;

    PRINTLOG(VIDEO, LOG_INFO, "QEMU VGA FB virtual address: 0x%llx", fb_bar_addr_va);
    PRINTLOG(VIDEO, LOG_INFO, "QEMU VGA MMIO address: 0x%llx", mmio_bar_addr_fa);
    PRINTLOG(VIDEO, LOG_INFO, "QEMU VGA MMIO size: 0x%llx", mmio_bar_size);

    uint8_t* edid = qemuvga_device->mmio_data + VIDEO_QEMU_VGA_EDID_OFFSET;

    if(video_edid_get_max_resolution(edid, (uint32_t*)&qemuvga_device->max_width, (uint32_t*)&qemuvga_device->max_height) != 0) {
        PRINTLOG(VIDEO, LOG_WARNING, "Failed to get max resolution from EDID, using from screen info");
        qemuvga_device->max_width  = screen_info.width;
        qemuvga_device->max_height = screen_info.height;
    }

    PRINTLOG(VIDEO, LOG_INFO, "QEMU VGA max resolution from EDID: %dx%d", qemuvga_device->max_width, qemuvga_device->max_height);

    uint64_t dispi_offset = (uint64_t)(qemuvga_device->mmio_data + VIDEO_QEMU_VGA_BOCHS_DISPI_OFFSET);

    uint16_t old_xres = mmio_read(dispi_offset + VIDEO_QEMU_VGA_VBE_DISPI_INDEX_XRES * 2, 2);
    uint16_t old_yres = mmio_read(dispi_offset + VIDEO_QEMU_VGA_VBE_DISPI_INDEX_YRES * 2, 2);
    uint16_t old_bpp  = mmio_read(dispi_offset + VIDEO_QEMU_VGA_VBE_DISPI_INDEX_BPP * 2, 2);

    PRINTLOG(VIDEO, LOG_INFO, "QEMU VGA current resolution: %dx%d bpp %d", old_xres, old_yres, old_bpp);

    PRINTLOG(VIDEO, LOG_INFO, "QEMU VGA setting resolution to: %dx%d bpp %d", qemuvga_device->max_width, qemuvga_device->max_height, old_bpp);


    uint8_t* tmp_new_fb = memory_malloc_ext(heap, qemuvga_device->max_width * qemuvga_device->max_height * sizeof(color_t), 0);

    if(!tmp_new_fb) {
        PRINTLOG(VIDEO, LOG_ERROR, "Failed to allocate memory for tmp_old_fb");

        memory_free(qemuvga_device);
        qemuvga_device = NULL;

        return -1;
    }

    lock_acquire(video_lock);

    video_fb_copy_contents_to_frame_buffer(tmp_new_fb,
                                           qemuvga_device->max_width,
                                           qemuvga_device->max_height,
                                           qemuvga_device->max_width);


    mmio_write(dispi_offset + VIDEO_QEMU_VGA_VBE_DISPI_INDEX_ENABLE * 2, VIDEO_QEMU_VGA_VBE_DISPI_DISABLED, 2);
    mmio_write(dispi_offset + VIDEO_QEMU_VGA_VBE_DISPI_INDEX_BPP * 2, old_bpp, 2);
    mmio_write(dispi_offset + VIDEO_QEMU_VGA_VBE_DISPI_INDEX_XRES * 2, (uint16_t)qemuvga_device->max_width, 2);
    mmio_write(dispi_offset + VIDEO_QEMU_VGA_VBE_DISPI_INDEX_YRES * 2, (uint16_t)qemuvga_device->max_height, 2);
    mmio_write(dispi_offset + VIDEO_QEMU_VGA_VBE_DISPI_INDEX_ENABLE * 2,
               VIDEO_QEMU_VGA_VBE_DISPI_ENABLED | VIDEO_QEMU_VGA_VBE_DISPI_LFB_ENABLED, 2);


    memory_memcopy(tmp_new_fb, (uint8_t*)fb_bar_addr_va, qemuvga_device->max_width * qemuvga_device->max_height * sizeof(color_t));

    memory_free(tmp_new_fb);


    screen_set_dimensions(
        qemuvga_device->max_width,
        qemuvga_device->max_height,
        qemuvga_device->max_width
        );

    PRINTLOG(VIDEO, LOG_INFO, "QEMU VGA set resolution to: %dx%d bpp %d", qemuvga_device->max_width, qemuvga_device->max_height, old_bpp);

    lock_release(video_lock);


    return 0;
}

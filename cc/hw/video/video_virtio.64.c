/**
 * @file video_virtio.64.c
 * @brief Video virtio driver
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <driver/video_virtio.h>
#include <video.h>
#include <driver/virtio.h>
#include <cpu/interrupt.h>
#include <apic.h>
#include <memory/paging.h>
#include <memory/frame.h>
#include <cpu/sync.h>
#include <future.h>
#include <systeminfo.h>
#include <utils.h>
#include <time/timer.h>
#include <cpu/task.h>
#include <driver/video_edid.h>
#include <cpu.h>
#include <logging.h>

MODULE("turnstone.kernel.hw.video.virtiogpu");

uint64_t virtio_gpu_select_features(virtio_dev_t* dev, uint64_t selected_features);
int8_t   virtio_gpu_create_queues(virtio_dev_t* dev);
int8_t   virtio_gpu_controlq_isr(interrupt_frame_t* frame, uint8_t irqno);
void     virtio_gpu_get_display_info(uint32_t scanout);
void     virtio_gpu_display_flush(uint32_t scanout, uint64_t buf_offset, uint32_t x, uint32_t y, uint32_t width, uint32_t height);

uint32_t resource_id = 0;
uint32_t screen_width = 0;
uint32_t screen_height = 0;

virtio_gpu_wrapper_t* virtio_gpu_wrapper = NULL;
lock_t virtio_gpu_lock = NULL;
lock_t virtio_gpu_flush_lock = NULL;

void video_text_print(char_t* string);

int8_t   virtio_gpu_controlq_isr(interrupt_frame_t* frame, uint8_t irqno) {
    UNUSED(frame);
    UNUSED(irqno);

    //video_text_print((char_t*)"virtio gpu control queue isr\n");

    virtio_dev_t* virtio_gpu_dev = virtio_gpu_wrapper->vgpu;

    volatile virtio_gpu_config_t* cfg = (volatile virtio_gpu_config_t*)virtio_gpu_dev->device_config;

    if(cfg->events_read) {
        cfg->events_clear = cfg->events_read;
    }

    if(virtio_gpu_lock) {
        lock_release(virtio_gpu_lock);
    }

    //video_text_print((char_t*)".");

    pci_msix_clear_pending_bit((pci_generic_device_t*)virtio_gpu_dev->pci_dev->pci_header, virtio_gpu_dev->msix_cap, 0);
    apic_eoi();

    return 0;
}

void virtio_gpu_display_flush(uint32_t scanout, uint64_t buf_offset, uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    if(width == 0 || height == 0) {
        return;
    }

    lock_acquire(virtio_gpu_flush_lock);
    virtio_gpu_lock = NULL;
    char_t buffer[100] = {0};
    volatile virtio_gpu_ctrl_hdr_t* hdr;

    //video_text_print((char_t*)"virtio gpu display flush\n");

    virtio_dev_t* virtio_gpu_dev = virtio_gpu_wrapper->vgpu;

    virtio_queue_ext_t* vq_control = &virtio_gpu_dev->queues[0];
    virtio_queue_avail_t* avail = virtio_queue_get_avail(virtio_gpu_dev, vq_control->vq);
    virtio_queue_descriptor_t* descs = virtio_queue_get_desc(virtio_gpu_dev, vq_control->vq);


    uint16_t desc_index = avail->ring[avail->index % virtio_gpu_dev->queue_size];
    uint8_t* offset = (uint8_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(descs[desc_index].address);

    virtio_gpu_transfer_to_host_2d_t* transfer_hdr = (virtio_gpu_transfer_to_host_2d_t*)offset;

    transfer_hdr->hdr.type = VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D;

    if(virtio_gpu_wrapper->num_scanouts == 1) { // the fucking bug
        transfer_hdr->hdr.flags = VIRTIO_GPU_FLAG_FENCE;
        transfer_hdr->hdr.fence_id = virtio_gpu_wrapper->fence_ids[scanout]++;
    } else {
        transfer_hdr->hdr.flags = 0;
        transfer_hdr->hdr.fence_id = 0;
    }

    transfer_hdr->hdr.ctx_id = 0;
    //transfer_hdr->hdr.padding = 0;

    transfer_hdr->resource_id = virtio_gpu_wrapper->resource_ids[scanout];
    transfer_hdr->rec.x = x;
    transfer_hdr->rec.y = y;
    transfer_hdr->rec.width = width;
    transfer_hdr->rec.height = height;
    transfer_hdr->offset = buf_offset;

    descs[desc_index].length = sizeof(virtio_gpu_transfer_to_host_2d_t);

    virtio_gpu_lock = lock_create_for_future();
    future_t fut = future_create(virtio_gpu_lock);

    avail->index++;
    vq_control->nd->vqn = 0;

    future_get_data_and_destroy(fut);

    desc_index = descs[desc_index].next;

    offset = (uint8_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(descs[desc_index].address);
    hdr = (volatile virtio_gpu_ctrl_hdr_t*)offset;

    if(hdr->type != VIRTIO_GPU_RESP_OK_NODATA) {
        video_text_print((char_t*)"virtio gpu transfer to host 2d failed: ");
        utoh_with_buffer(buffer, hdr->type);
        video_text_print(buffer);
        video_text_print((char_t*)"\n");
        memory_memclean(offset, sizeof(virtio_gpu_ctrl_hdr_t));

        lock_release(virtio_gpu_flush_lock);

        return;
    }

    memory_memclean(offset, sizeof(virtio_gpu_ctrl_hdr_t));


    desc_index = avail->ring[avail->index % virtio_gpu_dev->queue_size];

    offset = (uint8_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(descs[desc_index].address);
    virtio_gpu_resource_flush_t* flush_hdr = (virtio_gpu_resource_flush_t*)offset;

    flush_hdr->hdr.type = VIRTIO_GPU_CMD_RESOURCE_FLUSH;

    if(virtio_gpu_wrapper->num_scanouts == 1) { // the fucking bug
        transfer_hdr->hdr.flags = VIRTIO_GPU_FLAG_FENCE;
        transfer_hdr->hdr.fence_id = virtio_gpu_wrapper->fence_ids[scanout]++;
    } else {
        transfer_hdr->hdr.flags = 0;
        transfer_hdr->hdr.fence_id = 0;
    }

    flush_hdr->hdr.ctx_id = 0;
    //flush_hdr->hdr.padding = 0;

    flush_hdr->rec.x = x;
    flush_hdr->rec.y = y;
    flush_hdr->rec.width = width;
    flush_hdr->rec.height = height;

    flush_hdr->resource_id = virtio_gpu_wrapper->resource_ids[scanout];
    flush_hdr->padding = 0;

    descs[desc_index].length = sizeof(virtio_gpu_resource_flush_t);

    virtio_gpu_lock = lock_create_for_future();
    fut = future_create(virtio_gpu_lock);

    avail->index++;
    vq_control->nd->vqn = 0;

    future_get_data_and_destroy(fut);

    desc_index = descs[desc_index].next;

    offset = (uint8_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(descs[desc_index].address);
    hdr = (volatile virtio_gpu_ctrl_hdr_t*)offset;

    if(hdr->type != VIRTIO_GPU_RESP_OK_NODATA) {
        video_text_print((char_t*)"virtio gpu resource flush failed\n");
        utoh_with_buffer(buffer, hdr->type);
        video_text_print(buffer);
        video_text_print((char_t*)"\n");
        memory_memclean(offset, sizeof(virtio_gpu_ctrl_hdr_t));

        lock_release(virtio_gpu_flush_lock);

        return;
    }

    memory_memclean(offset, sizeof(virtio_gpu_ctrl_hdr_t));



    //video_text_print((char_t*)"virtio gpu display flush done\n");

    lock_release(virtio_gpu_flush_lock);
}

void virtio_gpu_get_display_info(uint32_t scanout) {
    virtio_gpu_flush_lock = lock_create();

    virtio_dev_t* virtio_gpu_dev = virtio_gpu_wrapper->vgpu;

    virtio_queue_ext_t* vq_control = &virtio_gpu_dev->queues[0];
    virtio_queue_avail_t* avail = virtio_queue_get_avail(virtio_gpu_dev, vq_control->vq);
    virtio_queue_descriptor_t* descs = virtio_queue_get_desc(virtio_gpu_dev, vq_control->vq);

    uint16_t desc_index;
    uint8_t* offset;
    future_t fut;
    virtio_gpu_ctrl_hdr_t* hdr;

    desc_index = avail->ring[avail->index % virtio_gpu_dev->queue_size];

    offset = (uint8_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(descs[desc_index].address);
    virtio_gpu_get_edid_t* edid_get_hdr = (virtio_gpu_get_edid_t*)offset;

    PRINTLOG(VIRTIOGPU, LOG_TRACE, "virtio gpu get edid info");

    edid_get_hdr->hdr.type = VIRTIO_GPU_CMD_GET_EDID;
    edid_get_hdr->hdr.flags = 0;
    edid_get_hdr->hdr.fence_id = 0;
    edid_get_hdr->hdr.ctx_id = 0;
    edid_get_hdr->scanout = scanout;
    edid_get_hdr->padding = 0;

    descs[desc_index].length = sizeof(virtio_gpu_get_edid_t);

    virtio_gpu_lock = lock_create_for_future();
    fut = future_create(virtio_gpu_lock);

    avail->index++;
    vq_control->nd->vqn = 0;

    future_get_data_and_destroy(fut);

    desc_index = descs[desc_index].next;


    offset = (uint8_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(descs[desc_index].address);
    virtio_gpu_resp_edid_t* edid = (virtio_gpu_resp_edid_t*)offset;

    PRINTLOG(VIRTIOGPU, LOG_TRACE, "queue desc length: %x flags: %x", descs[desc_index].length, descs[desc_index].flags);

    if(edid->hdr.type != VIRTIO_GPU_RESP_OK_EDID) {
        PRINTLOG(VIRTIOGPU, LOG_ERROR, "virtio gpu get edid failed: 0x%x", edid->hdr.type);

        return;
    }

    video_edid_get_max_resolution(edid->edid, &screen_width, &screen_height);

    PRINTLOG(VIRTIOGPU, LOG_INFO, "virtio gpu get edid success: %dx%d", screen_width, screen_height);

    memory_memclean(offset, sizeof(virtio_gpu_resp_edid_t));

    desc_index = avail->ring[avail->index % virtio_gpu_dev->queue_size];

    offset = (uint8_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(descs[desc_index].address);
    hdr = (virtio_gpu_ctrl_hdr_t*)offset;

    PRINTLOG(VIRTIOGPU, LOG_TRACE, "virtio gpu get display info");

    hdr->type = VIRTIO_GPU_CMD_GET_DISPLAY_INFO;
    hdr->flags = 0;
    hdr->fence_id = 0;
    hdr->ctx_id = 0;
    //hdr->padding = 0;

    descs[desc_index].length = sizeof(virtio_gpu_ctrl_hdr_t);

    virtio_gpu_lock = lock_create_for_future();
    fut = future_create(virtio_gpu_lock);

    avail->index++;
    vq_control->nd->vqn = 0;

    future_get_data_and_destroy(fut);

    desc_index = descs[desc_index].next;


    offset = (uint8_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(descs[desc_index].address);
    virtio_gpu_resp_display_info_t* info = (virtio_gpu_resp_display_info_t*)offset;

    PRINTLOG(VIRTIOGPU, LOG_TRACE, "queue desc length: %x flags: %x", descs[desc_index].length, descs[desc_index].flags);

    if(info->hdr.type != VIRTIO_GPU_RESP_OK_DISPLAY_INFO) {
        PRINTLOG(VIRTIOGPU, LOG_ERROR, "virtio gpu get display info failed: 0x%x", info->hdr.type);

        return;
    }

    memory_memclean(offset, sizeof(virtio_gpu_resp_display_info_t));

    resource_id = 1;

    desc_index = avail->ring[avail->index % virtio_gpu_dev->queue_size];
    offset = (uint8_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(descs[desc_index].address);
    virtio_gpu_resource_create_2d_t* create_hdr = (virtio_gpu_resource_create_2d_t*)offset;

    create_hdr->hdr.type = VIRTIO_GPU_CMD_RESOURCE_CREATE_2D;
    create_hdr->hdr.flags = 0;
    create_hdr->hdr.fence_id = 0;
    create_hdr->hdr.ctx_id = 0;
    //create_hdr->hdr.padding = 0;

    uint32_t screen_resource_id = resource_id++;
    virtio_gpu_wrapper->resource_ids[scanout] = screen_resource_id;
    virtio_gpu_wrapper->fence_ids[scanout] = 1;

    create_hdr->resource_id = screen_resource_id;
    create_hdr->format = VIRTIO_GPU_FORMAT_R8G8B8A8_UNORM;
    create_hdr->width = screen_width;
    create_hdr->height = screen_height;

    descs[desc_index].length = sizeof(virtio_gpu_resource_create_2d_t);

    virtio_gpu_lock = lock_create_for_future();
    fut = future_create(virtio_gpu_lock);

    avail->index++;
    vq_control->nd->vqn = 0;

    future_get_data_and_destroy(fut);

    desc_index = descs[desc_index].next;

    offset = (uint8_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(descs[desc_index].address);
    hdr = (virtio_gpu_ctrl_hdr_t*)offset;

    if(hdr->type != VIRTIO_GPU_RESP_OK_NODATA) {
        PRINTLOG(VIRTIOGPU, LOG_ERROR, "virtio gpu resource create failed: 0x%x", hdr->type);

        return;
    }

    memory_memclean(offset, sizeof(virtio_gpu_ctrl_hdr_t));

    uint64_t screen_size = screen_width * screen_height * sizeof(pixel_t);
    uint64_t screen_frm_cnt = (screen_size + FRAME_SIZE - 1) / FRAME_SIZE;

    frame_t* screen_frm = NULL;

    if(KERNEL_FRAME_ALLOCATOR->allocate_frame_by_count(KERNEL_FRAME_ALLOCATOR,
                                                       screen_frm_cnt,
                                                       FRAME_ALLOCATION_TYPE_BLOCK | FRAME_ALLOCATION_TYPE_RESERVED,
                                                       &screen_frm,
                                                       NULL) != 0) {
        PRINTLOG(VIRTIOGPU, LOG_ERROR, "failed to allocate screen frame");

        return;
    }

    uint64_t screen_fa = screen_frm->frame_address;
    uint64_t screen_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(screen_frm->frame_address);

    memory_paging_add_va_for_frame(screen_va, screen_frm, MEMORY_PAGING_PAGE_TYPE_NOEXEC);
    memory_memclean((void*)screen_va, screen_size);

    desc_index = avail->ring[avail->index % virtio_gpu_dev->queue_size];
    offset = (uint8_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(descs[desc_index].address);
    virtio_gpu_resource_attach_backing_t* attach_hdr = (virtio_gpu_resource_attach_backing_t*)offset;

    attach_hdr->hdr.type = VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING;
    attach_hdr->hdr.flags = 0;
    attach_hdr->hdr.fence_id = 0;
    attach_hdr->hdr.ctx_id = 0;
    //attach_hdr->hdr.padding = 0;

    attach_hdr->resource_id = screen_resource_id;
    attach_hdr->nr_entries = 1;
    attach_hdr->entries[0].addr = screen_fa;
    attach_hdr->entries[0].length = screen_size;

    descs[desc_index].length = sizeof(virtio_gpu_resource_attach_backing_t) +
                               sizeof(virtio_gpu_mem_entry_t) * attach_hdr->nr_entries;

    virtio_gpu_lock = lock_create_for_future();
    fut = future_create(virtio_gpu_lock);

    avail->index++;
    vq_control->nd->vqn = 0;

    future_get_data_and_destroy(fut);

    desc_index = descs[desc_index].next;

    offset = (uint8_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(descs[desc_index].address);
    hdr = (virtio_gpu_ctrl_hdr_t*)offset;

    if(hdr->type != VIRTIO_GPU_RESP_OK_NODATA) {
        PRINTLOG(VIRTIOGPU, LOG_ERROR, "virtio gpu resource attach backing failed: 0x%x", hdr->type);

        return;
    }

    memory_memclean(offset, sizeof(virtio_gpu_ctrl_hdr_t));

    desc_index = avail->ring[avail->index % virtio_gpu_dev->queue_size];
    offset = (uint8_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(descs[desc_index].address);

    virtio_gpu_transfer_to_host_2d_t* transfer_hdr = (virtio_gpu_transfer_to_host_2d_t*)offset;

    transfer_hdr->hdr.type = VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D;
    transfer_hdr->hdr.flags = 0;
    transfer_hdr->hdr.fence_id = 0;
    transfer_hdr->hdr.ctx_id = 0;
    //transfer_hdr->hdr.padding = 0;

    transfer_hdr->resource_id = screen_resource_id;
    transfer_hdr->rec.x = 0;
    transfer_hdr->rec.y = 0;
    transfer_hdr->rec.width = screen_width;
    transfer_hdr->rec.height = screen_height;
    transfer_hdr->offset = 0;

    descs[desc_index].length = sizeof(virtio_gpu_transfer_to_host_2d_t);

    virtio_gpu_lock = lock_create_for_future();
    fut = future_create(virtio_gpu_lock);

    avail->index++;
    vq_control->nd->vqn = 0;

    future_get_data_and_destroy(fut);

    desc_index = descs[desc_index].next;

    offset = (uint8_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(descs[desc_index].address);
    hdr = (virtio_gpu_ctrl_hdr_t*)offset;

    if(hdr->type != VIRTIO_GPU_RESP_OK_NODATA) {
        PRINTLOG(VIRTIOGPU, LOG_ERROR, "virtio gpu transfer to host 2d failed: 0x%x", hdr->type);

        return;
    }

    memory_memclean(offset, sizeof(virtio_gpu_ctrl_hdr_t));

    desc_index = avail->ring[avail->index % virtio_gpu_dev->queue_size];
    offset = (uint8_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(descs[desc_index].address);

    virtio_gpu_set_scanout_t* scanout_hdr = (virtio_gpu_set_scanout_t*)offset;

    scanout_hdr->hdr.type = VIRTIO_GPU_CMD_SET_SCANOUT;
    scanout_hdr->hdr.flags = 0;
    scanout_hdr->hdr.fence_id = 0;
    scanout_hdr->hdr.ctx_id = 0;
    //scanout_hdr->hdr.padding = 0;

    scanout_hdr->rec.x = 0;
    scanout_hdr->rec.y = 0;
    scanout_hdr->rec.width = screen_width;
    scanout_hdr->rec.height = screen_height;

    scanout_hdr->scanout_id = scanout;
    scanout_hdr->resource_id = screen_resource_id;

    descs[desc_index].length = sizeof(virtio_gpu_set_scanout_t);

    virtio_gpu_lock = lock_create_for_future();
    fut = future_create(virtio_gpu_lock);

    avail->index++;
    vq_control->nd->vqn = 0;

    future_get_data_and_destroy(fut);

    desc_index = descs[desc_index].next;

    offset = (uint8_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(descs[desc_index].address);
    hdr = (virtio_gpu_ctrl_hdr_t*)offset;

    if(hdr->type != VIRTIO_GPU_RESP_OK_NODATA) {
        PRINTLOG(VIRTIOGPU, LOG_ERROR, "virtio gpu set scanout failed: 0x%x", hdr->type);

        return;
    }

    memory_memclean(offset, sizeof(virtio_gpu_ctrl_hdr_t));

    if(scanout == 0) {
        SYSTEM_INFO->frame_buffer->width = screen_width;
        SYSTEM_INFO->frame_buffer->height = screen_height;
        SYSTEM_INFO->frame_buffer->buffer_size = screen_size;
        SYSTEM_INFO->frame_buffer->pixels_per_scanline = screen_width;
        SYSTEM_INFO->frame_buffer->physical_base_address = screen_fa;
        SYSTEM_INFO->frame_buffer->virtual_base_address = screen_va;

        video_copy_contents_to_frame_buffer((uint8_t*)screen_va, screen_width, screen_height, screen_width);
        video_refresh_frame_buffer_address();
        cpu_cli();
        VIDEO_DISPLAY_FLUSH = virtio_gpu_display_flush;
        cpu_sti();
    }

    VIDEO_DISPLAY_FLUSH(scanout, 0, 0, 0, screen_width, screen_height);
}

uint64_t virtio_gpu_select_features(virtio_dev_t* dev, uint64_t avail_features) {
    UNUSED(dev);
    PRINTLOG(VIRTIOGPU, LOG_INFO, "virtio gpu features: %llx", avail_features);

    return VIRTIO_GPU_F_VIRGL | VIRTIO_GPU_F_EDID | VIRTIO_GPU_F_RESOURCE_UUID;
}

int8_t virtio_gpu_create_queues(virtio_dev_t* vdev) {
    vdev->queues = memory_malloc(sizeof(virtio_queue_ext_t) * 2);

    uint64_t item_size = 2 << 10;

    PRINTLOG(VIRTIOGPU, LOG_TRACE, "building virtio gpu control queue");

    if(virtio_create_queue(vdev, 0, item_size, 0, 1, NULL, &virtio_gpu_controlq_isr, NULL) != 0) {
        PRINTLOG(VIRTIO, LOG_ERROR, "cannot create virtio gpu control queue");

        return -1;
    }

    PRINTLOG(VIRTIOGPU, LOG_TRACE, "virtio gpu control queue builed");

    PRINTLOG(VIRTIOGPU, LOG_TRACE, "building virtio gpu cursor queue");

    item_size = sizeof(virtio_gpu_update_cursor_t);

    if(virtio_create_queue(vdev, 1, item_size, 0, 1, NULL, NULL, NULL) != 0) {
        PRINTLOG(VIRTIO, LOG_ERROR, "cannot create virtio gpu cursor queue");

        return -1;
    }

    PRINTLOG(VIRTIOGPU, LOG_TRACE, "virtio gpu cursor queue builed");

    return 0;
}


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
int8_t virtio_video_init(memory_heap_t* heap, const pci_dev_t* pci_dev) {
    UNUSED(heap);

    virtio_dev_t* vgpu = virtio_get_device(pci_dev);

    if (vgpu == NULL) {
        PRINTLOG(VIRTIOGPU, LOG_ERROR, "virtio_get_device failed");

        return -1;
    }

    vgpu->queue_size = VIRTIO_QUEUE_SIZE;

    if(virtio_init_dev(vgpu, virtio_gpu_select_features, virtio_gpu_create_queues) != 0) {
        PRINTLOG(VIRTIOGPU, LOG_ERROR, "virtio gpu init failed");

        return -1;
    }

    virtio_gpu_wrapper = memory_malloc(sizeof(virtio_gpu_wrapper_t));

    if(virtio_gpu_wrapper == NULL) {
        PRINTLOG(VIRTIOGPU, LOG_ERROR, "virtio gpu init failed");

        return -1;
    }

    virtio_gpu_wrapper->vgpu = vgpu;

    virtio_gpu_config_t* vgpu_conf =  vgpu->device_config;

    virtio_gpu_wrapper->num_scanouts = vgpu_conf->num_scanouts;

    virtio_gpu_wrapper->fence_ids = memory_malloc(sizeof(uint64_t) * vgpu_conf->num_scanouts);
    virtio_gpu_wrapper->resource_ids = memory_malloc(sizeof(uint32_t) * vgpu_conf->num_scanouts);

    if(virtio_gpu_wrapper->fence_ids == NULL || virtio_gpu_wrapper->resource_ids == NULL) {
        PRINTLOG(VIRTIOGPU, LOG_ERROR, "virtio gpu init failed");

        return -1;
    }

    for(uint32_t i = 0; i < vgpu_conf->num_scanouts; i++) {
        virtio_gpu_get_display_info(i);
    }


    PRINTLOG(VIRTIOGPU, LOG_INFO, "virtio gpu init success");

    return 0;
}
#pragma GCC diagnostic pop

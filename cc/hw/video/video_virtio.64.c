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
#include <graphics/image.h>
#include <graphics/virgl.h>
#include <graphics/font.h>

MODULE("turnstone.kernel.hw.video.virtiogpu");

uint64_t virtio_gpu_select_features(virtio_dev_t* dev, uint64_t selected_features);
int8_t   virtio_gpu_create_queues(virtio_dev_t* dev);
int8_t   virtio_gpu_controlq_isr(interrupt_frame_ext_t* frame);
int8_t   virtio_gpu_cursorq_isr(interrupt_frame_ext_t* frame);
int8_t   virtio_gpu_display_init(uint32_t scanout);
int8_t   mouse_init(void);
int8_t   virtio_gpu_font_init(void);
void     virtio_gpu_display_flush(uint32_t scanout, uint64_t buf_offset, uint32_t x, uint32_t y, uint32_t width, uint32_t height);
void     mouse_move(uint32_t x, uint32_t y);
void     virtio_gpu_clear_screen_area(uint32_t x, uint32_t y, uint32_t width, uint32_t height, color_t background);

virtio_gpu_wrapper_t* virtio_gpu_wrapper = NULL;

static boolean_t virtio_gpu_dont_transfer_on_flush = false;

void video_text_print(char_t* string);

int8_t virtio_gpu_controlq_isr(interrupt_frame_ext_t* frame) {
    UNUSED(frame);

    // video_text_print((char_t*)"virtio gpu control queue isr\n");

    virtio_dev_t* virtio_gpu_dev = virtio_gpu_wrapper->vgpu;

    volatile virtio_gpu_config_t* cfg = (volatile virtio_gpu_config_t*)virtio_gpu_dev->device_config;

    if(cfg->events_read) {
        cfg->events_clear = cfg->events_read;
    }

    if(virtio_gpu_wrapper->lock) {
        lock_release(virtio_gpu_wrapper->lock);
    }

// video_text_print((char_t*)".");

    pci_msix_clear_pending_bit((pci_generic_device_t*)virtio_gpu_dev->pci_dev->pci_header, virtio_gpu_dev->msix_cap, 0);
    apic_eoi();

    return 0;
}

int8_t virtio_gpu_cursorq_isr(interrupt_frame_ext_t* frame) {
    UNUSED(frame);

    // video_text_print((char_t*)"virtio gpu control queue isr\n");

    virtio_dev_t* virtio_gpu_dev = virtio_gpu_wrapper->vgpu;

    volatile virtio_gpu_config_t* cfg = (volatile virtio_gpu_config_t*)virtio_gpu_dev->device_config;

    if(cfg->events_read) {
        cfg->events_clear = cfg->events_read;
    }

    if(virtio_gpu_wrapper->cursor_lock) {
        lock_release(virtio_gpu_wrapper->cursor_lock);
    }

// video_text_print((char_t*)".");

    pci_msix_clear_pending_bit((pci_generic_device_t*)virtio_gpu_dev->pci_dev->pci_header, virtio_gpu_dev->msix_cap, 1);
    apic_eoi();

    return 0;
}

static int8_t virtio_gpu_wait_for_queue_command(uint32_t queue_no, lock_t** lock, uint16_t desc_index) {
    virtio_dev_t* virtio_gpu_dev = virtio_gpu_wrapper->vgpu;

    virtio_queue_ext_t* vq_control = &virtio_gpu_dev->queues[queue_no];
    virtio_queue_avail_t* avail = virtio_queue_get_avail(virtio_gpu_dev, vq_control->vq);
    virtio_queue_descriptor_t* descs = virtio_queue_get_desc(virtio_gpu_dev, vq_control->vq);

    future_t* fut = NULL;

    if(lock) {
        *lock = lock_create_for_future(0);
        fut = future_create(*lock);
    }

    avail->index++;
    vq_control->nd->vqn = queue_no;

    if(!lock) {
        return 0;
    }

    future_get_data_and_destroy(fut);

    desc_index = descs[desc_index].next;

    uint8_t* offset = (uint8_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(descs[desc_index].address);
    volatile virtio_gpu_ctrl_hdr_t* hdr = (volatile virtio_gpu_ctrl_hdr_t*)offset;

    if(hdr->type != VIRTIO_GPU_RESP_OK_NODATA) {
        // char_t buffer[64] = {0};
        // video_text_print((char_t*)"virtio gpu transfer to host 2d failed: ");
        // utoh_with_buffer(buffer, hdr->type);
        // video_text_print(buffer);
        // video_text_print((char_t*)"\n");

        memory_memclean(offset, sizeof(virtio_gpu_ctrl_hdr_t));

        return -1;
    }

    memory_memclean(offset, sizeof(virtio_gpu_ctrl_hdr_t));

    return 0;
}

static int8_t virtio_gpu_queue_send_transfer3d(uint32_t queue_no, lock_t** lock,
                                               uint32_t context_id, uint32_t resource_id, uint32_t fence_id,
                                               uint64_t buf_offset,
                                               uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                                               uint32_t stride, uint32_t layer_stride, uint32_t level) {
    virtio_dev_t* virtio_gpu_dev = virtio_gpu_wrapper->vgpu;

    virtio_queue_ext_t* vq_control = &virtio_gpu_dev->queues[queue_no];
    virtio_queue_avail_t* avail = virtio_queue_get_avail(virtio_gpu_dev, vq_control->vq);
    virtio_queue_descriptor_t* descs = virtio_queue_get_desc(virtio_gpu_dev, vq_control->vq);


    uint16_t desc_index = avail->ring[avail->index % virtio_gpu_dev->queue_size];
    uint8_t* offset = (uint8_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(descs[desc_index].address);

    virtio_gpu_transfer_host_3d_t* transfer_hdr = (virtio_gpu_transfer_host_3d_t*)offset;

    transfer_hdr->hdr.type = VIRTIO_GPU_CMD_TRANSFER_TO_HOST_3D;

    if(virtio_gpu_wrapper->num_scanouts == 1) { // the fucking bug
        transfer_hdr->hdr.flags = VIRTIO_GPU_FLAG_FENCE;
        transfer_hdr->hdr.fence_id = fence_id;
    } else {
        transfer_hdr->hdr.flags = 0;
        transfer_hdr->hdr.fence_id = 0;
    }

    transfer_hdr->hdr.ctx_id = context_id;
    // transfer_hdr->hdr.padding = 0;

    transfer_hdr->resource_id = resource_id;
    transfer_hdr->box.x = x;
    transfer_hdr->box.y = y;
    transfer_hdr->box.z = 0;
    transfer_hdr->box.w = width;
    transfer_hdr->box.h = height;
    transfer_hdr->box.d = 1;
    transfer_hdr->offset = buf_offset;
    transfer_hdr->level = level;

    // TODO: find correct values
    transfer_hdr->stride = stride; // width * sizeof(pixel_t);
    transfer_hdr->layer_stride = layer_stride; // width * height * sizeof(pixel_t);


    descs[desc_index].length = sizeof(virtio_gpu_transfer_host_3d_t);

    return virtio_gpu_wait_for_queue_command(queue_no, lock, desc_index);
}

static int8_t virtio_gpu_queue_send_flush(uint32_t queue_no, lock_t** lock,
                                          uint32_t context_id, uint32_t resource_id, uint32_t fence_id,
                                          uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    virtio_dev_t* virtio_gpu_dev = virtio_gpu_wrapper->vgpu;

    virtio_queue_ext_t* vq_control = &virtio_gpu_dev->queues[queue_no];
    virtio_queue_avail_t* avail = virtio_queue_get_avail(virtio_gpu_dev, vq_control->vq);
    virtio_queue_descriptor_t* descs = virtio_queue_get_desc(virtio_gpu_dev, vq_control->vq);


    uint16_t desc_index = avail->ring[avail->index % virtio_gpu_dev->queue_size];
    uint8_t* offset = (uint8_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(descs[desc_index].address);

    virtio_gpu_resource_flush_t* flush_hdr = (virtio_gpu_resource_flush_t*)offset;

    flush_hdr->hdr.type = VIRTIO_GPU_CMD_RESOURCE_FLUSH;

    if(virtio_gpu_wrapper->num_scanouts == 1) { // the fucking bug
        flush_hdr->hdr.flags = VIRTIO_GPU_FLAG_FENCE;
        flush_hdr->hdr.fence_id = fence_id;
    } else {
        flush_hdr->hdr.flags = 0;
        flush_hdr->hdr.fence_id = 0;
    }

    flush_hdr->hdr.ctx_id = context_id;
    // flush_hdr->hdr.padding = 0;

    flush_hdr->rec.x = x;
    flush_hdr->rec.y = y;
    flush_hdr->rec.width = width;
    flush_hdr->rec.height = height;

    flush_hdr->resource_id = resource_id;
    flush_hdr->padding = 0;

    descs[desc_index].length = sizeof(virtio_gpu_resource_flush_t);


    return virtio_gpu_wait_for_queue_command(queue_no, lock, desc_index);
}

static int8_t virtio_gpu_queue_attach_backing(uint32_t queue_no, lock_t** lock,
                                              uint32_t context_id, uint32_t fence_id,
                                              uint32_t resource_id, uint64_t resource_physical_address, uint64_t resource_size) {
    virtio_dev_t* virtio_gpu_dev = virtio_gpu_wrapper->vgpu;

    virtio_queue_ext_t* vq_control = &virtio_gpu_dev->queues[queue_no];
    virtio_queue_avail_t* avail = virtio_queue_get_avail(virtio_gpu_dev, vq_control->vq);
    virtio_queue_descriptor_t* descs = virtio_queue_get_desc(virtio_gpu_dev, vq_control->vq);

    uint16_t desc_index = avail->ring[avail->index % virtio_gpu_dev->queue_size];
    uint8_t* offset = (uint8_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(descs[desc_index].address);
    virtio_gpu_resource_attach_backing_t* attach_hdr = (virtio_gpu_resource_attach_backing_t*)offset;

    attach_hdr->hdr.type = VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING;
    attach_hdr->hdr.flags = fence_id?VIRTIO_GPU_FLAG_FENCE:0;
    attach_hdr->hdr.fence_id = fence_id;
    attach_hdr->hdr.ctx_id = context_id;
    // attach_hdr->hdr.padding = 0;

    attach_hdr->resource_id = resource_id;
    attach_hdr->nr_entries = 1;
    attach_hdr->entries[0].addr = resource_physical_address;
    attach_hdr->entries[0].length = resource_size;

    descs[desc_index].length = sizeof(virtio_gpu_resource_attach_backing_t) +
                               sizeof(virtio_gpu_mem_entry_t) * attach_hdr->nr_entries;

    return virtio_gpu_wait_for_queue_command(queue_no, lock, desc_index);
}

static int8_t virtio_gpu_queue_context_attach_resource(uint32_t queue_no, lock_t** lock,
                                                       uint32_t context_id, uint32_t resource_id) {
    virtio_dev_t* virtio_gpu_dev = virtio_gpu_wrapper->vgpu;

    virtio_queue_ext_t* vq_control = &virtio_gpu_dev->queues[queue_no];
    virtio_queue_avail_t* avail = virtio_queue_get_avail(virtio_gpu_dev, vq_control->vq);
    virtio_queue_descriptor_t* descs = virtio_queue_get_desc(virtio_gpu_dev, vq_control->vq);

    uint16_t desc_index = avail->ring[avail->index % virtio_gpu_dev->queue_size];
    uint8_t* offset = (uint8_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(descs[desc_index].address);

    virtio_gpu_ctx_resource_t* ctx_attach_hdr = (virtio_gpu_ctx_resource_t*)offset;

    ctx_attach_hdr->hdr.type = VIRTIO_GPU_CMD_CTX_ATTACH_RESOURCE;
    ctx_attach_hdr->hdr.flags = 0;
    ctx_attach_hdr->hdr.fence_id = 0;
    ctx_attach_hdr->hdr.ctx_id = context_id;
    // ctx_attach_hdr->hdr.padding = 0;
    ctx_attach_hdr->resource_id = resource_id;

    descs[desc_index].length = sizeof(virtio_gpu_ctx_resource_t);

    return virtio_gpu_wait_for_queue_command(queue_no, lock, desc_index);
}

static int8_t virtio_gpu_queue_context_create_3d_resource(uint32_t queue_no, lock_t** lock,
                                                          uint32_t context_id, uint32_t resource_id, uint32_t fence_id,
                                                          uint32_t resource_width, uint32_t resource_height,
                                                          boolean_t is_y_0_top) {
    virtio_dev_t* virtio_gpu_dev = virtio_gpu_wrapper->vgpu;

    virtio_queue_ext_t* vq_control = &virtio_gpu_dev->queues[queue_no];
    virtio_queue_avail_t* avail = virtio_queue_get_avail(virtio_gpu_dev, vq_control->vq);
    virtio_queue_descriptor_t* descs = virtio_queue_get_desc(virtio_gpu_dev, vq_control->vq);

    uint16_t desc_index = avail->ring[avail->index % virtio_gpu_dev->queue_size];
    uint8_t* offset = (uint8_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(descs[desc_index].address);


    virtio_gpu_resource_create_3d_t* create_hdr = (virtio_gpu_resource_create_3d_t*)offset;

    create_hdr->hdr.type = VIRTIO_GPU_CMD_RESOURCE_CREATE_3D;
    create_hdr->hdr.flags = fence_id?VIRTIO_GPU_FLAG_FENCE:0;
    create_hdr->hdr.fence_id = fence_id;
    create_hdr->hdr.ctx_id = context_id;
    // create_hdr->hdr.padding = 0;

    create_hdr->resource_id = resource_id;
    create_hdr->target = VIRGL_TEXTURE_TARGET_2D;
    create_hdr->format = VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM;
    create_hdr->bind = VIRGL_BIND_RENDER_TARGET | VIRGL_BIND_SAMPLER_VIEW;
    create_hdr->width = resource_width;
    create_hdr->height = resource_height;
    create_hdr->depth = 1;
    create_hdr->array_size = 1;
    create_hdr->last_level = 0;
    create_hdr->nr_samples = 0;
    create_hdr->flags = is_y_0_top?VIRTIO_GPU_RESOURCE_FLAG_Y_0_TOP:0;

    descs[desc_index].length = sizeof(virtio_gpu_resource_create_3d_t);

    return virtio_gpu_wait_for_queue_command(queue_no, lock, desc_index);
}

static int8_t virtio_gpu_queue_send_commad(uint32_t queue_no, lock_t** lock,
                                           uint32_t fence_id, virgl_cmd_t* cmd) {
    virtio_dev_t* virtio_gpu_dev = virtio_gpu_wrapper->vgpu;

    virtio_queue_ext_t* vq_control = &virtio_gpu_dev->queues[queue_no];
    virtio_queue_avail_t* avail = virtio_queue_get_avail(virtio_gpu_dev, vq_control->vq);
    virtio_queue_descriptor_t* descs = virtio_queue_get_desc(virtio_gpu_dev, vq_control->vq);

    uint16_t desc_index = avail->ring[avail->index % virtio_gpu_dev->queue_size];
    uint8_t* offset = (uint8_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(descs[desc_index].address);

    virtio_gpu_cmd_submit_t* submit_hdr = (virtio_gpu_cmd_submit_t*)offset;

    submit_hdr->hdr.type = VIRTIO_GPU_CMD_SUBMIT_3D;
    submit_hdr->hdr.flags = fence_id?VIRTIO_GPU_FLAG_FENCE:0;
    submit_hdr->hdr.fence_id = fence_id;
    submit_hdr->hdr.ctx_id = virgl_cmd_get_context_id(cmd);

    submit_hdr->size = virgl_cmd_get_size(cmd);
    submit_hdr->padding = 0;

    virgl_cmd_write_commands(cmd, offset + sizeof(virtio_gpu_cmd_submit_t));

    descs[desc_index].length = sizeof(virtio_gpu_cmd_submit_t) + submit_hdr->size;

    return virtio_gpu_wait_for_queue_command(queue_no, lock, desc_index);
}

static int8_t virtio_gpu_create_surface(uint32_t resource_id, boolean_t is_texture,
                                        uint32_t fence_id,
                                        lock_t** lock) {
    virgl_cmd_t* cmd = virgl_renderer_get_cmd(virtio_gpu_wrapper->renderer);

    virgl_obj_surface_t obj_surface = {0};

    obj_surface.surface_id = 10;
    obj_surface.resource_id = resource_id;
    obj_surface.format = VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM;
    obj_surface.texture.level = 0;
    obj_surface.texture.layers = 0;

    if(virgl_encode_surface(cmd, &obj_surface, is_texture) != 0) {
        PRINTLOG(VIRTIOGPU, LOG_ERROR, "failed to encode surface");
        return -1;
    }

    virgl_obj_framebuffer_state_t obj_fb_state = {0};

    obj_fb_state.nr_cbufs = 1;
    obj_fb_state.zsurf_id = 0;
    obj_fb_state.cbuf_ids[0] = obj_surface.surface_id;

    if(virgl_encode_framebuffer_state(cmd, &obj_fb_state) != 0) {
        PRINTLOG(VIRTIOGPU, LOG_ERROR, "failed to encode framebuffer state");
        return -1;
    }

    virgl_cmd_clear_t clear_cmd = {0};

    clear_cmd.clear_flags = VIRGL_CLEAR_FLAG_CLEAR_COLOR0;

    clear_cmd.clear_color.f32[0] = 0.0f;
    clear_cmd.clear_color.f32[1] = 1.0f;
    clear_cmd.clear_color.f32[2] = 0.0f;
    clear_cmd.clear_color.f32[3] = 1.0f;

    clear_cmd.clear_depth = 0.0f;
    clear_cmd.clear_stencil = 0;

    if(virgl_encode_clear(cmd, &clear_cmd) != 0) {
        PRINTLOG(VIRTIOGPU, LOG_ERROR, "failed to encode clear");
        return -1;
    }

    if(virtio_gpu_queue_send_commad(0, lock, fence_id, cmd) != 0) {
        PRINTLOG(VIRTIOGPU, LOG_ERROR, "failed to send command");
        return -1;
    }

    return 0;
}

void virtio_gpu_display_flush(uint32_t scanout, uint64_t buf_offset, uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    if(width == 0 || height == 0) {
        return;
    }

    lock_acquire(virtio_gpu_wrapper->flush_lock);
    virtio_gpu_wrapper->lock = NULL;

    int8_t res = 0;

    if(!virtio_gpu_dont_transfer_on_flush) {
        res = virtio_gpu_queue_send_transfer3d(0, NULL,
                                               1, virtio_gpu_wrapper->resource_ids[scanout], virtio_gpu_wrapper->fence_ids[scanout]++,
                                               buf_offset, x, y, width, height,
                                               0, 0, 0);
        // width * sizeof(pixel_t), width * height * sizeof(pixel_t), 0); // FIXME: stride and layer_stride

        if(!res) {
            // TODO: handle error
            // force flush neverless
        }
    }

    virgl_cmd_t* cmd = virgl_renderer_get_cmd(virtio_gpu_wrapper->renderer);

    if(virgl_cmd_flush_commands(cmd) != 0) {
        // TODO: handle error
        return;
    }

    res = virtio_gpu_queue_send_flush(0, NULL,
                                      1, virtio_gpu_wrapper->resource_ids[scanout], virtio_gpu_wrapper->fence_ids[scanout]++,
                                      x, y, width, height);

    if(!res) {
        // TODO: handle error
        lock_release(virtio_gpu_wrapper->flush_lock);
        return;
    }

    lock_release(virtio_gpu_wrapper->flush_lock);
}

int8_t virtio_gpu_display_init(uint32_t scanout) {
    virtio_gpu_wrapper->flush_lock = lock_create();

    memory_heap_t* heap = memory_get_default_heap();

    virtio_gpu_wrapper->renderer = virgl_renderer_create(heap,
                                                         1,
                                                         0,
                                                         &virtio_gpu_wrapper->lock,
                                                         &virtio_gpu_wrapper->fence_ids[0],
                                                         virtio_gpu_queue_send_commad);

    virtio_dev_t* virtio_gpu_dev = virtio_gpu_wrapper->vgpu;
    virtio_gpu_config_t* vcfg = (virtio_gpu_config_t*)virtio_gpu_dev->device_config;

    virtio_queue_ext_t* vq_control = &virtio_gpu_dev->queues[0];
    virtio_queue_avail_t* avail = virtio_queue_get_avail(virtio_gpu_dev, vq_control->vq);
    virtio_queue_descriptor_t* descs = virtio_queue_get_desc(virtio_gpu_dev, vq_control->vq);

    uint16_t desc_index;
    uint8_t* offset;
    future_t* fut;
    virtio_gpu_ctrl_hdr_t* hdr;

    /* start get edid */

    desc_index = avail->ring[avail->index % virtio_gpu_dev->queue_size];

    offset = (uint8_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(descs[desc_index].address);
    virtio_gpu_get_edid_t* edid_get_hdr = (virtio_gpu_get_edid_t*)offset;

    PRINTLOG(VIRTIOGPU, LOG_TRACE, "virtio gpu get edid info");

    edid_get_hdr->hdr.type = VIRTIO_GPU_CMD_GET_EDID;
    edid_get_hdr->hdr.flags = 0;
    edid_get_hdr->hdr.fence_id = 0;
    edid_get_hdr->hdr.ctx_id = 1;
    edid_get_hdr->scanout = scanout;
    edid_get_hdr->padding = 0;

    descs[desc_index].length = sizeof(virtio_gpu_get_edid_t);

    virtio_gpu_wrapper->lock = lock_create_for_future(0);
    fut = future_create(virtio_gpu_wrapper->lock);

    avail->index++;
    vq_control->nd->vqn = 0;

    future_get_data_and_destroy(fut);

    desc_index = descs[desc_index].next;


    offset = (uint8_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(descs[desc_index].address);
    virtio_gpu_resp_edid_t* edid = (virtio_gpu_resp_edid_t*)offset;

    PRINTLOG(VIRTIOGPU, LOG_TRACE, "queue desc length: %x flags: %x", descs[desc_index].length, descs[desc_index].flags);

    if(edid->hdr.type != VIRTIO_GPU_RESP_OK_EDID) {
        PRINTLOG(VIRTIOGPU, LOG_ERROR, "virtio gpu get edid failed: 0x%x", edid->hdr.type);

        return -1;
    }

    video_edid_get_max_resolution(edid->edid, &virtio_gpu_wrapper->screen_width, &virtio_gpu_wrapper->screen_height);

    PRINTLOG(VIRTIOGPU, LOG_INFO, "virtio gpu get edid success: %dx%d", virtio_gpu_wrapper->screen_width, virtio_gpu_wrapper->screen_height);

    memory_memclean(offset, sizeof(virtio_gpu_resp_edid_t));

    /* end get edid */

    /* start get display info */

    desc_index = avail->ring[avail->index % virtio_gpu_dev->queue_size];

    offset = (uint8_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(descs[desc_index].address);
    hdr = (virtio_gpu_ctrl_hdr_t*)offset;

    PRINTLOG(VIRTIOGPU, LOG_TRACE, "virtio gpu get display info");

    hdr->type = VIRTIO_GPU_CMD_GET_DISPLAY_INFO;
    hdr->flags = 0;
    hdr->fence_id = 0;
    hdr->ctx_id = 1;
    // hdr->padding = 0;

    descs[desc_index].length = sizeof(virtio_gpu_ctrl_hdr_t);

    virtio_gpu_wrapper->lock = lock_create_for_future(0);
    fut = future_create(virtio_gpu_wrapper->lock);

    avail->index++;
    vq_control->nd->vqn = 0;

    future_get_data_and_destroy(fut);

    desc_index = descs[desc_index].next;


    offset = (uint8_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(descs[desc_index].address);
    virtio_gpu_resp_display_info_t* info = (virtio_gpu_resp_display_info_t*)offset;

    PRINTLOG(VIRTIOGPU, LOG_TRACE, "queue desc length: %x flags: %x", descs[desc_index].length, descs[desc_index].flags);

    if(info->hdr.type != VIRTIO_GPU_RESP_OK_DISPLAY_INFO) {
        PRINTLOG(VIRTIOGPU, LOG_ERROR, "virtio gpu get display info failed: 0x%x", info->hdr.type);

        return -1;
    }

    memory_memclean(offset, sizeof(virtio_gpu_resp_display_info_t));

    /* end get display info */

    /* start capset */
    PRINTLOG(VIRTIOGPU, LOG_INFO, "virtio capset count: %d", vcfg->num_capsets);


    /* end capset */

    int8_t res = 0;

    /* start context create */

    desc_index = avail->ring[avail->index % virtio_gpu_dev->queue_size];

    offset = (uint8_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(descs[desc_index].address);

    virtio_gpu_ctx_create_t* ctx_create_hdr = (virtio_gpu_ctx_create_t*)offset;

    ctx_create_hdr->hdr.type = VIRTIO_GPU_CMD_CTX_CREATE;
    ctx_create_hdr->hdr.flags = 0;
    ctx_create_hdr->hdr.fence_id = 0;
    ctx_create_hdr->hdr.ctx_id = 1;
    // ctx_create_hdr->hdr.padding = 0;
    ctx_create_hdr->nlen = 0;

    descs[desc_index].length = sizeof(virtio_gpu_ctx_create_t);

    res = virtio_gpu_wait_for_queue_command(0, &virtio_gpu_wrapper->lock, desc_index);

    if(res != 0) {
        PRINTLOG(VIRTIOGPU, LOG_ERROR, "virtio gpu context create failed");
        return -1;
    }

    /* end context create */

    /* start resource create */

    uint32_t screen_resource_id = virgl_renderer_get_next_resource_id(virtio_gpu_wrapper->renderer);
    virtio_gpu_wrapper->resource_ids[scanout] = screen_resource_id;
    virtio_gpu_wrapper->fence_ids[scanout] = 1;

    res = virtio_gpu_queue_context_create_3d_resource(0, &virtio_gpu_wrapper->lock,
                                                      1, screen_resource_id, virtio_gpu_wrapper->fence_ids[scanout]++,
                                                      virtio_gpu_wrapper->screen_width, virtio_gpu_wrapper->screen_height, false);

    if(res != 0) {
        PRINTLOG(VIRTIOGPU, LOG_ERROR, "virtio gpu context create 3d resource failed");
        return -1;
    }

    /* end resource create */

    /* start resource attach context */

    res = virtio_gpu_queue_context_attach_resource(0, &virtio_gpu_wrapper->lock, 1, screen_resource_id);

    if(res != 0) {
        PRINTLOG(VIRTIOGPU, LOG_ERROR, "virtio gpu context attach resource failed");
        return -1;
    }

    /* end resource attach context */

    /* start resource attach backing */

    uint64_t screen_size = virtio_gpu_wrapper->screen_width * virtio_gpu_wrapper->screen_height * sizeof(pixel_t);
    uint64_t screen_frm_cnt = (screen_size + FRAME_SIZE - 1) / FRAME_SIZE;

    frame_t* screen_frm = NULL;

    if(frame_get_allocator()->allocate_frame_by_count(frame_get_allocator(),
                                                      screen_frm_cnt,
                                                      FRAME_ALLOCATION_TYPE_BLOCK | FRAME_ALLOCATION_TYPE_RESERVED,
                                                      &screen_frm,
                                                      NULL) != 0) {
        PRINTLOG(VIRTIOGPU, LOG_ERROR, "failed to allocate screen frame");

        return -1;
    }

    uint64_t screen_fa = screen_frm->frame_address;
    uint64_t screen_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(screen_frm->frame_address);

    memory_paging_add_va_for_frame(screen_va, screen_frm, MEMORY_PAGING_PAGE_TYPE_NOEXEC);
    memory_memclean((void*)screen_va, screen_size);


    res = virtio_gpu_queue_attach_backing(0, &virtio_gpu_wrapper->lock,
                                          1, 0, screen_resource_id, screen_fa, screen_size);

    if(res != 0) {
        PRINTLOG(VIRTIOGPU, LOG_ERROR, "virtio gpu attach backing failed");
        return -1;
    }

    /* end resource attach backing */

    /* start transfer to host 3d */

    uint32_t stride = virtio_gpu_wrapper->screen_width * sizeof(pixel_t);
    uint32_t layer_stride = virtio_gpu_wrapper->screen_width * virtio_gpu_wrapper->screen_height * sizeof(pixel_t);

    res = virtio_gpu_queue_send_transfer3d(0, &virtio_gpu_wrapper->lock,
                                           1, screen_resource_id, 0,
                                           0, 0, 0, virtio_gpu_wrapper->screen_width, virtio_gpu_wrapper->screen_height,
                                           stride, layer_stride, 0);

    if(res != 0) {
        PRINTLOG(VIRTIOGPU, LOG_ERROR, "virtio gpu transfer to host 3d failed");
        return -1;
    }

    /* end transfer to host 3d */

    /* start create surface */

    res = virtio_gpu_create_surface(screen_resource_id, true,
                                    virtio_gpu_wrapper->fence_ids[scanout]++,
                                    &virtio_gpu_wrapper->lock);

    if(res != 0) {
        PRINTLOG(VIRTIOGPU, LOG_ERROR, "virtio gpu create surface failed");
        return -1;
    }

    /* end create surface */

    /* start set scanout */

    desc_index = avail->ring[avail->index % virtio_gpu_dev->queue_size];
    offset = (uint8_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(descs[desc_index].address);

    virtio_gpu_set_scanout_t* scanout_hdr = (virtio_gpu_set_scanout_t*)offset;

    scanout_hdr->hdr.type = VIRTIO_GPU_CMD_SET_SCANOUT;
    scanout_hdr->hdr.flags = 0;
    scanout_hdr->hdr.fence_id = 0;
    scanout_hdr->hdr.ctx_id = 1;
    // scanout_hdr->hdr.padding = 0;

    scanout_hdr->rec.x = 0;
    scanout_hdr->rec.y = 0;
    scanout_hdr->rec.width = virtio_gpu_wrapper->screen_width;
    scanout_hdr->rec.height = virtio_gpu_wrapper->screen_height;

    scanout_hdr->scanout_id = scanout;
    scanout_hdr->resource_id = screen_resource_id;

    descs[desc_index].length = sizeof(virtio_gpu_set_scanout_t);

    res = virtio_gpu_wait_for_queue_command(0, &virtio_gpu_wrapper->lock, desc_index);

    if(res != 0) {
        PRINTLOG(VIRTIOGPU, LOG_ERROR, "virtio gpu set scanout failed");
        return -1;
    }

    /* end set scanout */

    if(scanout == 0) {
        SYSTEM_INFO->frame_buffer->width = virtio_gpu_wrapper->screen_width;
        SYSTEM_INFO->frame_buffer->height = virtio_gpu_wrapper->screen_height;
        SYSTEM_INFO->frame_buffer->buffer_size = screen_size;
        SYSTEM_INFO->frame_buffer->pixels_per_scanline = virtio_gpu_wrapper->screen_width;
        SYSTEM_INFO->frame_buffer->physical_base_address = screen_fa;
        SYSTEM_INFO->frame_buffer->virtual_base_address = screen_va;

        PRINTLOG(VIRTIOGPU, LOG_INFO, "new screen frame buffer address 0x%llx", screen_va);

        video_copy_contents_to_frame_buffer((uint8_t*)screen_va, virtio_gpu_wrapper->screen_width, virtio_gpu_wrapper->screen_height, virtio_gpu_wrapper->screen_width);
        video_refresh_frame_buffer_address();
        VIDEO_DISPLAY_FLUSH = virtio_gpu_display_flush;
        VIDEO_CLEAR_SCREEN_AREA = virtio_gpu_clear_screen_area;
    }

    VIDEO_DISPLAY_FLUSH(scanout, 0, 0, 0, virtio_gpu_wrapper->screen_width, virtio_gpu_wrapper->screen_height);

    return 0;
}

static void mouse_move_internal(virtio_gpu_ctrl_type_t type, uint32_t x, uint32_t y) {
    virtio_dev_t* virtio_gpu_dev = virtio_gpu_wrapper->vgpu;

    virtio_queue_ext_t* vq_control = &virtio_gpu_dev->queues[1];
    virtio_queue_avail_t* avail = virtio_queue_get_avail(virtio_gpu_dev, vq_control->vq);
    virtio_queue_descriptor_t* descs = virtio_queue_get_desc(virtio_gpu_dev, vq_control->vq);

    uint16_t desc_index;
    uint8_t* offset;
    desc_index = avail->ring[avail->index % virtio_gpu_dev->queue_size];
    offset = (uint8_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(descs[desc_index].address);

    virtio_gpu_update_cursor_t* update_cursor_hdr = (virtio_gpu_update_cursor_t*)offset;

    update_cursor_hdr->hdr.type = type;
    update_cursor_hdr->hdr.flags = VIRTIO_GPU_FLAG_FENCE;
    update_cursor_hdr->hdr.fence_id = virtio_gpu_wrapper->fence_ids[0]++;
    update_cursor_hdr->hdr.ctx_id = 1;
    // update_cursor_hdr->hdr.padding = 0;

    update_cursor_hdr->resource_id = virtio_gpu_wrapper->mouse_resource_id;
    update_cursor_hdr->pos.x = x;
    update_cursor_hdr->pos.y = y;
    update_cursor_hdr->hot_x = 0;
    update_cursor_hdr->hot_y = 0;

    descs[desc_index].length = sizeof(virtio_gpu_update_cursor_t);

    int8_t res = 0;

    res = virtio_gpu_wait_for_queue_command(1, &virtio_gpu_wrapper->cursor_lock, desc_index);

    if(res != 0) {
        // TODO: handle error
    }
}

void virtio_gpu_clear_screen_area(uint32_t x, uint32_t y, uint32_t width, uint32_t height, color_t background) {
    memory_heap_t* heap = memory_get_default_heap();

    virgl_cmd_t* cmd = virgl_renderer_get_cmd(virtio_gpu_wrapper->renderer);

    virgl_cmd_clear_texture_t* clear_texture = memory_malloc_ext(heap, sizeof(virgl_cmd_clear_texture_t), 0);

    if(clear_texture == NULL) {
        return;
    }

    clear_texture->texture_id = virtio_gpu_wrapper->resource_ids[0];
    clear_texture->level = 0;
    clear_texture->box.x = x;
    clear_texture->box.y = y;
    clear_texture->box.z = 0;
    clear_texture->box.w = width;
    clear_texture->box.h = height;
    clear_texture->box.d = 1;

    clear_texture->clear_color.f32[0] = background.red / 255.0f;
    clear_texture->clear_color.f32[1] = background.green / 255.0f;
    clear_texture->clear_color.f32[2] = background.blue / 255.0f;
    clear_texture->clear_color.f32[3] = background.alpha / 255.0f;

    if(virgl_encode_clear_texture(cmd, clear_texture) != 0) {
        PRINTLOG(VIRTIOGPU, LOG_ERROR, "failed to encode clear texture");
        memory_free_ext(heap, clear_texture);
        return;
    }

    memory_free_ext(heap, clear_texture);
}

static void virtio_gpu_scrool_screen(void) {
    font_table_t* font_table = font_get_font_table();
    memory_heap_t* heap = memory_get_default_heap();

    virgl_cmd_t* cmd = virgl_renderer_get_cmd(virtio_gpu_wrapper->renderer);

    virgl_copy_region_t* copy_region = memory_malloc_ext(heap, sizeof(virgl_copy_region_t), 0);

    if(copy_region == NULL) {
        return;
    }

    copy_region->src_resource_id = virtio_gpu_wrapper->resource_ids[0];
    copy_region->src_level = 0;
    copy_region->src_box.x = 0;
    copy_region->src_box.y = font_table->font_height;
    copy_region->src_box.z = 0;
    copy_region->src_box.w = virtio_gpu_wrapper->screen_width;
    copy_region->src_box.h = virtio_gpu_wrapper->screen_height - font_table->font_height;
    copy_region->src_box.d = 1;


    copy_region->dst_resource_id = virtio_gpu_wrapper->resource_ids[0];
    copy_region->dst_level = 0;
    copy_region->dst_x = 0;
    copy_region->dst_y = 0;
    copy_region->dst_z = 0;

    if(virgl_encode_copy_region(cmd, copy_region) != 0) {
        PRINTLOG(VIRTIOGPU, LOG_ERROR, "failed to encode copy region");
        memory_free_ext(heap, copy_region);
        return;
    }

    // copy empty line

    memory_memclean(copy_region, sizeof(virgl_copy_region_t));

    copy_region->src_resource_id = virtio_gpu_wrapper->font_empty_line_resource_id;
    copy_region->src_level = 0;
    copy_region->src_box.x = 0;
    copy_region->src_box.y = 0;
    copy_region->src_box.z = 0;
    copy_region->src_box.w =  virtio_gpu_wrapper->screen_width;
    copy_region->src_box.h = font_table->font_height;
    copy_region->src_box.d = 1;


    copy_region->dst_resource_id = virtio_gpu_wrapper->resource_ids[0];
    copy_region->dst_level = 0;
    copy_region->dst_x = 0;
    copy_region->dst_y = virtio_gpu_wrapper->screen_height - font_table->font_height;
    copy_region->dst_z = 0;

    if(virgl_encode_copy_region(cmd, copy_region) != 0) {
        PRINTLOG(VIRTIOGPU, LOG_ERROR, "failed to encode copy region");
        memory_free_ext(heap, copy_region);
        return;
    }

    memory_free_ext(heap, copy_region);
}

static void virtio_gpu_print_glyph_with_stride(wchar_t wc,
                                               color_t foreground, color_t background,
                                               pixel_t* destination_base_address,
                                               uint32_t x, uint32_t y,
                                               uint32_t stride) {
    UNUSED(background);
    UNUSED(foreground);

    UNUSED(destination_base_address);
    UNUSED(stride);

    memory_heap_t* heap = memory_get_default_heap();

    font_table_t* font_table = font_get_font_table();

    if(wc >= font_table->glyph_count){
        wc = 0;
    }

    virgl_cmd_t* cmd = virgl_renderer_get_cmd(virtio_gpu_wrapper->renderer);

    virgl_copy_region_t* copy_region = memory_malloc_ext(heap, sizeof(virgl_copy_region_t), 0);

    if(copy_region == NULL) {
        return;
    }

    copy_region->src_resource_id = virtio_gpu_wrapper->font_resource_id;
    copy_region->src_level = 0;
    copy_region->src_box.x = font_table->font_width * (wc % font_table->column_count);
    copy_region->src_box.y = font_table->font_height * (wc / font_table->column_count);
    copy_region->src_box.z = 0;
    copy_region->src_box.w = font_table->font_width;
    copy_region->src_box.h = font_table->font_height;
    copy_region->src_box.d = 1;


    copy_region->dst_resource_id = virtio_gpu_wrapper->resource_ids[0];
    copy_region->dst_level = 0;
    copy_region->dst_x = x * font_table->font_width;
    copy_region->dst_y = y * font_table->font_height;
    copy_region->dst_z = 0;

    if(virgl_encode_copy_region(cmd, copy_region) != 0) {
        PRINTLOG(VIRTIOGPU, LOG_ERROR, "failed to encode copy region");
        memory_free_ext(heap, copy_region);
        return;
    }

    memory_free_ext(heap, copy_region);
}

static void* virtio_gpu_font_empty_line_value = NULL;

static int8_t virtio_gpu_font_init_empty_line(void) {
    font_table_t* font_table = font_get_font_table();
    uint32_t font_empty_line_res_width = virtio_gpu_wrapper->screen_width;
    uint32_t font_empty_line_res_height = font_table->font_height;
    uint32_t font_empty_line_res_size = font_empty_line_res_width * font_empty_line_res_height * sizeof(pixel_t);

    uint64_t font_empty_line_phy_addr = 0;

    virtio_gpu_font_empty_line_value = memory_malloc(font_empty_line_res_size);

    if(virtio_gpu_font_empty_line_value == NULL) {
        PRINTLOG(VIRTIOGPU, LOG_ERROR, "failed to allocate font_empty_line value");
        return -1;
    }

    if(memory_paging_get_physical_address((uint64_t)virtio_gpu_font_empty_line_value, &font_empty_line_phy_addr) != 0) {
        PRINTLOG(VIRTIOGPU, LOG_ERROR, "failed to get font_empty_line physical address");
        return -1;
    }

    PRINTLOG(VIRTIOGPU, LOG_INFO, "font_empty_line physical address: 0x%llx", font_empty_line_phy_addr);
    PRINTLOG(VIRTIOGPU, LOG_INFO, "font_empty_line dimension: %dx%d", font_empty_line_res_width, font_empty_line_res_height);
    PRINTLOG(VIRTIOGPU, LOG_INFO, "font_empty_line size: %d", font_empty_line_res_size);

    uint32_t font_empty_line_resource_id = virgl_renderer_get_next_resource_id(virtio_gpu_wrapper->renderer);
    virtio_gpu_wrapper->font_empty_line_resource_id = font_empty_line_resource_id;

    int8_t res = 0;

    /* start resource create */

    res = virtio_gpu_queue_context_create_3d_resource(0, &virtio_gpu_wrapper->lock,
                                                      1, font_empty_line_resource_id, virtio_gpu_wrapper->fence_ids[0]++,
                                                      font_empty_line_res_width, font_empty_line_res_height,
                                                      false);

    if(res != 0) {
        PRINTLOG(VIRTIOGPU, LOG_ERROR, "virtio gpu context create 3d resource for font_empty_line failed");
        return -1;
    }

    /* end resource create */

    /* start resource attach context */

    res = virtio_gpu_queue_context_attach_resource(0, &virtio_gpu_wrapper->lock, 1, font_empty_line_resource_id);

    /* end resource attach context */

    res = virtio_gpu_queue_attach_backing(0, &virtio_gpu_wrapper->lock, 1, virtio_gpu_wrapper->fence_ids[0]++,
                                          font_empty_line_resource_id, font_empty_line_phy_addr, font_empty_line_res_size);

    if(res != 0) {
        PRINTLOG(VIRTIOGPU, LOG_ERROR, "virtio gpu attach backing for font_empty_line failed");
        return -1;
    }

    uint32_t stride = font_empty_line_res_width * sizeof(pixel_t);
    uint32_t layer_stride = font_empty_line_res_width * font_empty_line_res_height * sizeof(pixel_t);

    res = virtio_gpu_queue_send_transfer3d(0, &virtio_gpu_wrapper->lock,
                                           1, font_empty_line_resource_id, virtio_gpu_wrapper->fence_ids[0]++,
                                           0, 0, 0, font_empty_line_res_width, font_empty_line_res_height,
                                           stride, layer_stride, 0);

    if(res != 0) {
        PRINTLOG(VIRTIOGPU, LOG_ERROR, "virtio gpu transfer to host 3d for font_empty_line failed");
        return -1;
    }

    return 0;
}

int8_t virtio_gpu_font_init(void) {
    font_table_t* font_table = font_get_font_table();
    uint32_t font_res_width = font_table->font_width * font_table->column_count;
    uint32_t font_res_height = font_table->font_height * font_table->row_count;
    uint32_t font_res_size = font_res_width * font_res_height * sizeof(pixel_t);

    uint64_t font_phy_addr = 0;

    if(memory_paging_get_physical_address((uint64_t)font_table->bitmap, &font_phy_addr) != 0) {
        PRINTLOG(VIRTIOGPU, LOG_ERROR, "failed to get font physical address");
        return -1;
    }

    PRINTLOG(VIRTIOGPU, LOG_INFO, "font physical address: 0x%llx", font_phy_addr);

    uint32_t font_resource_id = virgl_renderer_get_next_resource_id(virtio_gpu_wrapper->renderer);
    virtio_gpu_wrapper->font_resource_id = font_resource_id;

    int8_t res = 0;

    /* start resource create */

    res = virtio_gpu_queue_context_create_3d_resource(0, &virtio_gpu_wrapper->lock,
                                                      1, font_resource_id, virtio_gpu_wrapper->fence_ids[0]++,
                                                      font_res_width, font_res_height,
                                                      false);

    if(res != 0) {
        PRINTLOG(VIRTIOGPU, LOG_ERROR, "virtio gpu context create 3d resource for font failed");
        return -1;
    }

    /* end resource create */

    /* start resource attach context */

    res = virtio_gpu_queue_context_attach_resource(0, &virtio_gpu_wrapper->lock, 1, font_resource_id);

    /* end resource attach context */

    res = virtio_gpu_queue_attach_backing(0, &virtio_gpu_wrapper->lock, 1, virtio_gpu_wrapper->fence_ids[0]++,
                                          font_resource_id, font_phy_addr, font_res_size);

    if(res != 0) {
        PRINTLOG(VIRTIOGPU, LOG_ERROR, "virtio gpu attach backing for font failed");
        return -1;
    }

    uint32_t stride = font_res_width * sizeof(pixel_t);
    uint32_t layer_stride = font_res_width * font_res_height * sizeof(pixel_t);

    res = virtio_gpu_queue_send_transfer3d(0, &virtio_gpu_wrapper->lock,
                                           1, font_resource_id, virtio_gpu_wrapper->fence_ids[0]++,
                                           0, 0, 0, font_res_width, font_res_height,
                                           stride, layer_stride, 0);

    if(res != 0) {
        PRINTLOG(VIRTIOGPU, LOG_ERROR, "virtio gpu transfer to host 3d for font failed");
        return -1;
    }

    if(virtio_gpu_font_init_empty_line() != 0) {
        PRINTLOG(VIRTIOGPU, LOG_ERROR, "failed to init font empty line");
        return -1;
    }

    virtio_gpu_dont_transfer_on_flush = true;
    VIDEO_PRINT_GLYPH_WITH_STRIDE = virtio_gpu_print_glyph_with_stride;
    VIDEO_SCROLL_SCREEN = virtio_gpu_scrool_screen;

    return 0;
}


int8_t mouse_init(void) {
    graphics_raw_image_t* mouse_image = video_get_mouse_image();

    if(mouse_image == NULL || mouse_image->data == NULL) {
        PRINTLOG(VIRTIOGPU, LOG_ERROR, "failed to get mouse image");
        return -1;
    }

    if(mouse_image->width != 64 || mouse_image->height != 64) {
        PRINTLOG(VIRTIOGPU, LOG_ERROR, "mouse image must be 64x64");

        return -1;
    }

    virtio_gpu_wrapper->mouse_width = mouse_image->width;
    virtio_gpu_wrapper->mouse_height = mouse_image->height;

    uint32_t mouse_resource_id = virgl_renderer_get_next_resource_id(virtio_gpu_wrapper->renderer);
    virtio_gpu_wrapper->mouse_resource_id = mouse_resource_id;

    int8_t res = 0;

    /* start resource create */

    res = virtio_gpu_queue_context_create_3d_resource(0, &virtio_gpu_wrapper->lock,
                                                      1, mouse_resource_id, virtio_gpu_wrapper->fence_ids[0]++,
                                                      virtio_gpu_wrapper->mouse_width, virtio_gpu_wrapper->mouse_height,
                                                      true);

    if(res != 0) {
        PRINTLOG(VIRTIOGPU, LOG_ERROR, "virtio gpu context create 3d resource for mouse failed");
        return -1;
    }

    /* end resource create */

    /* start resource attach context */

    res = virtio_gpu_queue_context_attach_resource(0, &virtio_gpu_wrapper->lock, 1, mouse_resource_id);

    /* end resource attach context */

    uint64_t mouse_size = mouse_image->width * mouse_image->height * sizeof(pixel_t);
    uint64_t mouse_frm_cnt = (mouse_size + FRAME_SIZE - 1) / FRAME_SIZE;

    frame_t* mouse_frm = NULL;

    if(frame_get_allocator()->allocate_frame_by_count(frame_get_allocator(),
                                                      mouse_frm_cnt,
                                                      FRAME_ALLOCATION_TYPE_BLOCK | FRAME_ALLOCATION_TYPE_RESERVED,
                                                      &mouse_frm,
                                                      NULL) != 0) {
        PRINTLOG(VIRTIOGPU, LOG_ERROR, "failed to allocate mouse frame");

        return -1;
    }

    uint64_t mouse_fa = mouse_frm->frame_address;
    uint64_t mouse_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(mouse_frm->frame_address);

    memory_paging_add_va_for_frame(mouse_va, mouse_frm, MEMORY_PAGING_PAGE_TYPE_NOEXEC);
    memory_memcopy(mouse_image->data, (void*)mouse_va, mouse_size);

    memory_free(mouse_image->data);
    memory_free(mouse_image);

    res = virtio_gpu_queue_attach_backing(0, &virtio_gpu_wrapper->lock, 1, virtio_gpu_wrapper->fence_ids[0]++,
                                          mouse_resource_id, mouse_fa, mouse_size);

    if(res != 0) {
        PRINTLOG(VIRTIOGPU, LOG_ERROR, "virtio gpu attach backing for mouse failed");
        return -1;
    }

    uint32_t stride = virtio_gpu_wrapper->mouse_width * sizeof(pixel_t);
    uint32_t layer_stride = virtio_gpu_wrapper->mouse_width * virtio_gpu_wrapper->mouse_height * sizeof(pixel_t);

    res = virtio_gpu_queue_send_transfer3d(0, &virtio_gpu_wrapper->lock,
                                           1, mouse_resource_id, virtio_gpu_wrapper->fence_ids[0]++,
                                           0, 0, 0, virtio_gpu_wrapper->mouse_width, virtio_gpu_wrapper->mouse_height,
                                           stride, layer_stride, 0);

    if(res != 0) {
        PRINTLOG(VIRTIOGPU, LOG_ERROR, "virtio gpu transfer to host 3d for mouse failed");
        return -1;
    }

    mouse_move_internal(VIRTIO_GPU_CMD_UPDATE_CURSOR, 0, 0);

    VIDEO_MOVE_CURSOR = mouse_move;

    return 0;
}

void mouse_move(uint32_t x, uint32_t y) {
    mouse_move_internal(VIRTIO_GPU_CMD_MOVE_CURSOR, x, y);
}

uint64_t virtio_gpu_select_features(virtio_dev_t* dev, uint64_t avail_features) {
    UNUSED(dev);
    PRINTLOG(VIRTIOGPU, LOG_INFO, "virtio gpu features: %llx", avail_features);

    return VIRTIO_GPU_F_VIRGL | VIRTIO_GPU_F_EDID | VIRTIO_GPU_F_RESOURCE_UUID;
}

int8_t virtio_gpu_create_queues(virtio_dev_t* vdev) {
    vdev->queues = memory_malloc(sizeof(virtio_queue_ext_t) * 2);

    uint64_t item_size = 4 << 10;

    PRINTLOG(VIRTIOGPU, LOG_TRACE, "building virtio gpu control queue");

    if(virtio_create_queue(vdev, 0, item_size, 0, 1, NULL, &virtio_gpu_controlq_isr, NULL) != 0) {
        PRINTLOG(VIRTIO, LOG_ERROR, "cannot create virtio gpu control queue");

        return -1;
    }

    PRINTLOG(VIRTIOGPU, LOG_TRACE, "virtio gpu control queue builed");

    PRINTLOG(VIRTIOGPU, LOG_TRACE, "building virtio gpu cursor queue");

    item_size = sizeof(virtio_gpu_update_cursor_t);

    if(virtio_create_queue(vdev, 1, item_size, 0, 1, NULL, &virtio_gpu_cursorq_isr, NULL) != 0) {
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

    int8_t res = 0;

    for(uint32_t i = 0; i < vgpu_conf->num_scanouts; i++) {
        res = virtio_gpu_display_init(i);

        if(res != 0) {
            PRINTLOG(VIRTIOGPU, LOG_ERROR, "virtio gpu init failed");

            return -1;
        }
    }

    res = mouse_init();

    if(res != 0) {
        PRINTLOG(VIRTIOGPU, LOG_ERROR, "virtio gpu init failed");

        return -1;
    }

    res = virtio_gpu_font_init();

    if(res != 0) {
        PRINTLOG(VIRTIOGPU, LOG_ERROR, "virtio gpu init failed");

        return -1;
    }

    PRINTLOG(VIRTIOGPU, LOG_INFO, "virtio gpu init success");

    return 0;
}
#pragma GCC diagnostic pop

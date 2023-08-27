/**
 * @file video_virtio.h
 * @brief video support for virtio devices
 */

#ifndef __VIDEO_VIRTIO_H
#define __VIDEO_VIRTIO_H 0

#include <types.h>
#include <memory.h>
#include <pci.h>

#define VIRTIO_GPU_F_VIRGL 0
#define VIRTIO_GPU_F_EDID 1
#define VIRTIO_GPU_F_RESOURCE_UUID 2

typedef struct virtio_gpu_config_t {
    uint32_t events_read;
    uint32_t events_clear;
    uint32_t num_scanouts;
    uint32_t reserved;
} __attribute__((packed)) virtio_gpu_config_t;

typedef enum virtio_gpu_ctrl_type_t {
    VIRTIO_GPU_CMD_GET_DISPLAY_INFO = 0x0100,
    VIRTIO_GPU_CMD_RESOURCE_CREATE_2D = 0x0101,
    VIRTIO_GPU_CMD_RESOURCE_UNREF = 0x0102,
    VIRTIO_GPU_CMD_SET_SCANOUT = 0x0103,
    VIRTIO_GPU_CMD_RESOURCE_FLUSH = 0x0104,
    VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D = 0x0105,
    VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING = 0x0106,
    VIRTIO_GPU_CMD_RESOURCE_DETACH_BACKING = 0x0107,
    VIRTIO_GPU_CMD_GET_CAPSET_INFO = 0x0108,
    VIRTIO_GPU_CMD_GET_CAPSET = 0x0109,
    VIRTIO_GPU_CMD_GET_EDID = 0x010a,

    VIRTIO_GPU_CMD_UPDATE_CURSOR = 0x0300,
    VIRTIO_GPU_CMD_MOVE_CURSOR = 0x0301,

    VIRTIO_GPU_RESP_OK_NODATA = 0x1100,
    VIRTIO_GPU_RESP_OK_DISPLAY_INFO = 0x1101,
    VIRTIO_GPU_RESP_OK_CAPSET_INFO = 0x1102,
    VIRTIO_GPU_RESP_OK_CAPSET = 0x1103,
    VIRTIO_GPU_RESP_OK_EDID = 0x1104,

    VIRTIO_GPU_RESP_ERR_UNSPEC = 0x1200,
    VIRTIO_GPU_RESP_ERR_OUT_OF_MEMORY = 0x1201,
    VIRTIO_GPU_RESP_ERR_INVALID_SCANOUT_ID = 0x1202,
    VIRTIO_GPU_RESP_ERR_INVALID_RESOURCE_ID = 0x1203,
    VIRTIO_GPU_RESP_ERR_INVALID_CONTEXT_ID = 0x1204,
    VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER = 0x1205,
} virtio_gpu_ctrl_type_t;

#define VIRTIO_GPU_FLAG_FENCE (1 << 0)

typedef struct virtio_gpu_ctrl_hdr_t {
    volatile uint32_t type;
    uint32_t          flags;
    uint64_t          fence_id;
    uint32_t          ctx_id;
    uint32_t          padding;
} __attribute__((packed)) virtio_gpu_ctrl_hdr_t;

#define VIRTIO_GPU_MAX_SCANOUTS 16

typedef struct virtio_gpu_rect_t {
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
} __attribute__((packed)) virtio_gpu_rect_t;

typedef struct virtio_gpu_display_one_t {
    virtio_gpu_rect_t rect;
    uint32_t          enabled;
    uint32_t          flags;
} __attribute__((packed)) virtio_gpu_display_one_t;

typedef struct virtio_gpu_resp_display_info_t {
    virtio_gpu_ctrl_hdr_t    hdr;
    virtio_gpu_display_one_t pmodes[VIRTIO_GPU_MAX_SCANOUTS];
} __attribute__((packed)) virtio_gpu_resp_display_info_t;

typedef struct virtio_gpu_get_edid_t {
    virtio_gpu_ctrl_hdr_t hdr;
    uint32_t              scanout;
    uint32_t              padding;
} __attribute__((packed)) virtio_gpu_get_edid_t;

typedef struct virtio_gpu_resp_edid_t {
    virtio_gpu_ctrl_hdr_t hdr;
    uint32_t              size;
    uint32_t              padding;
    uint8_t               edid[1024];
} __attribute__((packed)) virtio_gpu_resp_edid_t;

typedef enum virtio_gpu_formats_t {
    VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM = 1,
    VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM = 2,
    VIRTIO_GPU_FORMAT_A8R8G8B8_UNORM = 3,
    VIRTIO_GPU_FORMAT_X8R8G8B8_UNORM = 4,
    VIRTIO_GPU_FORMAT_R8G8B8A8_UNORM = 67,
    VIRTIO_GPU_FORMAT_X8B8G8R8_UNORM = 68,
    VIRTIO_GPU_FORMAT_A8B8G8R8_UNORM = 121,
    VIRTIO_GPU_FORMAT_R8G8B8X8_UNORM = 134,
} virtio_gpu_formats_t;

typedef struct virtio_gpu_resource_create_2d_t {
    virtio_gpu_ctrl_hdr_t hdr;
    uint32_t              resource_id;
    uint32_t              format;
    uint32_t              width;
    uint32_t              height;
} __attribute__((packed)) virtio_gpu_resource_create_2d_t;

typedef struct virtio_gpu_resource_unref_t {
    virtio_gpu_ctrl_hdr_t hdr;
    uint32_t              resource_id;
    uint32_t              padding;
} __attribute__((packed)) virtio_gpu_resource_unref_t;

typedef struct virtio_gpu_set_scanout_t {
    virtio_gpu_ctrl_hdr_t hdr;
    virtio_gpu_rect_t     rec;
    uint32_t              scanout_id;
    uint32_t              resource_id;
} __attribute__((packed)) virtio_gpu_set_scanout_t;

typedef struct virtio_gpu_resource_flush_t {
    virtio_gpu_ctrl_hdr_t hdr;
    virtio_gpu_rect_t     rec;
    uint32_t              resource_id;
    uint32_t              padding;
} __attribute__((packed)) virtio_gpu_resource_flush_t;

typedef struct virtio_gpu_transfer_to_host_2d_t {
    virtio_gpu_ctrl_hdr_t hdr;
    virtio_gpu_rect_t     rec;
    uint64_t              offset;
    uint32_t              resource_id;
    uint32_t              padding;
} __attribute__((packed)) virtio_gpu_transfer_to_host_2d_t;

typedef struct virtio_gpu_mem_entry_t {
    uint64_t addr;
    uint32_t length;
    uint32_t padding;
} __attribute__((packed)) virtio_gpu_mem_entry_t;

typedef struct virtio_gpu_resource_attach_backing_t {
    virtio_gpu_ctrl_hdr_t  hdr;
    uint32_t               resource_id;
    uint32_t               nr_entries;
    virtio_gpu_mem_entry_t entries[];
} __attribute__((packed)) virtio_gpu_resource_attach_backing_t;

typedef struct virtio_gpu_resource_detach_backing_t {
    virtio_gpu_ctrl_hdr_t hdr;
    uint32_t              resource_id;
    uint32_t              padding;
} __attribute__((packed)) virtio_gpu_resource_detach_backing_t;

typedef struct virtio_gpu_cursor_pos_t {
    uint32_t scanout;
    uint32_t x;
    uint32_t y;
    uint32_t padding;
} __attribute__((packed)) virtio_gpu_cursor_pos_t;

typedef struct virtio_gpu_update_cursor_t {
    virtio_gpu_ctrl_hdr_t   hdr;
    virtio_gpu_cursor_pos_t pos;
    uint32_t                resource_id;
    uint32_t                hot_x;
    uint32_t                hot_y;
    uint32_t                padding;
} __attribute__((packed)) virtio_gpu_update_cursor_t;

int8_t virtio_video_init(memory_heap_t* heap, const pci_dev_t* device);


#endif /* __VIDEO_VIRTIO_H */

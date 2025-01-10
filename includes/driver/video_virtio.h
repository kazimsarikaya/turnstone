/**
 * @file video_virtio.h
 * @brief video support for virtio devices
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef __VIDEO_VIRTIO_H
#define __VIDEO_VIRTIO_H 0

#include <types.h>
#include <driver/virtio.h>
#include <memory.h>
#include <pci.h>
#include <graphics/virgl.h>

#define VIRTIO_GPU_F_VIRGL 0
#define VIRTIO_GPU_F_EDID 1
#define VIRTIO_GPU_F_RESOURCE_UUID 2
#define VIRTIO_GPU_F_RESOURCE_BLOB 3
#define VIRTIO_GPU_F_CONTEXT_INIT 4

typedef struct virtio_gpu_config_t {
    uint32_t events_read;
    uint32_t events_clear;
    uint32_t num_scanouts;
    uint32_t num_capsets;
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
    VIRTIO_GPU_CMD_RESOURCE_ASSIGN_UUID = 0x010b,
    VIRTIO_GPU_CMD_RESOURCE_CREATE_BLOB = 0x010c,
    VIRTIO_GPU_CMD_SET_SCANOUT_BLOB = 0x010d,

    VIRTIO_GPU_CMD_CTX_CREATE = 0x0200,
    VIRTIO_GPU_CMD_CTX_DESTROY = 0x0201,
    VIRTIO_GPU_CMD_CTX_ATTACH_RESOURCE = 0x0202,
    VIRTIO_GPU_CMD_CTX_DETACH_RESOURCE = 0x0203,
    VIRTIO_GPU_CMD_RESOURCE_CREATE_3D = 0x0204,
    VIRTIO_GPU_CMD_TRANSFER_TO_HOST_3D = 0x0205,
    VIRTIO_GPU_CMD_TRANSFER_FROM_HOST_3D = 0x0206,
    VIRTIO_GPU_CMD_SUBMIT_3D = 0x0207,
    VIRTIO_GPU_CMD_RESOURCE_MAP_BLOB = 0x0208,
    VIRTIO_GPU_CMD_RESOURCE_UNMAP_BLOB = 0x0209,

    VIRTIO_GPU_CMD_UPDATE_CURSOR = 0x0300,
    VIRTIO_GPU_CMD_MOVE_CURSOR = 0x0301,

    VIRTIO_GPU_RESP_OK_NODATA = 0x1100,
    VIRTIO_GPU_RESP_OK_DISPLAY_INFO = 0x1101,
    VIRTIO_GPU_RESP_OK_CAPSET_INFO = 0x1102,
    VIRTIO_GPU_RESP_OK_CAPSET = 0x1103,
    VIRTIO_GPU_RESP_OK_EDID = 0x1104,
    VIRTIO_GPU_RESP_OK_RESOURCE_UUID = 0x1105,
    VIRTIO_GPU_RESP_OK_MAP_INFO = 0x1106,

    VIRTIO_GPU_RESP_ERR_UNSPEC = 0x1200,
    VIRTIO_GPU_RESP_ERR_OUT_OF_MEMORY = 0x1201,
    VIRTIO_GPU_RESP_ERR_INVALID_SCANOUT_ID = 0x1202,
    VIRTIO_GPU_RESP_ERR_INVALID_RESOURCE_ID = 0x1203,
    VIRTIO_GPU_RESP_ERR_INVALID_CONTEXT_ID = 0x1204,
    VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER = 0x1205,
} virtio_gpu_ctrl_type_t;

#define VIRTIO_GPU_FLAG_FENCE (1 << 0)
#define VIRTIO_GPU_FLAG_INFO_RING_IDX (1 << 1)

typedef struct virtio_gpu_ctrl_hdr_t {
    volatile uint32_t type;
    uint32_t          flags;
    uint64_t          fence_id;
    uint32_t          ctx_id;
    uint8_t           ring_idx;
    uint8_t           padding[3];
} __attribute__((packed)) virtio_gpu_ctrl_hdr_t;

_Static_assert(sizeof(virtio_gpu_ctrl_hdr_t) == 24, "virtio_gpu_ctrl_hdr_t size is not correct");

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

typedef struct virtio_gpu_get_capset_info_t {
    virtio_gpu_ctrl_hdr_t hdr;
    uint32_t              capset_index;
    uint32_t              padding;
} __attribute__((packed)) virtio_gpu_get_capset_info_t;

#define VIRTIO_GPU_CAPSET_VIRGL 1
#define VIRTIO_GPU_CAPSET_VIRGL2 2
#define VIRTIO_GPU_CAPSET_GFXSTREAM 3
#define VIRTIO_GPU_CAPSET_VENUS 4
#define VIRTIO_GPU_CAPSET_CROSS_DOMAIN 5

typedef struct virtio_gpu_resp_capset_info_t {
    virtio_gpu_ctrl_hdr_t hdr;
    int32_t               capset_id;
    int32_t               capset_max_version;
    int32_t               capset_max_size;
    int32_t               padding;
} __attribute__((packed)) virtio_gpu_resp_capset_info_t;

typedef struct virtio_gpu_get_capset_t {
    virtio_gpu_ctrl_hdr_t hdr;
    uint32_t              capset_id;
    uint32_t              padding;
} __attribute__((packed)) virtio_gpu_get_capset_t;

typedef struct virtio_gpu_resp_capset_t {
    virtio_gpu_ctrl_hdr_t hdr;
    uint8_t               capset_data[];
} __attribute__((packed)) virtio_gpu_resp_capset_t;

typedef struct virtio_gpu_resource_assign_uuid_t {
    virtio_gpu_ctrl_hdr_t hdr;
    uint32_t              resource_id;
    uint32_t              padding;
} __attribute__((packed)) virtio_gpu_resource_assign_uuid_t;

typedef struct virtio_gpu_resp_resource_uuid_t {
    virtio_gpu_ctrl_hdr_t hdr;
    uint8_t               uuid[16];
} __attribute__((packed)) virtio_gpu_resp_resource_uuid_t;

#define VIRTIO_GPU_BLOB_MEM_GUEST 1
#define VIRTIO_GPU_BLOB_MEM_HOST3D 2
#define VIRTIO_GPU_BLOB_MEM_HOST3D_GUEST 3

#define VIRTIO_GPU_BLOB_FLAG_USE_MAPPABLE 1
#define VIRTIO_GPU_BLOB_FLAG_USE_SHAREABLE 2
#define VIRTIO_GPU_BLOB_FLAG_USE_CROSS_DEVICE 4

typedef struct virtio_gpu_resource_create_blob_t {
    virtio_gpu_ctrl_hdr_t hdr;
    uint32_t              resource_id;
    uint32_t              blob_mem;
    uint32_t              blob_flags;
    uint32_t              nr_entries;
    uint64_t              blob_id;
    uint64_t              size;
} __attribute__((packed)) virtio_gpu_resource_create_blob_t;

typedef struct virtio_gpu_set_scanout_blob_t {
    virtio_gpu_ctrl_hdr_t hdr;
    virtio_gpu_rect_t     rec;
    uint32_t              scanout_id;
    uint32_t              resource_id;
    uint32_t              width;
    uint32_t              height;
    uint32_t              format;
    uint32_t              padding;
    uint32_t              strides[4];
    uint32_t              offsets[4];
} __attribute__((packed)) virtio_gpu_set_scanout_blob_t;

#define VIRTIO_GPU_CONTEXT_INIT_CAPSET_ID_MASK 0x000000ff
typedef struct virtio_gpu_ctx_create_t {
    virtio_gpu_ctrl_hdr_t hdr;
    uint32_t              nlen;
    uint32_t              context_init;
    char_t                debug_name[64];
} __attribute__((packed)) virtio_gpu_ctx_create_t;

typedef struct virtio_gpu_ctx_destroy_t {
    virtio_gpu_ctrl_hdr_t hdr;
} __attribute__((packed)) virtio_gpu_ctx_destroy_t;

/* VIRTIO_GPU_CMD_CTX_ATTACH_RESOURCE, VIRTIO_GPU_CMD_CTX_DETACH_RESOURCE */
typedef struct virtio_gpu_ctx_resource_t {
    virtio_gpu_ctrl_hdr_t hdr;
    uint32_t              resource_id;
    uint32_t              padding;
} __attribute__((packed)) virtio_gpu_ctx_resource_t;

#define VIRTIO_GPU_RESOURCE_FLAG_Y_0_TOP (1 << 0)

typedef struct virtio_gpu_resource_create_3d_t {
    virtio_gpu_ctrl_hdr_t hdr;
    uint32_t              resource_id;
    uint32_t              target;
    uint32_t              format;
    uint32_t              bind;
    uint32_t              width;
    uint32_t              height;
    uint32_t              depth;
    uint32_t              array_size;
    uint32_t              last_level;
    uint32_t              nr_samples;
    uint32_t              flags;
    uint32_t              padding;
} __attribute__((packed)) virtio_gpu_resource_create_3d_t;

typedef struct virtio_gpu_box_t {
    uint32_t x, y, z;
    uint32_t w, h, d;
} __attribute__((packed)) virtio_gpu_box_t;

/* VIRTIO_GPU_CMD_TRANSFER_TO_HOST_3D, VIRTIO_GPU_CMD_TRANSFER_FROM_HOST_3D */
typedef struct virtio_gpu_transfer_host_3d_t {
    virtio_gpu_ctrl_hdr_t hdr;
    virtio_gpu_box_t      box;
    uint64_t              offset;
    uint32_t              resource_id;
    uint32_t              level;
    uint32_t              stride;
    uint32_t              layer_stride;
} __attribute__((packed)) virtio_gpu_transfer_host_3d_t;

typedef struct virtio_gpu_cmd_submit_t {
    virtio_gpu_ctrl_hdr_t hdr;
    uint32_t              size;
    uint32_t              padding;
} __attribute__((packed)) virtio_gpu_cmd_submit_t;

_Static_assert(sizeof(virtio_gpu_cmd_submit_t) == 32, "virtio_gpu_cmd_submit_t size is not correct");

typedef struct virtio_gpu_resource_map_blob_t {
    virtio_gpu_ctrl_hdr_t hdr;
    uint32_t              resource_id;
    uint32_t              padding;
    uint64_t              offset;
} __attribute__((packed)) virtio_gpu_resource_map_blob_t;

#define VIRTIO_GPU_MAP_CACHE_MASK     0x0f
#define VIRTIO_GPU_MAP_CACHE_NONE     0x00
#define VIRTIO_GPU_MAP_CACHE_CACHED   0x01
#define VIRTIO_GPU_MAP_CACHE_UNCACHED 0x02
#define VIRTIO_GPU_MAP_CACHE_WC       0x03

typedef struct virtio_gpu_resp_map_info_t {
    virtio_gpu_ctrl_hdr_t hdr;
    uint32_t              map_info;
    uint32_t              padding;
} __attribute__((packed)) virtio_gpu_resp_map_info_t;

typedef struct virtio_gpu_resource_unmap_blob_t {
    virtio_gpu_ctrl_hdr_t hdr;
    uint32_t              resource_id;
    uint32_t              padding;
} __attribute__((packed)) virtio_gpu_resource_unmap_blob_t;

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

typedef struct virtio_gpu_wrapper_t {
    virtio_dev_t*     vgpu;
    uint32_t          num_scanouts;
    uint64_t*         fence_ids;
    uint32_t*         resource_ids;
    uint32_t*         surface_ids;
    uint32_t          mouse_resource_id;
    uint32_t          font_resource_id;
    uint32_t          font_empty_line_resource_id;
    uint32_t          screen_width;
    uint32_t          screen_height;
    uint32_t          mouse_width;
    uint32_t          mouse_height;
    lock_t*           lock;
    lock_t*           cursor_lock;
    lock_t*           flush_lock;
    virgl_renderer_t* renderer;
} virtio_gpu_wrapper_t;

int8_t virtio_video_init(memory_heap_t* heap, const pci_dev_t* device);


#endif /* __VIDEO_VIRTIO_H */

/**
 * @file console_virtio.h
 * @brief Virtio console header.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___CONSOLE_VIRTIO_H
#define ___CONSOLE_VIRTIO_H 0

#include <types.h>
#include <pci.h>
#include <driver/virtio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VIRTIO_CONSOLE_VENDOR_ID 0x1AF4
#define VIRTIO_CONSOLE_DEVICE_ID 0x1003

#define VIRTIO_CONSOLE_F_SIZE             (1ULL << 0)
#define VIRTIO_CONSOLE_F_MULTIPORT        (1ULL << 1)
#define VIRTIO_CONSOLE_F_EMERG_WRITE      (1ULL << 2)

#define VIRTIO_CONSOLE_DEVICE_READY       0
#define VIRTIO_CONSOLE_DEVICE_ADD         1
#define VIRTIO_CONSOLE_DEVICE_REMOVE      2
#define VIRTIO_CONSOLE_PORT_READY         3
#define VIRTIO_CONSOLE_CONSOLE_PORT       4
#define VIRTIO_CONSOLE_RESIZE             5
#define VIRTIO_CONSOLE_PORT_OPEN          6
#define VIRTIO_CONSOLE_PORT_NAME          7

typedef struct virtio_console_config_t {
    uint16_t cols;
    uint16_t rows;
    uint32_t max_nr_ports;
    uint32_t emerg_wr;
}__attribute__((packed)) virtio_console_config_t;

typedef struct virtio_console_control_t {
    uint32_t id;
    uint16_t event;
    uint16_t value;
}__attribute__((packed)) virtio_console_control_t;

#define VIRTIO_CONSOLE_MAX_PORT_NAME 63
#define VIRTIO_CONSOLE_CONTROL_VQ_SIZE (sizeof(virtio_console_control_t) + VIRTIO_CONSOLE_MAX_PORT_NAME + 1)
#define VIRTIO_CONSOLE_DATA_VQ_SIZE 4096

typedef struct virtio_console_resize_t {
    uint16_t cols;
    uint16_t rows;
}__attribute__((packed)) virtio_console_resize_t;

int8_t console_virtio_init(void);

// vdagent protocol types and signatures

#define VIRTIO_CONSOLE_VDAGENT_PORT_NAME "com.turnstoneos.clipboard.0"

typedef enum vdp_port_t {
    VDP_CLIENT_PORT = 1,
    VDP_SERVER_PORT = 2,
} vdp_port_t;

typedef struct vdi_chunk_header_t {
    uint32_t port;
    uint32_t size;
    uint8_t  data[];
}__attribute__((packed)) vdi_chunk_header_t;

#define VD_AGENT_PROTOCOL 1
#define VD_AGENT_MAX_DATA_SIZE 2048

typedef enum vdagent_message_type_t {
    VD_AGENT_MOUSE_STATE = 1,
    VD_AGENT_MONITORS_CONFIG,
    VD_AGENT_REPLY,
    VD_AGENT_CLIPBOARD, ///< 4
    VD_AGENT_DISPLAY_CONFIG,
    VD_AGENT_ANNOUNCE_CAPABILITIES, ///< 6
    VD_AGENT_CLIPBOARD_GRAB, ///< 7
    VD_AGENT_CLIPBOARD_REQUEST, ///< 8
    VD_AGENT_CLIPBOARD_RELEASE, ///< 9
    VD_AGENT_FILE_XFER_START,
    VD_AGENT_FILE_XFER_STATUS,
    VD_AGENT_FILE_XFER_DATA,
    VD_AGENT_CLIENT_DISCONNECTED,
    VD_AGENT_MAX_CLIPBOARD, ///< 14
    VD_AGENT_AUDIO_VOLUME_SYNC,
    VD_AGENT_GRAPHICS_DEVICE_INFO,
    VD_AGENT_END_MESSAGE,
} vdagent_message_type_t;

typedef struct vdagent_message_t {
    uint32_t protocol;
    uint32_t type;
    uint64_t opaque;
    uint32_t size;
    uint8_t  data[];
}__attribute__((packed)) vdagent_message_t;

typedef enum vdagent_cap_t {
    VD_AGENT_CAP_MOUSE_STATE = 0,
    VD_AGENT_CAP_MONITORS_CONFIG,
    VD_AGENT_CAP_REPLY,
    VD_AGENT_CAP_CLIPBOARD, ///< 3
    VD_AGENT_CAP_DISPLAY_CONFIG,
    VD_AGENT_CAP_CLIPBOARD_BY_DEMAND, ///< 5
    VD_AGENT_CAP_CLIPBOARD_SELECTION, ///< 6
    VD_AGENT_CAP_SPARSE_MONITORS_CONFIG,
    VD_AGENT_CAP_GUEST_LINEEND_LF, ///< 8
    VD_AGENT_CAP_GUEST_LINEEND_CRLF,
    VD_AGENT_CAP_MAX_CLIPBOARD, ///< 10
    VD_AGENT_CAP_AUDIO_VOLUME_SYNC,
    VD_AGENT_CAP_MONITORS_CONFIG_POSITION,
    VD_AGENT_CAP_FILE_XFER_DISABLED,
    VD_AGENT_CAP_FILE_XFER_DETAILED_ERRORS,
    VD_AGENT_CAP_GRAPHICS_DEVICE_INFO,
    VD_AGENT_CAP_CLIPBOARD_NO_RELEASE_ON_REGRAB, ///< 16
    VD_AGENT_CAP_CLIPBOARD_GRAB_SERIAL, ///< 17
    VD_AGENT_END_CAP,
} vdagent_cap_t;

typedef struct vdagent_cap_announce_t {
    uint32_t request;
    uint32_t caps;
}__attribute__((packed)) vdagent_cap_announce_t;

typedef enum vdagent_clipboard_selection_t {
    VD_AGENT_CLIPBOARD_SELECTION_CLIPBOARD = 0,
    VD_AGENT_CLIPBOARD_SELECTION_PRIMARY,
    VD_AGENT_CLIPBOARD_SELECTION_SECONDARY,
} vdagent_clipboard_selection_t;

typedef enum vdagent_clipboard_type_t {
    VD_AGENT_CLIPBOARD_NONE = 0,
    VD_AGENT_CLIPBOARD_UTF8_TEXT,
    VD_AGENT_CLIPBOARD_IMAGE_PNG,
    VD_AGENT_CLIPBOARD_IMAGE_BMP,
    VD_AGENT_CLIPBOARD_IMAGE_TIFF,
    VD_AGENT_CLIPBOARD_IMAGE_JPG,
    VD_AGENT_CLIPBOARD_FILE_LIST,
} vdagent_clipboard_type_t;

typedef struct vdagent_clipboard_t {
    uint8_t  selection;
    uint8_t  reserved[3];
    uint32_t type;
    uint8_t  data[];
}__attribute__((packed)) vdagent_clipboard_t;

#define VD_AGENT_CLIPBOARD_MAX_DATA_SIZE (1 << 20) // 1MB

typedef struct vdagent_clipboard_grab_t {
    uint8_t  selection;
    uint8_t  reserved[3];
    uint32_t serial;
    uint32_t types[];
}__attribute__((packed)) vdagent_clipboard_grab_t;

typedef struct vdagent_clipboard_request_t {
    uint8_t  selection;
    uint8_t  reserved[3];
    uint32_t type;
}__attribute__((packed)) vdagent_clipboard_request_t;

typedef struct vdagent_clipboard_release_t {
    uint8_t selection;
    uint8_t reserved[3];
}__attribute__((packed)) vdagent_clipboard_release_t;

typedef struct vdagent_max_clipboard_t {
    uint32_t max;
}__attribute__((packed)) vdagent_max_clipboard_t;

int8_t clipboard_send_text(const char_t* text_message);

#ifdef __cplusplus
}
#endif

#endif

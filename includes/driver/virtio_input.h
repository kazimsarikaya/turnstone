/**
 * @file virtio_input.h
 * @brief virtio keyboard device interface
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#ifndef ___DEVICE_VIRTIO_INPUT_H
/*! prevent duplicate header error macro */
#define ___DEVICE_VIRTIO_INPUT_H 0

#include <types.h>

typedef enum {
	VIRTIO_INPUT_CFG_UNSET = 0x00,
	VIRTIO_INPUT_CFG_ID_NAME = 0x01,
	VIRTIO_INPUT_CFG_ID_SERIAL = 0x02,
	VIRTIO_INPUT_CFG_ID_DEVIDS = 0x03,
	VIRTIO_INPUT_CFG_PROP_BITS = 0x10,
	VIRTIO_INPUT_CFG_EV_BITS = 0x11,
	VIRTIO_INPUT_CFG_ABS_INFO = 0x12,
}virtio_input_config_select_t;

typedef struct virtio_input_absinfo_s {
	uint32_t min;
	uint32_t max;
	uint32_t fuzz;
	uint32_t flat;
	uint32_t res;
} __attribute__((packed)) virtio_input_absinfo_t;

typedef struct virtio_input_devids_s {
	uint16_t bustype;
	uint16_t vendor;
	uint16_t product;
	uint16_t version;
} __attribute__((packed)) virtio_input_devids_t;

typedef struct virtio_input_config_s {
	uint8_t select;
	uint8_t subsel;
	uint8_t size;
	uint8_t reserved[5];
	union {
		char_t string[128];
		uint8_t bitmap[128];
		virtio_input_absinfo_t abs;
		virtio_input_devids_t ids;
	} u;
} __attribute__((packed)) virtio_input_config_t;

typedef struct virtio_input_event_s {
	uint16_t type;
	uint16_t code;
	uint32_t value;
} __attribute__((packed)) virtio_input_event_t;

#endif

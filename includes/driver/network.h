/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___NETWORK_H
#define ___NETWORK_H 0

#include <types.h>
#include <linkedlist.h>

#define NETWORK_DEVICE_VENDOR_ID_VIRTIO  0x1AF4
#define NETWORK_DEVICE_DEVICE_ID_VIRTNET1 0x1000
#define NETWORK_DEVICE_DEVICE_ID_VIRTNET2 0x1041

#define NETWORK_DEVICE_VENDOR_ID_INTEL  0x8086
#define NETWORK_DEVICE_DEVICE_ID_E1000  0x100F

extern linkedlist_t network_received_packets;

int8_t network_init();

#endif

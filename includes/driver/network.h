#ifndef ___NETWORK_H
#define ___NETWORK_H 0

#include <types.h>

#define NETWORK_DEVICE_VENDOR_ID_VIRTIO  0x1AF4
#define NETWORK_DEVICE_DEVICE_ID_VIRTNET1 0x1000
#define NETWORK_DEVICE_DEVICE_ID_VIRTNET2 0x1041

int8_t network_init();

#endif

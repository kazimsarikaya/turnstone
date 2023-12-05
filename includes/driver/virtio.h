/**
 * @file virtio.h
 * @brief Virtio header.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___VIRTIO_H
#define ___VIRTIO_H 0

#include <types.h>
#include <pci.h>
#include <cpu/interrupt.h>

typedef enum {
    VIRTIO_DEVICE_STATUS_ACKKNOWLEDGE=1,
    VIRTIO_DEVICE_STATUS_DRIVER=2,
    VIRTIO_DEVICE_STATUS_DRIVER_OK=4,
    VIRTIO_DEVICE_STATUS_FEATURES_OK=8,
    VIRTIO_DEVICE_STATUS_DEVICE_NEED_RESET=64,
    VIRTIO_DEVICE_STATUS_FAILED=128,
} virtio_device_status_t;

#define VIRTIO_F_NOTIFY_ON_EMPTY   (1ULL << 24)
#define VIRTIO_F_ANY_LAYOUT        (1ULL << 27) /* Arbitrary descriptor layouts. */
#define VIRTIO_F_INDIRECT_DESC     (1ULL << 28) /* Support for indirect descriptors */
#define VIRTIO_F_EVENT_IDX         (1ULL << 29) /* Support for avail_event and used_event fields */
#define VIRTIO_F_UNUSED            (1ULL << 30) /* test purpose */
#define VIRTIO_F_VERSION_1         (1ULL << 32)
#define VIRTIO_F_ACCESS_PLATFORM   (1ULL << 33)
#define VIRTIO_F_RING_PACKED       (1ULL << 34)
#define VIRTIO_F_IN_ORDER          (1ULL << 35)
#define VIRTIO_F_ORDER_PLATFORM    (1ULL << 36)
#define VIRTIO_F_SR_IOV            (1ULL << 37)
#define VIRTIO_F_NOTIFICATION_DATA (1ULL << 38)


#define VIRTIO_QUEUE_DESC_F_AVAIL     (1 << 7)
#define VIRTIO_QUEUE_DESC_F_USED      (1 << 15)
#define VIRTIO_QUEUE_DESC_F_WRITE      2 /* This marks a buffer as device write-only (otherwise device read-only). */
#define VIRTIO_QUEUE_DESC_F_NEXT       1 /* This marks a buffer as continuing via the next field. */
#define VIRTIO_QUEUE_DESC_F_INDIRECT   4 /* This means the buffer contains a list of buffer descriptors. */

#define VIRTIO_QUEUE_SIZE 0x100

typedef struct {
    uint64_t address; /* Address (guest-physical). */
    uint32_t length; /* Length. */
    uint16_t flags;
    uint16_t next; /* Next field if flags & NEXT */
} __attribute__((packed)) virtio_queue_descriptor_t;

typedef struct {
    virtio_queue_descriptor_t descriptors[0]; /* len / sizeof(struct virtio_queue_desc_t) */
} __attribute__((packed)) virtio_queue_indirect_descriptor_table_t;


#define VIRTIO_QUEUE_AVAIL_F_NO_INTERRUPT      1

typedef struct {
    uint16_t flags;
    uint16_t index;
    uint16_t ring[]; /* queue size */
    /* uint16_t used_event; Only if VIRTIO_F_EVENT_IDX */
} __attribute__((packed)) virtio_queue_avail_t;

typedef struct {
    uint32_t id; /* Index of start of used descriptor chain. */
    uint32_t length; /* Total length of the descriptor chain which was used (written to) */
} __attribute__((packed)) virtio_queue_used_element_t;


#define VIRTIO_QUEUE_USED_F_NO_NOTIFY  1

typedef struct {
    uint16_t                    flags;
    uint16_t                    index;
    virtio_queue_used_element_t ring[]; /* queue size */
    /* uint16_t used_event; Only if VIRTIO_F_EVENT_IDX */
} __attribute__((packed)) virtio_queue_used_t;

typedef struct {
    uint64_t address;
    uint32_t length;
    uint16_t id;
    uint16_t flags;
} __attribute__((packed)) virtio_pqueue_desc_t;

typedef struct {
    virtio_pqueue_desc_t descriptors[0]; /* len / sizeof(struct pvirtq_desc) */
} __attribute__((packed)) virtio_pqueue_indirect_descriptor_table_t;

typedef struct {
    uint16_t desc_event_off   : 16;
    uint16_t desc_event_wrap  : 1;
    uint16_t desc_event_flags : 2;
    uint16_t reserved         : 15;
} __attribute__((packed)) virtio_pqueue_event_suppress_t;

typedef void * virtio_queue_t;

/*
   typedef struct {
   virtio_queue_descriptor_t descriptors[VIRTIO_QUEUE_SIZE] __attribute__ ((aligned(16)));
   virtio_queue_avail_t avail __attribute__ ((aligned(2)));
   virtio_queue_used_t used __attribute__ ((aligned(4)));
   uint16_t last_used_index;
   }__attribute__((packed)) virtio_queue_t;

 */

/*
   static inline int8_t virtio_queue_need_event(uint16_t event_idx, uint16_t new_idx, uint16_t old_idx)
   {
   return (uint16_t)(new_idx - event_idx - 1) < (uint16_t)(new_idx - old_idx);
   }

   // Get location of event indices (only with VIRTIO_F_EVENT_IDX)
   static inline uint16_t* virtio_queue_used_event(virtio_queue_t* vq)
   {
   return &vq->avail->ring[vq->num];
   }

   // Get location of event indices (only with VIRTIO_F_EVENT_IDX)
   static inline uint16_t* virtio_queue_avail_event(virtio_queue_t* vq)
   {
   return (uint16_t*)&vq->used->ring[vq->num];
   }
 */

#define VIRTIO_QUEUE_RING_EVENT_FLAGS_ENABLE  0x0
#define VIRTIO_QUEUE_RING_EVENT_FLAGS_DISABLE 0x1
#define VIRTIO_QUEUE_RING_EVENT_FLAGS_DESC    0x2

typedef struct virtio_pci_common_config {
    uint32_t device_feature_select; /* read-write */
    uint32_t device_feature; /* read-only for driver */
    uint32_t driver_feature_select; /* read-write */
    uint32_t driver_feature; /* read-write */
    uint16_t msix_config; /* read-write */
    uint16_t num_queues; /* read-only for driver */
    uint8_t  device_status; /* read-write */
    uint8_t  config_generation; /* read-only for driver */
    uint16_t queue_select; /* read-write */
    uint16_t queue_size; /* read-write */
    uint16_t queue_msix_vector; /* read-write */
    uint16_t queue_enable; /* read-write */
    uint16_t queue_notify_offset; /* read-only for driver */
    uint64_t queue_desc; /* read-write */
    uint64_t queue_driver; /* read-write */
    uint64_t queue_device; /* read-write */
} __attribute__((packed)) virtio_pci_common_config_t;

typedef enum {
    VIRTIO_PCI_CAP_COMMON_CFG=1,
    VIRTIO_PCI_CAP_NOTIFY_CFG=2,
    VIRTIO_PCI_CAP_ISR_CFG=3,
    VIRTIO_PCI_CAP_DEVICE_CFG=4,
    VIRTIO_PCI_CAP_PCI_CFG=5,
} virtio_pic_cap_type_t;

typedef struct virtio_pci_cap {
    uint8_t               capability_id; /* Generic PCI field: PCI_CAP_ID_VNDR */
    uint8_t               next_pointer; /* Generic PCI field: next ptr. */
    uint8_t               capability_length; /* Generic PCI field: capability length */
    virtio_pic_cap_type_t config_type : 8; /* Identifies the structure. */
    uint8_t               bar_no; /* Where to find it. */
    uint8_t               padding[3]; /* Pad to full dword. */
    uint32_t              offset; /* Offset within bar. */
    uint32_t              length; /* Length of the structure, in bytes. */
} __attribute__((packed)) virtio_pci_cap_t;

typedef struct {
    virtio_pci_cap_t capability;
    uint32_t         notify_offset_multiplier; /* cap.offset + queue_notify_off * notify_off_multiplier   */
} __attribute__((packed)) virtio_pci_notify_cap_t;

typedef struct {
    virtio_pci_cap_t capability;
    uint32_t         data; /* Data for BAR access. uint8_t[4] */
}__attribute__((packed)) virtio_pci_config_cap_t;



#define VIRTIO_MSI_X_NO_VECTOR            0xffff

typedef struct {
    uint32_t vqn         : 16;
    uint32_t next_offset : 15;
    uint32_t next_wrap   : 1;
}__attribute__((packed)) virtio_notification_data_t;



#define VIRTIO_IOPORT_DEVICE_F        0x00
#define VIRTIO_IOPORT_DRIVER_F        0x04
#define VIRTIO_IOPORT_VQ_ADDR         0x08
#define VIRTIO_IOPORT_VQ_SIZE         0x0C
#define VIRTIO_IOPORT_VQ_SELECT       0x0E
#define VIRTIO_IOPORT_VQ_NOTIFY       0x10
#define VIRTIO_IOPORT_DEVICE_STATUS   0x12
#define VIRTIO_IOPORT_ISR_STATUS      0x13
#define VIRTIO_IOPORT_CFG_MSIX_VECTOR 0x14
#define VIRTIO_IOPORT_VQ_MSIX_VECTOR  0x16


typedef struct {
    virtio_queue_t              vq;
    uint16_t                    last_used_index;
    virtio_notification_data_t* nd;
}virtio_queue_ext_t;

typedef struct {
    const pci_dev_t*            pci_dev;
    boolean_t                   is_legacy;
    boolean_t                   has_msix;
    pci_capability_msix_t*      msix_cap;
    uint8_t                     irq_base;
    uint16_t                    iobase;
    virtio_pci_common_config_t* common_config;
    uint32_t                    notify_offset_multiplier;
    uint8_t*                    notify_offset;
    void*                       isr_config;
    void*                       device_config;
    uint64_t                    features;
    uint64_t                    selected_features;
    uint16_t                    max_vq_count;
    uint16_t                    queue_size;
    uint64_t                    queue_avail_offset;
    uint64_t                    queue_used_offset;
    virtio_queue_ext_t*         queues;
    linkedlist_t*               return_queue;
    void*                       extra_data;
    uint64_t                    rx_task_id;
}virtio_dev_t;


static inline uint64_t virtio_queue_get_size(virtio_dev_t* vdev) {
    uint64_t size = 0;
    size += sizeof(virtio_queue_descriptor_t) * vdev->queue_size;

    if(size & 1) {
        size++;
    }

    vdev->queue_avail_offset = size;

    size += sizeof(uint16_t) + sizeof(uint16_t) + sizeof(uint16_t) * vdev->queue_size;

    if(vdev->selected_features & VIRTIO_F_EVENT_IDX) {
        size += sizeof(uint16_t);
    }

    if(size % 4) {
        size += 4 - (size % 4);
    }

    vdev->queue_used_offset = size;

    size += sizeof(uint16_t) + sizeof(uint16_t) + sizeof(virtio_queue_used_element_t) * vdev->queue_size;


    if(vdev->selected_features & VIRTIO_F_EVENT_IDX) {
        size += sizeof(uint16_t);
    }

    return size;
}

static inline virtio_queue_descriptor_t* virtio_queue_get_desc(virtio_dev_t* vdev, virtio_queue_t queue){
    UNUSED(vdev);
    return (virtio_queue_descriptor_t*)queue;
}

static inline virtio_queue_avail_t* virtio_queue_get_avail(virtio_dev_t* vdev, virtio_queue_t queue){
    return (virtio_queue_avail_t*)(((uint8_t*)queue) + vdev->queue_avail_offset);
}

static inline virtio_queue_used_t* virtio_queue_get_used(virtio_dev_t* vdev, virtio_queue_t queue){
    return (virtio_queue_used_t*)(((uint8_t*)queue) + vdev->queue_used_offset);
}

typedef int8_t (* virtio_queue_item_builder_f)(virtio_dev_t* vdev, void* queue_item);

int8_t virtio_create_queue(virtio_dev_t* vdev, uint16_t queue_no, uint64_t queue_item_size, boolean_t write, boolean_t iter_rw, virtio_queue_item_builder_f item_builder, interrupt_irq modern, interrupt_irq legacy);

virtio_dev_t* virtio_get_device(const pci_dev_t* pci_dev);

typedef uint64_t (* virtio_select_features_f)(virtio_dev_t* vdev, uint64_t avail_features);
typedef int8_t   (* virtio_create_queues_f)(virtio_dev_t* vdev);

int8_t virtio_init_dev(virtio_dev_t* vdev, virtio_select_features_f select_features, virtio_create_queues_f create_queues);

#endif

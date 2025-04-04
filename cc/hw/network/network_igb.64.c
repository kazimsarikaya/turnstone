/**
 * @file network_igb.64.c
 * @brief Intel 8254x (igb) network driver.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <driver/network_igb.h>
#include <memory.h>
#include <utils.h>
#include <logging.h>
#include <time/timer.h>
#include <network.h>
#include <network/network_ethernet.h>
#include <network/network_dhcpv4.h>
#include <memory/frame.h>
#include <memory/paging.h>
#include <acpi.h>
#include <acpi/aml.h>
#include <cpu/interrupt.h>
#include <cpu.h>
#include <apic.h>
#include <utils.h>
#include <cpu/task.h>

MODULE("turnstone.kernel.hw.network.igb");

list_t* igb_net_devs = NULL;

static uint32_t network_igb_read_mmio(const network_igb_dev_t* dev, uint32_t offset) {
    return *((volatile uint32_t*)(dev->mmio_va + offset));
}

static void network_igb_write_mmio(const network_igb_dev_t* dev, uint32_t offset, uint32_t value) {
    *((volatile uint32_t*)(dev->mmio_va + offset)) = value;
}

static void network_igb_reset(const network_igb_dev_t* dev) {
    network_igb_write_mmio(dev, NETWORK_IGB_REG_CTRL, NETWORK_IGB_CTRL_RST);
    time_timer_spinsleep(1000);

    while( network_igb_read_mmio(dev, NETWORK_IGB_REG_CTRL) & NETWORK_IGB_CTRL_RST ) {
        time_timer_spinsleep(1000);
    }
}

static void network_igb_read_mac_address(const network_igb_dev_t* dev, network_mac_address_t* mac) {
    uint32_t ral = network_igb_read_mmio(dev, NETWORK_IGB_REG_RAL);
    uint32_t rah = network_igb_read_mmio(dev, NETWORK_IGB_REG_RAH);

    uint8_t* mac_octets = (uint8_t*)mac;

    mac_octets[0] = (ral >> 0) & 0xFF;
    mac_octets[1] = (ral >> 8) & 0xFF;
    mac_octets[2] = (ral >> 16) & 0xFF;
    mac_octets[3] = (ral >> 24) & 0xFF;

    mac_octets[4] = (rah >> 0) & 0xFF;
    mac_octets[5] = (rah >> 8) & 0xFF;
}

static void network_igb_disable_interrupts(const network_igb_dev_t* dev) {
    network_igb_write_mmio(dev, NETWORK_IGB_REG_IMC, 0xFFFFFFFF);
}

static void network_igb_enable_interrupts(const network_igb_dev_t* dev) {
    pci_msix_clear_pending_bit((pci_generic_device_t*)dev->pci_netdev->pci_header, dev->msix_cap, 0);
    pci_msix_clear_pending_bit((pci_generic_device_t*)dev->pci_netdev->pci_header, dev->msix_cap, 1);
    pci_msix_clear_pending_bit((pci_generic_device_t*)dev->pci_netdev->pci_header, dev->msix_cap, 2);


    network_igb_write_mmio(dev, NETWORK_IGB_REG_EIAC, 0x7); // dont use auto clear
    network_igb_write_mmio(dev, NETWORK_IGB_REG_EIMS, 0x7); // int 0, 1, 2
    network_igb_write_mmio(dev, NETWORK_IGB_REG_GPIE, 1 << 4); // enable multiple isr
    network_igb_write_mmio(dev, NETWORK_IGB_REG_IMS, 0xC00054);
    network_igb_read_mmio(dev, NETWORK_IGB_REG_ICR);
}


static int8_t network_igb_process_tx(void) {

    for(uint64_t dev_idx = 0; dev_idx < list_size(igb_net_devs); dev_idx++) {
        network_igb_dev_t* dev = (network_igb_dev_t*)list_get_data_at_position(igb_net_devs, dev_idx);

        dev->return_queue = list_create_queue_with_heap(NULL);
        task_add_message_queue(dev->return_queue);

        void** args = memory_malloc(sizeof(void*) * 2);

        if(args == NULL) {
            return -1;
        }

        args[0] = (void*)dev->mac;
        args[1] = dev->return_queue;

        task_create_task(NULL, 1 << 20, 64 << 10, &network_dhcpv4_send_discover, 2, args, "dhcp");
    }

    while(1) {
        boolean_t packet_exists = 0;

        for(uint64_t dev_idx = 0; dev_idx < list_size(igb_net_devs); dev_idx++) {
            network_igb_dev_t* dev = (network_igb_dev_t*)list_get_data_at_position(igb_net_devs, dev_idx);

            while(list_size(dev->return_queue)) {
                const network_transmit_packet_t* packet = list_queue_pop(dev->return_queue);

                if(packet) {
                    PRINTLOG(NETWORK, LOG_TRACE, "network packet will be sended with length 0x%llx", packet->packet_len);
                    packet_exists = 1;

                    uint8_t* buffer = (uint8_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(dev->tx_desc[dev->tx_tail].address);

                    memory_memcopy(packet->packet_data, buffer, packet->packet_len);

                    dev->tx_desc[dev->tx_tail].length = packet->packet_len;
                    dev->tx_desc[dev->tx_tail].cmd = 3;

                    memory_free(packet->packet_data);
                    memory_free((void*)packet);

                    // update the tail so the hardware knows it's ready
                    dev->tx_tail = (dev->tx_tail + 1) % NETWORK_IGB_NUM_TX_DESCRIPTORS;
                    network_igb_write_mmio(dev, NETWORK_IGB_REG_TDT, dev->tx_tail);

                    dev->tx_count++;

                }

                PRINTLOG(NETWORK, LOG_TRACE, "tx queue size 0x%llx", list_size(dev->return_queue));
            }

        }

        if(packet_exists == 0) {
            task_set_message_waiting();
            task_yield();
        }

    }

    return 0;
}

static int8_t network_igb_rx_init(network_igb_dev_t* dev) {
    PRINTLOG(IGB, LOG_TRACE, "try to initialize rx queue");

    frame_allocator_t* fa = frame_get_allocator();

    // allocate a 10K buffer for the receive queue
    uint64_t packet_buffer_size = NETWORK_IGB_RX_BUFFER_SIZE * NETWORK_IGB_NUM_RX_DESCRIPTORS;
    uint64_t packet_buffer_frm_cnt = (packet_buffer_size + FRAME_SIZE - 1)  / FRAME_SIZE;

    frame_t* rx_packet_buffer_frames;

    if(fa->allocate_frame_by_count(fa, packet_buffer_frm_cnt, FRAME_ALLOCATION_TYPE_BLOCK | FRAME_ALLOCATION_TYPE_RESERVED, &rx_packet_buffer_frames, NULL) != 0) {
        PRINTLOG(IGB, LOG_ERROR, "cannot allocate frames for rx packet buffer");

        return -1;
    }

    rx_packet_buffer_frames->frame_attributes |= FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED;

    uint64_t rx_packet_buffer_fa = rx_packet_buffer_frames->frame_address;
    uint64_t rx_packet_buffer_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(rx_packet_buffer_fa);
    memory_paging_add_va_for_frame(rx_packet_buffer_va, rx_packet_buffer_frames, MEMORY_PAGING_PAGE_TYPE_NOEXEC);

    memory_memset((void*)rx_packet_buffer_va, 0, packet_buffer_size);

    dev->rx_packet_buffer_fa = rx_packet_buffer_fa;
    dev->rx_packet_buffer_va = rx_packet_buffer_va;

    // allocate a 256 byte buffer for the receive queue headers
    uint64_t header_buffer_size = NETWORK_IGB_RX_HEADER_SIZE * NETWORK_IGB_NUM_RX_DESCRIPTORS;
    uint64_t header_buffer_frm_cnt = (header_buffer_size + FRAME_SIZE - 1)  / FRAME_SIZE;

    frame_t* rx_header_buffer_frames;

    if(fa->allocate_frame_by_count(fa, header_buffer_frm_cnt, FRAME_ALLOCATION_TYPE_BLOCK | FRAME_ALLOCATION_TYPE_RESERVED, &rx_header_buffer_frames, NULL) != 0) {
        PRINTLOG(IGB, LOG_ERROR, "cannot allocate frames for rx header buffer");

        return -1;
    }

    rx_header_buffer_frames->frame_attributes |= FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED;

    uint64_t rx_header_buffer_fa = rx_header_buffer_frames->frame_address;
    uint64_t rx_header_buffer_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(rx_header_buffer_frames->frame_address);
    memory_paging_add_va_for_frame(rx_header_buffer_va, rx_header_buffer_frames, MEMORY_PAGING_PAGE_TYPE_NOEXEC);

    memory_memset((void*)rx_header_buffer_va, 0, header_buffer_size);

    dev->rx_header_buffer_fa = rx_header_buffer_fa;
    dev->rx_header_buffer_va = rx_header_buffer_va;

    // allocate a 16 byte buffer for the receive queue meta data
    uint64_t queue_meta_frm_cnt = ((sizeof(network_igb_rx_desc_t) * NETWORK_IGB_NUM_RX_DESCRIPTORS)  + FRAME_SIZE - 1 ) / FRAME_SIZE;

    frame_t* queue_meta_frames;

    if(fa->allocate_frame_by_count(fa, queue_meta_frm_cnt, FRAME_ALLOCATION_TYPE_BLOCK | FRAME_ALLOCATION_TYPE_RESERVED, &queue_meta_frames, NULL) != 0) {
        PRINTLOG(IGB, LOG_ERROR, "cannot allocate frames for rx queue meta");

        return -1;
    }

    queue_meta_frames->frame_attributes |= FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED;


    uint64_t queue_meta_fa = queue_meta_frames->frame_address;
    uint64_t queue_meta_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(queue_meta_frames->frame_address);
    memory_paging_add_va_for_frame(queue_meta_va, queue_meta_frames, MEMORY_PAGING_PAGE_TYPE_NOEXEC);

    memory_memset((void*)queue_meta_va, 0, sizeof(network_igb_rx_desc_t) * NETWORK_IGB_NUM_RX_DESCRIPTORS);

    // this should be first
    network_igb_write_mmio(dev, NETWORK_IGB_REG_SRRCTL, 0x600040A); // 10K packet buffer, 256 byte header format, type 2

    network_igb_write_mmio(dev, NETWORK_IGB_REG_RDBAL, queue_meta_fa & 0xFFFFFFFF);
    network_igb_write_mmio(dev, NETWORK_IGB_REG_RDBAH, (queue_meta_fa >> 32) & 0xFFFFFFFF);
    dev->rx_desc = (network_igb_rx_desc_t*)queue_meta_va;

    PRINTLOG(IGB, LOG_TRACE, "filling rx queue at 0x%llx", queue_meta_va);

    for(int32_t i = 0; i < NETWORK_IGB_NUM_RX_DESCRIPTORS; i++ ) {
        dev->rx_desc[i].read.pkt_addr = (rx_packet_buffer_fa + i * NETWORK_IGB_RX_BUFFER_SIZE); // | 0x1; // set the LSB to 1
        dev->rx_desc[i].read.hdr_addr = (rx_header_buffer_fa + i * NETWORK_IGB_RX_HEADER_SIZE); // | 0x1; // set the LSB to 1
    }

    // receive buffer length; NETWORK_IGB_NUM_RX_DESCRIPTORS 16-byte descriptors
    network_igb_write_mmio(dev, NETWORK_IGB_REG_RDLEN, (uint32_t)(NETWORK_IGB_NUM_RX_DESCRIPTORS * sizeof(network_igb_rx_desc_t)));

    // setup head and tail pointers
    network_igb_write_mmio(dev, NETWORK_IGB_REG_RDH, 0);
    network_igb_write_mmio(dev, NETWORK_IGB_REG_RDT, NETWORK_IGB_NUM_RX_DESCRIPTORS - 1);
    dev->rx_tail = NETWORK_IGB_NUM_RX_DESCRIPTORS - 1;


    network_igb_write_mmio(dev, NETWORK_IGB_REG_RXDCTL, network_igb_read_mmio(dev, NETWORK_IGB_REG_RXDCTL) |
                           NETWORK_IGB_RXDCTL_ENABLE);

    // set the receieve control register (promisc ON, 8K pkt size)
    network_igb_write_mmio(dev, NETWORK_IGB_REG_RCTL, NETWORK_IGB_RCTL_LPE | NETWORK_IGB_RCTL_BAM);

    PRINTLOG(IGB, LOG_TRACE, "rx queue initialized");

    return 0;

}

static int8_t network_igb_tx_init(network_igb_dev_t* dev) {
    uint64_t queue_size = (8192 + 16) * NETWORK_IGB_NUM_TX_DESCRIPTORS;

    uint64_t queue_frm_cnt = (queue_size + FRAME_SIZE - 1)  / FRAME_SIZE;
    uint64_t queue_meta_frm_cnt = ((sizeof(network_igb_rx_desc_t) * NETWORK_IGB_NUM_TX_DESCRIPTORS)  + FRAME_SIZE - 1 ) / FRAME_SIZE;


    frame_t* queue_frames;
    frame_t* queue_meta_frames;

    PRINTLOG(IGB, LOG_TRACE, "tx queue size 0x%llx queue frm count 0x%llx meta frm count 0x%llx", queue_size, queue_frm_cnt, queue_meta_frm_cnt);

    if(frame_get_allocator()->allocate_frame_by_count(frame_get_allocator(), queue_frm_cnt, FRAME_ALLOCATION_TYPE_BLOCK | FRAME_ALLOCATION_TYPE_RESERVED, &queue_frames, NULL) != 0 ||
       frame_get_allocator()->allocate_frame_by_count(frame_get_allocator(), queue_meta_frm_cnt, FRAME_ALLOCATION_TYPE_BLOCK | FRAME_ALLOCATION_TYPE_RESERVED, &queue_meta_frames, NULL) != 0) {
        PRINTLOG(IGB, LOG_ERROR, "cannot allocate frames for tx queue");

        return -1;
    }

    queue_frames->frame_attributes |= FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED;
    queue_meta_frames->frame_attributes |= FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED;

    uint64_t queue_fa = queue_frames->frame_address;
    uint64_t queue_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(queue_frames->frame_address);
    memory_paging_add_va_for_frame(queue_va, queue_frames, MEMORY_PAGING_PAGE_TYPE_NOEXEC);

    uint64_t queue_meta_fa = queue_meta_frames->frame_address;
    uint64_t queue_meta_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(queue_meta_frames->frame_address);
    memory_paging_add_va_for_frame(queue_meta_va, queue_meta_frames, MEMORY_PAGING_PAGE_TYPE_NOEXEC);

    network_igb_write_mmio(dev, NETWORK_IGB_REG_TDBAL, queue_meta_fa & 0xFFFFFFFF);
    network_igb_write_mmio(dev, NETWORK_IGB_REG_TDBAH, (queue_meta_fa >> 32) & 0xFFFFFFFF);
    dev->tx_desc = (network_igb_tx_desc_t*)queue_meta_va;

    PRINTLOG(IGB, LOG_TRACE, "filling tx queue at 0x%llx", queue_meta_va);

    for(int32_t i = 0; i < NETWORK_IGB_NUM_TX_DESCRIPTORS; i++ ) {
        dev->tx_desc[i].address = queue_fa + i * (8192 + 16);
        dev->tx_desc[i].cmd = 0;
    }

    // receive buffer length; NETWORK_IGB_NUM_RX_DESCRIPTORS 16-byte descriptors
    network_igb_write_mmio(dev, NETWORK_IGB_REG_TDLEN, (uint32_t)(NETWORK_IGB_NUM_TX_DESCRIPTORS * 16));

    // setup head and tail pointers
    network_igb_write_mmio(dev, NETWORK_IGB_REG_TDH, 0);
    network_igb_write_mmio(dev, NETWORK_IGB_REG_TDT, NETWORK_IGB_NUM_TX_DESCRIPTORS - 1);
    dev->tx_tail = 0;

    network_igb_write_mmio(dev, NETWORK_IGB_REG_TXDCTL, network_igb_read_mmio(dev, NETWORK_IGB_REG_TXDCTL) |
                           NETWORK_IGB_TXDCTL_ENABLE);

    // set the transmit control register (padshortpackets)
    network_igb_write_mmio(dev, NETWORK_IGB_REG_TCTL, NETWORK_IGB_TCTL_EN | NETWORK_IGB_TCTL_PSP);

    PRINTLOG(IGB, LOG_TRACE, "tx queue initialized");

    return 0;
}

extern uint64_t network_rx_task_id;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
// This can be used stand-alone or from an interrupt handler
static int32_t network_igb_process_rx(uint64_t args_cnt, void** args) {
    if(args_cnt != 1) {
        PRINTLOG(IGB, LOG_ERROR, "invalid args count");
        return -1;
    }

    network_igb_dev_t* dev = (network_igb_dev_t*)args[0];

    if(!dev) {
        PRINTLOG(IGB, LOG_ERROR, "invalid dev");
        return -1;
    }


    cpu_cli();
    pci_msix_update_lapic((pci_generic_device_t*)dev->pci_netdev->pci_header, dev->msix_cap, 0);
    pci_msix_clear_pending_bit((pci_generic_device_t*)dev->pci_netdev->pci_header, dev->msix_cap, 0);
    task_set_interruptible();
    cpu_sti();

    while(true) {
        boolean_t notify_network_rx = true;

        if(network_received_packets != NULL && dev->return_queue != NULL) {

            while(true) {
                // advance the tail pointer
                ((network_igb_dev_t*)dev)->rx_tail = (dev->rx_tail + 1) % NETWORK_IGB_NUM_RX_DESCRIPTORS;
                // check if the next descriptor is ready
                uint32_t status_error = dev->rx_desc[dev->rx_tail].wb.upper.status_error;
                // first 20bits are status, last 12 bits are error
                uint32_t status = status_error & 0xFFFFF;
                uint32_t error = status_error >> 20;

                PRINTLOG(IGB, LOG_TRACE, "rx status 0x%x error 0x%x", status, error);

                if( !(status & 1) ) { // descriptor is not ready
                    ((network_igb_dev_t*)dev)->rx_tail = (dev->rx_tail - 1) % NETWORK_IGB_NUM_RX_DESCRIPTORS;
                    PRINTLOG(IGB, LOG_TRACE, "rx descriptor is not ready");
                    break;
                }

                // we get packet address with calculated offset
                uint8_t* pkt = (uint8_t*)(dev->rx_packet_buffer_va + dev->rx_tail * NETWORK_IGB_RX_BUFFER_SIZE);
                uint16_t pktlen = dev->rx_desc[dev->rx_tail].wb.upper.length;
                boolean_t dropflag = 0;

                if( pktlen < 60 ) {
                    dropflag = 1;
                }

                if(error) {
                    PRINTLOG(IGB, LOG_WARNING, "device has rx errors 0x%x", error);

                    dropflag = 1;
                }

                if( !dropflag ) {
                    // send the packet to higher layers for parsing
                    PRINTLOG(IGB, LOG_TRACE, "packet received with len 0x%x", pktlen);

                    pkt = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(pkt);

                    network_received_packet_t* packet = memory_malloc_ext(list_get_heap(network_received_packets), sizeof(network_received_packet_t), 0);

                    if(packet == NULL) {
                        PRINTLOG(IGB, LOG_ERROR, "failed to allocate packet");
                        notify_network_rx = true;

                        task_yield();

                        continue;
                    }

                    packet->packet_len = pktlen;
                    packet->return_queue = dev->return_queue;
                    packet->network_info = (void*)dev->mac;
                    packet->network_type = NETWORK_TYPE_ETHERNET;

                    packet->packet_data = memory_malloc_ext(list_get_heap(network_received_packets), pktlen, 0);

                    if(packet->packet_data == NULL) {
                        PRINTLOG(IGB, LOG_ERROR, "failed to allocate packet");
                        memory_free_ext(list_get_heap(network_received_packets), packet);
                        notify_network_rx = true;

                        task_yield();

                        continue;
                    }

                    memory_memcopy(pkt, packet->packet_data, pktlen);

                    if(list_queue_push(network_received_packets, packet) == -1ULL) {
                        PRINTLOG(IGB, LOG_ERROR, "failed to queue packet");
                        memory_free_ext(list_get_heap(network_received_packets), packet->packet_data);
                        memory_free_ext(list_get_heap(network_received_packets), packet);
                    } else {
                        PRINTLOG(IGB, LOG_TRACE, "packet queued");

                        if(notify_network_rx && network_rx_task_id) {
                            task_set_message_received(network_rx_task_id);
                            PRINTLOG(IGB, LOG_TRACE, "cleared message waiting for rx task 0x%llx", network_rx_task_id);
                            notify_network_rx = false;
                        }
                    }


                }

                // restore the descriptor's packet and header addresses
                dev->rx_desc[dev->rx_tail].read.pkt_addr = dev->rx_packet_buffer_fa + dev->rx_tail * NETWORK_IGB_RX_BUFFER_SIZE;
                dev->rx_desc[dev->rx_tail].read.hdr_addr = dev->rx_header_buffer_fa + dev->rx_tail * NETWORK_IGB_RX_HEADER_SIZE;

                // update RX counts and the tail pointer
                ((network_igb_dev_t*)dev)->rx_count++;

                // write the tail to the device
                network_igb_write_mmio(dev, NETWORK_IGB_REG_RDT, dev->rx_tail);
                PRINTLOG(IGB, LOG_TRACE, "rx tail 0x%x", dev->rx_tail);
            }

            PRINTLOG(IGB, LOG_TRACE, "rx queue size 0x%llx", list_size(network_received_packets));
            pci_msix_clear_pending_bit((pci_generic_device_t*)dev->pci_netdev->pci_header, dev->msix_cap, 0);

        }

        task_set_message_waiting();
        task_yield();
    }

    return 0;
}
#pragma GCC diagnostic pop

void video_text_print(const char* str);

static int8_t network_igb_other_isr(interrupt_frame_ext_t* frame)  {
    UNUSED(frame);

    const network_igb_dev_t* dev = list_get_data_at_position(igb_net_devs, 0);

    if(!dev) {
        video_text_print("no dev\n");
        return -1;
    }

    uint32_t eicr = network_igb_read_mmio(dev, NETWORK_IGB_REG_EICR);
    uint32_t isr = network_igb_read_mmio(dev, NETWORK_IGB_REG_ICR);

    char_t buffer1[64] = {0};
    char_t buffer2[64] = {0};

    utoh_with_buffer(buffer1, eicr);
    utoh_with_buffer(buffer2, isr);

    video_text_print("other isr eicr 0x");
    video_text_print(buffer1);
    video_text_print(" isr 0x");
    video_text_print(buffer2);

    boolean_t is_other = eicr & (1 << 2);

    if(!is_other) {
        video_text_print(" not other");
    }

    video_text_print("\n");

    // clearing the pending interrupts
    // never clear 0. and 7. bit set them 0 on isr
    isr &= ~(1 << 0);
    isr &= ~(1 << 7);
    network_igb_write_mmio(dev, NETWORK_IGB_REG_EICR, 1 << 2);
    network_igb_write_mmio(dev, NETWORK_IGB_REG_ICR, isr);


    pci_msix_clear_pending_bit((pci_generic_device_t*)dev->pci_netdev->pci_header, dev->msix_cap, 2);

    apic_eoi();
    return 0;
}

static int8_t network_igb_tx_isr(interrupt_frame_ext_t* frame)  {
    UNUSED(frame);

    const network_igb_dev_t* dev = list_get_data_at_position(igb_net_devs, 0);

    if(!dev) {
        video_text_print("no dev\n");
        return -1;
    }

    uint32_t eicr = network_igb_read_mmio(dev, NETWORK_IGB_REG_EICR);
    uint32_t isr = network_igb_read_mmio(dev, NETWORK_IGB_REG_ICR);

    char_t buffer1[64] = {0};
    char_t buffer2[64] = {0};

    utoh_with_buffer(buffer1, eicr);
    utoh_with_buffer(buffer2, isr);

    video_text_print("tx isr eicr 0x");
    video_text_print(buffer1);
    video_text_print(" isr 0x");
    video_text_print(buffer2);

    boolean_t is_tx = eicr & (1 << 1);

    if(!is_tx) {
        video_text_print(" not tx");
    }

    video_text_print("\n");

    // clearing the pending interrupts
    network_igb_write_mmio(dev, NETWORK_IGB_REG_EICR, 1 << 1);
    network_igb_write_mmio(dev, NETWORK_IGB_REG_ICR, 1 << 0); // clear tx isr


    pci_msix_clear_pending_bit((pci_generic_device_t*)dev->pci_netdev->pci_header, dev->msix_cap, 1);

    apic_eoi();
    return 0;
}

static int8_t network_igb_rx_isr(interrupt_frame_ext_t* frame)  {
    UNUSED(frame);

    const network_igb_dev_t* dev = NULL;

    uint8_t rx_isr = frame->interrupt_number - INTERRUPT_IRQ_BASE;

    for(uint64_t i = 0; i < list_size(igb_net_devs); i++) {
        dev = list_get_data_at_position(igb_net_devs, i);

        if(dev->rx_isr == rx_isr) {
            break;
        }
    }

    if(!dev) {
        video_text_print("no dev\n");
        return -1;
    }

    uint32_t eicr = network_igb_read_mmio(dev, NETWORK_IGB_REG_EICR);
    uint32_t isr = network_igb_read_mmio(dev, NETWORK_IGB_REG_ICR);

    char_t buffer1[64] = {0};
    char_t buffer2[64] = {0};

    utoh_with_buffer(buffer1, eicr);
    utoh_with_buffer(buffer2, isr);

    boolean_t is_rx = eicr & (1 << 0);

    video_text_print("rx isr eicr 0x");
    video_text_print(buffer1);
    video_text_print(" isr 0x");
    video_text_print(buffer2);

    if(!is_rx) {
        video_text_print(" not rx");
    }

    video_text_print("\n");

    if(dev->rx_task_id) {
        task_set_interrupt_received(dev->rx_task_id);
    }

    // clearing the pending interrupts
    network_igb_write_mmio(dev, NETWORK_IGB_REG_EICR, 1 << 0);
    network_igb_write_mmio(dev, NETWORK_IGB_REG_ICR, 1 << 7); // clear rx isr


    // pci_msix_clear_pending_bit((pci_generic_device_t*)dev->pci_netdev->pci_header, dev->msix_cap, 0);

    apic_eoi();

    return 0;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
int8_t network_igb_init(const pci_dev_t* pci_netdev) {
    // logging_set_level(IGB, LOG_TRACE);
    pci_common_header_t* pci_header = pci_netdev->pci_header;

    igb_net_devs = list_create_list_with_heap(NULL);

    if(igb_net_devs == NULL) {
        PRINTLOG(IGB, LOG_ERROR, "cannot create igb network devices list");

        return -1;
    }

    network_igb_dev_t* dev = memory_malloc(sizeof(network_igb_dev_t));

    if(dev == NULL) {
        PRINTLOG(IGB, LOG_ERROR, "cannot create igb network device");

        return -1;
    }

    dev->pci_netdev = pci_netdev; // pci structure

    dev->rx_count = 0;
    dev->tx_count = 0;
    dev->packets_dropped = 0;

    pci_generic_device_t* pci_dev = (pci_generic_device_t*)pci_header;



    if(pci_dev->common_header.status.capabilities_list) {
        PRINTLOG(IGB, LOG_TRACE, "pci dev %02x:%02x:%02x.%02x -> cap pointer 0x%x",
                 pci_netdev->group_number, pci_netdev->bus_number, pci_netdev->device_number, pci_netdev->function_number,
                 pci_dev->capabilities_pointer);

        pci_capability_t* pci_cap = (pci_capability_t*)(((uint8_t*)pci_dev) + pci_dev->capabilities_pointer);

        while(pci_cap->capability_id != 0xFF) {
            PRINTLOG(IGB, LOG_TRACE, "cap 0x%x next 0x%x",
                     pci_cap->capability_id, pci_cap->next_pointer);

            if(pci_cap->capability_id == PCI_DEVICE_CAPABILITY_MSIX) {
                PRINTLOG(IGB, LOG_TRACE, "msix capability found");
                dev->msix_cap = (pci_capability_msix_t*)pci_cap;
                break;
            }

            if(pci_cap->next_pointer == NULL) {
                break;
            }

            pci_cap = (pci_capability_t*)(((uint8_t*)pci_dev) + pci_cap->next_pointer);
        }
    }

    if(!dev->msix_cap) {
        PRINTLOG(IGB, LOG_ERROR, "no msix capability found");

        memory_free(dev);

        return -1;
    }

    if(pci_msix_configure(pci_dev, dev->msix_cap) != 0) {
        PRINTLOG(IGB, LOG_ERROR, "cannot configure msix");

        memory_free(dev);

        return -1;
    }

    pci_bar_register_t* bar = &pci_dev->bar0;

    uint64_t bar_va = 0;

    if(bar->bar_type.type == 0) {
        uint64_t bar_fa = pci_get_bar_address(pci_dev, 0);

        PRINTLOG(IGB, LOG_TRACE, "frame address at bar 0x%llx", bar_fa);

        frame_t* bar_frames = frame_get_allocator()->get_reserved_frames_of_address(frame_get_allocator(), (void*)bar_fa);
        uint64_t size = pci_get_bar_size(pci_dev, 0);
        PRINTLOG(IGB, LOG_TRACE, "bar size 0x%llx", size);
        uint64_t bar_frm_cnt = (size + FRAME_SIZE - 1) / FRAME_SIZE;
        frame_t bar_req_frm = {bar_fa, bar_frm_cnt, FRAME_TYPE_RESERVED, 0};

        bar_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(bar_fa);

        if(bar_frames == NULL) {
            PRINTLOG(IGB, LOG_TRACE, "cannot find reserved frames for 0x%llx and try to reserve", bar_fa);

            if(frame_get_allocator()->allocate_frame(frame_get_allocator(), &bar_req_frm) != 0) {
                PRINTLOG(IGB, LOG_ERROR, "cannot allocate frame");
                memory_free(dev);

                return NULL;
            }
        }

        memory_paging_add_va_for_frame(bar_va, &bar_req_frm, MEMORY_PAGING_PAGE_TYPE_NOEXEC);
    }

    if(bar_va == 0) {
        memory_free(dev);

        return -1;
    }

    // register the MMIO address
    dev->mmio_va = bar_va;

    PRINTLOG(IGB, LOG_TRACE, "mmio address at 0x%llx", dev->mmio_va);

    // disable interrupts
    network_igb_disable_interrupts(dev);

    // reset the device
    network_igb_reset(dev);

    // disable interrupts
    network_igb_disable_interrupts(dev);

    PRINTLOG(IGB, LOG_TRACE, "device reset");

    uint32_t status = network_igb_read_mmio(dev, NETWORK_IGB_REG_STATUS);

    PRINTLOG(IGB, LOG_TRACE, "device status 0x%x", status);

    // read the MAC address
    network_igb_read_mac_address(dev, &dev->mac);

    uint8_t* mac_tmp = (uint8_t*)&dev->mac;

    PRINTLOG(IGB, LOG_TRACE, "device has mac %02x:%02x:%02x:%02x:%02x:%02x", mac_tmp[0], mac_tmp[1], mac_tmp[2], mac_tmp[3], mac_tmp[4], mac_tmp[5]);

    // set the LINK UP
    network_igb_write_mmio(dev, NETWORK_IGB_REG_CTRL, network_igb_read_mmio(dev, NETWORK_IGB_REG_CTRL) | NETWORK_IGB_CTRL_SLU);

    for(int32_t i = 0; i < 128; i++) {
        network_igb_write_mmio(dev, NETWORK_IGB_REG_MTA + i * 4, 0);
    }

    // start the RX/TX processes
    if(network_igb_rx_init(dev) != 0) {
        PRINTLOG(IGB, LOG_ERROR, "cannot initialize rx queue");
        memory_free(dev);

        return -1;
    }

    if(network_igb_tx_init(dev) != 0) {
        PRINTLOG(IGB, LOG_ERROR, "cannot initialize tx queue");
        memory_free(dev);

        return -1;
    }

    // register the interrupt handler
    dev->rx_isr = pci_msix_set_isr(pci_dev, dev->msix_cap, 0, &network_igb_rx_isr);
    dev->tx_isr = pci_msix_set_isr(pci_dev, dev->msix_cap, 1, &network_igb_tx_isr);
    dev->other_isr = pci_msix_set_isr(pci_dev, dev->msix_cap, 2, &network_igb_other_isr);

    network_igb_write_mmio(dev, NETWORK_IGB_REG_IVAR, 0x8180);
    network_igb_write_mmio(dev, NETWORK_IGB_REG_IVAR_MISC, 0x8200);

    network_igb_write_mmio(dev, NETWORK_IGB_REG_VLAN_ETHER_TYPE, 0x8100);

    network_igb_write_mmio(dev, 0xb608, 0x1800000);


    network_igb_write_mmio(dev, NETWORK_IGB_REG_MDIC, 0x4201b40);

    network_igb_write_mmio(dev, NETWORK_IGB_REG_CTRL_EXT, network_igb_read_mmio(dev, NETWORK_IGB_REG_CTRL_EXT) | 0x10000000);

    // enable all interrupts (and clear existing pending ones)
    network_igb_enable_interrupts(dev);

    network_igb_write_mmio(dev, NETWORK_IGB_REG_RCTL, network_igb_read_mmio(dev, NETWORK_IGB_REG_RCTL) | NETWORK_IGB_RCTL_EN);

    void** rx_args = memory_malloc(sizeof(void*) * 1);

    if(rx_args == NULL) {
        PRINTLOG(IGB, LOG_ERROR, "cannot allocate memory for rx task args");

        return -1;
    }

    rx_args[0] = (void*)dev;

    uint64_t rx_task_id = task_create_task(NULL, 2 << 20, 64 << 10, &network_igb_process_rx, 1, rx_args, "igb rx");
    dev->rx_task_id = rx_task_id;


    task_create_task(NULL, 2 << 20, 64 << 10, &network_igb_process_tx, 0, NULL, "igb tx");

    list_list_insert(igb_net_devs, dev);

    PRINTLOG(IGB, LOG_INFO, "device initialized");

    return 0;
}
#pragma GCC diagnostic pop

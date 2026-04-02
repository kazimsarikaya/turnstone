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
#include <memory/frame.h>
#include <memory/paging.h>
#include <acpi.h>
#include <acpi/aml.h>
#include <cpu/interrupt.h>
#include <cpu.h>
#include <cpu/cpu_state.h>
#include <apic.h>
#include <utils.h>
#include <cpu/task.h>
#include <strings.h>
#include <device/mmio.h>

MODULE("turnstone.kernel.hw.network.igb");

list_t* igb_net_devs = NULL;

static inline uint32_t network_igb_read_mmio(const network_igb_dev_t* dev, uint32_t offset) {
    return mmio_read(dev->mmio_va + offset, sizeof(uint32_t));
}

static inline void network_igb_write_mmio(const network_igb_dev_t* dev, uint32_t offset, uint32_t value) {
    mmio_write(dev->mmio_va + offset, value, sizeof(uint32_t));
}

static inline void network_igb_reset(const network_igb_dev_t* dev) {
    network_igb_write_mmio(dev, NETWORK_IGB_REG_CTRL, NETWORK_IGB_CTRL_RST);
    time_timer_spinsleep(1000);

    while( network_igb_read_mmio(dev, NETWORK_IGB_REG_CTRL) & NETWORK_IGB_CTRL_RST ) {
        time_timer_spinsleep(1000);
    }
}

static inline void network_igb_read_mac_address(const network_igb_dev_t* dev, network_mac_address_t* mac) {
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

static inline uint16_t network_igb_get_mtu(const network_igb_dev_t* dev) {
    uint32_t rctl = network_igb_read_mmio(dev, NETWORK_IGB_REG_RCTL);

    if(rctl & 0x100) {
        return 1500;
    }

    uint32_t rlpml = network_igb_read_mmio(dev, NETWORK_IGB_REG_RLPML);

    return rlpml - 22;
}

static inline void network_igb_disable_interrupts(const network_igb_dev_t* dev) {
    network_igb_write_mmio(dev, NETWORK_IGB_REG_IMC, 0xFFFFFFFF);
}

static inline void network_igb_enable_interrupts(const network_igb_dev_t* dev) {
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
    PRINTLOG(NETWORK, LOG_INFO, "network tx task started. number of network devices: 0x%llx", list_size(igb_net_devs));

    for(uint64_t dev_idx = 0; dev_idx < list_size(igb_net_devs); dev_idx++) {
        network_igb_dev_t* dev = (network_igb_dev_t*)list_get_data_at_position(igb_net_devs, dev_idx);

        cpu_cli();
        pci_msix_update_lapic((pci_generic_device_t*)dev->pci_netdev->pci_header, dev->msix_cap, 1);
        pci_msix_clear_pending_bit((pci_generic_device_t*)dev->pci_netdev->pci_header, dev->msix_cap, 1);
        cpu_sti();

        dev->return_queue = list_create_queue_with_heap(NULL);
        task_add_message_queue(dev->return_queue);

        if(network_register_network_info(dev->mac, dev->mtu, dev->return_queue,
                                         true, true, 122) != 0) {
            PRINTLOG(NETWORK, LOG_ERROR, "failed to register network info for device with mac %02x:%02x:%02x:%02x:%02x:%02x",
                     dev->mac[0], dev->mac[1], dev->mac[2],
                     dev->mac[3], dev->mac[4], dev->mac[5]);

            return -1;
        }

        void** args = memory_malloc(sizeof(void*) * 2);

        if(args == NULL) {
            return -1;
        }

        char_t* dhcp_task_name = strprintf("dhcp-%02x%02x%02x%02x%02x%02x",
                                           dev->mac[0], dev->mac[1], dev->mac[2],
                                           dev->mac[3], dev->mac[4], dev->mac[5]);

        args[0] = (void*)dev->mac;
        args[1] = dev->return_queue;

        task_create_task(dhcp_task_name, network_dhcpv4_send_discover,
                         2, args,
                         2 << 20, 64 << 10,
                         .proximity_domain_hint = cpu_state->proximity_domain);
        memory_free(dhcp_task_name);
    }

    while(true) {
        boolean_t packet_exists = false;

        for(uint64_t dev_idx = 0; dev_idx < list_size(igb_net_devs); dev_idx++) {
            network_igb_dev_t* dev = (network_igb_dev_t*)list_get_data_at_position(igb_net_devs, dev_idx);

            while(list_size(dev->return_queue)) {
                const network_transmit_packet_t* packet = list_queue_pop(dev->return_queue);

                if(packet) {
                    packet_exists = true;

                    // first context

                    dev->tx_desc[dev->tx_tail].context.vlan_macip_lens = packet->is_vlan_tagged ?
                                                                         (packet->vlan_id << NETWORK_IGB_TX_FLAGS_VLAN_SHIFT) : 0;
                    dev->tx_desc[dev->tx_tail].transmit.read.cmd_type_len = NETWORK_IGB_ADVTXD_DCMD_DEXT |
                                                                            NETWORK_IGB_ADVTXD_DTYP_CTXT;


                    dev->tx_tail = (dev->tx_tail + 1) % NETWORK_IGB_NUM_TX_DESCRIPTORS;

                    // then transmit

                    uint8_t* buffer = (uint8_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(HARDWARE, dev->tx_desc[dev->tx_tail].transmit.read.buffer_addr);

                    memory_memcopy(packet->packet_data, buffer, packet->packet_len);

                    dev->tx_desc[dev->tx_tail].transmit.read.cmd_type_len = NETWORK_IGB_ADVTXD_DCMD_EOP |
                                                                            NETWORK_IGB_ADVTXD_DCMD_DEXT |
                                                                            NETWORK_IGB_ADVTXD_DTYP_DATA |
                                                                            (packet->is_vlan_tagged ? NETWORK_IGB_ADVTXD_DCMD_VLE : 0) |
                                                                            packet->packet_len;

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

        if(!packet_exists) {
            task_yield_with_message_waiting();
        }

    }

    return 0;
}

static int8_t network_igb_rx_init(network_igb_dev_t* dev) {
    PRINTLOG(IGB, LOG_TRACE, "try to initialize rx queue");

    frame_allocator_t* fa = frame_get_allocator();

    // allocate a 10K buffer for the receive queue
    uint64_t packet_buffer_size    = NETWORK_IGB_RX_BUFFER_SIZE * NETWORK_IGB_NUM_RX_DESCRIPTORS;
    uint64_t packet_buffer_frm_cnt = (packet_buffer_size + FRAME_SIZE - 1)  / FRAME_SIZE;

    frame_t* rx_packet_buffer_frames;

    if(fa->allocate_frame_by_count(fa, dev->pci_netdev->proximity_domain,
                                   packet_buffer_frm_cnt, FRAME_ALLOCATION_TYPE_BLOCK | FRAME_ALLOCATION_TYPE_RESERVED,
                                   &rx_packet_buffer_frames, NULL) != 0) {
        PRINTLOG(IGB, LOG_ERROR, "cannot allocate frames for rx packet buffer");

        return -1;
    }

    rx_packet_buffer_frames->frame_attributes |= FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED;

    uint64_t rx_packet_buffer_fa = rx_packet_buffer_frames->frame_address;
    uint64_t rx_packet_buffer_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(HARDWARE, rx_packet_buffer_fa);
    if(memory_paging_add_va_for_frame(rx_packet_buffer_va, rx_packet_buffer_frames, MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
        PRINTLOG(IGB, LOG_ERROR, "cannot map rx packet buffer frames");

        fa->release_frame(fa, rx_packet_buffer_frames);

        return -1;
    }

    memory_memclean((void*)rx_packet_buffer_va, packet_buffer_size);

    dev->rx_packet_buffer_fa = rx_packet_buffer_fa;
    dev->rx_packet_buffer_va = rx_packet_buffer_va;

    // allocate a 256 byte buffer for the receive queue headers
    uint64_t header_buffer_size    = NETWORK_IGB_RX_HEADER_SIZE * NETWORK_IGB_NUM_RX_DESCRIPTORS;
    uint64_t header_buffer_frm_cnt = (header_buffer_size + FRAME_SIZE - 1)  / FRAME_SIZE;

    frame_t* rx_header_buffer_frames;

    if(fa->allocate_frame_by_count(fa, dev->pci_netdev->proximity_domain,
                                   header_buffer_frm_cnt, FRAME_ALLOCATION_TYPE_BLOCK | FRAME_ALLOCATION_TYPE_RESERVED,
                                   &rx_header_buffer_frames, NULL) != 0) {
        PRINTLOG(IGB, LOG_ERROR, "cannot allocate frames for rx header buffer");

        fa->release_frame(fa, rx_packet_buffer_frames);

        return -1;
    }

    rx_header_buffer_frames->frame_attributes |= FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED;

    uint64_t rx_header_buffer_fa = rx_header_buffer_frames->frame_address;
    uint64_t rx_header_buffer_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(HARDWARE, rx_header_buffer_frames->frame_address);
    if(memory_paging_add_va_for_frame(rx_header_buffer_va, rx_header_buffer_frames, MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
        PRINTLOG(IGB, LOG_ERROR, "cannot map rx header buffer frames");

        fa->release_frame(fa, rx_header_buffer_frames);
        fa->release_frame(fa, rx_packet_buffer_frames);

        return -1;
    }

    memory_memclean((void*)rx_header_buffer_va, header_buffer_size);

    dev->rx_header_buffer_fa = rx_header_buffer_fa;
    dev->rx_header_buffer_va = rx_header_buffer_va;

    // allocate a 16 byte buffer for the receive queue meta data
    uint64_t queue_meta_frm_cnt = ((sizeof(network_igb_rx_desc_t) * NETWORK_IGB_NUM_RX_DESCRIPTORS)  + FRAME_SIZE - 1 ) / FRAME_SIZE;

    frame_t* queue_meta_frames;

    if(fa->allocate_frame_by_count(fa, dev->pci_netdev->proximity_domain,
                                   queue_meta_frm_cnt, FRAME_ALLOCATION_TYPE_BLOCK | FRAME_ALLOCATION_TYPE_RESERVED,
                                   &queue_meta_frames, NULL) != 0) {
        PRINTLOG(IGB, LOG_ERROR, "cannot allocate frames for rx queue meta");

        fa->release_frame(fa, rx_header_buffer_frames);
        fa->release_frame(fa, rx_packet_buffer_frames);

        return -1;
    }

    queue_meta_frames->frame_attributes |= FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED;


    uint64_t queue_meta_fa = queue_meta_frames->frame_address;
    uint64_t queue_meta_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(HARDWARE, queue_meta_frames->frame_address);
    if(memory_paging_add_va_for_frame(queue_meta_va, queue_meta_frames, MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
        PRINTLOG(IGB, LOG_ERROR, "cannot map rx queue meta frames");

        fa->release_frame(fa, queue_meta_frames);
        fa->release_frame(fa, rx_header_buffer_frames);
        fa->release_frame(fa, rx_packet_buffer_frames);

        return -1;
    }

    memory_memclean((void*)queue_meta_va, sizeof(network_igb_rx_desc_t) * NETWORK_IGB_NUM_RX_DESCRIPTORS);

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

    uint64_t queue_frm_cnt      = (queue_size + FRAME_SIZE - 1)  / FRAME_SIZE;
    uint64_t queue_meta_frm_cnt = ((sizeof(network_igb_rx_desc_t) * NETWORK_IGB_NUM_TX_DESCRIPTORS)  + FRAME_SIZE - 1 ) / FRAME_SIZE;


    frame_t* queue_frames;
    frame_t* queue_meta_frames;

    PRINTLOG(IGB, LOG_TRACE, "tx queue size 0x%llx queue frm count 0x%llx meta frm count 0x%llx", queue_size, queue_frm_cnt, queue_meta_frm_cnt);

    frame_allocator_t* fa = frame_get_allocator();

    if(fa->allocate_frame_by_count(fa, dev->pci_netdev->proximity_domain,
                                   queue_frm_cnt, FRAME_ALLOCATION_TYPE_BLOCK | FRAME_ALLOCATION_TYPE_RESERVED,
                                   &queue_frames, NULL) != 0 ||
       fa->allocate_frame_by_count(fa, dev->pci_netdev->proximity_domain,
                                   queue_meta_frm_cnt, FRAME_ALLOCATION_TYPE_BLOCK | FRAME_ALLOCATION_TYPE_RESERVED,
                                   &queue_meta_frames, NULL) != 0) {
        PRINTLOG(IGB, LOG_ERROR, "cannot allocate frames for tx queue");

        return -1;
    }

    queue_frames->frame_attributes      |= FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED;
    queue_meta_frames->frame_attributes |= FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED;

    uint64_t queue_fa = queue_frames->frame_address;
    uint64_t queue_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(HARDWARE, queue_frames->frame_address);
    if(memory_paging_add_va_for_frame(queue_va, queue_frames, MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
        PRINTLOG(IGB, LOG_ERROR, "cannot map tx queue frames");

        frame_get_allocator()->release_frame(frame_get_allocator(), queue_frames);
        frame_get_allocator()->release_frame(frame_get_allocator(), queue_meta_frames);

        return -1;
    }

    uint64_t queue_meta_fa = queue_meta_frames->frame_address;
    uint64_t queue_meta_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(HARDWARE, queue_meta_frames->frame_address);
    if(memory_paging_add_va_for_frame(queue_meta_va, queue_meta_frames, MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
        PRINTLOG(IGB, LOG_ERROR, "cannot map tx queue meta frames");

        frame_get_allocator()->release_frame(frame_get_allocator(), queue_frames);
        frame_get_allocator()->release_frame(frame_get_allocator(), queue_meta_frames);

        return -1;
    }

    memory_memclean((void*)queue_va, queue_size);
    memory_memclean((void*)queue_meta_va, sizeof(network_igb_tx_desc_t) * NETWORK_IGB_NUM_TX_DESCRIPTORS);

    network_igb_write_mmio(dev, NETWORK_IGB_REG_TDBAL, queue_meta_fa & 0xFFFFFFFF);
    network_igb_write_mmio(dev, NETWORK_IGB_REG_TDBAH, (queue_meta_fa >> 32) & 0xFFFFFFFF);
    dev->tx_desc = (network_igb_tx_desc_t*)queue_meta_va;

    PRINTLOG(IGB, LOG_TRACE, "filling tx queue at 0x%llx", queue_meta_va);

    for(int32_t i = 0; i < NETWORK_IGB_NUM_TX_DESCRIPTORS; i++ ) {
        if(i % 2 == 0) {
            dev->tx_desc[i].context.vlan_macip_lens = 0;
            dev->tx_desc[i].context.seqnum_seed     = 0;
            dev->tx_desc[i].context.type_tucmd_mlhl = 0;
            dev->tx_desc[i].context.mss_l4len_idx   = 0;
            continue;
        } else {
            dev->tx_desc[i].transmit.read.buffer_addr   = queue_fa + i * (8192 + 16);
            dev->tx_desc[i].transmit.read.cmd_type_len  = 0;
            dev->tx_desc[i].transmit.read.olinfo_status = 0;
        }
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

    while(!dev->return_queue) {
        task_yield();
    }

    while(true) {
        boolean_t notify_network_rx = true;

        if(network_received_packets != NULL && dev->return_queue != NULL) {

            while(true) {
                // advance the tail pointer
                ((network_igb_dev_t*)dev)->rx_tail = (dev->rx_tail + 1) % NETWORK_IGB_NUM_RX_DESCRIPTORS;
                // check if the next descriptor is ready
                network_igb_rx_desc_t* desc = (network_igb_rx_desc_t*)&dev->rx_desc[dev->rx_tail];
                uint32_t status_error       = desc->wb.upper.status_error;
                // first 20bits are status, last 12 bits are error
                uint32_t status = status_error & 0xFFFFF;
                uint32_t error  = status_error >> 20;

                PRINTLOG(IGB, LOG_TRACE, "rx status 0x%x error 0x%x", status, error);

                if( !(status & NETWORK_IGB_RXD_STAT_DD) ) { // descriptor is not ready
                    ((network_igb_dev_t*)dev)->rx_tail = (dev->rx_tail - 1) % NETWORK_IGB_NUM_RX_DESCRIPTORS;
                    PRINTLOG(IGB, LOG_TRACE, "rx descriptor is not ready");
                    break;
                }

                // we get packet address with calculated offset
                uint8_t* pkt       = (uint8_t*)(dev->rx_packet_buffer_va + dev->rx_tail * NETWORK_IGB_RX_BUFFER_SIZE);
                uint16_t pktlen    = desc->wb.upper.length;
                uint16_t vlan_id   = desc->wb.upper.vlan;
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

                    pkt = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(HARDWARE, pkt);

                    network_received_packet_t* packet = memory_malloc_ext(list_get_heap(network_received_packets), sizeof(network_received_packet_t), 0);

                    if(packet == NULL) {
                        PRINTLOG(IGB, LOG_ERROR, "failed to allocate packet");
                        notify_network_rx = true;

                        task_yield();

                        continue;
                    }

                    packet->packet_len     = pktlen;
                    packet->return_queue   = dev->return_queue;
                    packet->network_type   = NETWORK_TYPE_ETHERNET;
                    packet->is_vlan_tagged = status & NETWORK_IGB_RXD_STAT_VD ? true : false;
                    packet->vlan_id        = vlan_id;
                    packet->tx_task_id     = dev->tx_task_id;

                    memory_memcopy(dev->mac, packet->mac, sizeof(network_mac_address_t));

                    packet->packet_data = memory_malloc_ext(list_get_heap(network_received_packets), dev->mtu + 22, 0);

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

        task_yield_with_message_waiting();
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

    uint32_t isr = network_igb_read_mmio(dev, NETWORK_IGB_REG_ICR);

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
    pci_common_header_t* pci_header = &pci_netdev->pci_header->common;

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

    dev->rx_count        = 0;
    dev->tx_count        = 0;
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

        uint64_t size = pci_get_bar_size(pci_dev, 0);
        PRINTLOG(IGB, LOG_TRACE, "bar size 0x%llx", size);
        uint64_t bar_frm_cnt = (size + FRAME_SIZE - 1) / FRAME_SIZE;
        frame_t bar_req_frm  = {pci_netdev->proximity_domain, bar_fa, bar_frm_cnt, FRAME_TYPE_RESERVED, 0};

        bar_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(HARDWARE, bar_fa);

        if(memory_paging_add_va_for_frame(bar_va, &bar_req_frm, MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
            PRINTLOG(IGB, LOG_ERROR, "cannot map bar frames");

            memory_free(dev);

            return -1;
        }
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

    PRINTLOG(IGB, LOG_TRACE, "device has mac %02x:%02x:%02x:%02x:%02x:%02x",
             mac_tmp[0], mac_tmp[1], mac_tmp[2], mac_tmp[3], mac_tmp[4], mac_tmp[5]);

    dev->mtu = network_igb_get_mtu(dev);

    uint32_t igb_ctrl = network_igb_read_mmio(dev, NETWORK_IGB_REG_CTRL);

    igb_ctrl |= NETWORK_IGB_CTRL_SLU | NETWORK_IGB_CTRL_VME;

    // set the LINK UP
    network_igb_write_mmio(dev, NETWORK_IGB_REG_CTRL, igb_ctrl);

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
    dev->rx_isr    = pci_msix_set_isr(pci_dev, dev->msix_cap, 0, &network_igb_rx_isr);
    dev->tx_isr    = pci_msix_set_isr(pci_dev, dev->msix_cap, 1, &network_igb_tx_isr);
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

    list_list_insert(igb_net_devs, dev);

    PRINTLOG(IGB, LOG_INFO, "device proximity domain 0x%x", pci_netdev->proximity_domain);

    void** rx_args = memory_malloc(sizeof(void*) * 1);

    if(rx_args == NULL) {
        PRINTLOG(IGB, LOG_ERROR, "cannot allocate memory for rx task args");

        return -1;
    }

    rx_args[0] = (void*)dev;

    char_t* rx_task_name = strprintf("igb-rx-%02x%02x%02x%02x%02x%02x",
                                     dev->mac[0], dev->mac[1], dev->mac[2],
                                     dev->mac[3], dev->mac[4], dev->mac[5]);

    uint64_t rx_task_id = task_create_task(rx_task_name, network_igb_process_rx,
                                           1, rx_args,
                                           2 << 20, 64 << 10,
                                           .proximity_domain_hint = pci_netdev->proximity_domain);

    dev->rx_task_id = rx_task_id;
    memory_free(rx_task_name);

    // FIXME: this should be device specific and not global
    uint64_t tx_task_id = task_create_task("igb-tx", network_igb_process_tx,
                                           .heap_size             = 2 << 20, 64 << 10,
                                           .proximity_domain_hint = pci_netdev->proximity_domain);

    dev->tx_task_id = tx_task_id;

    PRINTLOG(IGB, LOG_INFO, "device initialized");

    return 0;
}
#pragma GCC diagnostic pop

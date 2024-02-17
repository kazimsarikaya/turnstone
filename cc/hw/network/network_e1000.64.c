/**
 * @file network_e1000.64.c
 * @brief Intel 8254x (e1000) network driver.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <driver/network_e1000.h>
#include <memory.h>
#include <utils.h>
#include <logging.h>
#include <time/timer.h>
#include <network.h>
#include <network/network_ethernet.h>
#include <memory/frame.h>
#include <memory/paging.h>
#include <acpi.h>
#include <acpi/aml.h>
#include <cpu/interrupt.h>
#include <cpu.h>
#include <apic.h>
#include <utils.h>
#include <cpu/task.h>

MODULE("turnstone.kernel.hw.network.e1000");

list_t* e1000_net_devs = NULL;

uint16_t network_e1000_eeprom_read(network_e1000_dev_t* dev, uint8_t addr);
uint16_t network_e1000_phy_read(network_e1000_dev_t* dev, int regaddr);
void     network_e1000_rx_enable(network_e1000_dev_t* dev);
int8_t   network_e1000_rx_init(network_e1000_dev_t* dev);
int8_t   network_e1000_tx_init(network_e1000_dev_t* dev);
void     network_e1000_rx_poll(const network_e1000_dev_t* netdev);
int8_t   network_e1000_rx_isr(interrupt_frame_ext_t* frame);
int8_t   network_e1000_process_tx(void);
void     network_e1000_phy_write(network_e1000_dev_t* dev, int regaddr, uint16_t data);


int8_t network_e1000_process_tx(void) {

    for(uint64_t dev_idx = 0; dev_idx < list_size(e1000_net_devs); dev_idx++) {
        const network_e1000_dev_t* dev = list_get_data_at_position(e1000_net_devs, dev_idx);
        task_add_message_queue(dev->transmit_queue);
    }

    while(1) {
        boolean_t packet_exists = 0;

        for(uint64_t dev_idx = 0; dev_idx < list_size(e1000_net_devs); dev_idx++) {
            network_e1000_dev_t* dev = (network_e1000_dev_t*)list_get_data_at_position(e1000_net_devs, dev_idx);

            while(list_size(dev->transmit_queue)) {
                const network_transmit_packet_t* packet = list_queue_pop(dev->transmit_queue);

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
                    dev->tx_tail = (dev->tx_tail + 1) % NETWORK_E1000_NUM_TX_DESCRIPTORS;
                    dev->mmio->tdt = dev->tx_tail;

                    dev->tx_count++;

                }

                PRINTLOG(NETWORK, LOG_TRACE, "tx queue size 0x%llx", list_size(dev->transmit_queue));
            }

        }

        if(packet_exists == 0) {
            task_set_message_waiting();
            task_yield();
        }

    }

    return 0;
}


uint16_t network_e1000_eeprom_read(network_e1000_dev_t* dev, uint8_t addr) {
    uint16_t data = 0;
    uint32_t tmp = 0;

    dev->mmio->eerd = ((1) | ((uint32_t)(addr) << 8) );

    while( !((tmp = dev->mmio->eerd) & (1 << 4)) ) {
        time_timer_spinsleep(1);
    }

    data = (uint16_t)((tmp >> 16) & 0xFFFF);

    return data;
}

uint16_t network_e1000_phy_read(network_e1000_dev_t* dev, int regaddr) {
    uint16_t data = 0;

    dev->mmio->mdic = (((regaddr & 0x1F) << 16) | NETWORK_E1000_MDIC_PHYADD | NETWORK_E1000_MDIC_I | NETWORK_E1000_MDIC_OP_READ);

    while( !(dev->mmio->mdic & (NETWORK_E1000_MDIC_R | NETWORK_E1000_MDIC_E)) ) {
        time_timer_spinsleep(1);
    }

    if( dev->mmio->mdic & NETWORK_E1000_MDIC_E ) {
        PRINTLOG(E1000, LOG_ERROR, "i825xx: MDI READ ERROR\n");

        return -1;
    }

    data = (uint16_t)(dev->mmio->mdic & 0xFFFF);

    return data;
}

void network_e1000_phy_write(network_e1000_dev_t* dev, int regaddr, uint16_t data) {
    dev->mmio->mdic = ((data & 0xFFFF) | ((regaddr & 0x1F) << 16) | NETWORK_E1000_MDIC_PHYADD | NETWORK_E1000_MDIC_I | NETWORK_E1000_MDIC_OP_WRITE);

    while( !(dev->mmio->mdic & (NETWORK_E1000_MDIC_R | NETWORK_E1000_MDIC_E)) )
    {
        time_timer_spinsleep(1);
    }

    if( dev->mmio->mdic & NETWORK_E1000_MDIC_E ) {
        PRINTLOG(E1000, LOG_ERROR, "i825xx: MDI WRITE ERROR\n");
        return;
    }
}

void network_e1000_rx_enable(network_e1000_dev_t* dev) {
    dev->mmio->rctl |= NETWORK_E1000_RCTL_EN;
}

int8_t network_e1000_rx_init(network_e1000_dev_t* dev) {
    PRINTLOG(E1000, LOG_TRACE, "try to initialize rx queue");

    uint64_t queue_size = (8192 + 16) * NETWORK_E1000_NUM_RX_DESCRIPTORS;

    uint64_t queue_frm_cnt = (queue_size + FRAME_SIZE - 1)  / FRAME_SIZE;
    uint64_t queue_meta_frm_cnt = ((sizeof(network_e1000_rx_desc_t) * NETWORK_E1000_NUM_RX_DESCRIPTORS)  + FRAME_SIZE - 1 ) / FRAME_SIZE;


    frame_t* queue_frames;
    frame_t* queue_meta_frames;

    PRINTLOG(E1000, LOG_TRACE, "rx queue size 0x%llx queue frm count 0x%llx meta frm count 0x%llx", queue_size, queue_frm_cnt, queue_meta_frm_cnt);


    if(KERNEL_FRAME_ALLOCATOR->allocate_frame_by_count(KERNEL_FRAME_ALLOCATOR, queue_frm_cnt, FRAME_ALLOCATION_TYPE_BLOCK | FRAME_ALLOCATION_TYPE_RESERVED, &queue_frames, NULL) == 0 &&
       KERNEL_FRAME_ALLOCATOR->allocate_frame_by_count(KERNEL_FRAME_ALLOCATOR, queue_meta_frm_cnt, FRAME_ALLOCATION_TYPE_BLOCK | FRAME_ALLOCATION_TYPE_RESERVED, &queue_meta_frames, NULL) == 0) {
        queue_frames->frame_attributes |= FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED;
        queue_meta_frames->frame_attributes |= FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED;

        uint64_t queue_fa = queue_frames->frame_address;
        uint64_t queue_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(queue_frames->frame_address);
        memory_paging_add_va_for_frame(queue_va, queue_frames, MEMORY_PAGING_PAGE_TYPE_NOEXEC);

        uint64_t queue_meta_fa = queue_meta_frames->frame_address;
        uint64_t queue_meta_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(queue_meta_frames->frame_address);
        memory_paging_add_va_for_frame(queue_meta_va, queue_meta_frames, MEMORY_PAGING_PAGE_TYPE_NOEXEC);

        dev->mmio->rdbal = queue_meta_fa & 0xFFFFFFFF;
        dev->mmio->rdbah = (queue_meta_fa >> 32) & 0xFFFFFFFF;
        dev->rx_desc = (network_e1000_rx_desc_t*)queue_meta_va;

        PRINTLOG(E1000, LOG_TRACE, "filling rx queue at 0x%llx", queue_meta_va);

        for(int32_t i = 0; i < NETWORK_E1000_NUM_RX_DESCRIPTORS; i++ ) {
            dev->rx_desc[i].address = queue_fa + i * (8192 + 16);
            dev->rx_desc[i].status = 0;
        }

        // receive buffer length; NETWORK_E1000_NUM_RX_DESCRIPTORS 16-byte descriptors
        dev->mmio->rdlen = (uint32_t)(NETWORK_E1000_NUM_RX_DESCRIPTORS * 16);

        // setup head and tail pointers
        dev->mmio->rdh = 0;
        dev->mmio->rdt = NETWORK_E1000_NUM_RX_DESCRIPTORS;
        dev->rx_tail = 0;

        // set the receieve control register (promisc ON, 8K pkt size)
        dev->mmio->rctl = (NETWORK_E1000_RCTL_SBP | NETWORK_E1000_RCTL_UPE | NETWORK_E1000_RCTL_MPE | NETWORK_E1000_RDMTS_HALF | NETWORK_E1000_RCTL_SECRC | NETWORK_E1000_RCTL_LPE | NETWORK_E1000_RCTL_BAM | NETWORK_E1000_RCTL_BSIZE_8192);

        PRINTLOG(E1000, LOG_TRACE, "rx queue initialized");

        return 0;
    }

    return -1;
}

int8_t network_e1000_tx_init(network_e1000_dev_t* dev) {
    uint64_t queue_size = (8192 + 16) * NETWORK_E1000_NUM_TX_DESCRIPTORS;

    uint64_t queue_frm_cnt = (queue_size + FRAME_SIZE - 1)  / FRAME_SIZE;
    uint64_t queue_meta_frm_cnt = ((sizeof(network_e1000_rx_desc_t) * NETWORK_E1000_NUM_TX_DESCRIPTORS)  + FRAME_SIZE - 1 ) / FRAME_SIZE;


    frame_t* queue_frames;
    frame_t* queue_meta_frames;

    PRINTLOG(E1000, LOG_TRACE, "tx queue size 0x%llx queue frm count 0x%llx meta frm count 0x%llx", queue_size, queue_frm_cnt, queue_meta_frm_cnt);

    if(KERNEL_FRAME_ALLOCATOR->allocate_frame_by_count(KERNEL_FRAME_ALLOCATOR, queue_frm_cnt, FRAME_ALLOCATION_TYPE_BLOCK | FRAME_ALLOCATION_TYPE_RESERVED, &queue_frames, NULL) == 0 &&
       KERNEL_FRAME_ALLOCATOR->allocate_frame_by_count(KERNEL_FRAME_ALLOCATOR, queue_meta_frm_cnt, FRAME_ALLOCATION_TYPE_BLOCK | FRAME_ALLOCATION_TYPE_RESERVED, &queue_meta_frames, NULL) == 0) {
        queue_frames->frame_attributes |= FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED;
        queue_meta_frames->frame_attributes |= FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED;

        uint64_t queue_fa = queue_frames->frame_address;
        uint64_t queue_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(queue_frames->frame_address);
        memory_paging_add_va_for_frame(queue_va, queue_frames, MEMORY_PAGING_PAGE_TYPE_NOEXEC);

        uint64_t queue_meta_fa = queue_meta_frames->frame_address;
        uint64_t queue_meta_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(queue_meta_frames->frame_address);
        memory_paging_add_va_for_frame(queue_meta_va, queue_meta_frames, MEMORY_PAGING_PAGE_TYPE_NOEXEC);

        dev->mmio->tdbal = queue_meta_fa & 0xFFFFFFFF;
        dev->mmio->tdbah = (queue_meta_fa >> 32) & 0xFFFFFFFF;
        dev->tx_desc = (network_e1000_tx_desc_t*)queue_meta_va;

        PRINTLOG(E1000, LOG_TRACE, "filling tx queue at 0x%llx", queue_meta_va);

        for(int32_t i = 0; i < NETWORK_E1000_NUM_TX_DESCRIPTORS; i++ ) {
            dev->tx_desc[i].address = queue_fa + i * (8192 + 16);
            dev->tx_desc[i].cmd = 0;
        }

        // receive buffer length; NETWORK_E1000_NUM_RX_DESCRIPTORS 16-byte descriptors
        dev->mmio->tdlen = (uint32_t)(NETWORK_E1000_NUM_TX_DESCRIPTORS * 16);

        // setup head and tail pointers
        dev->mmio->tdh = 0;
        dev->mmio->tdt = NETWORK_E1000_NUM_RX_DESCRIPTORS;
        dev->tx_tail = 0;

        // set the transmit control register (padshortpackets)
        dev->mmio->tctl = (NETWORK_E1000_TCTL_EN | NETWORK_E1000_TCTL_PSP);

        PRINTLOG(E1000, LOG_TRACE, "tx queue initialized");

        return 0;
    }

    return -1;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
// This can be used stand-alone or from an interrupt handler
void network_e1000_rx_poll(const network_e1000_dev_t* dev) {
    while( (dev->rx_desc[dev->rx_tail].status & (1 << 0)) ) {
        // raw packet and packet length (excluding CRC)
        uint8_t* pkt = (void*)dev->rx_desc[dev->rx_tail].address;
        uint16_t pktlen = dev->rx_desc[dev->rx_tail].length;
        boolean_t dropflag = 0;

        if( pktlen < 60 ) {
            dropflag = 1;
        }

        // while not technically an error, there is no support in this driver
        if( !(dev->rx_desc[dev->rx_tail].status & (1 << 1)) ) {
            dropflag = 1;
        }

        if( dev->rx_desc[dev->rx_tail].errors ) {
            PRINTLOG(E1000, LOG_WARNING, "device has rx errors 0x%x", dev->rx_desc[dev->rx_tail].errors);

            dropflag = 1;
        }

        if( !dropflag ) {
            // send the packet to higher layers for parsing
            PRINTLOG(E1000, LOG_TRACE, "packet received with len 0x%x", pktlen);

            pkt = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(pkt);

            network_received_packet_t* packet = memory_malloc(sizeof(network_received_packet_t));

            if(packet == NULL) {
                continue;
            }

            packet->packet_len = pktlen;
            packet->return_queue = dev->transmit_queue;
            packet->network_info = (void*)dev->mac;
            packet->network_type = NETWORK_TYPE_ETHERNET;

            packet->packet_data = memory_malloc(pktlen);

            if(packet->packet_data == NULL) {
                memory_free(packet);

                continue;
            }

            memory_memcopy(pkt, packet->packet_data, pktlen);

            list_queue_push(network_received_packets, packet);
        }

        // update RX counts and the tail pointer
        ((network_e1000_dev_t*)dev)->rx_count++;
        dev->rx_desc[dev->rx_tail].status = (uint16_t)(0);

        ((network_e1000_dev_t*)dev)->rx_tail = (dev->rx_tail + 1) % NETWORK_E1000_NUM_RX_DESCRIPTORS;

        // write the tail to the device
        dev->mmio->rdt = dev->rx_tail;
    }
}
#pragma GCC diagnostic pop

int8_t network_e1000_rx_isr(interrupt_frame_ext_t* frame)  {
    uint8_t intnum = frame->interrupt_number;

    const network_e1000_dev_t* dev = list_get_data_at_position(e1000_net_devs, 0);

    // read the pending interrupt status
    uint32_t icr = dev->mmio->isr_status;

    PRINTLOG(E1000, LOG_TRACE, "interrupt received isr status: %i", icr);

    // tx success stuff
    icr &= ~(3);

    // LINK STATUS CHANGE
    if( icr & (1 << 2) ) {
        icr &= ~(1 << 2);
        dev->mmio->ctrl |= NETWORK_E1000_CTRL_SLU;
    }

    // RX underrun / min threshold
    if( (icr & (1 << 6)) || (icr & (1 << 4)) ) {
        icr &= ~((1 << 6) | (1 << 4));
        PRINTLOG(E1000, LOG_WARNING, "underrun (rx_head = 0x%x, rx_tail = 0x%x)\n", dev->mmio->rdh, dev->rx_tail);

        volatile int32_t i;

        for(i = 0; i < NETWORK_E1000_NUM_RX_DESCRIPTORS; i++) {
            if( dev->rx_desc[i].status ) {
                PRINTLOG(E1000, LOG_TRACE, " pending descriptor (i=0x%x, status=0x%x)\n", i, dev->rx_desc[i].status );
            }
        }

        network_e1000_rx_poll(dev);
    }

    // packet is pending
    if( icr & (1 << 7) ) {
        icr &= ~(1 << 7);
        network_e1000_rx_poll(dev);
    }

    if(icr & 0x8000) {
        icr &= ~(0x8000);
        PRINTLOG(E1000, LOG_TRACE, " tx at low (zero)");
    }

    if( icr ) {
        PRINTLOG(E1000, LOG_WARNING, "unhandled interrupt 0x%02x received! icr 0x%x", intnum, icr);
    }


    // clearing the pending interrupts
    UNUSED(dev->mmio->isr_status);

    apic_eoi();

    return 0;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
int8_t network_e1000_init(const pci_dev_t* pci_netdev) {
    pci_common_header_t* pci_header = pci_netdev->pci_header;

    e1000_net_devs = list_create_list_with_heap(NULL);

    if(e1000_net_devs == NULL) {
        PRINTLOG(E1000, LOG_ERROR, "cannot create e1000 network devices list");

        return -1;
    }

    network_e1000_dev_t* dev = memory_malloc(sizeof(network_e1000_dev_t));

    if(dev == NULL) {
        PRINTLOG(E1000, LOG_ERROR, "cannot create e1000 network device");

        return -1;
    }

    dev->pci_netdev = pci_netdev; // pci structure

    dev->rx_count = 0;
    dev->tx_count = 0;
    dev->packets_dropped = 0;

    dev->transmit_queue = list_create_queue_with_heap(NULL);

    pci_generic_device_t* pci_dev = (pci_generic_device_t*)pci_header;

    pci_bar_register_t* bar = &pci_dev->bar0;

    uint64_t bar_va = 0;

    if(bar->bar_type.type == 0) {
        uint64_t bar_fa = pci_get_bar_address(pci_dev, 0);

        PRINTLOG(E1000, LOG_TRACE, "frame address at bar 0x%llx", bar_fa);

        frame_t* bar_frames = KERNEL_FRAME_ALLOCATOR->get_reserved_frames_of_address(KERNEL_FRAME_ALLOCATOR, (void*)bar_fa);
        uint64_t size = pci_get_bar_size(pci_dev, 0);
        PRINTLOG(E1000, LOG_TRACE, "bar size 0x%llx", size);
        uint64_t bar_frm_cnt = (size + FRAME_SIZE - 1) / FRAME_SIZE;
        frame_t bar_req_frm = {bar_fa, bar_frm_cnt, FRAME_TYPE_RESERVED, 0};

        bar_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(bar_fa);

        if(bar_frames == NULL) {
            PRINTLOG(E1000, LOG_TRACE, "cannot find reserved frames for 0x%llx and try to reserve", bar_fa);

            if(KERNEL_FRAME_ALLOCATOR->allocate_frame(KERNEL_FRAME_ALLOCATOR, &bar_req_frm) != 0) {
                PRINTLOG(E1000, LOG_ERROR, "cannot allocate frame");
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
    dev->mmio = (network_e1000_mmio_t*)bar_va;

    PRINTLOG(E1000, LOG_TRACE, "device hasnot msi looking for interrupts at acpi");
    uint8_t ic;

    uint64_t addr = pci_netdev->device_number;
    addr <<= 16;

    addr |= pci_netdev->function_number;

    uint8_t* ints = acpi_device_get_interrupts(ACPI_CONTEXT->acpi_parser_context, addr, &ic);

    if(ic == 0) { // need look with function as 0xFFFF
        addr |= 0xFFFF;
        ints = acpi_device_get_interrupts(ACPI_CONTEXT->acpi_parser_context, addr, &ic);
    }

    if(ic) {
        PRINTLOG(E1000, LOG_TRACE, "device has %i interrupts, will use pina: 0x%02x", ic, ints[ic - 1]);

        dev->irq_base = ints[ic - 1];

        memory_free(ints);
    } else {
        PRINTLOG(E1000, LOG_ERROR, "device hasnot any interrupts");
        memory_free(dev);

        return -1;
    }

    uint8_t mac_tmp[6] = {0};

    // get the MAC address
    uint16_t mac16 = network_e1000_eeprom_read(dev, 0);
    mac_tmp[0] = (mac16 & 0xFF);
    mac_tmp[1] = (mac16 >> 8) & 0xFF;
    mac16 = network_e1000_eeprom_read(dev, 1);
    mac_tmp[2] = (mac16 & 0xFF);
    mac_tmp[3] = (mac16 >> 8) & 0xFF;
    mac16 = network_e1000_eeprom_read(dev, 2);
    mac_tmp[4] = (mac16 & 0xFF);
    mac_tmp[5] = (mac16 >> 8) & 0xFF;

    PRINTLOG(E1000, LOG_TRACE, "device has mac %02x:%02x:%02x:%02x:%02x:%02x", mac_tmp[0], mac_tmp[1], mac_tmp[2], mac_tmp[3], mac_tmp[4], mac_tmp[5]);

    memory_memcopy(mac_tmp, dev->mac, sizeof(network_mac_address_t));

    // set the LINK UP
    dev->mmio->ctrl |= NETWORK_E1000_CTRL_SLU;

    for(int32_t i = 0; i < 128; i++ ) {
        dev->mmio->mta[i] = 0;
    }

    // start the RX/TX processes
    network_e1000_rx_init(dev);
    network_e1000_tx_init(dev);

    uint8_t isrnum = dev->irq_base;
    interrupt_irq_set_handler(isrnum, &network_e1000_rx_isr);
    // apic_ioapic_setup_irq(isrnum, APIC_IOAPIC_TRIGGER_MODE_LEVEL);
    apic_ioapic_enable_irq(isrnum);

    // enable all interrupts (and clear existing pending ones)
    dev->mmio->ims = 0x1F6DC;
    UNUSED(dev->mmio->isr_status);

    network_e1000_rx_enable(dev);

    task_create_task(NULL, 64 << 10, 1 << 20, &network_e1000_process_tx, 0, NULL, "e1000 tx");

    list_list_insert(e1000_net_devs, dev);

    PRINTLOG(E1000, LOG_INFO, "device initialized");

    return 0;
}
#pragma GCC diagnostic pop

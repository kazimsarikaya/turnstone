#include <driver/network_virtio.h>
#include <driver/virtio.h>
#include <video.h>
#include <linkedlist.h>
#include <pci.h>
#include <ports.h>
#include <driver/network.h>
#include <memory/frame.h>
#include <memory/paging.h>
#include <time/timer.h>
#include <acpi.h>
#include <acpi/aml.h>
#include <cpu/interrupt.h>
#include <cpu.h>
#include <apic.h>
#include <utils.h>
#include <cpu/task.h>


linkedlist_t virtio_net_devs = NULL;

int8_t network_virtio_set_msix_isr(network_virtio_dev_t* dev, uint16_t msix_vector, interrupt_irq isr);
int8_t network_virtio_clear_msix_pending_bit(network_virtio_dev_t* dev, uint16_t msix_vector);

int8_t network_virtio_rx_isr(interrupt_frame_t* frame, uint8_t intnum);
int8_t network_virtio_tx_isr(interrupt_frame_t* frame, uint8_t intnum);
int8_t network_virtio_ctrl_isr(interrupt_frame_t* frame, uint8_t intnum);
int8_t network_virtio_config_isr(interrupt_frame_t* frame, uint8_t intnum);
int8_t network_virtio_combined_isr(interrupt_frame_t* frame, uint8_t intnum);

int8_t network_virtio_process_tx(){

	for(uint64_t dev_idx = 0; dev_idx < linkedlist_size(virtio_net_devs); dev_idx++) {
		network_virtio_dev_t* dev = linkedlist_get_data_at_position(virtio_net_devs, dev_idx);
		task_add_message_queue(dev->transmit_queue);
	}

	while(1) {
		boolean_t packet_exists = 0;

		for(uint64_t dev_idx = 0; dev_idx < linkedlist_size(virtio_net_devs); dev_idx++) {
			network_virtio_dev_t* dev = linkedlist_get_data_at_position(virtio_net_devs, dev_idx);

			while(linkedlist_size(dev->transmit_queue)) {
				network_transmit_packet_t* packet = linkedlist_queue_pop(dev->transmit_queue);

				if(packet) {
					PRINTLOG(NETWORK, LOG_TRACE, "network packet will be sended with length 0x%lx", packet->packet_len);
					packet_exists = 1;

					uint16_t avail_tx_id = dev->vq_tx->avail.index;

					uint8_t* offset = (uint8_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(dev->vq_tx->descriptors[avail_tx_id % dev->queue_size].address);
					virtio_network_header_t* hdr = (virtio_network_header_t*)offset;

					if(dev->features & VIRTIO_NETWORK_F_MRG_RXBUF) {
						hdr->header_length = sizeof(virtio_network_header_t);
					} else {
						hdr->header_length = sizeof(virtio_network_header_t) - 2;
					}

					offset += sizeof(virtio_network_header_t);

					memory_memcopy(packet->packet_data, offset, packet->packet_len);

					dev->vq_tx->descriptors[avail_tx_id % dev->queue_size].length = sizeof(virtio_network_header_t) + packet->packet_len;

					memory_free(packet->packet_data);
					memory_free(packet);

					dev->vq_tx->avail.index++;
					dev->nd_tx->vqn = 1;
				}

				PRINTLOG(NETWORK, LOG_TRACE, "tx queue size 0x%lx", linkedlist_size(dev->transmit_queue));
			}

		}

		if(packet_exists == 0) {
			task_set_message_waiting();
			task_yield();
		}

	}

	return 0;
}

int8_t network_virtio_rx_isr(interrupt_frame_t* frame, uint8_t intnum) {
	UNUSED(frame);

	PRINTLOG(VIRTIONET, LOG_TRACE, "packet received int 0x%02x", intnum);

	network_virtio_dev_t* dev = linkedlist_get_data_at_position(virtio_net_devs, 0);

	while(dev->vq_rx->last_used_index < dev->vq_rx->used.index) {
		network_received_packet_t* packet = memory_malloc(sizeof(network_received_packet_t));

		uint64_t packet_len = dev->vq_rx->used.ring[dev->vq_rx->last_used_index % dev->queue_size].length;
		uint16_t packet_desc_id = dev->vq_rx->used.ring[dev->vq_rx->last_used_index % dev->queue_size].id;

		uint8_t* offset = (uint8_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(dev->vq_rx->descriptors[packet_desc_id].address);
		virtio_network_header_t* hdr = (virtio_network_header_t*)offset;

		if(dev->features & VIRTIO_NETWORK_F_MRG_RXBUF) {
			hdr->header_length = sizeof(virtio_network_header_t);
		} else {
			hdr->header_length = sizeof(virtio_network_header_t) - 2;
		}

		offset += sizeof(virtio_network_header_t);
		packet_len -= sizeof(virtio_network_header_t);

		packet->packet_len = packet_len;
		packet->return_queue = dev->transmit_queue;

		packet->packet_data = memory_malloc(packet_len);
		memory_memcopy(offset, packet->packet_data, packet_len);
		memory_memcopy(dev->mac, packet->device_mac_address, sizeof(mac_address_t));

		dev->vq_rx->descriptors[packet_desc_id].flags = VIRTIO_QUEUE_DESC_F_WRITE;

		dev->vq_rx->avail.ring[dev->vq_rx->avail.index % dev->queue_size] = packet_desc_id;
		dev->vq_rx->avail.index++;
		dev->nd_rx->vqn = 0;

		PRINTLOG(VIRTIONET, LOG_TRACE, "packet received with length 0x%lx", packet_len);

		linkedlist_queue_push(network_received_packets, packet);

		dev->vq_rx->last_used_index++;
	}

	network_virtio_clear_msix_pending_bit(dev, 0);
	apic_eoi();

	return 0;
}

int8_t network_virtio_tx_isr(interrupt_frame_t* frame, uint8_t intnum) {
	UNUSED(frame);

	PRINTLOG(VIRTIONET, LOG_TRACE, "packet sended int 0x%02x", intnum);

	network_virtio_dev_t* dev = linkedlist_get_data_at_position(virtio_net_devs, 0);

	network_virtio_clear_msix_pending_bit(dev, 1);
	apic_eoi();

	return 0;
}

int8_t network_virtio_ctrl_isr(interrupt_frame_t* frame, uint8_t intnum) {
	UNUSED(frame);

	PRINTLOG(VIRTIONET, LOG_TRACE, "control int 0x%02x", intnum);

	apic_eoi();

	return 0;
}

int8_t network_virtio_config_isr(interrupt_frame_t* frame, uint8_t intnum) {
	UNUSED(frame);

	PRINTLOG(VIRTIONET, LOG_TRACE, "config int 0x%02x", intnum);

	return -1;
}

int8_t network_virtio_combined_isr(interrupt_frame_t* frame, uint8_t intnum) {
	UNUSED(frame);

	PRINTLOG(VIRTIONET, LOG_TRACE, "combined int 0x%02x", intnum);

	return -1;
}

int8_t network_virtio_ctrl_set_promisc(network_virtio_dev_t* dev){

	uint16_t empty_desc = dev->vq_ctrl->avail.index;

	uint64_t va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA((dev->vq_ctrl->descriptors[empty_desc].address));

	uint8_t* offset = (uint8_t*)va;

	virtio_network_control_t* ctrl = (virtio_network_control_t*)(offset);

	ctrl->class = VIRTIO_NETWORK_CTRL_MAC;
	ctrl->command = VIRTIO_NETWORK_CTRL_MAC_ADDR_SET;
	memory_memcopy(dev->mac, offset + 2, sizeof(mac_address_t));


	dev->vq_ctrl->descriptors[empty_desc].length = 2 + sizeof(mac_address_t);

	dev->vq_ctrl->descriptors[empty_desc].flags = VIRTIO_QUEUE_DESC_F_NEXT;
	dev->vq_ctrl->descriptors[empty_desc].next = empty_desc + 1;


	dev->vq_ctrl->descriptors[empty_desc + 1].length = 1;

	dev->vq_ctrl->descriptors[empty_desc + 1].flags = VIRTIO_QUEUE_DESC_F_WRITE;
	dev->vq_ctrl->descriptors[empty_desc + 1].next = empty_desc + 1;


	dev->vq_ctrl->avail.ring[dev->vq_ctrl->avail.index] = empty_desc;

	dev->vq_ctrl->avail.index++;

	dev->nd_ctrl->vqn = 2;

	return 0;
}

uint64_t network_virtio_select_features(network_virtio_dev_t* dev, uint64_t avail_features){
	uint64_t req_features = 0;

	if(avail_features & VIRTIO_NETWORK_F_MAC) {
		PRINTLOG(VIRTIONET, LOG_TRACE, "device has mac feature", 0);
		req_features |= VIRTIO_NETWORK_F_MAC;

		if(dev->is_legacy) {
			int16_t offset = 0;

			if(dev->has_msix != 1) {
				offset = -2;
			}

			uint8_t mac_bytes[6];

			mac_bytes[0] = inb(dev->iobase + VIRTIO_NETWORK_IOPORT_MAC + offset);
			offset++;
			mac_bytes[1] = inb(dev->iobase + VIRTIO_NETWORK_IOPORT_MAC + offset);
			offset++;
			mac_bytes[2] = inb(dev->iobase + VIRTIO_NETWORK_IOPORT_MAC + offset);
			offset++;
			mac_bytes[3] = inb(dev->iobase + VIRTIO_NETWORK_IOPORT_MAC + offset);
			offset++;
			mac_bytes[4] = inb(dev->iobase + VIRTIO_NETWORK_IOPORT_MAC + offset);
			offset++;
			mac_bytes[5] = inb(dev->iobase + VIRTIO_NETWORK_IOPORT_MAC + offset);

			memory_memcopy(mac_bytes, dev->mac, sizeof(mac_address_t));
		} else {
			memory_memcopy(dev->device_config->mac, dev->mac, sizeof(mac_address_t));
		}
	}

	if(avail_features & VIRTIO_NETWORK_F_STATUS) {
		PRINTLOG(VIRTIONET, LOG_TRACE, "device has link status feature", 0);
		req_features |= VIRTIO_NETWORK_F_STATUS;
	}

	if(avail_features & VIRTIO_NETWORK_F_MRG_RXBUF) {
		PRINTLOG(VIRTIONET, LOG_TRACE, "device has merge receive buffers feature", 0);
		req_features |= VIRTIO_NETWORK_F_MRG_RXBUF;
	}

	if(avail_features & VIRTIO_NETWORK_F_CTRL_VQ) {
		PRINTLOG(VIRTIONET, LOG_TRACE, "device has control vq feature", 0);
		req_features |= VIRTIO_NETWORK_F_CTRL_VQ;

		if(avail_features & VIRTIO_NETWORK_F_CTRL_RX) {
			PRINTLOG(VIRTIONET, LOG_TRACE, "device has control rx feature", 0);
			req_features |= VIRTIO_NETWORK_F_CTRL_RX;
		}

		if(avail_features & VIRTIO_NETWORK_F_CTRL_VLAN) {
			PRINTLOG(VIRTIONET, LOG_TRACE, "device has control vlan feature", 0);
			req_features |= VIRTIO_NETWORK_F_CTRL_VLAN;
		}

		if(avail_features & VIRTIO_NETWORK_F_GUEST_ANNOUNCE) {
			PRINTLOG(VIRTIONET, LOG_TRACE, "device has control announce feature", 0);
			req_features |= VIRTIO_NETWORK_F_GUEST_ANNOUNCE;
		}

		if(avail_features & VIRTIO_NETWORK_F_MQ) {
			PRINTLOG(VIRTIONET, LOG_TRACE, "device has control max vq count feature", 0);
			req_features |= VIRTIO_NETWORK_F_MQ;

			if(dev->is_legacy) {
				dev->max_vq_count = inw(dev->iobase + VIRTIO_NETWORK_IOPORT_MAX_VQ_COUNT);
			} else {
				dev->max_vq_count = dev->common_config->num_queues;
			}

		} else {
			dev->max_vq_count = 3;
		}

		if(avail_features & VIRTIO_NETWORK_F_CTRL_MAC_ADDR) {
			PRINTLOG(VIRTIONET, LOG_TRACE, "device has control mac feature", 0);
			req_features |= VIRTIO_NETWORK_F_CTRL_MAC_ADDR;
		}
	}

	if(avail_features & VIRTIO_NETWORK_F_MTU) {
		PRINTLOG(VIRTIONET, LOG_TRACE, "device has mtu feature", 0);

		req_features |= VIRTIO_NETWORK_F_MTU;

		if(dev->is_legacy) {
			dev->mtu = inw(dev->iobase + VIRTIO_NETWORK_IOPORT_MTU);
		} else {
			dev->mtu = dev->device_config->mtu;
		}

	} else {
		dev->mtu = 1500;
	}

	if(avail_features & VIRTIO_F_NOTIFICATION_DATA) {
		PRINTLOG(VIRTIONET, LOG_TRACE, "device has notification data feature", 0);

		req_features |= VIRTIO_F_NOTIFICATION_DATA;
	}

	return req_features;
}

int8_t network_virtio_create_queues(network_virtio_dev_t* dev){
	PRINTLOG(VIRTIO, LOG_TRACE, "vq count %i", dev->max_vq_count);

	for(int32_t i = 0; i < dev->max_vq_count; i++) {
		if(dev->is_legacy) {
			outw(dev->iobase + VIRTIO_IOPORT_VQ_SELECT, i);
			time_timer_spinsleep(1000);
			outw(dev->iobase + VIRTIO_IOPORT_VQ_SIZE, VIRTIO_QUEUE_SIZE);
			time_timer_spinsleep(1000);
		} else {
			dev->common_config->queue_select = i;
			time_timer_spinsleep(1000);
			dev->common_config->queue_size = VIRTIO_QUEUE_SIZE;
			time_timer_spinsleep(1000);
		}
	}

	uint64_t queue_size = VIRTIO_QUEUE_SIZE * 1536;

	uint64_t queue_frm_cnt = (queue_size + FRAME_SIZE - 1) / FRAME_SIZE;
	uint64_t queue_meta_frm_cnt = (sizeof(virtio_queue_t) + FRAME_SIZE - 1 ) / FRAME_SIZE;


	frame_t* queue_frames;
	frame_t* queue_meta_frames;

	queue_frames = NULL;

	if(KERNEL_FRAME_ALLOCATOR->allocate_frame_by_count(KERNEL_FRAME_ALLOCATOR, queue_frm_cnt, FRAME_ALLOCATION_TYPE_BLOCK | FRAME_ALLOCATION_TYPE_RESERVED, &queue_frames, NULL) == 0 &&
	   KERNEL_FRAME_ALLOCATOR->allocate_frame_by_count(KERNEL_FRAME_ALLOCATOR, queue_meta_frm_cnt, FRAME_ALLOCATION_TYPE_BLOCK | FRAME_ALLOCATION_TYPE_RESERVED, &queue_meta_frames, NULL) == 0) {
		queue_frames->frame_attributes |= FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED;
		queue_meta_frames->frame_attributes |= FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED;

		uint64_t queue_fa = queue_frames->frame_address;
		uint64_t queue_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(queue_frames->frame_address);
		memory_paging_add_va_for_frame(queue_va, queue_frames, MEMORY_PAGING_PAGE_TYPE_NOEXEC);

		PRINTLOG(VIRTIO, LOG_TRACE, "receive queue data is at fa 0x%lx vs 0x%lx", queue_fa, queue_va);

		uint64_t queue_meta_fa = queue_meta_frames->frame_address;
		uint64_t queue_meta_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(queue_meta_frames->frame_address);
		memory_paging_add_va_for_frame(queue_meta_va, queue_meta_frames, MEMORY_PAGING_PAGE_TYPE_NOEXEC);

		virtio_queue_t* vq = (virtio_queue_t*)queue_meta_va;

		dev->vq_rx = vq;

		for(int32_t i = 0; i < VIRTIO_QUEUE_SIZE; i++) {
			vq->descriptors[i].address = queue_fa + i * VIRTIO_NETWORK_QUEUE_ITEM_LENGTH;
			vq->descriptors[i].length = VIRTIO_NETWORK_QUEUE_ITEM_LENGTH;

			vq->descriptors[i].flags = VIRTIO_QUEUE_DESC_F_WRITE;

/*
      if(i == VIRTIO_QUEUE_SIZE - 1) {
        vq->descriptors[i].flags = VIRTIO_QUEUE_DESC_F_WRITE | VIRTIO_QUEUE_DESC_F_AVAIL;
      } else {
        vq->descriptors[i].flags = VIRTIO_QUEUE_DESC_F_WRITE | VIRTIO_QUEUE_DESC_F_NEXT | VIRTIO_QUEUE_DESC_F_AVAIL;
        vq->descriptors[i].next = i + 1;
      }
 */

			virtio_network_header_t* hdr = (virtio_network_header_t*)(queue_va + i * VIRTIO_NETWORK_QUEUE_ITEM_LENGTH);

			hdr->header_length = sizeof(virtio_network_header_t);

			if(dev->features & VIRTIO_NETWORK_F_MRG_RXBUF) {
				hdr->header_length = sizeof(virtio_network_header_t);
			} else {
				hdr->header_length = sizeof(virtio_network_header_t) - 2;
			}

			vq->avail.ring[i]  = i;
		}

		vq->avail.index = VIRTIO_QUEUE_SIZE;

		PRINTLOG(VIRTIONET, LOG_TRACE, "receive queue builded", 0);

		if(dev->is_legacy) {

			printf("----- %i %i %i %i\n", offsetof_field(virtio_queue_t, descriptors), offsetof_field(virtio_queue_t, avail), offsetof_field(virtio_queue_t, used), sizeof(virtio_queue_avail_t));
			outw(dev->iobase + VIRTIO_IOPORT_VQ_SELECT, 0);
			time_timer_spinsleep(1000);
			outw(dev->iobase + VIRTIO_IOPORT_VQ_SIZE, VIRTIO_QUEUE_SIZE);
			time_timer_spinsleep(1000);
			outl(dev->iobase + VIRTIO_IOPORT_VQ_ADDR, queue_meta_fa);
			time_timer_spinsleep(1000);

			if(dev->has_msix) {
				outw(dev->iobase + VIRTIO_IOPORT_VQ_MSIX_VECTOR, 0);
				time_timer_spinsleep(1000);

				while(inw(dev->iobase + VIRTIO_IOPORT_VQ_MSIX_VECTOR) != 0) {
					PRINTLOG(VIRTIONET, LOG_WARNING, "receive queue msix not configured re-trying", 0);
					outw(dev->iobase + VIRTIO_IOPORT_VQ_MSIX_VECTOR, 0);
					time_timer_spinsleep(1000);
				}

				network_virtio_set_msix_isr(dev, 0, &network_virtio_rx_isr);
			} else {
				uint8_t isrnum = dev->irq_base;
				interrupt_irq_set_handler(isrnum, &network_virtio_combined_isr);
				apic_ioapic_setup_irq(isrnum, APIC_IOAPIC_TRIGGER_MODE_EDGE);
				apic_ioapic_enable_irq(isrnum);

				PRINTLOG(VIRTIONET, LOG_TRACE, "receive queue combined isr 0x%02x",  dev->irq_base);
			}

			outw(dev->iobase + VIRTIO_IOPORT_VQ_NOTIFY, 0);
		} else {
			dev->common_config->queue_select = 0;
			time_timer_spinsleep(1000);

			if(dev->common_config->queue_size != VIRTIO_QUEUE_SIZE) {
				PRINTLOG(VIRTIONET, LOG_ERROR, "queue size missmatch 0x%lx", dev->common_config->queue_size);

				return -1;
			}

			dev->common_config->queue_size = VIRTIO_QUEUE_SIZE;
			time_timer_spinsleep(1000);
			dev->common_config->queue_desc = MEMORY_PAGING_GET_FA_FOR_RESERVED_VA(((uint64_t)&vq->descriptors));
			time_timer_spinsleep(1000);
			dev->common_config->queue_driver = MEMORY_PAGING_GET_FA_FOR_RESERVED_VA(((uint64_t)&vq->avail));
			time_timer_spinsleep(1000);
			dev->common_config->queue_device = MEMORY_PAGING_GET_FA_FOR_RESERVED_VA(((uint64_t)&vq->used));
			time_timer_spinsleep(1000);

			dev->common_config->queue_enable = 1;
			time_timer_spinsleep(1000);

			if(dev->has_msix) {
				dev->common_config->queue_msix_vector = 0;
				time_timer_spinsleep(1000);

				while(dev->common_config->queue_msix_vector != 0) {
					PRINTLOG(VIRTIONET, LOG_WARNING, "receive queue msix not configured re-trying", 0);
					dev->common_config->queue_msix_vector = 0;
					time_timer_spinsleep(1000);
				}

				network_virtio_set_msix_isr(dev, 0, &network_virtio_rx_isr);
			} else {
				uint8_t isrnum = dev->irq_base;
				interrupt_irq_set_handler(isrnum, &network_virtio_combined_isr);
				apic_ioapic_setup_irq(isrnum, APIC_IOAPIC_TRIGGER_MODE_EDGE);
				apic_ioapic_enable_irq(isrnum);

				PRINTLOG(VIRTIONET, LOG_TRACE, "receive queue combined isr 0x%02x",  dev->irq_base);
			}

			virtio_notification_data_t* nd = (virtio_notification_data_t*)(dev->common_config->queue_notify_offset * dev->notify_offset_multiplier + dev->notify_offset);

			dev->nd_rx = nd;

			nd->vqn = 0;

			PRINTLOG(VIRTIONET, LOG_TRACE, "notify data at 0x%lx for queue %i is notified", nd, 0);
		}

		PRINTLOG(VIRTIONET, LOG_TRACE, "receive queue configured", 0);
	} else {
		PRINTLOG(VIRTIONET, LOG_ERROR, "cannot allocate queue frames", 0);

		return -1;
	}


	if(KERNEL_FRAME_ALLOCATOR->allocate_frame_by_count(KERNEL_FRAME_ALLOCATOR, queue_frm_cnt, FRAME_ALLOCATION_TYPE_BLOCK | FRAME_ALLOCATION_TYPE_RESERVED, &queue_frames, NULL) == 0 &&
	   KERNEL_FRAME_ALLOCATOR->allocate_frame_by_count(KERNEL_FRAME_ALLOCATOR, queue_meta_frm_cnt, FRAME_ALLOCATION_TYPE_BLOCK | FRAME_ALLOCATION_TYPE_RESERVED, &queue_meta_frames, NULL) == 0) {
		queue_frames->frame_attributes |= FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED;
		queue_meta_frames->frame_attributes |= FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED;

		uint64_t queue_fa = queue_frames->frame_address;
		uint64_t queue_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(queue_frames->frame_address);
		memory_paging_add_va_for_frame(queue_va, queue_frames, MEMORY_PAGING_PAGE_TYPE_NOEXEC);

		PRINTLOG(VIRTIO, LOG_TRACE, "transmit queue data is at fa 0x%lx vs 0x%lx", queue_fa, queue_va);

		uint64_t queue_meta_fa = queue_meta_frames->frame_address;
		uint64_t queue_meta_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(queue_meta_frames->frame_address);
		memory_paging_add_va_for_frame(queue_meta_va, queue_meta_frames, MEMORY_PAGING_PAGE_TYPE_NOEXEC);

		virtio_queue_t* vq = (virtio_queue_t*)queue_meta_va;
		dev->vq_tx = vq;

		for(int32_t i = 0; i < VIRTIO_QUEUE_SIZE; i++) {
			vq->descriptors[i].address = queue_fa + i * VIRTIO_NETWORK_QUEUE_ITEM_LENGTH;
			vq->descriptors[i].length = VIRTIO_NETWORK_QUEUE_ITEM_LENGTH;

/*
      if(i == VIRTIO_QUEUE_SIZE - 1) {
        vq->descriptors[i].flags = 0;
      } else {
        vq->descriptors[i].flags = VIRTIO_QUEUE_DESC_F_NEXT;
        vq->descriptors[i].next = i + 1;
      }
 */
			virtio_network_header_t* hdr = (virtio_network_header_t*)(queue_va + i * VIRTIO_NETWORK_QUEUE_ITEM_LENGTH);

			hdr->header_length = sizeof(virtio_network_header_t);

			if(dev->features & VIRTIO_NETWORK_F_MRG_RXBUF) {
				hdr->header_length = sizeof(virtio_network_header_t);
			} else {
				hdr->header_length = sizeof(virtio_network_header_t) - 2;
			}

			vq->avail.ring[i]  = i;
		}

		PRINTLOG(VIRTIONET, LOG_TRACE, "transmit queue builded", 0);

		if(dev->is_legacy) {
			outw(dev->iobase + VIRTIO_IOPORT_VQ_SELECT, 1);
			time_timer_spinsleep(1000);
			outw(dev->iobase + VIRTIO_IOPORT_VQ_SIZE, VIRTIO_QUEUE_SIZE);
			time_timer_spinsleep(1000);
			outl(dev->iobase + VIRTIO_IOPORT_VQ_ADDR, queue_meta_fa);
			time_timer_spinsleep(1000);

			if(dev->has_msix) {
				outw(dev->iobase + VIRTIO_IOPORT_VQ_MSIX_VECTOR, 1);
				time_timer_spinsleep(1000);

				while(inw(dev->iobase + VIRTIO_IOPORT_VQ_MSIX_VECTOR) != 1) {
					PRINTLOG(VIRTIONET, LOG_WARNING, "transmit queue msix not configured re-trying", 0);
					outw(dev->iobase + VIRTIO_IOPORT_VQ_MSIX_VECTOR, 1);
					time_timer_spinsleep(1000);
				}

				network_virtio_set_msix_isr(dev, 1, &network_virtio_tx_isr);
			} else {
				uint8_t isrnum = dev->irq_base;
				interrupt_irq_set_handler(isrnum, &network_virtio_combined_isr);
				apic_ioapic_setup_irq(isrnum, APIC_IOAPIC_TRIGGER_MODE_EDGE);
				apic_ioapic_enable_irq(isrnum);

				PRINTLOG(VIRTIONET, LOG_TRACE, "transmit queue combined isr 0x%02x",  dev->irq_base);
			}

			outw(dev->iobase + VIRTIO_IOPORT_VQ_NOTIFY, 1);
		} else {
			dev->common_config->queue_select = 1;
			time_timer_spinsleep(1000);

			if(dev->common_config->queue_size != VIRTIO_QUEUE_SIZE) {
				PRINTLOG(VIRTIONET, LOG_ERROR, "queue size missmatch 0x%lx", dev->common_config->queue_size);

				return -1;
			}

			dev->common_config->queue_size = VIRTIO_QUEUE_SIZE;
			time_timer_spinsleep(1000);
			dev->common_config->queue_desc = MEMORY_PAGING_GET_FA_FOR_RESERVED_VA(((uint64_t)&vq->descriptors));
			time_timer_spinsleep(1000);
			dev->common_config->queue_driver = MEMORY_PAGING_GET_FA_FOR_RESERVED_VA(((uint64_t)&vq->avail));
			time_timer_spinsleep(1000);
			dev->common_config->queue_device = MEMORY_PAGING_GET_FA_FOR_RESERVED_VA(((uint64_t)&vq->used));
			time_timer_spinsleep(1000);

			dev->common_config->queue_enable = 1;
			time_timer_spinsleep(1000);

			if(dev->has_msix) {

				dev->common_config->queue_msix_vector = 1;
				time_timer_spinsleep(1000);

				while(dev->common_config->queue_msix_vector != 1) {
					PRINTLOG(VIRTIONET, LOG_WARNING, "transmit queue msix not configured re-trying", 0);
					dev->common_config->queue_msix_vector = 1;
					time_timer_spinsleep(1000);
				}

				network_virtio_set_msix_isr(dev, 1, &network_virtio_tx_isr);
			} else {
				uint8_t isrnum = dev->irq_base;
				interrupt_irq_set_handler(isrnum, &network_virtio_combined_isr);
				apic_ioapic_setup_irq(isrnum, APIC_IOAPIC_TRIGGER_MODE_EDGE);
				apic_ioapic_enable_irq(isrnum);

				PRINTLOG(VIRTIONET, LOG_TRACE, "transmit queue combined isr 0x%02x",  dev->irq_base);
			}

			virtio_notification_data_t* nd = (virtio_notification_data_t*)(dev->common_config->queue_notify_offset * dev->notify_offset_multiplier + dev->notify_offset);

			dev->nd_tx = nd;

			nd->vqn = 1;

			PRINTLOG(VIRTIONET, LOG_TRACE, "notify data at 0x%lx for queue %i is notified", nd, 1);
		}

		PRINTLOG(VIRTIONET, LOG_TRACE, "transmit queue configured", 0);

	} else {
		PRINTLOG(VIRTIONET, LOG_ERROR, "cannot allocate queue frames", 0);

		return -1;
	}

	if(dev->features & VIRTIO_NETWORK_F_CTRL_VQ) {
		queue_frm_cnt = 1; //control queue needs only 1 frames

		if(KERNEL_FRAME_ALLOCATOR->allocate_frame_by_count(KERNEL_FRAME_ALLOCATOR, queue_frm_cnt, FRAME_ALLOCATION_TYPE_BLOCK | FRAME_ALLOCATION_TYPE_RESERVED, &queue_frames, NULL) == 0 &&
		   KERNEL_FRAME_ALLOCATOR->allocate_frame_by_count(KERNEL_FRAME_ALLOCATOR, queue_meta_frm_cnt, FRAME_ALLOCATION_TYPE_BLOCK | FRAME_ALLOCATION_TYPE_RESERVED, &queue_meta_frames, NULL) == 0) {
			queue_frames->frame_attributes |= FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED;
			queue_meta_frames->frame_attributes |= FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED;

			uint64_t queue_fa = queue_frames->frame_address;
			uint64_t queue_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(queue_frames->frame_address);
			memory_paging_add_va_for_frame(queue_va, queue_frames, MEMORY_PAGING_PAGE_TYPE_NOEXEC);

			PRINTLOG(VIRTIO, LOG_TRACE, "control queue data is at fa 0x%lx vs 0x%lx", queue_fa, queue_va);

			uint64_t queue_meta_fa = queue_meta_frames->frame_address;
			uint64_t queue_meta_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(queue_meta_frames->frame_address);
			memory_paging_add_va_for_frame(queue_meta_va, queue_meta_frames, MEMORY_PAGING_PAGE_TYPE_NOEXEC);

			virtio_queue_t* vq = (virtio_queue_t*)queue_meta_va;
			dev->vq_ctrl = vq;

			for(int32_t i = 0; i < VIRTIO_QUEUE_SIZE; i++) {
				vq->descriptors[i].address = queue_fa + i * VIRTIO_NETWORK_CTRL_QUEUE_ITEM_LENGTH;
				vq->descriptors[i].length = VIRTIO_NETWORK_CTRL_QUEUE_ITEM_LENGTH;

/*
        if(i == VIRTIO_QUEUE_SIZE - 1) {
          vq->descriptors[i].flags = 0;
        } else {
          vq->descriptors[i].flags = VIRTIO_QUEUE_DESC_F_NEXT;
          vq->descriptors[i].next = i + 1;
        }
 */
			}

			PRINTLOG(VIRTIONET, LOG_TRACE, "control queue builded", 0);

			if(dev->is_legacy) {
				outw(dev->iobase + VIRTIO_IOPORT_VQ_SELECT, 2);
				time_timer_spinsleep(1000);
				outw(dev->iobase + VIRTIO_IOPORT_VQ_SIZE, VIRTIO_QUEUE_SIZE);
				time_timer_spinsleep(1000);
				outl(dev->iobase + VIRTIO_IOPORT_VQ_ADDR, queue_meta_fa);
				time_timer_spinsleep(1000);

				if(dev->has_msix) {
					outw(dev->iobase + VIRTIO_IOPORT_VQ_MSIX_VECTOR, 2);
					time_timer_spinsleep(1000);

					while(inw(dev->iobase + VIRTIO_IOPORT_VQ_MSIX_VECTOR) != 2) {
						PRINTLOG(VIRTIONET, LOG_WARNING, "control queue msix not configured re-trying", 0);
						outw(dev->iobase + VIRTIO_IOPORT_VQ_MSIX_VECTOR, 2);
						time_timer_spinsleep(1000);
					}

					network_virtio_set_msix_isr(dev, 2, &network_virtio_ctrl_isr);
				} else {
					uint8_t isrnum = dev->irq_base;
					interrupt_irq_set_handler(isrnum, &network_virtio_combined_isr);
					apic_ioapic_setup_irq(isrnum, APIC_IOAPIC_TRIGGER_MODE_EDGE);
					apic_ioapic_enable_irq(isrnum);

					PRINTLOG(VIRTIONET, LOG_TRACE, "control queue combined isr 0x%02x",  dev->irq_base);
				}

				outw(dev->iobase + VIRTIO_IOPORT_VQ_NOTIFY, 2);
			} else {
				dev->common_config->queue_select = 2;
				time_timer_spinsleep(1000);

				if(dev->common_config->queue_size != VIRTIO_QUEUE_SIZE) {
					PRINTLOG(VIRTIONET, LOG_ERROR, "queue size missmatch 0x%lx", dev->common_config->queue_size);

					return -1;
				}

				dev->common_config->queue_size = VIRTIO_QUEUE_SIZE;
				time_timer_spinsleep(1000);
				dev->common_config->queue_desc = MEMORY_PAGING_GET_FA_FOR_RESERVED_VA(((uint64_t)&vq->descriptors));
				time_timer_spinsleep(1000);
				dev->common_config->queue_driver = MEMORY_PAGING_GET_FA_FOR_RESERVED_VA(((uint64_t)&vq->avail));
				time_timer_spinsleep(1000);
				dev->common_config->queue_device = MEMORY_PAGING_GET_FA_FOR_RESERVED_VA(((uint64_t)&vq->used));
				time_timer_spinsleep(1000);

				dev->common_config->queue_enable = 1;
				time_timer_spinsleep(1000);

				dev->common_config->queue_msix_vector = 2;
				time_timer_spinsleep(1000);

				if(dev->has_msix) {

					while(dev->common_config->queue_msix_vector != 2) {
						PRINTLOG(VIRTIONET, LOG_WARNING, "control queue msix not configured re-trying", 0);
						dev->common_config->queue_msix_vector = 2;
						time_timer_spinsleep(1000);
					}

					network_virtio_set_msix_isr(dev, 2, &network_virtio_ctrl_isr);
				} else {
					uint8_t isrnum = dev->irq_base;
					interrupt_irq_set_handler(isrnum, &network_virtio_combined_isr);
					apic_ioapic_setup_irq(isrnum, APIC_IOAPIC_TRIGGER_MODE_EDGE);
					apic_ioapic_enable_irq(isrnum);

					PRINTLOG(VIRTIONET, LOG_TRACE, "control queue combined isr 0x%02x",  dev->irq_base);
				}

				virtio_notification_data_t* nd = (virtio_notification_data_t*)(dev->common_config->queue_notify_offset * dev->notify_offset_multiplier + dev->notify_offset);

				dev->nd_ctrl = nd;

				nd->vqn = 2;

				PRINTLOG(VIRTIONET, LOG_TRACE, "notify data at 0x%lx for queue %i is notified", nd, 2);
			}

			PRINTLOG(VIRTIONET, LOG_TRACE, "control queue configured", 0);

		} else {
			PRINTLOG(VIRTIONET, LOG_ERROR, "cannot allocate queue frames", 0);

			return -1;
		}
	}

	if(dev->is_legacy) {
		if(dev->has_msix) {
			outw(dev->iobase + VIRTIO_IOPORT_CFG_MSIX_VECTOR, 3);
			time_timer_spinsleep(1000);

			while(inw(dev->iobase + VIRTIO_IOPORT_CFG_MSIX_VECTOR) != 3) {
				PRINTLOG(VIRTIONET, LOG_WARNING, "config msix not configured re-trying", 0);
				outw(dev->iobase + VIRTIO_IOPORT_CFG_MSIX_VECTOR, 3);
				time_timer_spinsleep(1000);
			}

			network_virtio_set_msix_isr(dev, 3, &network_virtio_config_isr);
		}
	} else if(dev->has_msix) {
		dev->common_config->msix_config = 3;
		time_timer_spinsleep(1000);

		while(dev->common_config->msix_config != 3) {
			PRINTLOG(VIRTIONET, LOG_WARNING, "config msix not configured re-trying", 0);
			dev->common_config->msix_config = 3;
			time_timer_spinsleep(1000);
		}

		network_virtio_set_msix_isr(dev, 3, &network_virtio_config_isr);
	}

	PRINTLOG(VIRTIONET, LOG_TRACE, "queue configuration completed", 0);

	return 0;
}

int8_t network_virtio_set_msix_isr(network_virtio_dev_t* dev, uint16_t msix_vector, interrupt_irq isr) {
	pci_generic_device_t* pci_dev = (pci_generic_device_t*)dev->pci_netdev->pci_header;
	pci_bar_register_t* bar = &pci_dev->bar0;
	bar += dev->msix_bir;

	uint64_t msix_table_address = bar->memory_space_bar.base_address;
	msix_table_address <<= 4;

	if(bar->memory_space_bar.type == 2) {
		bar++;
		uint64_t tmp = (uint64_t)(*((uint32_t*)bar));
		msix_table_address = tmp << 32 | msix_table_address;
	}

	msix_table_address += dev->msix_table_offset;

	pci_capability_msix_table_t* msix_table = (pci_capability_msix_table_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(msix_table_address);

	uint8_t intnum = interrupt_get_next_empty_interrupt();
	msix_table->entries[msix_vector].message_address = 0xFEE00000;
	msix_table->entries[msix_vector].message_data = intnum;
	msix_table->entries[msix_vector].masked = 0;

	uint8_t isrnum = intnum - INTERRUPT_IRQ_BASE;
	interrupt_irq_set_handler(isrnum, isr);

	PRINTLOG(VIRTIONET, LOG_TRACE, "config isr 0x%02x",  msix_table->entries[msix_vector].message_data);

	return 0;
}

int8_t network_virtio_clear_msix_pending_bit(network_virtio_dev_t* dev, uint16_t msix_vector) {
	pci_generic_device_t* pci_dev = (pci_generic_device_t*)dev->pci_netdev->pci_header;
	pci_bar_register_t* bar = &pci_dev->bar0;
	bar += dev->msix_pendind_bit_bir;


	uint64_t msix_pendind_bit_table_address = bar->memory_space_bar.base_address;
	msix_pendind_bit_table_address <<= 4;

	if(bar->memory_space_bar.type == 2) {
		bar++;
		uint64_t tmp = (uint64_t)(*((uint32_t*)bar));
		msix_pendind_bit_table_address = tmp << 32 | msix_pendind_bit_table_address;
	}

	msix_pendind_bit_table_address += dev->msix_pendind_bit_table_offset;

	uint8_t* pending_bit_table = (uint8_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(msix_pendind_bit_table_address);

	uint8_t mask1 = (1 << dev->msix_table_size) - 1;

	uint8_t mask2 = ~(1 << msix_vector);
	mask1 &=  mask2;

	*pending_bit_table &= mask1;

	PRINTLOG(VIRTIONET, LOG_TRACE, "pending bit cleared %i",  msix_vector);

	return 0;
}


int8_t network_virtio_init_legacy(network_virtio_dev_t* virtio_net_dev){
	int8_t errors = 0;

	outb(virtio_net_dev->iobase + VIRTIO_IOPORT_DEVICE_STATUS, 0);
	time_timer_spinsleep(1000);
	outb(virtio_net_dev->iobase + VIRTIO_IOPORT_DEVICE_STATUS, VIRTIO_DEVICE_STATUS_ACKKNOWLEDGE);
	time_timer_spinsleep(1000);
	outb(virtio_net_dev->iobase + VIRTIO_IOPORT_DEVICE_STATUS, VIRTIO_DEVICE_STATUS_ACKKNOWLEDGE | VIRTIO_DEVICE_STATUS_DRIVER);

	uint32_t avail_features = inl(virtio_net_dev->iobase + VIRTIO_IOPORT_DEVICE_F);
	uint64_t req_features = network_virtio_select_features(virtio_net_dev, avail_features);

	PRINTLOG(VIRTIONET, LOG_TRACE, "features avail 0x%08x req 0x%08x", avail_features, req_features);

	outl(virtio_net_dev->iobase + VIRTIO_IOPORT_DRIVER_F, req_features);
	time_timer_spinsleep(1000);

	uint32_t new_avail_features = inl(virtio_net_dev->iobase + VIRTIO_IOPORT_DRIVER_F);

	if(new_avail_features == req_features) {
		PRINTLOG(VIRTIONET, LOG_TRACE, "device accepted requested features", 0);

		virtio_net_dev->features = req_features;

		errors += network_virtio_create_queues(virtio_net_dev);

		if(errors == 0) {
			outb(virtio_net_dev->iobase + VIRTIO_IOPORT_DEVICE_STATUS, VIRTIO_DEVICE_STATUS_ACKKNOWLEDGE | VIRTIO_DEVICE_STATUS_DRIVER | VIRTIO_DEVICE_STATUS_DRIVER_OK);
			time_timer_spinsleep(1000);
			uint8_t new_dev_status = inb(virtio_net_dev->iobase + VIRTIO_IOPORT_DEVICE_STATUS);

			if(new_dev_status & VIRTIO_DEVICE_STATUS_DRIVER_OK) {
				PRINTLOG(VIRTIONET, LOG_TRACE, "device accepted ok status", 0);

				printf("------ 0x%02x 0x%02x\n", inb(virtio_net_dev->iobase + VIRTIO_NETWORK_IOPORT_LINK_STATUS), inb(virtio_net_dev->iobase + VIRTIO_IOPORT_DEVICE_STATUS));
			} else {
				PRINTLOG(VIRTIONET, LOG_ERROR, "device doesnot accepted ok status", 0);

				errors += -1;
			}
		}
	} else {
		PRINTLOG(VIRTIONET, LOG_ERROR, "device doesnot accepted requested features, device offers: 0x%lx", new_avail_features);

		errors += -1;
	}

	return errors == 0?0:-1;
}

int8_t network_virtio_init_modern(network_virtio_dev_t* virtio_net_dev){
	int8_t errors = 0;

	PRINTLOG(VIRTIONET, LOG_TRACE, "addr 0x%lx", virtio_net_dev->common_config);

	virtio_net_dev->common_config->device_status = 0;
	time_timer_spinsleep(1000);
	virtio_net_dev->common_config->device_status = VIRTIO_DEVICE_STATUS_ACKKNOWLEDGE;
	time_timer_spinsleep(1000);
	virtio_net_dev->common_config->device_status |= VIRTIO_DEVICE_STATUS_DRIVER;
	time_timer_spinsleep(1000);

	virtio_net_dev->common_config->device_feature_select = 1;
	time_timer_spinsleep(1000);
	uint64_t avail_features = virtio_net_dev->common_config->device_feature;
	avail_features <<= 32;
	virtio_net_dev->common_config->device_feature_select = 0;
	time_timer_spinsleep(1000);
	avail_features |= virtio_net_dev->common_config->device_feature;

	uint64_t req_features = network_virtio_select_features(virtio_net_dev, avail_features);

	PRINTLOG(VIRTIONET, LOG_TRACE, "features avail 0x%lx req 0x%lx", avail_features, req_features);

	virtio_net_dev->common_config->driver_feature_select = 1;
	time_timer_spinsleep(1000);
	virtio_net_dev->common_config->driver_feature = req_features >> 32;
	time_timer_spinsleep(1000);
	virtio_net_dev->common_config->driver_feature_select = 0;
	time_timer_spinsleep(1000);
	virtio_net_dev->common_config->driver_feature = req_features;
	time_timer_spinsleep(1000);
	virtio_net_dev->common_config->device_status |= VIRTIO_DEVICE_STATUS_FEATURES_OK;
	time_timer_spinsleep(1000);
	uint8_t new_dev_status = virtio_net_dev->common_config->device_status;

	if(new_dev_status & VIRTIO_DEVICE_STATUS_FEATURES_OK) {
		PRINTLOG(VIRTIONET, LOG_TRACE, "device accepted requested features", 0);

		virtio_net_dev->features = req_features;

		errors += network_virtio_create_queues(virtio_net_dev);

		if(errors == 0) {
			PRINTLOG(VIRTIONET, LOG_TRACE, "try to set driver ok", 0);

			virtio_net_dev->common_config->device_status |= VIRTIO_DEVICE_STATUS_DRIVER_OK;
			time_timer_spinsleep(1000);
			new_dev_status = virtio_net_dev->common_config->device_status;

			if(new_dev_status & VIRTIO_DEVICE_STATUS_DRIVER_OK) {
				PRINTLOG(VIRTIONET, LOG_TRACE, "device accepted ok status", 0);
			} else {
				PRINTLOG(VIRTIONET, LOG_ERROR, "device doesnot accepted ok status", 0);

				errors += -1;
			}
		}


	} else {
		PRINTLOG(VIRTIONET, LOG_ERROR, "device doesnot accepted requested features", 0);

		errors += -1;
	}

	return errors == 0?0:-1;
}

int8_t network_virtio_init(pci_dev_t* pci_netdev){
	PRINTLOG(VIRTIONET, LOG_INFO, "virtnet device starting", 0);
	int8_t errors = 0;

	pci_common_header_t* pci_header = pci_netdev->pci_header;

	virtio_net_devs = linkedlist_create_list_with_heap(NULL);

	if(virtio_net_devs == NULL) {
		PRINTLOG(VIRTIONET, LOG_ERROR, "cannot create virtio network devices list", 0);

		return -1;
	}

	network_virtio_dev_t* virtio_net_dev = memory_malloc(sizeof(network_virtio_dev_t));

	if(virtio_net_dev == NULL) {
		PRINTLOG(VIRTIONET, LOG_ERROR, "cannot create virtio device", 0);

		return -1;
	}

	virtio_net_dev->pci_netdev = pci_netdev;
	virtio_net_dev->queue_size = VIRTIO_QUEUE_SIZE;
	virtio_net_dev->transmit_queue = linkedlist_create_queue_with_heap(NULL);

	linkedlist_list_insert(virtio_net_devs, virtio_net_dev);

	virtio_net_dev->is_legacy = 1;

	pci_generic_device_t* pci_dev = (pci_generic_device_t*)pci_header;

	if(pci_dev->common_header.status.capabilities_list) {
		pci_capability_t* pci_cap = (pci_capability_t*)(((uint8_t*)pci_dev) + pci_dev->capabilities_pointer);

		while(pci_cap->capability_id != 0xFF) {
			PRINTLOG(VIRTIO, LOG_TRACE, "capability id: %i noff 0x%x", pci_cap->capability_id, pci_cap->next_pointer);

			if(pci_cap->capability_id == PCI_DEVICE_CAPABILITY_MSIX) {
				pci_capability_msix_t* msix_cap = (pci_capability_msix_t*)pci_cap;

				virtio_net_dev->msix_cap = msix_cap;

				msix_cap->enable = 1;
				msix_cap->function_mask = 0;

				PRINTLOG(VIRTIO, LOG_TRACE, "device has msix cap enabled %i fmask %i", msix_cap->enable, msix_cap->function_mask);
				PRINTLOG(VIRTIO, LOG_TRACE, "msix bir %i tables offset 0x%lx  size 0x%lx", msix_cap->bir, msix_cap->table_offset, msix_cap->table_size + 1);
				PRINTLOG(VIRTIO, LOG_TRACE, "msix pending bit bir %i tables offset 0x%lx", msix_cap->pending_bit_bir, msix_cap->pending_bit_offset);

				virtio_net_dev->has_msix = 1;
				virtio_net_dev->msix_table_size = msix_cap->table_size + 1;
				virtio_net_dev->msix_bir = msix_cap->bir;
				virtio_net_dev->msix_table_offset = msix_cap->table_offset;
				virtio_net_dev->msix_pendind_bit_bir = msix_cap->pending_bit_bir;
			} else if(pci_cap->capability_id == PCI_DEVICE_CAPABILITY_VENDOR) {
				virtio_net_dev->is_legacy = 0;
				virtio_pci_cap_t* vcap = (virtio_pci_cap_t*)pci_cap;

				PRINTLOG(VIRTIO, LOG_TRACE, "virtio cap type 0x%02x bar %i off 0x%x len 0x%x", vcap->config_type, vcap->bar_no, vcap->offset, vcap->length);

				pci_bar_register_t* bar = &pci_dev->bar0;

				bar += vcap->bar_no;

				if(bar->bar_type.type == 0) {
					uint64_t bar_fa = (uint64_t)bar->memory_space_bar.base_address;
					bar_fa <<= 4;

					if(bar->memory_space_bar.type == 2) {
						bar++;
						uint64_t tmp = (uint64_t)(*((uint32_t*)bar));
						bar_fa = tmp << 32 | bar_fa;
					}

					PRINTLOG(VIRTIONET, LOG_TRACE, "frame address at bar 0x%lx", bar_fa);

					frame_t* bar_frames = KERNEL_FRAME_ALLOCATOR->get_reserved_frames_of_address(KERNEL_FRAME_ALLOCATOR, (void*)bar_fa);

					uint64_t bar_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(bar_fa);

					if(bar_frames == NULL) {
						PRINTLOG(VIRTIO, LOG_TRACE, "cannot find reserved frames for 0x%lx and try to reserve", bar_fa);
						frame_t tmp_frm = {bar_fa, 1, FRAME_TYPE_RESERVED, 0};

						if(KERNEL_FRAME_ALLOCATOR->allocate_frame(KERNEL_FRAME_ALLOCATOR, &tmp_frm) != 0) {
							PRINTLOG(VIRTIO, LOG_ERROR, "cannot allocate frame", 0);

							return -1;
						}

						bar_frames = &tmp_frm;
					}

					if((bar_frames->frame_attributes & FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED) != FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED) {
						memory_paging_add_va_for_frame(MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(bar_frames->frame_address), bar_frames, MEMORY_PAGING_PAGE_TYPE_NOEXEC);
						bar_frames->frame_attributes |= FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED;
					}

					switch (vcap->config_type) {
					case VIRTIO_PCI_CAP_COMMON_CFG:
						virtio_net_dev->common_config = (virtio_pci_common_config_t*)(bar_va + vcap->offset);
						PRINTLOG(VIRTIONET, LOG_TRACE, "common config address 0x%lx", virtio_net_dev->common_config);
						break;
					case VIRTIO_PCI_CAP_NOTIFY_CFG:
						virtio_net_dev->notify_offset_multiplier = ((virtio_pci_notify_cap_t*)vcap)->notify_offset_multiplier;
						virtio_net_dev->notify_offset = (uint8_t*)(bar_va + vcap->offset);
						PRINTLOG(VIRTIONET, LOG_TRACE, "notify multipler 0x%lx address 0x%lx", virtio_net_dev->notify_offset_multiplier, virtio_net_dev->notify_offset);
						break;
					case VIRTIO_PCI_CAP_ISR_CFG:
						virtio_net_dev->isr_config = (void*)(bar_va + vcap->offset);
						PRINTLOG(VIRTIONET, LOG_TRACE, "isr config address 0x%lx", virtio_net_dev->isr_config);
						break;
					case VIRTIO_PCI_CAP_DEVICE_CFG:
						virtio_net_dev->device_config = (virtio_network_config_t*)(bar_va + vcap->offset);
						PRINTLOG(VIRTIONET, LOG_TRACE, "device config address 0x%lx", virtio_net_dev->device_config);
						break;
					case VIRTIO_PCI_CAP_PCI_CFG:
						break;
					default:
						errors += -1;
						break;
					}


				} else {
					PRINTLOG(VIRTIO, LOG_ERROR, "unexpected io space bar", 0);
				}

			} else {
				PRINTLOG(VIRTIO, LOG_WARNING, "not implemented cap 0x%02x", pci_cap->capability_id);
			}

			if(pci_cap->next_pointer == NULL) {
				break;
			}

			pci_cap = (pci_capability_t*)(((uint8_t*)pci_dev) + pci_cap->next_pointer);
		}
	}

	if(!virtio_net_dev->has_msix) {
		PRINTLOG(VIRTIO, LOG_TRACE, "device hasnot msi looking for interrupts at acpi", 0);
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
			PRINTLOG(VIRTIO, LOG_TRACE, "device has %i interrupts, will use pina: 0x%02x", ic, ints[ic - 1]);

			virtio_net_dev->irq_base = ints[ic - 1];

			memory_free(ints);
		} else {
			PRINTLOG(VIRTIO, LOG_ERROR, "device hasnot any interrupts", 0);

			return -1;
		}
	}

	if(virtio_net_dev->is_legacy) {
		virtio_net_dev->iobase = pci_dev->bar0.io_space_bar.base_address << 2;

		PRINTLOG(VIRTIO, LOG_TRACE, "device is legacy iobase: 0x%04x",  virtio_net_dev->iobase);

		errors += network_virtio_init_legacy(virtio_net_dev);

	} else {
		PRINTLOG(VIRTIO, LOG_TRACE, "device is modern", 0);

		errors += network_virtio_init_modern(virtio_net_dev);
	}

	if(errors == 0) {
		task_create_task(NULL, 64 << 10, &network_virtio_process_tx);

		PRINTLOG(VIRTIONET, LOG_INFO, "virtnet device started", 0);

		return 0;
	}

	PRINTLOG(VIRTIONET, LOG_ERROR, "virtnet device starting failed. error count: %i", errors);

	return -1;
}

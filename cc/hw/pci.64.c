/**
 * @file pci.64.c
 * @brief pci implementation
 */
#include <types.h>
#include <pci.h>
#include <memory.h>
#include <memory/paging.h>
#include <acpi.h>
#include <utils.h>
#include <video.h>
#include <ports.h>

typedef struct {
	memory_heap_t* heap;
	acpi_table_mcfg_t* mcfg;
	uint16_t group_number_count;
	uint16_t group_number;
	uint8_t bus_number;
	uint8_t device_number;
	uint8_t function_number;
	int8_t end_of_iter;
}pci_iterator_internal_t;

int8_t pci_iterator_destroy(iterator_t* iterator);
iterator_t* pci_iterator_next(iterator_t* iterator);
int8_t pci_iterator_end_of_iterator(iterator_t* iterator);
void* pci_iterator_get_item(iterator_t* iterator);


int8_t pci_io_port_write_data(uint32_t address, uint32_t data, uint8_t bc) {
	outl(address, PCI_IO_PORT_CONFIG);

	switch (bc) {
	case 1:
		outb(PCI_IO_PORT_DATA, data & 0xFF);
		break;
	case 2:
		outw(PCI_IO_PORT_DATA, data & 0xFFFF);
		break;
	case 4:
		outl(PCI_IO_PORT_DATA, data);
		break;
	}

	return 0;
}

uint32_t pci_io_port_read_data(uint32_t address, uint8_t bc) {
	outl(address, PCI_IO_PORT_CONFIG);

	uint32_t res = 0;

	switch (bc) {
	case 1:
		res = inb(PCI_IO_PORT_DATA);
		break;
	case 2:
		res = inw(PCI_IO_PORT_DATA);
		break;
	case 4:
		res = inl(PCI_IO_PORT_DATA);
		break;
	}

	return res;
}


iterator_t* pci_iterator_create_with_heap(memory_heap_t* heap, acpi_table_mcfg_t* mcfg){
	iterator_t* iter = memory_malloc_ext(heap, sizeof(iterator_t), 0x0);
	pci_iterator_internal_t* iter_metadata = memory_malloc_ext(heap, sizeof(pci_iterator_internal_t), 0x0);
	iter_metadata->heap = heap;
	iter_metadata->mcfg = mcfg;

	size_t count = mcfg->header.length - sizeof(acpi_sdt_header_t) - sizeof_field(acpi_table_mcfg_t, reserved0);
	count /= 16;
	iter_metadata->group_number_count = count;

	size_t not_used_fa;
	int8_t dev_found = -1;

	for(size_t i = 0; i < iter_metadata->group_number_count; i++) {
		iter_metadata->group_number = i;
		for(size_t bus_addr = mcfg->pci_segment_group_config[i].bus_start;
		    bus_addr <= mcfg->pci_segment_group_config[i].bus_end;
		    bus_addr++) {
			iter_metadata->bus_number = bus_addr;
			for(size_t dev_addr = 0; dev_addr < PCI_DEVICE_MAX_COUNT; dev_addr++) {
				iter_metadata->device_number = dev_addr;
				for(size_t func_addr = 0; func_addr < PCI_FUNCTION_MAX_COUNT; func_addr++) {
					iter_metadata->function_number = func_addr;
					size_t pci_mmio_addr = mcfg->pci_segment_group_config[i].base_address + ( bus_addr << 20 | dev_addr << 15 | func_addr << 12 );
					memory_paging_add_page_ext(iter_metadata->heap, NULL, pci_mmio_addr, pci_mmio_addr, MEMORY_PAGING_PAGE_TYPE_4K);
					pci_common_header_t* pci_hdr = (pci_common_header_t*)pci_mmio_addr;
					if(pci_hdr->vendor_id != 0xFFFF) {
						dev_found = 0;
						break;
					} else {
						memory_paging_delete_page_ext_with_heap(iter_metadata->heap, NULL, pci_mmio_addr, &not_used_fa);
					}
				} // func_addr loop
				if(dev_found == 0) {
					break;
				}
			} // dev_addr loop
			if(dev_found == 0) {
				break;
			}
		} // bus_addr loop
		if(dev_found == 0) {
			break;
		}
	} // bus_group loop

	if(dev_found == 0) {
		iter_metadata->end_of_iter = -1;
	} else {
		iter_metadata->end_of_iter = 0;
	}

	iter->metadata = iter_metadata;
	iter->destroy = &pci_iterator_destroy;
	iter->next = &pci_iterator_next;
	iter->end_of_iterator = &pci_iterator_end_of_iterator;
	iter->get_item = &pci_iterator_get_item;
	return iter;
}


int8_t pci_iterator_destroy(iterator_t* iterator){
	pci_iterator_internal_t* iter_metadata = (pci_iterator_internal_t*)iterator->metadata;
	memory_heap_t* heap = iter_metadata->heap;
	memory_free_ext(heap, iter_metadata);
	memory_free_ext(heap, iterator);
	return 0;
}

iterator_t* pci_iterator_next(iterator_t* iterator){
	pci_iterator_internal_t* iter_metadata = (pci_iterator_internal_t*)iterator->metadata;

	int8_t dev_found = -1;
	int8_t check_func0 = -1;
	size_t not_used_fa;

	for(size_t bus_group = iter_metadata->group_number; bus_group < iter_metadata->group_number_count; bus_group++) {
		iter_metadata->group_number = bus_group;

		for(size_t bus_addr = iter_metadata->bus_number;
		    bus_addr < iter_metadata->mcfg->pci_segment_group_config[bus_group].bus_end;
		    bus_addr++) {
			iter_metadata->bus_number = bus_addr;

			for(size_t dev_addr = iter_metadata->device_number; dev_addr < PCI_DEVICE_MAX_COUNT; dev_addr++) {
				iter_metadata->device_number = dev_addr;

				//calculate mmio address of device
				size_t pci_mmio_addr = iter_metadata->mcfg->pci_segment_group_config[bus_group].base_address + ( bus_addr << 20 | dev_addr << 15 | 0 << 12 );

				// add page for it. we need access
				if(memory_paging_add_page_ext(iter_metadata->heap, NULL, pci_mmio_addr, pci_mmio_addr, MEMORY_PAGING_PAGE_TYPE_4K) != 0) {
					printf("\nKERN: FATAL cannot add page for pci dev at %02x:%02x:%02x mmio addr: 0x%08lx\n", bus_group, bus_addr, dev_addr, pci_mmio_addr);

					iter_metadata->end_of_iter = 0; //end iter

					return iterator;
				}

				pci_common_header_t* pci_hdr = (pci_common_header_t*)pci_mmio_addr;

				if(pci_hdr->vendor_id != 0xFFFF) { // look for vendor_id
					if(check_func0 == 0) { // one/multi func check?
						dev_found = 0;

						break;
					} else {
						if(pci_hdr->header_type.multifunction == 1) {
							iter_metadata->function_number++;

							for(size_t func_addr = iter_metadata->function_number; func_addr < PCI_FUNCTION_MAX_COUNT; func_addr++) {
								iter_metadata->function_number = func_addr;

								size_t pci_mmio_addr_f = iter_metadata->mcfg->pci_segment_group_config[bus_group].base_address + ( bus_addr << 20 | dev_addr << 15 | func_addr << 12 );

								// add page for multi function device
								if(memory_paging_add_page_ext(iter_metadata->heap, NULL, pci_mmio_addr_f, pci_mmio_addr_f, MEMORY_PAGING_PAGE_TYPE_4K) != 0) {
									printf("\nKERN: FATAL cannot add page for pci dev at %02x:%02x:%02x.%02x mmio addr: 0x%08lx\n", bus_group, bus_addr, dev_addr, func_addr, pci_mmio_addr_f);

									iter_metadata->end_of_iter = 0; //end iter

									return iterator;
								}

								pci_hdr = (pci_common_header_t*)pci_mmio_addr_f;

								if(pci_hdr->vendor_id != 0xFFFF) {
									dev_found = 0;

									break;
								} else {
									// no def so delete page for function of device
									memory_paging_delete_page_ext_with_heap(iter_metadata->heap, NULL, pci_mmio_addr_f, &not_used_fa);
								}
							}
						} // end of one/multi check

					}
				} else { // no device there than delete page added for pci_mmio_addr
					memory_paging_delete_page_ext_with_heap(iter_metadata->heap, NULL, pci_mmio_addr, &not_used_fa);
				}

				if(dev_found == 0) {
					break;
				} else {
					iter_metadata->function_number = 0;
					check_func0 = 0;
				}

			} //end of device loop

			if(dev_found == 0) {
				break;
			} else {
				iter_metadata->device_number = 0;
			}
		} // bus loop end

		if(dev_found == 0) {
			break;
		}else if((bus_group + 1) < iter_metadata->group_number_count) {
			iter_metadata->bus_number = iter_metadata->mcfg->pci_segment_group_config[bus_group + 1].bus_start;
		}
	} //end of bus group loop

	if(dev_found == -1) {
		iter_metadata->end_of_iter = 0;
	}

	return iterator;
}

int8_t pci_iterator_end_of_iterator(iterator_t* iterator){
	pci_iterator_internal_t* iter_metadata = (pci_iterator_internal_t*)iterator->metadata;
	return iter_metadata->end_of_iter;
}

void* pci_iterator_get_item(iterator_t* iterator){
	pci_iterator_internal_t* iter_metadata = (pci_iterator_internal_t*)iterator->metadata;
	memory_heap_t* heap = iter_metadata->heap;
	pci_dev_t* d = memory_malloc_ext(heap, sizeof(pci_dev_t), 0x0);
	d->group_number = iter_metadata->group_number;
	d->bus_number = iter_metadata->bus_number;
	d->device_number = iter_metadata->device_number;
	d->function_number = iter_metadata->function_number;
	d->pci_header = (pci_common_header_t*)(iter_metadata->mcfg->pci_segment_group_config[d->group_number].base_address +
	                                       (((size_t)d->bus_number ) << 20 |
	                                        ((size_t)d->device_number) << 15 |
	                                        ((size_t)d->function_number ) << 12)
	                                       );
	return d;
}

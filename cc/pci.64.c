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

typedef struct {
	memory_heap_t* heap;
	acpi_table_mcfg_t* mcfg;
	uint16_t group_number;
	uint16_t group_number_count;
	uint8_t bus_number;
	uint8_t device_number;
	uint8_t function_number;
	pci_common_header_t* pci_header;
	int8_t end_of_iter;
}pci_iterator_internal_t;

int8_t pci_iterator_destroy(iterator_t* iterator);
iterator_t* pci_iterator_next(iterator_t* iterator);
int8_t pci_iterator_end_of_iterator(iterator_t* iterator);
void* pci_iterator_get_item(iterator_t* iterator);

iterator_t* pci_iterator_create_with_heap(memory_heap_t* heap, acpi_table_mcfg_t* mcfg){
	iterator_t* iter = memory_malloc_ext(heap, sizeof(iterator_t), 0x0);
	pci_iterator_internal_t* iter_metadata = memory_malloc_ext(heap, sizeof(pci_iterator_internal_t), 0x0);
	iter_metadata->heap = heap;
	iter_metadata->mcfg = mcfg;
	iter_metadata->end_of_iter = -1;

	iter_metadata->group_number = 0;
	iter_metadata->device_number = 0;
	iter_metadata->function_number = 0;

	size_t count = mcfg->header.length - sizeof(acpi_sdt_header_t) - sizeof_field(acpi_table_mcfg_t, reserved0);
	count /= 16;
	iter_metadata->group_number_count = count;
	iter_metadata->bus_number = mcfg->pci_segment_group_config[0].bus_start;

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
	while(iter_metadata->group_number < iter_metadata->group_number_count) {
		size_t pci_mmio_addr = iter_metadata->mcfg->pci_segment_group_config[iter_metadata->group_number].base_address;
		size_t bus_start = iter_metadata->mcfg->pci_segment_group_config[iter_metadata->group_number].bus_start;
		size_t bus_end = iter_metadata->mcfg->pci_segment_group_config[iter_metadata->group_number].bus_end;
		for(size_t bus = bus_start; bus <= bus_end; bus++) {
			size_t bus_addr = iter_metadata->bus_number - bus_start;
			size_t dev_addr = iter_metadata->device_number;
			size_t func_addr = iter_metadata->function_number;
			pci_mmio_addr = pci_mmio_addr + ( bus_addr << 20 | dev_addr << 15 | func_addr << 12 );
			memory_paging_add_page_ext(iter_metadata->heap, NULL, pci_mmio_addr, pci_mmio_addr, MEMORY_PAGING_PAGE_TYPE_2M);
			pci_common_header_t* pci_hdr = (pci_common_header_t*)pci_mmio_addr;

			iter_metadata->pci_header = pci_hdr;

		}
		iter_metadata->group_number++;
	}
	return iterator;
}

int8_t pci_iterator_end_of_iterator(iterator_t* iterator){
	pci_iterator_internal_t* iter_metadata = (pci_iterator_internal_t*)iterator->metadata;
	return iter_metadata->end_of_iter;
}

void* pci_iterator_get_item(iterator_t* iterator){
	pci_iterator_internal_t* iter_metadata = (pci_iterator_internal_t*)iterator->metadata;
	return iter_metadata->pci_header;
}

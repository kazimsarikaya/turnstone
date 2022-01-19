/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include "setup.h"
#include <strings.h>
#include <linker.h>
#include <systeminfo.h>
#include <cpu.h>

efi_system_table_t* ST;
efi_boot_services_t* BS;

typedef int8_t (* kernel_start_t)(system_info_t* sysinfo);

int64_t efi_main(efi_handle_t image, efi_system_table_t* system_table) {
	ST = system_table;
	BS = system_table->boot_services;

	video_clear_screen();

	PRINTLOG(EFI, LOG_INFO, "%s", "TURNSTONE EFI Loader Starting...");

	void* heap_area;
	int64_t heap_size = 1024 * 4096;

	efi_status_t res = BS->allocate_pool(EFI_LOADER_DATA, heap_size, &heap_area);

	if(res == EFI_SUCCESS) {
		PRINTLOG(EFI, LOG_DEBUG, "memory pool created", 0);

		size_t start = (size_t)heap_area;

		memory_heap_t* heap = memory_create_heap_simple(start, start + heap_size);

		memory_set_default_heap(heap);

		if(heap) {
			PRINTLOG(EFI, LOG_DEBUG, "heap created at 0x%lp with size 0x%x", heap_area, heap_size);
		} else {
			PRINTLOG(EFI, LOG_DEBUG, "heap creation failed", 0);
		}


		video_frame_buffer_t* vfb = NULL;

		efi_guid_t gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
		efi_gop_t* gop;
		res = BS->locate_protocol(&gop_guid, NULL, (efi_handle_t*)&gop);

		if(res == EFI_SUCCESS) {
			uint64_t next_mode = gop->mode->mode;
			for(int64_t i = 0; i < gop->mode->max_mode; i++) {
				uint64_t gop_mode_size = 0;
				efi_gop_mode_info_t* gop_mi = NULL;
				if(gop->query_mode(gop, i, &gop_mode_size, &gop_mi) == EFI_SUCCESS && gop_mi->horizontal_resolution == 1280 && gop_mi->vertical_resolution == 1024) {
					next_mode = i;
					break;
				}
			}

			gop->set_mode(gop, next_mode);


			vfb = memory_malloc(sizeof(video_frame_buffer_t));

			vfb->physical_base_address = gop->mode->frame_buffer_base;
			vfb->virtual_base_address = gop->mode->frame_buffer_base;
			vfb->buffer_size = gop->mode->frame_buffer_size;
			vfb->width = gop->mode->information->horizontal_resolution;
			vfb->height = gop->mode->information->vertical_resolution;
			vfb->pixels_per_scanline = gop->mode->information->pixels_per_scanline;

			PRINTLOG(EFI, LOG_DEBUG, "frame buffer info %ix%i pps %i at 0x%lp size 0x%lx", vfb->width, vfb->height, vfb->pixels_per_scanline, vfb->physical_base_address, vfb->buffer_size);
			PRINTLOG(EFI, LOG_DEBUG, "vfb address 0x%lp", vfb);

		} else {
			PRINTLOG(EFI, LOG_FATAL, "gop handle failed %i. Halting...", res);
			cpu_hlt();
		}

		efi_guid_t bio_guid = EFI_BLOCK_IO_PROTOCOL_GUID;
		efi_handle_t handles[128];
		uint64_t handle_size = sizeof(handles);
		res = BS->locate_handle(EFI_BY_PROTOCOL, &bio_guid, NULL, &handle_size, (efi_handle_t*)&handles);

		if(res == EFI_SUCCESS) {
			handle_size /= (uint64_t)sizeof(efi_handle_t);

			PRINTLOG(EFI, LOG_DEBUG, "block devs retrived. count %i", handle_size);

			block_file_t* blk_devs = (block_file_t*)memory_malloc(handle_size * sizeof(block_file_t));
			int64_t blk_dev_cnt = 0;
			int64_t sys_disk_idx = -1;

			if(blk_devs) {

				for(uint64_t i = 0; i < handle_size; i++) {

					if(handles[i] && !EFI_ERROR(BS->handle_protocol(handles[i], &bio_guid, (void**) &blk_devs[blk_dev_cnt].bio)) &&
					   blk_devs[blk_dev_cnt].bio && blk_devs[blk_dev_cnt].bio->media && blk_devs[blk_dev_cnt].bio->media->block_size > 0) {

						PRINTLOG(EFI, LOG_DEBUG, "disk %i mid %i block size: %i removable %i present %i readonly %i size %li",
						         blk_dev_cnt, blk_devs[blk_dev_cnt].bio->media->media_id, blk_devs[blk_dev_cnt].bio->media->block_size,
						         blk_devs[blk_dev_cnt].bio->media->removable_media,
						         blk_devs[blk_dev_cnt].bio->media->media_present, blk_devs[blk_dev_cnt].bio->media->readonly,
						         blk_devs[blk_dev_cnt].bio->media->last_block);

						if(blk_devs[blk_dev_cnt].bio->media->block_size == 512) {
							uint8_t* buffer = memory_malloc(512);

							res = blk_devs[blk_dev_cnt].bio->read(blk_devs[blk_dev_cnt].bio, blk_devs[blk_dev_cnt].bio->media->media_id, 0, 512, buffer);

							if(res == EFI_SUCCESS) {
								efi_pmbr_partition_t* pmbr = (efi_pmbr_partition_t*)&buffer[0x1be];

								if(pmbr->part_type == EFI_PMBR_PART_TYPE) {
									PRINTLOG(EFI, LOG_DEBUG, "gpt disk id %li", blk_dev_cnt);
									sys_disk_idx = blk_dev_cnt;
									memory_free(buffer);

									break;
								}
							}

							memory_free(buffer);

						}

						blk_dev_cnt++;


					} // end of checking disk existence

				} // end of iter over all disks

				uint64_t frm_start_1mib = 1 << 20;

				if(BS->allocate_pages(EFI_ALLOCATE_ADDRESS, EFI_LOADER_DATA, 0x100, &frm_start_1mib) == 0) {

					memory_memclean((void*)frm_start_1mib, 0x100 * 4096);

					if(sys_disk_idx != -1) {
						PRINTLOG(EFI, LOG_DEBUG, "openning sys disk %li", sys_disk_idx);

						disk_t* sys_disk = efi_disk_impl_open(blk_devs[sys_disk_idx].bio);

						if(sys_disk) {
							PRINTLOG(EFI, LOG_DEBUG, "openning as gpt disk", 0);

							sys_disk = gpt_get_or_create_gpt_disk(sys_disk);

							PRINTLOG(EFI, LOG_DEBUG, "gpt disk getted", 0);

							disk_partition_context_t* part_ctx = sys_disk->get_partition(sys_disk, 1);

							PRINTLOG(EFI, LOG_DEBUG, "kernel start lba %x end lba %x", part_ctx->start_lba, part_ctx->end_lba);

							uint8_t* kernel_data;
							int64_t kernel_size = (part_ctx->end_lba - part_ctx->start_lba  + 1) * blk_devs[sys_disk_idx].bio->media->block_size;

							PRINTLOG(EFI, LOG_DEBUG, "kernel size %li", kernel_size);

							if(sys_disk->read(sys_disk, part_ctx->start_lba, part_ctx->end_lba - part_ctx->start_lba  + 1, &kernel_data) == 0) {
								PRINTLOG(EFI, LOG_DEBUG, "kernel loaded at 0x%lp", kernel_data);

								int64_t kernel_page_count = kernel_size / 4096 + 0x150;         // adding extra pages for stack and heap
								if(kernel_size % 4096) {
									kernel_page_count++;
								}

								int64_t new_kernel_2m_factor = 0;
								new_kernel_2m_factor = kernel_page_count / 512;
								if(kernel_page_count % 512) {
									new_kernel_2m_factor++;
								}
								kernel_page_count = new_kernel_2m_factor * 512;

								PRINTLOG(EFI, LOG_DEBUG, "new kernel page count 0x%lx", kernel_page_count);

								uint64_t new_kernel_address = 2 << 20;

								if(BS->allocate_pages(EFI_ALLOCATE_ADDRESS, EFI_LOADER_DATA, kernel_page_count, &new_kernel_address) == 0) {
									PRINTLOG(EFI, LOG_DEBUG, "alloc pages for new kernel succed at 0x%lx", new_kernel_address);

									memory_memclean((void*)new_kernel_address, kernel_page_count * 4096);

									if(linker_memcopy_program_and_relink((size_t)kernel_data, new_kernel_address, ((size_t)kernel_data) + 0x100 - 1) == 0) {
										PRINTLOG(EFI, LOG_DEBUG, "moving kernel at 0x%lx succed", new_kernel_address);
										memory_free(kernel_data);


										PRINTLOG(EFI, LOG_DEBUG, "conf table count %i", system_table->configuration_table_entry_count);
										efi_guid_t acpi_table_v2_guid = EFI_ACPI_20_TABLE_GUID;
										efi_guid_t acpi_table_v1_guid = EFI_ACPI_TABLE_GUID;

										void* acpi_rsdp = NULL;
										void* acpi_xrsdp = NULL;

										for (uint64_t i = 0; i <  system_table->configuration_table_entry_count; i++ ) {
											if(efi_guid_equal(acpi_table_v2_guid, system_table->configuration_table[i].vendor_guid) == 0) {
												acpi_xrsdp = system_table->configuration_table[i].vendor_table;
											} else if(efi_guid_equal(acpi_table_v1_guid, system_table->configuration_table[i].vendor_guid) == 0) {
												acpi_rsdp = system_table->configuration_table[i].vendor_table;
											}
										}

										uint8_t* mmap = NULL;
										uint64_t map_size, map_key, descriptor_size;
										uint32_t descriptor_version;

										BS->get_memory_map(&map_size, (efi_memory_descriptor_t*)mmap, &map_key, &descriptor_size, &descriptor_version);
										PRINTLOG(EFI, LOG_DEBUG, "mmap size %li desc size %li ver %li", map_size, descriptor_size, descriptor_version);

										mmap = memory_malloc(map_size);

										if(BS->get_memory_map(&map_size, (efi_memory_descriptor_t*)mmap, &map_key, &descriptor_size, &descriptor_version) == EFI_SUCCESS) {
											system_info_t* sysinfo = memory_malloc(sizeof(system_info_t));
											sysinfo->boot_type = SYSTEM_INFO_BOOT_TYPE_DISK;
											sysinfo->mmap_data = mmap;
											sysinfo->mmap_size = map_size;
											sysinfo->mmap_descriptor_size = descriptor_size;
											sysinfo->mmap_descriptor_version = descriptor_version;
											sysinfo->frame_buffer = vfb;
											sysinfo->acpi_version = acpi_xrsdp != NULL?2:1;
											sysinfo->acpi_table = acpi_xrsdp != NULL?acpi_xrsdp:acpi_rsdp;
											sysinfo->kernel_start = new_kernel_address;
											sysinfo->kernel_4k_frame_count = kernel_page_count;

											PRINTLOG(EFI, LOG_INFO, "calling kernel @ 0x%lp with sysinfo @ 0x%lp", new_kernel_address, sysinfo);

											BS->exit_boot_services(image, map_key);

											kernel_start_t ks = (kernel_start_t)new_kernel_address;


											ks(sysinfo);


										} else {
											PRINTLOG(EFI, LOG_ERROR, "cannot fill memory map.", 0);
										}

									}         // endof linker

								} else {
									PRINTLOG(EFI, LOG_ERROR, "cannot alloc pages for new kernel", 0);
								}

							} else {
								PRINTLOG(EFI, LOG_ERROR, "kernel load failed", 0);
							}

						} else {
							PRINTLOG(EFI, LOG_ERROR, "sys disk open failed", 0);
						}

					}
				} else {
					PRINTLOG(EFI, LOG_ERROR, "cannot allocate frame for kernel usage at 1mib to 2mib", 0);
				}

			} // end of malloc success

		} else {
			PRINTLOG(EFI, LOG_ERROR, "blocks devs retrivation failed. code: 0x%x", res);
		}



	} else {
		PRINTLOG(EFI, LOG_ERROR, "memory pool creation failed. err code 0x%x", res);
	}

	PRINTLOG(EFI, LOG_FATAL, "efi app could not have finished correctly, infinite loop started. Halting...", 0);

	cpu_hlt();

	return EFI_LOAD_ERROR;
}

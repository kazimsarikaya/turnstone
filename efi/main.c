#include "setup.h"
#include <strings.h>
#include <linker.h>
#include <systeminfo.h>

efi_system_table_t* ST;
efi_boot_services_t* BS;

typedef int8_t (* kernel_start_t)(system_info_t* sysinfo);

int64_t efi_main(efi_handle_t image, efi_system_table_t* system_table) {
	ST = system_table;
	BS = system_table->boot_services;

	video_clear_screen();

	printf("%s\n", "hello from efi app");

	void* heap_area;
	int64_t heap_size = 1024 * 4096;

	efi_status_t res = BS->allocate_pool(EFI_LOADER_DATA, heap_size, &heap_area);

	if(res == EFI_SUCCESS) {
		printf("memory pool created\n");

		size_t start = (size_t)heap_area;

		memory_heap_t* heap = memory_create_heap_simple(start, start + heap_size);

		memory_set_default_heap(heap);

		if(heap) {
			printf("heap created at 0x%p with size 0x%x\n", heap_area, heap_size);
		} else {
			printf("heap creation failed\n");
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

			vfb->base_address = (void*)gop->mode->frame_buffer_base;
			vfb->buffer_size = gop->mode->frame_buffer_size;
			vfb->width = gop->mode->information->horizontal_resolution;
			vfb->height = gop->mode->information->vertical_resolution;
			vfb->pixels_per_scanline = gop->mode->information->pixels_per_scanline;

			printf("frame buffer info %ix%i pps %i at 0x%p size 0x%lx\n", vfb->width, vfb->height, vfb->pixels_per_scanline, vfb->base_address, vfb->buffer_size);
			printf("vfb address 0x%p\n", vfb);

		} else {
			printf("gop handle failed %i\n", res);
		}

		efi_guid_t bio_guid = EFI_BLOCK_IO_PROTOCOL_GUID;
		efi_handle_t handles[128];
		uint64_t handle_size = sizeof(handles);
		res = BS->locate_handle(EFI_BY_PROTOCOL, &bio_guid, NULL, &handle_size, (efi_handle_t*)&handles);

		if(res == EFI_SUCCESS) {
			handle_size /= (uint64_t)sizeof(efi_handle_t);

			printf("block devs retrived. count %i\n", handle_size);

			block_file_t* blk_devs = (block_file_t*)memory_malloc(handle_size * sizeof(block_file_t));
			int64_t blk_dev_cnt = 0;
			int64_t sys_disk_idx = -1;

			if(blk_devs) {

				for(int64_t i = 0; i < handle_size; i++) {

					if(handles[i] && !EFI_ERROR(BS->handle_protocol(handles[i], &bio_guid, (void**) &blk_devs[blk_dev_cnt].bio)) &&
					   blk_devs[blk_dev_cnt].bio && blk_devs[blk_dev_cnt].bio->media && blk_devs[blk_dev_cnt].bio->media->block_size > 0) {

						printf("disk %i mid %i block size: %i removable %i present %i readonly %i size %li\n",
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
									printf("gpt disk id %li\n", blk_dev_cnt);
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

				if(sys_disk_idx != -1) {
					printf("openning sys disk %li\n", sys_disk_idx);

					disk_t* sys_disk = efi_disk_impl_open(blk_devs[sys_disk_idx].bio);

					if(sys_disk) {
						printf("openning as gpt disk\n");

						sys_disk = gpt_get_or_create_gpt_disk(sys_disk);

						printf("gpt disk getted\n");

						disk_partition_context_t* part_ctx = sys_disk->get_partition(sys_disk, 1);

						printf("kernel start lba %x end lba %x\n", part_ctx->start_lba, part_ctx->end_lba);

						uint8_t* kernel_data;
						int64_t kernel_size = (part_ctx->end_lba - part_ctx->start_lba  + 1) * blk_devs[sys_disk_idx].bio->media->block_size;

						printf("kernel size %li\n", kernel_size);

						if(sys_disk->read(sys_disk, part_ctx->start_lba, part_ctx->end_lba - part_ctx->start_lba  + 1, &kernel_data) == 0) {
							printf("kernel loaded at 0x%p\n", kernel_data);

							int64_t kernel_page_count = kernel_size / 4096 + 0x120; // adding extra pages for stack and heap
							if(kernel_size % 4096) {
								kernel_page_count++;
							}

							uint64_t new_kernel_address = 2 << 20;

							if(BS->allocate_pages(EFI_ALLOCATE_ADDRESS, EFI_LOADER_DATA, kernel_page_count, &new_kernel_address) == 0) {
								printf("alloc pages for new kernel succed at 0x%lx\n", new_kernel_address);

								if(linker_memcopy_program_and_relink((size_t)kernel_data, new_kernel_address, ((size_t)kernel_data) + 0x100 - 1) == 0) {
									printf("moving kernel at 0x%lx succed\n", new_kernel_address);
									memory_free(kernel_data);

									efi_memory_descriptor_t* mmap = NULL;
									uint64_t map_size, map_key, descriptor_size;
									uint32_t descriptor_version;

									BS->get_memory_map(&map_size, mmap, &map_key, &descriptor_size, &descriptor_version);
									printf("mmap size %li desc size %li ver %li\n", map_size, descriptor_size, descriptor_version);

									mmap = memory_malloc(map_size);

									if(BS->get_memory_map(&map_size, mmap, &map_key, &descriptor_size, &descriptor_version) == EFI_SUCCESS) {
										system_info_t* sysinfo = memory_malloc(sizeof(system_info_t));
										sysinfo->boot_type = SYSTEM_INFO_BOOT_TYPE_DISK;
										sysinfo->mmap = mmap;
										sysinfo->mmap_size = map_size;
										sysinfo->mmap_descriptor_size = descriptor_size;
										sysinfo->mmap_descriptor_version = descriptor_version;
										sysinfo->frame_buffer = vfb;


										printf("calling kernel with sysinfo @ 0x%p\n", sysinfo);

										BS->exit_boot_services(image, map_key);

										kernel_start_t ks = (kernel_start_t)new_kernel_address;


										ks(sysinfo);


									} else {
										printf("cannot fill memory map\n");
									}

								} // endof linker

							} else {
								printf("cannot alloc pages for new kernel\n");
								memory_free(kernel_data);
							}

						} else {
							printf("kernel load failed\n");
							memory_free(kernel_data);
						}




						memory_free(part_ctx);
					} else {
						printf("sys disk open failed\n");
					}

				}

			} // end of malloc success

		} else {
			printf("blocks devs retrivation failed. code: 0x%x\n", res);
		}



	} else {
		printf("memory pool creation failed. err code 0x%x\n", res);
	}

	printf("efi app finished, infinite loop started.\n");

	while(1);

	return EFI_SUCCESS;
}

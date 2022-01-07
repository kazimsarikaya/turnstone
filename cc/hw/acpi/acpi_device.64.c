/**
 * @file acpi_device.64.c
 * @brief acpi device utils
 */
#include <acpi/aml_internal.h>
#include <acpi/aml_resource.h>
#include <strings.h>
#include <video.h>
#include <utils.h>
#include <memory/frame.h>
#include <memory/paging.h>
#include <linkedlist.h>

int8_t acpi_aml_intmap_addr_sorter(const void* data1, const void* data2){
	acpi_aml_interrupt_map_item_t* item1 = (acpi_aml_interrupt_map_item_t*)data1;
	acpi_aml_interrupt_map_item_t* item2 = (acpi_aml_interrupt_map_item_t*)data2;

	if(item1->address < item2->address) {
		return -1;
	}

	if(item1->address > item2->address) {
		return 1;
	}

	return 0;
}

int8_t acpi_aml_intmap_eq(const void* data1, const void* data2){
	acpi_aml_interrupt_map_item_t* item1 = (acpi_aml_interrupt_map_item_t*)data1;
	acpi_aml_interrupt_map_item_t* item2 = (acpi_aml_interrupt_map_item_t*)data2;

	if(item1->address == item2->address && item1->interrupt_no == item2->interrupt_no) {
		return 0;
	}

	return -1;
}

int8_t acpi_build_interrupt_map(acpi_aml_parser_context_t* ctx){
	acpi_aml_object_t* val_obj;
	int32_t err_cnt = 0;
	iterator_t* iter;

	PRINTLOG(ACPI, LOG_DEBUG, "selecting pic mode", 0);

	val_obj = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_object_t), 0);

	if(val_obj == NULL) {
		PRINTLOG(ACPI, LOG_FATAL, "cannot allocate param", 0);

		return -1;
	}

	val_obj->type = ACPI_AML_OT_NUMBER;
	val_obj->number.value = 0;
	val_obj->number.bytecnt = 8;

	if(acpi_aml_execute(ctx, ctx->pic, NULL, val_obj) != 0) {
		memory_free_ext(ctx->heap, val_obj);
		PRINTLOG(ACPI, LOG_ERROR, "cannot execute pic method", 0);

		return -1;
	}

	PRINTLOG(ACPI, LOG_DEBUG, "pic mode selected, pic interrupt devices will be disabled", 0);

	memory_free_ext(ctx->heap, val_obj);

	iter = linkedlist_iterator_create(ctx->devices);

	while(iter->end_of_iterator(iter) != 0) {
		acpi_aml_device_t* d = iter->get_item(iter);

		if(d->prt) {
			PRINTLOG(ACPI, LOG_TRACE, "device has prt method executing", 0);

			acpi_aml_object_t* prt_table = NULL;

			if(acpi_aml_execute(ctx, d->prt, &prt_table) != 0) {
				PRINTLOG(ACPI, LOG_ERROR, "cannot execute prt method", 0);
			} else if(prt_table == NULL || prt_table->type != ACPI_AML_OT_PACKAGE) {
				PRINTLOG(ACPI, LOG_ERROR, "prt table is null or not package", 0);
			} else {
				iterator_t* prt_iter = linkedlist_iterator_create(prt_table->package.elements);

				while(prt_iter->end_of_iterator(prt_iter) != 0) {
					acpi_aml_object_t* item = prt_iter->get_item(prt_iter);

					if(item->type == ACPI_AML_OT_PACKAGE) {
						acpi_aml_object_t* int_dev_ref = linkedlist_get_data_at_position(item->package.elements, 2);

						if(int_dev_ref->type == ACPI_AML_OT_RUNTIMEREF) {
							acpi_aml_device_t* int_dev = acpi_device_lookup(ctx, int_dev_ref->name);

							if(int_dev) {
								if(int_dev->dis && int_dev->disabled != 1) {
									PRINTLOG(ACPI, LOG_TRACE, "try to disable interrupt device %s", int_dev->name);
									if(acpi_aml_execute(ctx, int_dev->dis, NULL) == 0) {
										PRINTLOG(ACPI, LOG_TRACE, "interrupt device %s disabled", int_dev->name);
										int_dev->disabled = 1;
									}
								}
							} else {
								PRINTLOG(ACPI, LOG_ERROR, "interrupt device not found: %s", int_dev_ref->name);
							}

						}
					}

					prt_iter = prt_iter->next(prt_iter);
				}


				prt_iter->destroy(prt_iter);
			}
		}

		iter = iter->next(iter);
	}

	iter->destroy(iter);


	PRINTLOG(ACPI, LOG_DEBUG, "pic interrupt devices disabled", 0);

	val_obj = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_object_t), 0);

	if(val_obj == NULL) {
		PRINTLOG(ACPI, LOG_FATAL, "cannot allocate param", 0);

		return -1;
	}

	val_obj->type = ACPI_AML_OT_NUMBER;
	val_obj->number.value = 1;
	val_obj->number.bytecnt = 8;

	if(acpi_aml_execute(ctx, ctx->pic, NULL, val_obj) != 0) {
		memory_free_ext(ctx->heap, val_obj);
		PRINTLOG(ACPI, LOG_ERROR, "cannot execute pic method", 0);

		return -1;
	}

	PRINTLOG(ACPI, LOG_DEBUG, "apic mode selected, map build started", 0);

	memory_free_ext(ctx->heap, val_obj);

	ctx->interrupt_map = linkedlist_create_sortedlist_with_heap(ctx->heap, acpi_aml_intmap_addr_sorter);
	linkedlist_set_equality_comparator(ctx->interrupt_map, acpi_aml_intmap_eq);

	iter = linkedlist_iterator_create(ctx->devices);

	while(iter->end_of_iterator(iter) != 0) {
		acpi_aml_device_t* d = iter->get_item(iter);

		if(d->prt) {
			PRINTLOG(ACPI, LOG_TRACE, "device has prt method executing", 0);

			acpi_aml_object_t* prt_table = NULL;

			if(acpi_aml_execute(ctx, d->prt, &prt_table) != 0) {
				PRINTLOG(ACPI, LOG_ERROR, "cannot execute prt method", 0);
			} else if(prt_table == NULL || prt_table->type != ACPI_AML_OT_PACKAGE) {
				PRINTLOG(ACPI, LOG_ERROR, "prt table is null or not package", 0);
			} else {

				iterator_t* prt_iter = linkedlist_iterator_create(prt_table->package.elements);

				while(prt_iter->end_of_iterator(prt_iter) != 0) {
					acpi_aml_object_t* item = prt_iter->get_item(prt_iter);

					if(item->type == ACPI_AML_OT_PACKAGE) {
						acpi_aml_object_t* addr_obj = linkedlist_get_data_at_position(item->package.elements, 0);

						if(addr_obj->type == ACPI_AML_OT_NUMBER) {
							uint64_t addr = addr_obj->number.value;

							acpi_aml_object_t* int_dev_ref = linkedlist_get_data_at_position(item->package.elements, 2);

							uint32_t int_no_val = 0;

							if(int_dev_ref->type == ACPI_AML_OT_NUMBER) {
								PRINTLOG(ACPI, LOG_TRACE, "direct int no", 0);

								int_no_val = int_dev_ref->number.value;

								if(int_no_val == 0) {
									PRINTLOG(ACPI, LOG_TRACE, "global direct int no", 0);
									acpi_aml_object_t* global_int_ref = linkedlist_get_data_at_position(item->package.elements, 3);

									int_no_val = global_int_ref->number.value;
								}

							} else if(int_dev_ref->type == ACPI_AML_OT_RUNTIMEREF) {
								PRINTLOG(ACPI, LOG_TRACE, "int device %s", int_dev_ref->name);

								acpi_aml_device_t* int_dev = acpi_device_lookup(ctx, int_dev_ref->name);

								if(int_dev && int_dev->interrupts) {
									acpi_aml_device_interrupt_t* int_obj = linkedlist_get_data_at_position(int_dev->interrupts, 0);

									int_no_val = int_obj->interrupt_no;
								} else {
									PRINTLOG(ACPI, LOG_ERROR, "apic int dev not found", 0);
									err_cnt += -1;
									int_no_val = 0;
								}
							} else {
								PRINTLOG(ACPI, LOG_ERROR, "malformed prt package", 0);
								err_cnt += -1;
								int_no_val = 0;
							}

							if(int_no_val) {
								acpi_aml_interrupt_map_item_t tmp_int_map_item = {addr, int_no_val};

								if(linkedlist_contains(ctx->interrupt_map, &tmp_int_map_item) != 0) {
									acpi_aml_interrupt_map_item_t* int_map_item = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_interrupt_map_item_t), 0);
									int_map_item->address = addr;
									int_map_item->interrupt_no = int_no_val;

									PRINTLOG(ACPI, LOG_TRACE, "apic map item addr 0x%lx intno 0x%x", addr, int_no_val);

									linkedlist_sortedlist_insert(ctx->interrupt_map, int_map_item);
								} else {
									PRINTLOG(ACPI, LOG_TRACE, "apic map item exists", 0);
								}

							}

						} else {
							PRINTLOG(ACPI, LOG_ERROR, "address isnot integer", 0);
							err_cnt += -1;
						}

					} else {
						PRINTLOG(ACPI, LOG_ERROR, "apic prt table isnot package", 0);
						err_cnt += -1;
					}

					prt_iter = prt_iter->next(prt_iter);
				}


				prt_iter->destroy(prt_iter);
			}
		}

		iter = iter->next(iter);
	}

	iter->destroy(iter);

	PRINTLOG(ACPI, LOG_TRACE, "interrupt map size %i error count %i", linkedlist_size(ctx->interrupt_map), err_cnt);
	PRINTLOG(ACPI, LOG_DEBUG, "apic mode map builded", 0);

	return err_cnt == 0?0:-1;
}

int8_t acpi_device_build(acpi_aml_parser_context_t* ctx) {
	uint64_t item_count = 0;

	ctx->devices = linkedlist_create_sortedlist_with_heap(ctx->heap, acpi_aml_device_name_comparator);
	acpi_aml_device_t* curr_device = NULL;


	iterator_t* iter = linkedlist_iterator_create(ctx->symbols);
	while(iter->end_of_iterator(iter) != 0) {
		acpi_aml_object_t* sym = iter->get_item(iter);

		if(sym == NULL && sym->name == NULL) {
			iter->destroy(iter);
			PRINTLOG(ACPI, LOG_FATAL, "NULL object at symbol table or no name", 0);
			return -1;
		}

		if(strends(sym->name, "_PIC") == 0) {
			ctx->pic = sym;
		}

		if(sym->type == ACPI_AML_OT_OPREGION && sym->opregion.region_space == ACPI_AML_RESOURCE_ADDRESS_SPACE_ID_MEMORY) {
			// TODO: add reserved frames

			frame_t f = {sym->opregion.region_offset, (sym->opregion.region_len + FRAME_SIZE - 1) / FRAME_SIZE, FRAME_TYPE_RESERVED, 0};
			uint64_t fva = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(sym->opregion.region_offset);

			if(memory_paging_add_va_for_frame(fva, &f, MEMORY_PAGING_PAGE_TYPE_UNKNOWN) != 0) {
				iter->destroy(iter);
				PRINTLOG(ACPI, LOG_FATAL, "cannot allocate pages for system memory opregion", 0);
				return -1;
			}


		} else if(sym->type == ACPI_AML_OT_DEVICE || strcmp(sym->name, "\\_SB_") == 0) {
			acpi_aml_device_t* new_device = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_device_t), 0);
			new_device->name = sym->name;
			linkedlist_sortedlist_insert(ctx->devices, new_device);

			if(curr_device != NULL) {
				int64_t len_diff = strlen(sym->name) - strlen(curr_device->name);

				if(len_diff == 0) {
					new_device->parent = curr_device->parent;
				} else if(len_diff == 4) {
					new_device->parent = curr_device;
				} else {
					len_diff = ABS(len_diff) / 4;

					while(curr_device && len_diff--) {
						curr_device = curr_device->parent;
					}

					new_device->parent = curr_device;
				}
			}

			curr_device = new_device;
			item_count++;
		} else {
			if(curr_device != NULL) {
				int64_t len_diff = strlen(sym->name) - strlen(curr_device->name);
				boolean_t need_check = 0;

				if(len_diff < 0) {
					len_diff = ABS(len_diff) / 4;

					while(curr_device && len_diff--) {
						curr_device = curr_device->parent;
					}

					if(curr_device != NULL) {
						need_check = 1;
					}

				} else if(len_diff == 4) {
					need_check = 1;
				} else if(len_diff == 0) {
					curr_device = curr_device->parent;

					if(curr_device != NULL && strlen(sym->name) - strlen(curr_device->name) == 4) {
						need_check = 1;
					}
				}

				if(need_check && strstarts(sym->name, curr_device->name) == 0) {
					if(strends(sym->name, "_ADR") == 0) {
						curr_device->adr = sym;
					}

					if(strends(sym->name, "_CRS") == 0) {
						curr_device->crs = sym;
					}

					if(strends(sym->name, "_DIS") == 0) {
						curr_device->dis = sym;
					}

					if(strends(sym->name, "_HID") == 0) {
						curr_device->hid = sym;
					}

					if(strends(sym->name, "_INI") == 0) {
						curr_device->ini = sym;
					}

					if(strends(sym->name, "_PRS") == 0) {
						curr_device->prs = sym;
					}

					if(strends(sym->name, "_PRT") == 0) {
						curr_device->prt = sym;
					}

					if(strends(sym->name, "_SRS") == 0) {
						curr_device->srs = sym;
					}

					if(strends(sym->name, "_STA") == 0) {
						curr_device->sta = sym;
					}

					if(strends(sym->name, "_UID") == 0) {
						curr_device->uid = sym;
					}
				}
			}
		}

		iter = iter->next(iter);
	}

	iter->destroy(iter);

	return 0;
}

acpi_aml_device_t* acpi_device_lookup(acpi_aml_parser_context_t* ctx, char_t* dev_name) {
	iterator_t* iter = linkedlist_iterator_create(ctx->devices);
	acpi_aml_device_t* res = NULL;

	while(iter->end_of_iterator(iter) != 0) {
		acpi_aml_device_t* d = iter->get_item(iter);

		if(strlen(dev_name) == 4) {
			if(strcmp(d->name + strlen(d->name) - 4, dev_name) == 0) {
				res = d;
				break;
			}
		} else {
			if(strcmp(d->name, dev_name) == 0) {
				res = d;
				break;
			}
		}

		iter = iter->next(iter);
	}

	iter->destroy(iter);


	return res;
}

int8_t acpi_device_reserve_memory_ranges(acpi_aml_parser_context_t* ctx) {

	KERNEL_FRAME_ALLOCATOR->release_acpi_reclaim_memory(KERNEL_FRAME_ALLOCATOR);

	iterator_t* dev_iter = linkedlist_iterator_create(ctx->devices);


	while(dev_iter->end_of_iterator(dev_iter) != 0) {
		acpi_aml_device_t* d = dev_iter->get_item(dev_iter);

		if(d->memory_ranges) {
			PRINTLOG(ACPI, LOG_DEBUG, "device %s has memory ranges", d->name);

			iterator_t* mem_iter = linkedlist_iterator_create(d->memory_ranges);


			while(mem_iter->end_of_iterator(mem_iter) != 0) {
				acpi_aml_device_memory_range_t* mem = mem_iter->get_item(mem_iter);

				PRINTLOG(ACPI, LOG_DEBUG, "device %s memory range [0x%lx,0x%lx]", d->name, mem->min, mem->max);

				uint64_t frm_cnt = (mem->max - mem->min + 1 + FRAME_SIZE - 1) / FRAME_SIZE;

				frame_t f = {mem->min, frm_cnt, 0, 0};

				KERNEL_FRAME_ALLOCATOR->reserve_system_frames(KERNEL_FRAME_ALLOCATOR, &f);

				mem_iter = mem_iter->next(mem_iter);
			}

			mem_iter->destroy(mem_iter);

		}

		dev_iter = dev_iter->next(dev_iter);
	}

	dev_iter->destroy(dev_iter);

	return 0;
}

int8_t acpi_device_init(acpi_aml_parser_context_t* ctx) {
	iterator_t* iter = linkedlist_iterator_create(ctx->devices);

	int32_t err_cnt = 0;

	while(iter->end_of_iterator(iter) != 0) {
		acpi_aml_device_t* d = iter->get_item(iter);

		PRINTLOG(ACPI, LOG_DEBUG, "device %s controlling for init and crs", d->name);

		boolean_t need_ini = 1;
		int64_t sta_value = 0;

		if(d->sta) {
			PRINTLOG(ACPI, LOG_TRACE, "device %s has sta method, reading...", d->name);

			if(acpi_aml_read_as_integer(ctx, d->sta, &sta_value) != 0) {
				PRINTLOG(ACPI, LOG_ERROR, "Cannot read device status", 0);
				err_cnt += -1;
			} else {
				PRINTLOG(ACPI, LOG_DEBUG, "Device sta method succeed. sta value: 0x%lx", sta_value);
			}

			if((sta_value & 1) != 1) {
				need_ini = 0;
			}
		}

		if(need_ini && d->ini) {
			PRINTLOG(ACPI, LOG_TRACE, "device %s has ini method, executing...", d->name);

			int8_t res = acpi_aml_execute(ctx, d->ini, NULL);

			if(res != 0) {
				PRINTLOG(ACPI, LOG_ERROR, "Device init method failed", 0);
				acpi_aml_print_object(ctx, d->ini);
				err_cnt += -1;
			} else {
				PRINTLOG(ACPI, LOG_DEBUG, "device %s init method succeed", d->name);
			}
		}

		if(d->crs) {

			if(d->crs->type == ACPI_AML_OT_BUFFER) {
				PRINTLOG(ACPI, LOG_TRACE, "device %s has crs with buffer len %i, enumarating...", d->name, d->crs->buffer.buflen);
				acpi_aml_resource_parse(ctx, d, d->crs);
			} else if(d->crs->type == ACPI_AML_OT_METHOD) {
				PRINTLOG(ACPI, LOG_TRACE, "device %s has crs with method, executing...", d->name);
				acpi_aml_object_t* crs_res = NULL;

				if(acpi_aml_execute(ctx, d->crs, &crs_res) == 0 && crs_res != NULL) {
					PRINTLOG(ACPI, LOG_TRACE, "device %s has crs with %i, executing...", d->name, crs_res->type);

					if(crs_res->type == ACPI_AML_OT_OPCODE_EXEC_RETURN) {
						acpi_aml_object_t* tmp = crs_res->opcode_exec_return;
						acpi_aml_destroy_object(ctx, crs_res);
						crs_res = tmp;
					}

					err_cnt += acpi_aml_resource_parse(ctx, d, crs_res);

					if(crs_res->name == NULL) {
						acpi_aml_destroy_object(ctx, crs_res);
					}

				} else {
					PRINTLOG(ACPI, LOG_ERROR, "device %s crs execution faild crs_res is null? %i", d->name, crs_res == NULL);
					err_cnt += -1;
				}
			} else {
				PRINTLOG(ACPI, LOG_ERROR, "device %s has crs with unknown type %i", d->name, d->crs->type);
				err_cnt += -1;
			}

		}

		iter = iter->next(iter);
	}

	iter->destroy(iter);

	if(err_cnt < 0) {
		PRINTLOG(ACPI, LOG_ERROR, "some devices have failed resource %i", err_cnt);
	} else if(err_cnt == 0) {
		PRINTLOG(ACPI, LOG_DEBUG, "all devices have succeed", 0);
	} else {
		PRINTLOG(ACPI, LOG_FATAL, "unexpected error state %i", err_cnt);
	}

	return err_cnt;
}

void acpi_device_print_all(acpi_aml_parser_context_t* ctx) {
	uint64_t item_count = 0;
	iterator_t* iter = linkedlist_iterator_create(ctx->devices);

	while(iter->end_of_iterator(iter) != 0) {
		acpi_aml_device_t* d = iter->get_item(iter);

		acpi_device_print(ctx, d);
		item_count++;
		iter = iter->next(iter);
	}

	iter->destroy(iter);

	printf("totoal devices %i\n", item_count );
}

void acpi_device_print(acpi_aml_parser_context_t* ctx, acpi_aml_device_t* d) {
	printf("device name %s ", d->name);

	if(d->parent) {
		printf("parent %s", d->parent->name);
	}

	printf("\n");

	if(d->adr) {
		acpi_aml_print_object(ctx, d->adr);
	}

	if(d->crs) {
		acpi_aml_print_object(ctx, d->crs);
	}

	if(d->dis) {
		acpi_aml_print_object(ctx, d->dis);
	}

	if(d->hid) {
		acpi_aml_print_object(ctx, d->hid);
	}

	if(d->ini) {
		acpi_aml_print_object(ctx, d->ini);
	}

	if(d->prs) {
		acpi_aml_print_object(ctx, d->prs);
	}

	if(d->prt) {
		acpi_aml_print_object(ctx, d->prt);
	}

	if(d->srs) {
		acpi_aml_print_object(ctx, d->srs);
	}

	if(d->sta) {
		acpi_aml_print_object(ctx, d->sta);
	}

	if(d->uid) {
		acpi_aml_print_object(ctx, d->uid);
	}

}

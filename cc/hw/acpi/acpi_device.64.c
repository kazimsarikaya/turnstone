/**
 * @file acpi_device.64.c
 * @brief acpi device utils
 */
#include <acpi/aml_internal.h>
#include <strings.h>
#include <video.h>
#include <utils.h>
#include <memory/frame.h>
#include <memory/paging.h>


int8_t acpi_device_build(acpi_aml_parser_context_t* ctx) {
	uint64_t item_count = 0;

	ctx->devices = linkedlist_create_sortedlist_with_heap(ctx->heap, acpi_aml_device_name_comparator);
	acpi_aml_device_t* curr_device = NULL;


	iterator_t* iter = linkedlist_iterator_create(ctx->symbols);
	while(iter->end_of_iterator(iter) != 0) {
		acpi_aml_object_t* sym = iter->get_item(iter);

		if(sym == NULL && sym->name == NULL) {
			iter->destroy(iter);
			PRINTLOG("ACPI", "FATAL", "NULL object at symbol table or no name", 0);
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
				PRINTLOG("ACPI", "FATAL", "cannot allocate pages for system memory opregion", 0);
				return -1;
			}


		} else if(sym->type == ACPI_AML_OT_DEVICE) {
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

		if(strcmp(d->name, dev_name) == 0) {
			res = d;
			break;
		}

		iter = iter->next(iter);
	}

	iter->destroy(iter);


	return res;
}

int8_t acpi_device_init(acpi_aml_parser_context_t* ctx) {
	iterator_t* iter = linkedlist_iterator_create(ctx->devices);
	int8_t res = 0;
	while(iter->end_of_iterator(iter) != 0) {
		acpi_aml_device_t* d = iter->get_item(iter);


		boolean_t need_ini = 1;
		int64_t sta_value = 0;

		if(d->sta) {
			if(acpi_aml_read_as_integer(ctx, d->sta, &sta_value) != 0) {
				PRINTLOG("ACPI", "ERROR", "Cannot read device status", 0);
				acpi_aml_print_object(ctx, d->sta);
				res = -1;

				break;
			}

			if((sta_value & 1) != 1) {
				need_ini = 0;
			}
		}

		if(need_ini && d->ini) {
			res = acpi_aml_execute(ctx, d->ini, NULL);

			if(res != 0) {
				PRINTLOG("ACPI", "ERROR", "Device init method failed", 0);
				acpi_aml_print_object(ctx, d->ini);
				res = -1;

				break;
			}
		}

		iter = iter->next(iter);
	}

	iter->destroy(iter);


	return res;
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

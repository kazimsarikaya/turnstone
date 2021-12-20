#include <efi.h>
#include <disk.h>
#include <crc.h>
#include <strings.h>
#include <memory.h>
#include <random.h>


int8_t efi_create_guid(efi_guid_t* guid){
	guid->time_low = rand();
	guid->time_mid = rand();
	guid->time_high_version = rand();

	for(int32_t i = 0; i < 8; i++) {
		guid->data[i] = rand();
	}

	return 0;
}


int8_t efi_guid_equal(efi_guid_t guid1, efi_guid_t guid2) {
	if(guid1.time_low == guid2.time_low && guid1.time_mid == guid2.time_mid &&  guid1.time_high_version == guid2.time_high_version) {
		for(int64_t i = 0; i < 8; i++) {
			if(guid1.data[i] != guid2.data[i]) {
				return -1;
			}
		}
		return 0;
	}

	return -1;
}



int8_t gpt_disk_close(disk_t* d) {

	gpt_disk_t* gd = (gpt_disk_t*)d;

	gd->underlaying_disk_pointer->close(gd->underlaying_disk_pointer);

	memory_free(gd->gpt_header);
	memory_free(gd->partitions);

	memory_free(gd);

	return 0;
}

int8_t gpt_write_gpt_metadata(disk_t* d) {
	gpt_disk_t* gd = (gpt_disk_t*)d;

	uint64_t size = d->get_disk_size(d);
	int64_t gpt_parts_size = sizeof(efi_partition_entry_t) * 128;
	int64_t max_lba = size / 512 - 1;
	uint64_t gpt_parts_block_count = gpt_parts_size / 512;

	int32_t pes_crc32 = CRC32_SEED;
	pes_crc32 = crc32_sum((uint8_t*)gd->partitions, gpt_parts_size, pes_crc32);
	pes_crc32 ^= CRC32_SEED;

	efi_partition_table_header_t* gpt =   (efi_partition_table_header_t* )gd->gpt_header;

	gpt->partition_entries_crc32 = pes_crc32;

	gpt->header.crc32 = 0;
	int32_t gpt_crc32 = CRC32_SEED;
	gpt_crc32 = crc32_sum((uint8_t*)gpt, sizeof(efi_partition_table_header_t), gpt_crc32);
	gpt_crc32 ^= CRC32_SEED;
	gpt->header.crc32 = gpt_crc32;

	d->write(d, 1, 1, (uint8_t*)gd->gpt_header);

	d->write(d, 2, gpt_parts_block_count, (uint8_t*)gd->partitions);
	d->write(d, gpt->last_usable_lba + 1, gpt_parts_block_count, (uint8_t*)gd->partitions);

	gpt->partition_entry_lba = gpt->last_usable_lba + 1;
	gpt->my_lba = max_lba;
	gpt->alternate_lba = 1;

	gpt->header.crc32 = 0;
	gpt_crc32 = CRC32_SEED;
	gpt_crc32 = crc32_sum((uint8_t*)gpt, sizeof(efi_partition_table_header_t), gpt_crc32);
	gpt_crc32 ^= CRC32_SEED;
	gpt->header.crc32 = gpt_crc32;

	d->write(d, max_lba, 1, (uint8_t*)gd->gpt_header);

	memory_free(gd->gpt_header);

	d->read(d, 1, 1, (uint8_t**)&gd->gpt_header);

	return 0;
}

int8_t gpt_check_and_format_if_need(disk_t* d) {
	gpt_disk_t* gd = (gpt_disk_t*)d;


	uint64_t size = d->get_disk_size(d);
	uint32_t max_sectors_tmp = -1;
	uint64_t max_sectors = max_sectors_tmp;
	uint64_t sector_count = size / 512;

	int64_t gpt_parts_size = sizeof(efi_partition_entry_t) * 128;
	int64_t max_lba = size / 512 - 1;
	uint64_t gpt_parts_block_count = gpt_parts_size / 512;

	uint8_t* data;

	d->read(d, 0, 1, &data);

	efi_pmbr_partition_t* pmbr_part = (efi_pmbr_partition_t*)(data + 0x1be);

	if(pmbr_part->part_type == EFI_PMBR_PART_TYPE) {
		memory_free(data);

		d->read(d, 1, 1, &data);

		efi_partition_table_header_t* gpt_table = (efi_partition_table_header_t*)data;

		if(gpt_table->header.signature == EFI_PART_TABLE_HEADER_SIGNATURE) {
			memory_free(data);

			d->read(d, 1, 1, (uint8_t**)&gd->gpt_header);

			d->read(d, 2, gpt_parts_block_count, (uint8_t**)&gd->partitions);

			return 0;
		}

		memory_free(data);
	} else {
		memory_free(data);
	}


	data = memory_malloc(512);

	pmbr_part = (efi_pmbr_partition_t*)(data + 0x1be);
	pmbr_part->first_chs.sector = 2;
	pmbr_part->part_type = EFI_PMBR_PART_TYPE;
	pmbr_part->last_chs.head = 0xFF;
	pmbr_part->last_chs.sector = 0xFF;
	pmbr_part->last_chs.cylinder = 0xFF;
	pmbr_part->first_lba = 1;
	if(sector_count > max_sectors) {
		pmbr_part->sector_count = max_sectors_tmp;
	} else {
		pmbr_part->sector_count = sector_count;
	}

	data[510] = 0x55;
	data[511] = 0xAA;

	d->write(d, 0, 1, data);

	memory_free(data);

	gd->gpt_header = memory_malloc(512);
	gd->partitions = memory_malloc(gpt_parts_size);


	efi_partition_table_header_t* gpt =   (efi_partition_table_header_t* )gd->gpt_header;

	gpt->header.signature = EFI_PART_TABLE_HEADER_SIGNATURE;
	gpt->header.revision = EFI_PART_TABLE_HEADER_REVISION;
	gpt->header.header_size = sizeof(efi_partition_table_header_t);

	gpt->my_lba = 1;
	gpt->alternate_lba = max_lba;
	gpt->first_usable_lba = gpt->my_lba + gpt_parts_size / 512 + 1;
	gpt->last_usable_lba =  gpt->alternate_lba - gpt_parts_size / 512 - 1;
	efi_create_guid(&gpt->disk_guid);
	gpt->partition_entry_lba = 2;
	gpt->partition_entry_count = 128;
	gpt->partition_entry_size = sizeof(efi_partition_entry_t);

	gpt_write_gpt_metadata(d);

	return 0;
}

uint8_t gpt_add_partition(disk_t* d, disk_partition_context_t* part_ctx){

	efi_partition_entry_t* ic = ( efi_partition_entry_t*)part_ctx->internal_context;

	gpt_disk_t* gd = (gpt_disk_t*)d;

	efi_guid_t unused = EFI_PART_TYPE_UNUSED_GUID;

	for(uint32_t i = 0; i < gd->gpt_header->partition_entry_count; i++) {
		if(efi_guid_equal(unused, gd->partitions[i].partition_type_guid) == 0) {
			memory_memcopy(ic, gd->partitions + i, sizeof(efi_partition_entry_t));

			gpt_write_gpt_metadata(d);
			return (uint8_t)i;
		}
	}

	return -1;
}

int8_t gpt_del_partition(disk_t* d, uint8_t partno){

	if(partno > 127) {
		return -1;
	}

	gpt_disk_t* gd = (gpt_disk_t*)d;

	memory_memclean(gd->partitions + partno, sizeof(efi_partition_entry_t));

	gpt_write_gpt_metadata(d);

	return 0;
}

disk_partition_context_t*  gpt_get_partition(disk_t* d, uint8_t partno) {
	if(partno > 127) {
		return NULL;
	}

	gpt_disk_t* gd = (gpt_disk_t*)d;

	disk_partition_context_t* res = memory_malloc(sizeof(disk_partition_context_t));

	res->internal_context = &gd->partitions[partno];
	res->start_lba = gd->partitions[partno].starting_lba;
	res->end_lba = gd->partitions[partno].ending_lba;

	return res;
}

iterator_t* gpt_get_partitions(disk_t* d){
	UNUSED(d);
	return NULL;
}

disk_partition_context_t* gpt_create_partition_context(efi_guid_t* type, char_t* name, uint64_t start, uint64_t end){
	disk_partition_context_t* res = memory_malloc(sizeof(disk_partition_context_t));

	efi_partition_entry_t* ic = memory_malloc(sizeof(efi_partition_entry_t));

	memory_memcopy(type, &ic->partition_type_guid, sizeof(efi_guid_t));

	efi_create_guid(&ic->unique_partition_guid);

	ic->starting_lba = start;
	ic->ending_lba = end;

	wchar_t* pname = char_to_wchar(name);
	memory_memcopy(pname, ic->partition_name, strlen(name) * sizeof(wchar_t));
	memory_free(pname);

	res->internal_context = ic;
	res->start_lba = ic->starting_lba;
	res->end_lba = ic->ending_lba;

	return res;
}

disk_t* gpt_get_or_create_gpt_disk(disk_t* underlaying_disk){
	gpt_disk_t* gd = memory_malloc(sizeof(gpt_disk_t));

	memory_memcopy(underlaying_disk, gd, sizeof(disk_t));

	gd->underlaying_disk.close = gpt_disk_close;
	gd->underlaying_disk.add_partition = gpt_add_partition;
	gd->underlaying_disk.del_partition = gpt_del_partition;
	gd->underlaying_disk.get_partition = gpt_get_partition;
	gd->underlaying_disk.get_partitions = gpt_get_partitions;

	gd->underlaying_disk_pointer = underlaying_disk;

	disk_t* d = (disk_t*)gd;

	gpt_check_and_format_if_need(d);

	return d;
}

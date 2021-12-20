#ifndef ___DISK_H
#define ___DISK_H 0

#include <types.h>
#include <iterator.h>

typedef void*  disk_context_t;
typedef struct {
	void* internal_context;
	uint64_t start_lba;
	uint64_t end_lba;
}disk_partition_context_t;

typedef struct disk {
	disk_context_t disk_context;
	uint64_t (* get_disk_size)(struct disk* d);
	int8_t (* write)(struct disk* d, uint64_t lba, uint64_t count, uint8_t* data);
	int8_t (* read)(struct disk* d, uint64_t lba, uint64_t count, uint8_t** data);
	int8_t (* close)(struct disk* d);
	uint8_t (* add_partition)(struct disk* d, disk_partition_context_t* part_ctx);
	int8_t (* del_partition)(struct disk* d, uint8_t partno);
	disk_partition_context_t*  (* get_partition)(struct disk* d, uint8_t partno);
	iterator_t* (* get_partitions)(struct disk* d);
} disk_t;

#endif

#ifndef ___FS_H
#define ___FS_H 0

#include <types.h>
#include <disk.h>
#include <iterator.h>
#include <time.h>

typedef enum {
	FS_STAT_TYPE_NULL,
	FS_STAT_TYPE_FILE,
	FS_STAT_TYPE_DIR
} fs_stat_type_t;

typedef struct {
	fs_stat_type_t type;
} fs_stat_t;

#define PATH_DELIMETER_STR "/"
#define PATH_DELIMETER_CHR '/'

typedef void* path_context_t;

typedef struct path {
	path_context_t context;
	char_t* (* get_fullpath)(struct path* self);
	char_t* (* get_name)(struct path* self);
	char_t* (* get_extension)(struct path* self);
	char_t* (* get_name_and_ext)(struct path* self);
	struct path* (* get_basepath)(struct path* self);
	int8_t (* is_root)(struct path* self);
	int8_t (* is_directory)(struct path* self);
	struct path* (* append)(struct path* self, struct path* child);
	int8_t (* close)(struct path* self);
} path_t;

typedef enum {
	FILE_SEEK_TYPE_SET,
	FILE_SEEK_TYPE_CUR,
	FILE_SEEK_TYPE_END,
} file_seek_type_t;

typedef void* file_context_t;

typedef struct file {
	file_context_t context;
	path_t* (* get_path)(struct file* self);
	fs_stat_type_t (* get_type)(struct file* self);
	time_t (* get_create_time)(struct file* self);
	time_t (* get_last_access_time)(struct file* self);
	time_t (* get_last_modification_time)(struct file* self);
	int8_t (* close)(struct file* self);
	int64_t (* write)(struct file* self, uint8_t* buf, int64_t buflen);
	int64_t (* read)(struct file* self, uint8_t* buf, int64_t buflen);
	int64_t (* seek)(struct file* self, int64_t pos, file_seek_type_t st);
	int64_t (* tell)(struct file* self);
	int8_t (* flush)(struct file* self);
}file_t;

#define DIRECTORY_CURRENT_DIR  "."
#define DIRECTORY_PARENT_DIR   ".."

typedef void* directory_context_t;

typedef struct directory {
	directory_context_t context;
	path_t* (* get_path)(struct directory* self);
	fs_stat_type_t (* get_type)(struct directory* self);
	time_t (* get_create_time)(struct directory* self);
	time_t (* get_last_access_time)(struct directory* self);
	time_t (* get_last_modification_time)(struct directory* self);
	int8_t (* close)(struct directory* self);
	iterator_t* (* list)(struct directory* self);
	struct directory* (* create_or_open_directory)(struct directory* self, path_t* p);
	file_t* (* create_or_open_file)(struct directory* self, path_t* p);
} directory_t;

typedef struct path_interface {
	void* context;
	path_t* (* get_path)(struct path_interface* self);
	fs_stat_type_t (* get_type)(struct path_interface* self);
	time_t (* get_create_time)(struct path_interface* self);
	time_t (* get_last_access_time)(struct path_interface* self);
	time_t (* get_last_modification_time)(struct path_interface* self);
	int8_t (* close)(struct path_interface* self);
}path_interface_t;

typedef void* filesystem_context_t;

typedef struct filesystem {
	filesystem_context_t context;
	directory_t* (* get_root_directory)(struct filesystem* self);
	uint64_t (* get_total_size)(struct filesystem* self);
	uint64_t (* get_free_size)(struct filesystem* self);
	directory_t* (* create_or_open_directory)(struct filesystem* self, path_t* p);
	file_t* (* create_or_open_file)(struct filesystem* self, path_t* p);
	fs_stat_t* (* stat)(struct filesystem* self, path_t* p);
	int8_t (* remove)(struct filesystem* self, path_t* p);
	int8_t (* close)(struct filesystem* self);
}filesystem_t;

path_t* filesystem_new_path(filesystem_t* fs, char_t* path_string);
path_interface_t* filesystem_empty_path_interface(filesystem_t* fs, path_t* p, fs_stat_type_t type);

#endif

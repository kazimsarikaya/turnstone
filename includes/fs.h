/**
 * @file fs.h
 * @brief Filesystem header.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___FS_H
#define ___FS_H 0

#include <types.h>
#include <disk.h>
#include <iterator.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum fs_stat_type_t {
    FS_STAT_TYPE_NULL,
    FS_STAT_TYPE_FILE,
    FS_STAT_TYPE_DIR
} fs_stat_type_t;

typedef struct fs_stat_t {
    fs_stat_type_t type;
} fs_stat_t;

#define PATH_DELIMETER_STR "/"
#define PATH_DELIMETER_CHR '/'

typedef struct path_context_t path_context_t;

typedef struct path_t {
    path_context_t* context;
    char_t* (* get_fullpath)(const struct path_t* self);
    char_t* (* get_name)(const struct path_t* self);
    char_t* (* get_extension)(const struct path_t* self);
    char_t* (* get_name_and_ext)(const struct path_t* self);
    struct path_t* (* get_basepath)(const struct path_t* self);
    int8_t (* is_root)(const struct path_t* self);
    int8_t (* is_directory)(const struct path_t* self);
    struct path_t* (* append)(const struct path_t* self, const struct path_t* child);
    int8_t (* close)(const struct path_t* self);
} path_t;

typedef enum file_seek_type_t {
    FILE_SEEK_TYPE_SET,
    FILE_SEEK_TYPE_CUR,
    FILE_SEEK_TYPE_END,
} file_seek_type_t;

typedef struct file_context_t file_context_t;

typedef struct file_t {
    file_context_t* context;
    const path_t* (* get_path)(const struct file_t* self);
    fs_stat_type_t (* get_type)(struct file_t* self);
    time_t (* get_create_time)(struct file_t* self);
    time_t (* get_last_access_time)(struct file_t* self);
    time_t (* get_last_modification_time)(struct file_t* self);
    int8_t (* close)(struct file_t* self);
    int64_t (* write)(struct file_t* self, uint8_t* buf, int64_t buflen);
    int64_t (* read)(struct file_t* self, uint8_t* buf, int64_t buflen);
    int64_t (* seek)(struct file_t* self, int64_t pos, file_seek_type_t st);
    int64_t (* tell)(struct file_t* self);
    int8_t (* flush)(struct file_t* self);
}file_t;

#define DIRECTORY_CURRENT_DIR  "."
#define DIRECTORY_PARENT_DIR   ".."

typedef struct directory_context_t directory_context_t;

typedef struct directory_t {
    directory_context_t* context;
    const path_t* (* get_path)(const struct directory_t* self);
    fs_stat_type_t (* get_type)(struct directory_t* self);
    time_t (* get_create_time)(struct directory_t* self);
    time_t (* get_last_access_time)(struct directory_t* self);
    time_t (* get_last_modification_time)(struct directory_t* self);
    int8_t (* close)(struct directory_t* self);
    iterator_t* (* list)(struct directory_t* self);
    struct directory_t* (* create_or_open_directory)(struct directory_t* self, const path_t* p);
    file_t* (* create_or_open_file)(struct directory_t* self, const path_t* p);
} directory_t;

typedef struct path_interface_context_t path_interface_context_t;

typedef struct path_interface_t {
    path_interface_context_t* context;
    const path_t* (* get_path)(const struct path_interface_t* self);
    fs_stat_type_t (* get_type)(const struct path_interface_t* self);
    time_t (* get_create_time)(const struct path_interface_t* self);
    time_t (* get_last_access_time)(const struct path_interface_t* self);
    time_t (* get_last_modification_time)(const struct path_interface_t* self);
    int8_t (* close)(const struct path_interface_t* self);
}path_interface_t;

typedef struct filesystem_context_t filesystem_context_t;

typedef struct filesystem_t {
    filesystem_context_t* context;
    directory_t* (* get_root_directory)(struct filesystem_t* self);
    uint64_t (* get_total_size)(struct filesystem_t* self);
    uint64_t (* get_free_size)(struct filesystem_t* self);
    directory_t* (* create_or_open_directory)(struct filesystem_t* self, const path_t* p);
    file_t* (* create_or_open_file)(struct filesystem_t* self, const path_t* p);
    fs_stat_t* (* stat)(struct filesystem_t* self, const path_t* p);
    int8_t (* remove)(struct filesystem_t* self, const path_t* p);
    int8_t (* close)(struct filesystem_t* self);
}filesystem_t;

path_t*           filesystem_new_path(filesystem_t* fs, const char_t* path_string);
path_interface_t* filesystem_empty_path_interface(filesystem_t* fs, path_t* p, fs_stat_type_t type);

#ifdef __cplusplus
}
#endif

#endif

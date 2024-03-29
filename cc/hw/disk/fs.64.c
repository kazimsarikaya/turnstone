/**
 * @file fs.64.c
 * @brief Filesystem and path implementation.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <fs.h>
#include <memory.h>
#include <strings.h>

MODULE("turnstone.kernel.hw.disk.fs");


typedef struct path_context_t {
    char_t*       path_string;
    char_t**      parts;
    int64_t       part_count;
    int64_t*      part_lengths;
    char_t*       name;
    char_t*       ext;
    filesystem_t* fs;
} path_context_t;

typedef struct empty_path_context_t {
    filesystem_t*  fs;
    path_t*        path;
    fs_stat_type_t type;
} empty_path_context_t;

char_t*        fs_path_get_fullpath(const path_t* self);
char_t*        fs_path_get_name(const path_t* self);
char_t*        fs_path_get_extension(const path_t* self);
char_t*        fs_path_get_name_and_ext(const path_t* self);
path_t*        fs_path_get_basepath(const path_t* self);
int8_t         fs_path_is_root(const path_t* self);
int8_t         fs_path_is_directory(const path_t* self);
int8_t         fs_path_close(const path_t* self);
path_t*        fs_path_append(const path_t* self, const path_t* child);
directory_t*   fs_create_or_open_directory(filesystem_t* self, const path_t* p);
file_t*        fs_create_or_open_file(filesystem_t* self, const path_t* p);
const path_t*  fs_epi_get_path(const path_interface_t* self);
fs_stat_type_t fs_epi_get_type(const path_interface_t* self);
int8_t         fs_epi_close(const path_interface_t* self);


char_t* fs_path_get_fullpath(const path_t* self) {
    path_context_t* ctx = (path_context_t*)self->context;

    return ctx->path_string;
}

char_t* fs_path_get_name(const path_t* self) {
    path_context_t* ctx = (path_context_t*)self->context;

    return ctx->name;
}

char_t* fs_path_get_extension(const path_t* self) {
    path_context_t* ctx = (path_context_t*)self->context;

    return ctx->ext;
}

char_t* fs_path_get_name_and_ext(const path_t* self) {
    path_context_t* ctx = (path_context_t*)self->context;

    if(self->is_root(self) == 0) {
        return ctx->path_string;
    }

    char_t* last_part = ctx->parts[ctx->part_count - 1];

    return last_part;
}

path_t* fs_path_get_basepath(const path_t* self) {
    path_context_t* ctx = (path_context_t*)self->context;

    if(self->is_root(self) == 0) {
        return NULL;
    }

    char_t* max_path = memory_malloc(strlen(ctx->path_string));

    if(max_path == NULL) {
        return NULL;
    }

    int16_t idx = 0;

    for(int64_t i = 0; i < ctx->part_count - 1; i++) {
        if(ctx->part_lengths[i] != 0) {
            max_path[idx] = PATH_DELIMETER_CHR;
            idx++;
            memory_memcopy(ctx->parts[i], max_path + idx, ctx->part_lengths[i]);
            idx += ctx->part_lengths[i];
        }

    }

    if(strlen(max_path) == 0) {
        max_path[0] = PATH_DELIMETER_CHR;
    }

    path_t* p = filesystem_new_path(ctx->fs, max_path);

    memory_free(max_path);

    return p;
}

int8_t fs_path_is_root(const path_t* self) {
    path_context_t* ctx = (path_context_t*)self->context;

    return strcmp(PATH_DELIMETER_STR, ctx->path_string) == 0?0:-1;
}

int8_t fs_path_is_directory(const path_t* self) {
    path_context_t* ctx = (path_context_t*)self->context;

    fs_stat_t* stat = ctx->fs->stat(ctx->fs, self);

    int8_t res = (stat->type == FS_STAT_TYPE_DIR?0:-1);

    memory_free(stat);

    return res;
}

int8_t fs_path_close(const path_t* self) {
    path_context_t* ctx = (path_context_t*)self->context;

    memory_free(ctx->path_string);
    memory_free(ctx->parts);
    memory_free(ctx->part_lengths);
    memory_free(ctx->name);
    memory_free(ctx->ext);
    memory_free(ctx);
    memory_free((void*)self);

    return 0;
}


path_t* fs_path_append(const path_t* self, const path_t* child) {
    path_context_t* ctx = (path_context_t*)self->context;

    char_t* tmp = NULL;

    if(self->is_root(self) == 0) {
        tmp = strdup(PATH_DELIMETER_STR);
    } else {
        tmp = strcat(self->get_fullpath(self), PATH_DELIMETER_STR);
    }


    char_t* new_path_str = strcat(tmp, child->get_fullpath(child));

    path_t* p = filesystem_new_path(ctx->fs, new_path_str);

    memory_free(tmp);
    memory_free(new_path_str);

    return p;
}

path_t* filesystem_new_path(filesystem_t* fs, const char_t* path_string) {
    path_context_t* ctx = memory_malloc(sizeof(path_context_t));

    if(ctx == NULL) {
        return NULL;
    }

    ctx->path_string = strdup(path_string);
    ctx->parts = strsplit(ctx->path_string, PATH_DELIMETER_CHR, &ctx->part_lengths, &ctx->part_count);

    ctx->fs = fs;

    path_t* p = memory_malloc(sizeof(path_t));

    if(p == NULL) {
        memory_free(ctx);

        return NULL;
    }

    p->context = ctx;

    p->get_fullpath = fs_path_get_fullpath;
    p->get_name = fs_path_get_name;
    p->get_extension = fs_path_get_extension;
    p->get_name_and_ext = fs_path_get_name_and_ext;
    p->get_basepath = fs_path_get_basepath;
    p->is_root = fs_path_is_root;
    p->is_directory = fs_path_is_directory;
    p->append = fs_path_append;
    p->close = fs_path_close;


    if(p->is_root(p) == 0) {
        ctx->name = strdup(PATH_DELIMETER_STR);
        ctx->ext = NULL;
    } else if(strcmp(path_string, DIRECTORY_CURRENT_DIR) == 0 || strcmp(path_string, DIRECTORY_PARENT_DIR) == 0) {
        ctx->name = strdup(path_string);
    }else{
        char_t* last_part = ctx->parts[ctx->part_count - 1];

        int64_t name_part_count;
        int64_t* name_part_lengths;

        char_t** name_parts = strsplit(last_part, '.', &name_part_lengths, &name_part_count);

        ctx->name = strndup(name_parts[0], name_part_lengths[0]);


        if(name_part_count > 1) {
            ctx->ext = strdup(name_parts[name_part_count - 1]);
        }

        memory_free(name_parts);
        memory_free(name_part_lengths);
    }

    return p;
}

directory_t* fs_create_or_open_directory(filesystem_t* self, const path_t* p) {

    directory_t* root_dir = self->get_root_directory(self);

    directory_t* d = root_dir->create_or_open_directory(root_dir, p);

    root_dir->close(root_dir);

    return d;
}

file_t* fs_create_or_open_file(filesystem_t* self, const path_t* p) {

    directory_t* root_dir = self->get_root_directory(self);

    file_t* f = root_dir->create_or_open_file(root_dir, p);

    root_dir->close(root_dir);

    return f;
}

const path_t* fs_epi_get_path(const path_interface_t* self) {
    empty_path_context_t* ctx = (empty_path_context_t*)self->context;

    return ctx->path;
}

fs_stat_type_t fs_epi_get_type(const path_interface_t* self) {
    empty_path_context_t* ctx = (empty_path_context_t*)self->context;

    return ctx->type;
}

int8_t fs_epi_close(const path_interface_t* self) {
    empty_path_context_t* ctx = (empty_path_context_t*)self->context;
    ctx->path->close(ctx->path);
    memory_free(self->context);
    memory_free((void*)self);

    return 0;
}

path_interface_t* filesystem_empty_path_interface(filesystem_t* fs, path_t* p, fs_stat_type_t type) {
    empty_path_context_t* ctx = memory_malloc(sizeof(empty_path_context_t));

    if(ctx == NULL) {
        return NULL;
    }

    ctx->path = p;
    ctx->type = type;
    ctx->fs = fs;

    path_interface_t* pi = memory_malloc(sizeof(path_interface_t));

    if(pi == NULL) {
        memory_free(ctx);

        return NULL;
    }

    pi->context = (path_interface_context_t*)ctx;
    pi->get_path = fs_epi_get_path;
    pi->get_type = fs_epi_get_type;
    pi->close = fs_epi_close;

    return pi;
}

#include <fs.h>
#include <fat.h>
#include <memory.h>
#include <time.h>
#include <random.h>
#include <linkedlist.h>
#include <strings.h>
#include <utils.h>

file_t* fat32_new_file(filesystem_t* fs, directory_t* dir, uint32_t dirent_idx, uint32_t clusterno, int64_t size, path_t* p, time_t ct, time_t lat, time_t lmt);
directory_t* fat32_new_directory(filesystem_t* fs, uint32_t clusterno, path_t* p, time_t ct, time_t lat, time_t lmt);

path_interface_t* fat32_directory_or_file_create(directory_t* parent, path_t* child, fs_stat_type_t type);


uint64_t fat32_get_absulute_lba_from_clusterno(filesystem_t* fs, uint32_t clusterno);
uint32_t fat32_count_cluster(filesystem_t* fs, uint32_t clusterno);
uint8_t fat32_long_filename_checksum(char_t* file_name);
directory_t* fat32_create_or_open_directory(directory_t* self, path_t* p);
file_t* fat32_create_or_open_file(directory_t* self, path_t* p);
path_interface_t* fat32_create_or_open_directory_or_file(directory_t* self, path_t* p, fs_stat_type_t type);
int8_t fat32_write_cluster_data(filesystem_t* fs);

directory_t* fs_create_or_open_directory(filesystem_t* self, path_t* p);
file_t* fs_create_or_open_file(filesystem_t* self, path_t* p);

fat32_dirent_shortname_t* fat32_gen_dirents(path_t* p, fs_stat_type_t type, timeparsed_t* tp, int32_t* ent_cnt);

uint32_t fat32_get_empty_cluster(filesystem_t* fs);
int8_t  fat32_directory_write(directory_t* self, time_t mt);

typedef struct {
	filesystem_t* fs;
	uint32_t clusterno;
	path_t* file_path;
	time_t create_time;
	time_t last_accessed_time;
	time_t last_modification_time;
	directory_t* dir;
	uint32_t dirent_idx;
	int64_t current_position;
	int64_t size;
} fat32_fs_file_context_t;

typedef struct {
	filesystem_t* fs;
	uint32_t clusterno;
	path_t* directory_path;
	time_t create_time;
	time_t last_accessed_time;
	time_t last_modification_time;
	fat32_dirent_shortname_t* dirents;
	int64_t dirent_count;
} fat32_fs_directory_context_t;


typedef struct {
	disk_t* disk;
	uint8_t partno;
	uint64_t fat_part_start_lba;
	uint64_t data_start_lba;
	fat32_bpb_t* bpb;
	fat32_fsinfo_t* fsinfo;
	uint32_t* table;
} fat32_fs_context_t;

typedef struct {
	uint32_t clusterno;
	int64_t size;
	time_t ct;
	time_t lat;
	time_t lmt;
	uint32_t dirent_idx;
}fat32_dir_list_iter_extradata_t;

typedef struct {
	directory_t* dir;
	uint32_t current_idx;
	fat32_dir_list_iter_extradata_t extra_data;
} fat32_dir_list_iter_metadata_t;


uint32_t fat32_cluster_count(filesystem_t* fs, uint32_t clusterno) {
	fat32_fs_context_t* fs_ctx = (fat32_fs_context_t*)fs->context;

	uint64_t res = 0;

	if(fs_ctx->table[clusterno] == 0) {
		return res;
	}

	res = 1;

	while(fs_ctx->table[clusterno] < FAT32_CLUSTER_BAD) {
		res++;
		clusterno = fs_ctx->table[clusterno];
	}

	return res;
}

fat32_dirent_shortname_t* fat32_gen_dirents(path_t* p, fs_stat_type_t type, timeparsed_t* tp, int32_t* ent_cnt) {
	boolean_t need_ln_ent = 0;
	*ent_cnt = 1;

	fat32_dirent_shortname_t* res = NULL;

	if(str_is_upper(p->get_name(p)) != 0 || str_is_upper(p->get_extension(p)) != 0) {
		need_ln_ent = 1;
	}

	uint32_t path_len = strlen(p->get_name(p));

	if(strlen(p->get_extension(p))) {
		path_len += 1 + strlen(p->get_extension(p));
	}


	if(!need_ln_ent && strlen(p->get_name(p)) > 8) {
		need_ln_ent = 1;
	}

	if(!need_ln_ent && strlen(p->get_extension(p)) > 3) {
		need_ln_ent = 1;
	}

	if(!need_ln_ent && path_len > 11) {
		need_ln_ent = 1;
	}

	if(need_ln_ent) {
		int32_t lne_cnt = path_len / 13;

		if(path_len % 13) {
			lne_cnt++;
		}

		(*ent_cnt) += lne_cnt;

		res = memory_malloc(sizeof(fat32_dirent_shortname_t) * (*ent_cnt));

		// TODO: implement lfn

	} else {
		res = memory_malloc(sizeof(fat32_dirent_shortname_t) * (*ent_cnt));
	}

	int32_t last_idx = (*ent_cnt) - 1;

	memory_memset(&res[last_idx].name, ' ', 11);

	char_t* name = strndup(p->get_name(p), 8);

	if(need_ln_ent) {
		name[6] = '~';
		name[7] = '1';
	}

	memory_memcopy(name, &res[last_idx].name, strlen(name));
	memory_free(name);

	char_t* ext = strndup(p->get_extension(p), 3);

	memory_memcopy(ext, &res[last_idx].name[8], strlen(ext));
	memory_free(ext);

	res[last_idx].create_time.hours = tp->hours;
	res[last_idx].create_time.minutes = tp->minutes;
	res[last_idx].create_time.seconds = tp->seconds / 2;
	res[last_idx].create_date.year = tp->year - FAT32_YEAR_START;
	res[last_idx].create_date.month = tp->month;
	res[last_idx].create_date.day = tp->day;


	res[last_idx].last_accessed_date.year = tp->year - FAT32_YEAR_START;
	res[last_idx].last_accessed_date.month = tp->month;
	res[last_idx].last_accessed_date.day = tp->day;

	res[last_idx].last_modification_time.hours = tp->hours;
	res[last_idx].last_modification_time.minutes = tp->minutes;
	res[last_idx].last_modification_time.seconds = tp->seconds / 2;
	res[last_idx].last_modification_date.year = tp->year - FAT32_YEAR_START;
	res[last_idx].last_modification_date.month = tp->month;
	res[last_idx].last_modification_date.day = tp->day;

	if(type == FS_STAT_TYPE_DIR) {
		res[last_idx].attributes = FAT32_DIRENT_TYPE_DIRECTORY;
	}

	return res;
}

path_t* fat32_file_get_path(file_t* self) {
	fat32_fs_file_context_t* ctx = (fat32_fs_file_context_t*)self->context;

	return ctx->file_path;
}

int8_t fat32_file_close(file_t* self) {
	fat32_fs_file_context_t* ctx = (fat32_fs_file_context_t*)self->context;

	ctx->file_path->close(ctx->file_path);

	memory_free(ctx);
	memory_free(self);

	return 0;
}

int64_t fat32_file_write(file_t* self, uint8_t* buf, int64_t buflen) {
	fat32_fs_file_context_t* ctx = (fat32_fs_file_context_t*)self->context;
	fat32_fs_context_t* fs_ctx = (fat32_fs_context_t*)ctx->fs->context;

	int64_t w_cnt = 0;
	int64_t w_cnt_iter;
	int64_t rem = buflen;
	int64_t buf_offset = 0;
	int64_t cluster_data_size = fs_ctx->bpb->sectors_per_cluster * fs_ctx->bpb->bytes_per_sector;

	int64_t pos = ctx->current_position;

	int64_t cluster_offset = pos / cluster_data_size;
	int64_t cluster_data_offset = pos % cluster_data_size;

	uint32_t clusterno = ctx->clusterno;

	if(clusterno == 0) {
		clusterno = fat32_get_empty_cluster(ctx->fs);
		if(clusterno != -1U) {
			fs_ctx->fsinfo->last_allocated_cluster = clusterno;
			fs_ctx->fsinfo->free_cluster_count--;
			fs_ctx->table[clusterno] = FAT32_CLUSTER_END2;
			fat32_fs_directory_context_t* dir_ctx = (fat32_fs_directory_context_t*)ctx->dir->context;
			dir_ctx->dirents[ctx->dirent_idx].fat_number_high = clusterno >> 16;
			dir_ctx->dirents[ctx->dirent_idx].fat_number_low = clusterno;

			ctx->clusterno = clusterno;
		}
	}

	while(cluster_offset > 0) {
		if(fs_ctx->table[clusterno] >= FAT32_CLUSTER_END) {
			uint32_t old_clusterno = clusterno;
			clusterno = fat32_get_empty_cluster(ctx->fs);

			if(clusterno != -1U) {
				fs_ctx->fsinfo->last_allocated_cluster = clusterno;
				fs_ctx->fsinfo->free_cluster_count--;
				fs_ctx->table[clusterno] = FAT32_CLUSTER_END2;
				fs_ctx->table[old_clusterno] = clusterno;
			}
		}else {
			clusterno = fs_ctx->table[clusterno];
		}

		cluster_offset--;
	}

	uint8_t* tmp_data = NULL;


	while(rem > 0) {
		w_cnt_iter = MIN(rem, cluster_data_size);

		w_cnt_iter = MIN(w_cnt_iter, cluster_data_size - cluster_data_offset);

		if(cluster_data_offset) {
			fs_ctx->disk->read(fs_ctx->disk, fs_ctx->data_start_lba + clusterno, fs_ctx->bpb->sectors_per_cluster, &tmp_data);
		} else {
			tmp_data = memory_malloc(cluster_data_size);
		}


		memory_memcopy(buf + buf_offset, tmp_data + cluster_data_offset, w_cnt_iter);

		fs_ctx->disk->write(fs_ctx->disk, fs_ctx->data_start_lba + clusterno, fs_ctx->bpb->sectors_per_cluster, tmp_data);

		memory_free(tmp_data);

		cluster_data_offset = 0;

		w_cnt += w_cnt_iter;
		rem -= w_cnt_iter;
		buf_offset += w_cnt_iter;

		uint32_t old_clusterno = clusterno;
		clusterno = fs_ctx->table[clusterno];
		if(rem && clusterno >= FAT32_CLUSTER_END) {
			clusterno = fat32_get_empty_cluster(ctx->fs);
			if(clusterno != -1U) {
				fs_ctx->fsinfo->last_allocated_cluster = clusterno;
				fs_ctx->fsinfo->free_cluster_count--;
				fs_ctx->table[clusterno] = FAT32_CLUSTER_END2;
				fs_ctx->table[old_clusterno] = clusterno;
			}
		}
	}



	ctx->current_position += w_cnt;

	if(ctx->current_position > ctx->size) {
		ctx->size = ctx->current_position;
		fat32_fs_directory_context_t* dir_ctx = (fat32_fs_directory_context_t*)ctx->dir->context;
		dir_ctx->dirents[ctx->dirent_idx].file_size = ctx->size;

		fat32_directory_write(ctx->dir, 0);
	}


	fat32_write_cluster_data(ctx->fs);

	return w_cnt;
}

int64_t fat32_file_read(file_t* self, uint8_t* buf, int64_t buflen) {
	fat32_fs_file_context_t* ctx = (fat32_fs_file_context_t*)self->context;
	fat32_fs_context_t* fs_ctx = (fat32_fs_context_t*)ctx->fs->context;

	int64_t r_cnt = 0;
	int64_t r_cnt_iter;
	int64_t rem = buflen;
	int64_t buf_offset = 0;
	int64_t cluster_data_size = fs_ctx->bpb->sectors_per_cluster * fs_ctx->bpb->bytes_per_sector;

	int64_t pos = ctx->current_position;

	if(pos + rem > ctx->size) {
		rem = ctx->size - pos;
	}

	if(rem <= 0) {
		return rem;
	}

	int64_t cluster_offset = pos / cluster_data_size;
	int64_t cluster_data_offset = pos % cluster_data_size;
	int64_t cluster_data_rem = cluster_data_size - cluster_data_offset;

	uint32_t clusterno = ctx->clusterno;

	while(cluster_offset > 0) {
		clusterno = fs_ctx->table[clusterno];
		if(clusterno >= FAT32_CLUSTER_BAD) {
			return -1;
		}
		cluster_offset--;
	}


	uint8_t* tmp_data;

	while(rem > 0) {
		r_cnt_iter = MIN(rem, cluster_data_rem);

		fs_ctx->disk->read(fs_ctx->disk, fs_ctx->data_start_lba + clusterno, fs_ctx->bpb->sectors_per_cluster, &tmp_data);

		memory_memcopy(tmp_data, buf + buf_offset, r_cnt_iter);

		memory_free(tmp_data);

		r_cnt += r_cnt_iter;
		rem -= r_cnt_iter;
		buf_offset += r_cnt_iter;
		cluster_data_offset = 0;
		cluster_data_rem = cluster_data_size - cluster_data_offset;

		clusterno = fs_ctx->table[clusterno];
		if(clusterno >= FAT32_CLUSTER_BAD) {
			break;
		}
	}


	ctx->current_position += r_cnt;

	return r_cnt;
}

int64_t fat32_file_seek(file_t* self, int64_t pos, file_seek_type_t st) {
	fat32_fs_file_context_t* ctx = (fat32_fs_file_context_t*)self->context;


	if(st == FILE_SEEK_TYPE_SET) {
		if(pos + 1 >= ctx->size || pos < 0) {
			return -1;
		}

		ctx->current_position = pos;
	}

	if(st == FILE_SEEK_TYPE_END) {
		if(ctx->size - pos > ctx->size || ctx->size - pos < 0) {
			return -1;
		}

		ctx->current_position = ctx->size - pos - 1;
	}

	if(st == FILE_SEEK_TYPE_CUR) {
		if(ctx->current_position + pos + 1 >= ctx->size || ctx->current_position + pos < 0) {
			return -1;
		}

		ctx->current_position = ctx->current_position + pos;
	}


	return ctx->current_position;
}

int64_t fat32_file_tell(file_t* self) {
	fat32_fs_file_context_t* ctx = (fat32_fs_file_context_t*)self->context;

	return ctx->current_position;
}

int8_t fat32_file_flush(file_t* self) {
	UNUSED(self);
	return -1;
}

fs_stat_type_t fat32_file_get_type(file_t* self){
	UNUSED(self);

	return FS_STAT_TYPE_FILE;
}


time_t fat32_file_get_create_time(file_t* self) {
	fat32_fs_file_context_t* ctx = (fat32_fs_file_context_t*)self->context;

	return ctx->create_time;
}

time_t fat32_file_get_last_access_time(file_t* self) {
	fat32_fs_file_context_t* ctx = (fat32_fs_file_context_t*)self->context;

	return ctx->last_accessed_time;
}

time_t fat32_file_get_last_modification_time(file_t* self) {
	fat32_fs_file_context_t* ctx = (fat32_fs_file_context_t*)self->context;

	return ctx->last_modification_time;
}

file_t* fat32_new_file(filesystem_t* fs, directory_t* dir, uint32_t dirent_idx, uint32_t clusterno, int64_t size, path_t* p, time_t ct, time_t lat, time_t lmt) {
	fat32_fs_file_context_t* ctx = memory_malloc(sizeof(fat32_fs_file_context_t));

	ctx->fs = fs;
	ctx->dir = dir;
	ctx->dirent_idx = dirent_idx;
	ctx->clusterno = clusterno;
	ctx->file_path = p;
	ctx->size = size;
	ctx->create_time = ct;
	ctx->last_accessed_time = lat;
	ctx->last_modification_time = lmt;

	file_t* f = memory_malloc(sizeof(file_t));
	f->context = ctx;
	f->get_path = fat32_file_get_path;
	f->close = fat32_file_close;
	f->write = fat32_file_write;
	f->read = fat32_file_read;
	f->seek = fat32_file_seek;
	f->tell = fat32_file_tell;
	f->flush = fat32_file_flush;
	f->get_type = fat32_file_get_type;
	f->get_create_time = fat32_file_get_create_time;
	f->get_last_access_time = fat32_file_get_last_access_time;
	f->get_last_modification_time = fat32_file_get_last_modification_time;

	return f;
}


path_t* fat32_directory_get_path(directory_t* self) {
	fat32_fs_directory_context_t* ctx = (fat32_fs_directory_context_t*)self->context;

	return ctx->directory_path;
}

int8_t fat32_directory_close(directory_t* self) {
	fat32_fs_directory_context_t* ctx = (fat32_fs_directory_context_t*)self->context;

	ctx->directory_path->close(ctx->directory_path);

	memory_free(ctx->dirents);
	memory_free(ctx);
	memory_free(self);

	return 0;
}

void* fat32_dir_list_iter_notimpelemented(iterator_t* iter){
	UNUSED(iter);

	return NULL;
}

int8_t fat32_dir_list_iter_destroy(iterator_t* iter){
	memory_free(iter->metadata);
	memory_free(iter);

	return 0;
}

iterator_t* fat32_dir_list_iter_next(iterator_t* iter){
	fat32_dir_list_iter_metadata_t* metadata = (fat32_dir_list_iter_metadata_t*)iter->metadata;

	metadata->current_idx++;

	return iter;
}

int8_t fat32_dir_list_iter_end_of_iterator(iterator_t* iter){
	fat32_dir_list_iter_metadata_t* metadata = (fat32_dir_list_iter_metadata_t*)iter->metadata;
	fat32_fs_directory_context_t* ctx = (fat32_fs_directory_context_t*)metadata->dir->context;

	return metadata->current_idx >= ctx->dirent_count?0:1;
}


void* fat32_dir_list_iter_get_item(iterator_t* iter){
	fat32_dir_list_iter_metadata_t* metadata = (fat32_dir_list_iter_metadata_t*)iter->metadata;
	fat32_fs_directory_context_t* ctx = (fat32_fs_directory_context_t*)metadata->dir->context;


	uint32_t cur_idx = metadata->current_idx;

	while(cur_idx < ctx->dirent_count) {
		if(((uint8_t)ctx->dirents[cur_idx].name[0]) == FAT32_DIRENT_TYPE_UNUSED) {
			cur_idx++;
		} else {
			break;
		}
	}

	if(cur_idx == ctx->dirent_count) {
		return NULL;
	}

	if(ctx->dirents[cur_idx].attributes == FAT32_DIRENT_TYPE_VOLUMEID) {
		cur_idx++;
	}

	char_t* dirent_name = NULL;


	if((ctx->dirents[cur_idx].attributes & FAT32_DIRENT_TYPE_LONGNAME) == FAT32_DIRENT_TYPE_LONGNAME) {
		linkedlist_t* name_parts = linkedlist_create_stack();

		wchar_t* p;

		while((ctx->dirents[cur_idx].attributes & FAT32_DIRENT_TYPE_LONGNAME) == FAT32_DIRENT_TYPE_LONGNAME) {
			fat32_dirent_longname_t* ln = (fat32_dirent_longname_t*)&ctx->dirents[cur_idx];

			p = memory_malloc(sizeof(wchar_t) * 3);
			memory_memcopy(ln->name_part3, p, sizeof(wchar_t) * 2);

			for(int16_t i = 0; i < 2; i++) {
				if(p[i] == 0xffff) {
					p[i] = 0;
				}
			}

			linkedlist_stack_push(name_parts, p);

			p = memory_malloc(sizeof(wchar_t) * 7);
			memory_memcopy(ln->name_part2, p, sizeof(wchar_t) * 6);

			for(int16_t i = 0; i < 6; i++) {
				if(p[i] == 0xffff) {
					p[i] = 0;
				}
			}

			linkedlist_stack_push(name_parts, p);

			p = memory_malloc(sizeof(wchar_t) * 6);
			memory_memcopy(ln->name_part1, p, sizeof(wchar_t) * 5);

			for(int16_t i = 0; i < 5; i++) {
				if(p[i] == 0xffff) {
					p[i] = 0;
				}
			}

			linkedlist_stack_push(name_parts, p);

			cur_idx++;
		}

		wchar_t* wname = memory_malloc(sizeof(wchar_t) * 256);
		int64_t wname_idx = 0;

		while(linkedlist_size(name_parts)) {
			p = linkedlist_stack_pop(name_parts);

			memory_memcopy(p, wname + wname_idx, sizeof(wchar_t) * wchar_size(p));
			wname_idx += wchar_size(p);

			memory_free(p);
		}

		linkedlist_destroy(name_parts);

		dirent_name = wchar_to_char(wname);

		memory_free(wname);
	}


	fat32_dirent_shortname_t* dirent = ctx->dirents + cur_idx;

	if(dirent_name == NULL) {
		char_t* name = strtrim_right(strndup(dirent->name, 8));
		char_t* ext = strtrim_right(strndup(dirent->name + 8, 3));


		if(strlen(ext)) {
			char_t* tmp = strcat(name, ".");
			memory_free(name);
			dirent_name = strcat(tmp, ext);
			memory_free(tmp);
			memory_free(ext);
		} else {
			dirent_name = strdup(name);
			memory_free(name);
			memory_free(ext);
		}

	}

	char_t* base_path_string = ctx->directory_path->get_fullpath(ctx->directory_path);
	int64_t bp_len = strlen(base_path_string);

	char_t max_path[256];
	memory_memclean(max_path, 256);
	memory_memcopy(base_path_string, max_path, bp_len);

	if(ctx->directory_path->is_root(ctx->directory_path) != 0) {
		memory_memcopy(PATH_DELIMETER_STR, max_path + bp_len, strlen(PATH_DELIMETER_STR));
		bp_len += strlen(PATH_DELIMETER_STR);
	}

	memory_memcopy(dirent_name, max_path + bp_len, strlen(dirent_name));
	memory_free(dirent_name);

	path_t* dir_p = filesystem_new_path(ctx->fs, max_path);
	fs_stat_type_t st = FS_STAT_TYPE_FILE;


	if((dirent->attributes & FAT32_DIRENT_TYPE_DIRECTORY) == FAT32_DIRENT_TYPE_DIRECTORY) {
		st = FS_STAT_TYPE_DIR;
	}

	path_interface_t* pi = filesystem_empty_path_interface(ctx->fs, dir_p, st);

	uint32_t clusterno = ctx->dirents[cur_idx].fat_number_high;
	clusterno <<= 16;
	clusterno |=  ctx->dirents[cur_idx].fat_number_low;
	int64_t size = ctx->dirents[cur_idx].file_size;

	timeparsed_t ct = {ctx->dirents[cur_idx].create_date.year + FAT32_YEAR_START, ctx->dirents[cur_idx].create_date.month,
		                 ctx->dirents[cur_idx].create_date.day, ctx->dirents[cur_idx].create_time.hours,
		                 ctx->dirents[cur_idx].create_time.minutes, ctx->dirents[cur_idx].create_time.seconds * 2};
	metadata->extra_data.ct = timeparsed_to_time(&ct);

	timeparsed_t lat = {ctx->dirents[cur_idx].last_accessed_date.year + FAT32_YEAR_START, ctx->dirents[cur_idx].last_accessed_date.month,
		                  ctx->dirents[cur_idx].last_accessed_date.day, 0, 0, 0};
	metadata->extra_data.lat = timeparsed_to_time(&lat);

	timeparsed_t lmt = {ctx->dirents[cur_idx].last_modification_date.year + FAT32_YEAR_START, ctx->dirents[cur_idx].last_modification_date.month,
		                  ctx->dirents[cur_idx].last_modification_date.day, ctx->dirents[cur_idx].last_modification_time.hours,
		                  ctx->dirents[cur_idx].last_modification_time.minutes, ctx->dirents[cur_idx].last_modification_time.seconds * 2};
	metadata->extra_data.lmt = timeparsed_to_time(&lmt);

	metadata->extra_data.clusterno = clusterno;
	metadata->extra_data.size = size;
	metadata->extra_data.dirent_idx = cur_idx;

	metadata->current_idx = cur_idx;

	if(ctx->dirent_count > metadata->current_idx + 1) {
		if(ctx->dirents[cur_idx + 1].name[0] == NULL) {
			metadata->current_idx = ctx->dirent_count;
		}
	}

	return pi;
}

void* fat32_dir_list_iter_get_extra_data(iterator_t* iter){
	fat32_dir_list_iter_metadata_t* metadata = (fat32_dir_list_iter_metadata_t*)iter->metadata;
	return &metadata->extra_data;
}

iterator_t* fat32_directory_list(directory_t* self) {
	fat32_dir_list_iter_metadata_t* metadata = memory_malloc(sizeof(fat32_dir_list_iter_metadata_t));
	metadata->dir = self;

	iterator_t* iter = memory_malloc(sizeof(iterator_t));

	iter->metadata = metadata;
	iter->destroy = fat32_dir_list_iter_destroy;
	iter->next = fat32_dir_list_iter_next;
	iter->end_of_iterator = fat32_dir_list_iter_end_of_iterator;
	iter->get_item = fat32_dir_list_iter_get_item;
	iter->delete_item = fat32_dir_list_iter_notimpelemented;
	iter->get_extra_data = fat32_dir_list_iter_get_extra_data;


	return iter;
}

path_interface_t* fat32_create_or_open_directory_or_file(directory_t* self, path_t* p, fs_stat_type_t type) {
	fat32_fs_directory_context_t* ctx = (fat32_fs_directory_context_t*)self->context;

	char_t* f_path_str = p->get_name_and_ext(p);

	path_interface_t* res = NULL;

	filesystem_t* fs = ctx->fs;

	path_t* dir_path = ctx->directory_path;

	boolean_t found = 0;

	iterator_t* iter = self->list(self);

	while(iter->end_of_iterator(iter) != 0) {

		path_interface_t* pi = iter->get_item(iter);


		if(pi) {
			path_t* t_p = pi->get_path(pi);

			char_t* t_p_str = t_p->get_name_and_ext(t_p);

			if(strcmp(t_p_str, f_path_str) == 0) {
				found = 1;

				if(pi->get_type(pi) == type) {
					fat32_dir_list_iter_extradata_t* ed = iter->get_extra_data(iter);

					if(type == FS_STAT_TYPE_FILE) {
						res = (path_interface_t*)fat32_new_file(fs, self, ed->dirent_idx, ed->clusterno, ed->size, dir_path->append(dir_path, p), ed->ct, ed->lat, ed->lmt);
					} else if(type == FS_STAT_TYPE_DIR) {
						res = (path_interface_t*)fat32_new_directory(fs, ed->clusterno, dir_path->append(dir_path, p), ed->ct, ed->lat, ed->lmt);
					}

					pi->close(pi);

					break;
				} else {
					pi->close(pi);

					break;
				}
			}

			pi->close(pi);
		}

		iter = iter->next(iter);
	}

	iter->destroy(iter);



	if(!found) {
		res = (path_interface_t*)fat32_directory_or_file_create(self, p, type);
	}else {
		p->close(p);
	}

	return res;
}

uint32_t fat32_get_empty_cluster(filesystem_t* fs) {
	fat32_fs_context_t* ctx = fs->context;
	uint32_t lac = ctx->fsinfo->last_allocated_cluster + 1;
	uint32_t cluster_count = ctx->bpb->sectors_per_fat * ctx->bpb->bytes_per_sector / sizeof(uint32_t);

	while(ctx->table[lac] != 0 && ctx->fsinfo->free_cluster_count) {
		lac++;

		if(lac == cluster_count) {
			lac = 0;
		}
	}

	return lac;
}


int8_t  fat32_directory_write(directory_t* self, time_t mt) {
	fat32_fs_directory_context_t* ctx = (fat32_fs_directory_context_t*)self->context;
	fat32_fs_context_t* fs_ctx = (fat32_fs_context_t*)ctx->fs->context;

	if(mt != 0) {
		timeparsed_t* tp = time_to_timeparsed(mt);

		if(strncmp(ctx->dirents[0].name, ". ", 2) == 0) {
			ctx->dirents[0].last_modification_time.hours = tp->hours;
			ctx->dirents[0].last_modification_time.minutes = tp->minutes;
			ctx->dirents[0].last_modification_time.seconds = tp->seconds / 2;
			ctx->dirents[0].last_modification_date.year = tp->year - FAT32_YEAR_START;
			ctx->dirents[0].last_modification_date.month = tp->month;
			ctx->dirents[0].last_modification_date.day = tp->day;
		}

		memory_free(tp);
	}


	uint32_t data_size = ctx->dirent_count * sizeof(fat32_dirent_shortname_t);
	uint32_t clusterno = ctx->clusterno;
	uint32_t iter_len = fs_ctx->bpb->bytes_per_sector * fs_ctx->bpb->sectors_per_cluster;
	uint8_t* data = (uint8_t*)ctx->dirents;
	uint32_t offset = 0;

	while(data_size) {

		fs_ctx->disk->write(fs_ctx->disk, fs_ctx->data_start_lba + clusterno, fs_ctx->bpb->sectors_per_cluster, data + offset);

		data_size -= iter_len;
		offset += iter_len;

		clusterno = fs_ctx->table[clusterno];

		if(data_size && clusterno >= FAT32_CLUSTER_END) {
			clusterno = fat32_get_empty_cluster(ctx->fs);

			fs_ctx->fsinfo->free_cluster_count--;
			fs_ctx->fsinfo->last_allocated_cluster = clusterno;
			fs_ctx->table[clusterno] = FAT32_CLUSTER_END2;
		}
	}

	fat32_write_cluster_data(ctx->fs);

	return 0;
}

directory_t* fat32_directory_create(filesystem_t* fs, uint32_t parent_clusterno, uint32_t clusterno, path_t* p, time_t ct) {
	fat32_fs_context_t* fs_ctx = fs->context;

	uint32_t dir_dirent_cnt = fs_ctx->bpb->sectors_per_cluster * fs_ctx->bpb->bytes_per_sector / sizeof(uint32_t);

	fat32_dirent_shortname_t* dir_dirents = memory_malloc(sizeof(fat32_dirent_shortname_t) * dir_dirent_cnt);

	timeparsed_t* tp = time_to_timeparsed(ct);


	int32_t dirent_cnt = 0;
	fat32_dirent_shortname_t* dirents;

	path_t* tmp_dir = filesystem_new_path(fs, DIRECTORY_CURRENT_DIR);

	dirents = fat32_gen_dirents(tmp_dir, FS_STAT_TYPE_DIR, tp, &dirent_cnt);
	dirents[0].fat_number_high = clusterno >> 16;
	dirents[0].fat_number_low = clusterno;

	memory_memcopy(dirents, dir_dirents, sizeof(fat32_dirent_shortname_t));

	memory_free(dirents);
	tmp_dir->close(tmp_dir);

	tmp_dir = filesystem_new_path(fs, DIRECTORY_PARENT_DIR);

	dirents = fat32_gen_dirents(tmp_dir, FS_STAT_TYPE_DIR, tp, &dirent_cnt);
	if(parent_clusterno != FAT32_ROOT_DIR_CLUSTER_NUMBER) {
		dirents[0].fat_number_high = parent_clusterno >> 16;
		dirents[0].fat_number_low = parent_clusterno;
	}

	memory_memcopy(dirents, dir_dirents + 1, sizeof(fat32_dirent_shortname_t));

	memory_free(dirents);
	tmp_dir->close(tmp_dir);


	fs_ctx->disk->write(fs_ctx->disk, fs_ctx->data_start_lba + clusterno, fs_ctx->bpb->sectors_per_cluster, (uint8_t*)dir_dirents);

	fs_ctx->fsinfo->free_cluster_count--;
	fs_ctx->fsinfo->last_allocated_cluster = clusterno;

	fs_ctx->table[clusterno] = FAT32_CLUSTER_END2;

	fat32_write_cluster_data(fs);

	memory_free(tp);
	memory_free(dir_dirents);


	return fat32_new_directory(fs, clusterno, p, ct, ct, ct);
}

path_interface_t* fat32_directory_or_file_create(directory_t* parent, path_t* child, fs_stat_type_t type) {
	fat32_fs_directory_context_t* ctx = (fat32_fs_directory_context_t*)parent->context;

	timeparsed_t tp;
	timeparsed(&tp);
	time_t ct = timeparsed_to_time(&tp);

	int32_t dirent_cnt = 0;
	fat32_dirent_shortname_t* dirents = fat32_gen_dirents(child, type, &tp, &dirent_cnt);

	if(dirent_cnt == 0) {
		if(dirents) {
			memory_free(dirents);
		}

		return NULL;
	}

	int32_t idx = 0;
	for(; idx < ctx->dirent_count; idx++) {
		if(ctx->dirents[idx].name[0] == NULL) {
			break;
		}
	}

	if(idx + dirent_cnt >= ctx->dirent_count) {
		fat32_fs_context_t* fs_ctx = (fat32_fs_context_t*)ctx->fs->context;
		uint32_t inc = fs_ctx->bpb->bytes_per_sector * fs_ctx->bpb->sectors_per_cluster / sizeof(fat32_dirent_shortname_t);

		uint8_t* data = (uint8_t*)ctx->dirents;
		ctx->dirents = memory_malloc(sizeof(fat32_dirent_shortname_t) * (ctx->dirent_count + inc));

		memory_memcopy(data, ctx->dirents, sizeof(fat32_dirent_shortname_t) * ctx->dirent_count);
		memory_free(data);

		ctx->dirent_count += inc;
	}

	uint32_t clusterno = -1;

	if(type == FS_STAT_TYPE_DIR) {
		clusterno = fat32_get_empty_cluster(ctx->fs);

		dirents[dirent_cnt - 1].fat_number_high = clusterno >> 16;
		dirents[dirent_cnt - 1].fat_number_low = clusterno;
	} else {
		clusterno = 0;
	}



	memory_memcopy(dirents, ctx->dirents + idx, sizeof(fat32_dirent_shortname_t) * dirent_cnt);

	fat32_directory_write(parent, ct);


	memory_free(dirents);

	if(clusterno != -1U) {
		if(type == FS_STAT_TYPE_DIR) {
			return (path_interface_t*)fat32_directory_create(ctx->fs, ctx->clusterno, clusterno, child, ct);
		} else if(type == FS_STAT_TYPE_FILE) {
			return (path_interface_t*)fat32_new_file(ctx->fs, parent, idx + dirent_cnt - 1, clusterno, 0, child, ct, ct, ct);
		}

	}

	return NULL;
}

directory_t* fat32_create_or_open_directory(directory_t* self, path_t* p) {
	return (directory_t*)fat32_create_or_open_directory_or_file(self, p, FS_STAT_TYPE_DIR);
}

file_t* fat32_create_or_open_file(directory_t* self, path_t* p) {
	return (file_t*)fat32_create_or_open_directory_or_file(self, p, FS_STAT_TYPE_FILE);
}

fs_stat_type_t fat32_directory_get_type(struct directory* self){
	UNUSED(self);
	return FS_STAT_TYPE_DIR;
}

time_t fat32_directory_get_create_time(directory_t* self) {
	fat32_fs_directory_context_t* ctx = (fat32_fs_directory_context_t*)self->context;

	return ctx->create_time;
}

time_t fat32_directory_get_last_access_time(directory_t* self) {
	fat32_fs_directory_context_t* ctx = (fat32_fs_directory_context_t*)self->context;

	return ctx->last_accessed_time;
}

time_t fat32_directory_get_last_modification_time(directory_t* self) {
	fat32_fs_directory_context_t* ctx = (fat32_fs_directory_context_t*)self->context;

	return ctx->last_modification_time;
}


directory_t* fat32_new_directory(filesystem_t* fs, uint32_t clusterno, path_t* p, time_t ct, time_t lat, time_t lmt) {
	fat32_fs_context_t* fs_ctx = (fat32_fs_context_t*)fs->context;

	uint32_t cluster_count = fat32_cluster_count(fs, clusterno);
	uint32_t cluster_size = fs_ctx->bpb->sectors_per_cluster;
	uint32_t sector_size = fs_ctx->bpb->bytes_per_sector;

	fat32_fs_directory_context_t* ctx = memory_malloc(sizeof(fat32_fs_directory_context_t));

	ctx->fs = fs;
	ctx->clusterno = clusterno;
	ctx->directory_path = p;
	ctx->dirent_count = cluster_count * cluster_size * sector_size / sizeof(fat32_dirent_shortname_t);
	ctx->create_time = ct;
	ctx->last_accessed_time = lat;
	ctx->last_modification_time = lmt;

	char_t* data = memory_malloc(cluster_count * cluster_size * sector_size);

	uint32_t offset = 0;

	while(1) {

		uint32_t lba2read = fs_ctx->data_start_lba + clusterno;
		uint8_t* tmp_data;

		fs_ctx->disk->read(fs_ctx->disk, lba2read, cluster_size, &tmp_data);

		memory_memcopy(tmp_data, data + offset, cluster_size * sector_size);

		memory_free(tmp_data);

		clusterno = fs_ctx->table[clusterno];

		if(clusterno >= FAT32_CLUSTER_BAD) {
			break;
		}

		offset += cluster_size * sector_size;
	}

	ctx->dirents = (fat32_dirent_shortname_t*)data;

	if(ctx->clusterno == FAT32_ROOT_DIR_CLUSTER_NUMBER) {
		for(uint32_t cur_idx = 0; cur_idx < ctx->dirent_count; cur_idx++) {
			if(strncmp(ctx->dirents[cur_idx].name, ". ", 2) == 0 || ctx->dirents[cur_idx].attributes == FAT32_DIRENT_TYPE_VOLUMEID) {
				timeparsed_t tp_ct = {ctx->dirents[cur_idx].create_date.year + FAT32_YEAR_START, ctx->dirents[cur_idx].create_date.month,
					                    ctx->dirents[cur_idx].create_date.day, ctx->dirents[cur_idx].create_time.hours,
					                    ctx->dirents[cur_idx].create_time.minutes, ctx->dirents[cur_idx].create_time.seconds * 2};
				ctx->create_time = timeparsed_to_time(&tp_ct);

				timeparsed_t tp_lat = {ctx->dirents[cur_idx].last_accessed_date.year + FAT32_YEAR_START, ctx->dirents[cur_idx].last_accessed_date.month,
					                     ctx->dirents[cur_idx].last_accessed_date.day, 0, 0, 0};
				ctx->last_accessed_time = timeparsed_to_time(&tp_lat);

				timeparsed_t tp_lmt = {ctx->dirents[cur_idx].last_modification_date.year + FAT32_YEAR_START, ctx->dirents[cur_idx].last_modification_date.month,
					                     ctx->dirents[cur_idx].last_modification_date.day, ctx->dirents[cur_idx].last_modification_time.hours,
					                     ctx->dirents[cur_idx].last_modification_time.minutes, ctx->dirents[cur_idx].last_modification_time.seconds * 2};
				ctx->last_modification_time = timeparsed_to_time(&tp_lmt);

				break;
			}
		}


	}

	directory_t* d = memory_malloc(sizeof(directory_t));

	d->context = ctx;
	d->get_path = fat32_directory_get_path;
	d->list = fat32_directory_list;
	d->create_or_open_directory = fat32_create_or_open_directory;
	d->create_or_open_file = fat32_create_or_open_file;
	d->close = fat32_directory_close;
	d->get_type = fat32_directory_get_type;
	d->get_create_time = fat32_directory_get_create_time;
	d->get_last_access_time = fat32_directory_get_last_access_time;
	d->get_last_modification_time = fat32_directory_get_last_modification_time;

	return d;
}

uint64_t fat32_get_total_size(filesystem_t* self) {
	fat32_fs_context_t* ctx = self->context;

	uint64_t fat32_table_size = ctx->bpb->bytes_per_sector *  ctx->bpb->sectors_per_fat;
	fat32_table_size /= sizeof(int32_t);
	fat32_table_size -= 3;

	return fat32_table_size * ctx->bpb->bytes_per_sector;
}

uint64_t fat32_get_free_size(filesystem_t* self) {
	fat32_fs_context_t* ctx = self->context;

	return ctx->fsinfo->free_cluster_count * 512;
}

int8_t fat32_close(filesystem_t* self) {
	fat32_fs_context_t* ctx = self->context;

	memory_free(ctx->bpb);
	memory_free(ctx->fsinfo);
	memory_free(ctx->table);
	memory_free(ctx);
	memory_free(self);

	return 0;
}

fs_stat_t* fat32_stat(filesystem_t* self, path_t* p) {
	UNUSED(self);
	UNUSED(p);

	return NULL;
}

int8_t fat32_remove(filesystem_t* self, path_t* p) {
	UNUSED(self);
	UNUSED(p);

	return NULL;
}

directory_t* fat32_get_root_directory(filesystem_t* self) {
	fat32_fs_context_t* fs_ctx = self->context;

	path_t* root_path = filesystem_new_path(self, "/");
	uint32_t root_dir_cluster_number = fs_ctx->bpb->root_dir_cluster_number;

	return fat32_new_directory(self, root_dir_cluster_number, root_path, 0, 0, 0);
}

int8_t fat32_write_cluster_data(filesystem_t* fs){
	fat32_fs_context_t* ctx = fs->context;

	uint64_t fat_part_start_lba = ctx->fat_part_start_lba;

	ctx->disk->write(ctx->disk, fat_part_start_lba + ctx->bpb->fsinfo_sector, sizeof(fat32_fsinfo_t) / 512, (uint8_t*)ctx->fsinfo);
	ctx->disk->write(ctx->disk, fat_part_start_lba + ctx->bpb->backup_bpb + ctx->bpb->fsinfo_sector, sizeof(fat32_fsinfo_t) / 512, (uint8_t*)ctx->fsinfo);

	ctx->disk->write(ctx->disk, fat_part_start_lba + ctx->bpb->reserved_sectors, ctx->bpb->sectors_per_fat, (uint8_t*)ctx->table);
	ctx->disk->write(ctx->disk, fat_part_start_lba + ctx->bpb->reserved_sectors + ctx->bpb->sectors_per_fat, ctx->bpb->sectors_per_fat, (uint8_t*)ctx->table);

	return 0;
}

filesystem_t* fat32_get_or_create_fs(disk_t* d, uint8_t partno, char_t* volname){

	disk_partition_context_t* part_ctx = d->get_partition(d, partno);


	uint64_t fat_part_start_lba = part_ctx->start_lba;
	uint64_t fat_part_end_lba = part_ctx->end_lba;

	memory_free(part_ctx);

	fat32_bpb_t* fat32_bpb;
	fat32_fsinfo_t* fat32_fsinfo;

	d->read(d, fat_part_start_lba, sizeof(fat32_bpb_t) / 512, (uint8_t**)&fat32_bpb);
	d->read(d, fat_part_start_lba + FAT32_FSINFO_SECTOR, sizeof(fat32_fsinfo_t) / 512, (uint8_t**)&fat32_fsinfo);

	if(memory_memcompare(fat32_bpb->identifier, FAT32_IDENTIFIER, 8) == 0 &&
	   fat32_fsinfo->signature0 == FAT32_FSINFO_SIGNATURE0 &&
	   fat32_fsinfo->signature1 == FAT32_FSINFO_SIGNATURE1 &&
	   fat32_fsinfo->signature2 == FAT32_FSINFO_SIGNATURE2) {

		uint32_t* fat32_table;

		d->read(d, fat_part_start_lba + fat32_bpb->reserved_sectors, fat32_bpb->sectors_per_fat, (uint8_t**)&fat32_table);

		fat32_fs_context_t* ctx = memory_malloc(sizeof(fat32_fs_context_t));

		ctx->disk = d;
		ctx->partno = partno;
		ctx->fat_part_start_lba = fat_part_start_lba;
		ctx->data_start_lba = fat_part_start_lba + fat32_bpb->reserved_sectors + 2 * fat32_bpb->sectors_per_fat - 2;
		ctx->bpb = fat32_bpb;
		ctx->fsinfo = fat32_fsinfo;
		ctx->table = fat32_table;

		filesystem_t* fs = memory_malloc(sizeof(filesystem_t));
		fs->context = ctx;
		fs->get_root_directory = fat32_get_root_directory;
		fs->get_total_size = fat32_get_total_size;
		fs->get_free_size = fat32_get_free_size;
		fs->create_or_open_directory = fs_create_or_open_directory;
		fs->create_or_open_file = fs_create_or_open_file;
		fs->stat = fat32_stat;
		fs->remove = fat32_remove;
		fs->close = fat32_close;

		return fs;
	}

	fat32_bpb->jump[0] = 0xeb;
	fat32_bpb->jump[0] = 0x58;
	fat32_bpb->jump[0] = 0x90;
	memory_memcopy(FAT32_OEM_ID, fat32_bpb->oem_id, 8);
	fat32_bpb->bytes_per_sector = FAT32_BYTES_PER_SECTOR;
	fat32_bpb->sectors_per_cluster = 0x1;
	fat32_bpb->reserved_sectors = FAT32_RESERVED_SECTORS;
	fat32_bpb->fat_count = FAT32_FAT_COUNT;
	fat32_bpb->dirent_count = 0x0;
	fat32_bpb->sector_count = 0x0;
	fat32_bpb->media_descriptor_type = FAT32_MEDIA_DESCRIPTOR_TYPE;
	fat32_bpb->sectors_per_fat_unused = 0x0;
	fat32_bpb->sectors_per_track = FAT32_SECTORS_PER_TRACK;
	fat32_bpb->head_count = FAT32_HEAD_COUNT;
	fat32_bpb->hidden_sector_count = fat_part_start_lba;
	fat32_bpb->sectors_per_fat = (fat_part_end_lba - fat_part_start_lba + 1 - fat32_bpb->reserved_sectors) / 130;
	fat32_bpb->large_sector_count =   fat32_bpb->sectors_per_fat * 128 + 2 * fat32_bpb->sectors_per_fat +  fat32_bpb->reserved_sectors - 2;
	fat32_bpb->flags = 0x0;
	fat32_bpb->version_number = 0x0;
	fat32_bpb->root_dir_cluster_number = FAT32_ROOT_DIR_CLUSTER_NUMBER;
	fat32_bpb->fsinfo_sector = FAT32_FSINFO_SECTOR;
	fat32_bpb->backup_bpb = FAT32_BACKUP_BPB;
	fat32_bpb->drive_number = FAT32_DRIVE_NUMBER;
	fat32_bpb->signature = FAT32_SIGNATURE;
	fat32_bpb->serial_number = rand();
	memory_memcopy(volname, fat32_bpb->volume_label, 11);
	memory_memcopy(FAT32_IDENTIFIER, fat32_bpb->identifier, 8);
	fat32_bpb->boot_signature = FAT32_BOOT_SIGNATURE;

	fat32_fsinfo->signature0 = FAT32_FSINFO_SIGNATURE0;
	fat32_fsinfo->signature1 = FAT32_FSINFO_SIGNATURE1;
	fat32_fsinfo->signature2 = FAT32_FSINFO_SIGNATURE2;
	fat32_fsinfo->last_allocated_cluster = FAT32_ROOT_DIR_CLUSTER_NUMBER;
	fat32_fsinfo->free_cluster_count =  fat32_bpb->sectors_per_fat * 128 - FAT32_ROOT_DIR_CLUSTER_NUMBER - 1;

	uint64_t fat32_table_size = fat32_bpb->bytes_per_sector *  fat32_bpb->sectors_per_fat;
	uint32_t* fat32_table = memory_malloc(fat32_table_size);
	fat32_table[0] = FAT32_CLUSTER_END;
	fat32_table[1] = FAT32_CLUSTER_END2;
	fat32_table[2] = FAT32_CLUSTER_END;

	fat32_dirent_shortname_t* volid = memory_malloc(sizeof(fat32_dirent_shortname_t));
	memory_memcopy(FAT32_ESP_VOLUME_LABEL, volid->name, 11);
	volid->attributes = FAT32_DIRENT_TYPE_VOLUMEID;

	timeparsed_t tp;
	timeparsed(&tp);

	volid->create_time.seconds = tp.seconds / 2;
	volid->create_time.minutes = tp.minutes;
	volid->create_time.hours = tp.hours;

	volid->create_date.day = tp.day;
	volid->create_date.month = tp.month;
	volid->create_date.year = tp.year - FAT32_YEAR_START;

	volid->last_accessed_date.day = tp.day;
	volid->last_accessed_date.month = tp.month;
	volid->last_accessed_date.year = tp.year - FAT32_YEAR_START;

	volid->last_modification_time.seconds = tp.seconds / 2;
	volid->last_modification_time.minutes = tp.minutes;
	volid->last_modification_time.hours = tp.hours;

	volid->last_modification_date.day = tp.day;
	volid->last_modification_date.month = tp.month;
	volid->last_modification_date.year = tp.year - FAT32_YEAR_START;


	d->write(d, fat_part_start_lba, sizeof(fat32_bpb_t) / 512, (uint8_t*)fat32_bpb);
	d->write(d, fat_part_start_lba + fat32_bpb->backup_bpb, sizeof(fat32_bpb_t) / 512, (uint8_t*)fat32_bpb);

	uint8_t* data = memory_malloc(512);
	memory_memcopy(volid, data, sizeof(fat32_dirent_shortname_t));
	uint64_t root_dir_lba = fat_part_start_lba + fat32_bpb->reserved_sectors + 2 *  fat32_bpb->sectors_per_fat;
	d->write(d, root_dir_lba, 1, data);
	memory_free(data);
	memory_free(volid);

	fat32_fs_context_t* ctx = memory_malloc(sizeof(fat32_fs_context_t));

	ctx->disk = d;
	ctx->partno = partno;
	ctx->fat_part_start_lba = fat_part_start_lba;
	ctx->data_start_lba = fat_part_start_lba + fat32_bpb->reserved_sectors + 2 * fat32_bpb->sectors_per_fat - 2;
	ctx->bpb = fat32_bpb;
	ctx->fsinfo = fat32_fsinfo;
	ctx->table = fat32_table;

	filesystem_t* fs = memory_malloc(sizeof(filesystem_t));
	fs->context = ctx;
	fs->get_root_directory = fat32_get_root_directory;
	fs->get_total_size = fat32_get_total_size;
	fs->get_free_size = fat32_get_free_size;
	fs->create_or_open_directory = fs_create_or_open_directory;
	fs->create_or_open_file = fs_create_or_open_file;
	fs->stat = fat32_stat;
	fs->remove = fat32_remove;
	fs->close = fat32_close;

	fat32_write_cluster_data(fs);

	return fs;
}

uint8_t fat32_long_filename_checksum(char_t* file_name){
	uint8_t sum = 0;

	for(int16_t i = 0; i < 11; i++) {
		sum = ((sum & 1) ? 0x80 : 0) + (sum >> 1) + file_name[i];
	}

	return sum;
}

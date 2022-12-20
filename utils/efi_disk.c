/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#define RAMSIZE 0x800000
#include "setup.h"
#include <crc.h>
#include <disk.h>
#include <efi.h>
#include <fat.h>
#include <strings.h>
#include <utils.h>
#include <random.h>
#include <linkedlist.h>
#include <time.h>


typedef struct {
    FILE*    fp_disk;
    uint64_t file_size;
    uint64_t block_size;
} disk_file_context_t;

uint64_t disk_file_get_disk_size(disk_t* d){
    disk_file_context_t* ctx = (disk_file_context_t*)d->disk_context;
    return ctx->file_size;
}

int8_t disk_file_write(disk_t* d, uint64_t lba, uint64_t count, uint8_t* data) {
    disk_file_context_t* ctx = (disk_file_context_t*)d->disk_context;

    fseek(ctx->fp_disk, lba * ctx->block_size, SEEK_SET);

    fwrite(data, count * ctx->block_size, 1, ctx->fp_disk);
    fflush(ctx->fp_disk);

    return 0;
}

int8_t disk_file_read(disk_t* d, uint64_t lba, uint64_t count, uint8_t** data){
    disk_file_context_t* ctx = (disk_file_context_t*)d->disk_context;

    fseek(ctx->fp_disk, lba * ctx->block_size, SEEK_SET);

    *data = memory_malloc(count * ctx->block_size);
    fread(*data, count * ctx->block_size, 1, ctx->fp_disk);

    return 0;
}

int8_t disk_file_close(disk_t* d) {
    disk_file_context_t* ctx = (disk_file_context_t*)d->disk_context;
    fclose(ctx->fp_disk);

    memory_free(ctx);

    memory_free(d);

    return 0;
}

disk_t* disk_file_open(char_t* file_name, int64_t size) {

    FILE* fp_disk;

    if(size != -1) {
        fp_disk = fopen(file_name, "w");
        uint8_t data = 0;
        fseek(fp_disk, size - 1, SEEK_SET);
        fwrite(&data, 1, 1, fp_disk);
        fclose(fp_disk);
    }

    fp_disk = fopen(file_name, "r+");
    fseek(fp_disk, 0, SEEK_END);
    size = ftell(fp_disk);

    disk_file_context_t* ctx = memory_malloc(sizeof(disk_file_context_t));

    if(ctx == NULL) {
        fclose(fp_disk);

        return NULL;
    }

    ctx->fp_disk = fp_disk;
    ctx->file_size = size;
    ctx->block_size = 512;

    disk_t* d = memory_malloc(sizeof(disk_t));

    if(d == NULL) {
        memory_free(ctx);
        fclose(fp_disk);

        return NULL;
    }

    d->disk_context = ctx;
    d->get_disk_size = disk_file_get_disk_size;
    d->write = disk_file_write;
    d->read = disk_file_read;
    d->close = disk_file_close;

    return d;
}

int32_t main(int32_t argc, char** argv) {

    if (argc < 4) {
        printf("Error: not enough arguments\n");
        return -1;
    }

    time_t t;
    srand(time(&t));

    timeparsed_t tp;
    timeparsed(&tp);
    printf("%i %i %i %i %i %i %li\n", tp.year, tp.month, tp.day, tp.hours, tp.minutes, tp.seconds, timeparsed_to_time(&tp));

    crc32_init_table();

    int item = 1;
    char_t* disk_name = argv[item++];
    char_t* efi_boot_file_name = argv[item++];
    char_t* kernel_name = argv[item++];



    disk_t* d = NULL;

    if(1) {
        FILE* fp_kernel = fopen(kernel_name, "r");
        fseek(fp_kernel, 0, SEEK_END);
        int64_t kernel_size = ftell(fp_kernel);
        fseek(fp_kernel, 0, SEEK_SET);

        int64_t kernel_sec_count = kernel_size / 512;
        if(kernel_size % 512) {
            kernel_sec_count++;
        }

        d = disk_file_open(disk_name, 1 << 30);


        d = gpt_get_or_create_gpt_disk(d);


        disk_partition_context_t* part_ctx;

        efi_guid_t esp_guid = EFI_PART_TYPE_EFI_SYSTEM_PART_GUID;
        part_ctx = gpt_create_partition_context(&esp_guid, "efi", 2048, 206847);
        d->add_partition(d, part_ctx);
        memory_free(part_ctx->internal_context);
        memory_free(part_ctx);


        efi_guid_t kernel_guid = EFI_PART_TYPE_TURNSTONE_KERNEL_PART_GUID;
        part_ctx = gpt_create_partition_context(&kernel_guid, "kernel", 206848, 206848 + kernel_sec_count - 1);
        d->add_partition(d, part_ctx);
        memory_free(part_ctx->internal_context);
        memory_free(part_ctx);

        uint8_t* buf = memory_malloc(kernel_size);
        fread(buf, 1, kernel_size, fp_kernel);

        d->write(d, 206848, kernel_size / 512, buf);

        memory_free(buf);
        fclose(fp_kernel);

    } else {
        d = disk_file_open(disk_name, -1);
        d = gpt_get_or_create_gpt_disk(d);
    }

    if(d == NULL) {
        printf("disk open failed\n");
    }



    int res = -1;

    filesystem_t* fs = fat32_get_or_create_fs(d, 0, FAT32_ESP_VOLUME_LABEL);

    printf("disk size: %i\n", fs->get_total_size(fs));



    directory_t* root_dir = fs->get_root_directory(fs);

    if(root_dir == NULL) {
        print_error("root dir can not getted");
    }

    path_t* efi_path = filesystem_new_path(fs, "EFI");

    if(efi_path == NULL) {
        print_error("efi path can not created");
    }

    directory_t* efi_dir = root_dir->create_or_open_directory(root_dir, efi_path);

    if(efi_dir) {
        printf("dir created\n");
        path_t* boot_path = filesystem_new_path(fs, "BOOT");

        directory_t* boot_dir = efi_dir->create_or_open_directory(efi_dir, boot_path);

        if(boot_dir) {
            path_t* bootx64_efi_path = filesystem_new_path(fs, "BOOTX64.EFI");

            file_t* bootx64_efi_file = boot_dir->create_or_open_file(boot_dir, bootx64_efi_path);

            if(bootx64_efi_file) {
                printf("file created\n");

                FILE* fp_efi_boot = fopen(efi_boot_file_name, "r");

                uint8_t buf[4096];

                while(1) {
                    int64_t r = fread(buf, 1, 4096, fp_efi_boot);
                    if(r <= 0) {
                        break;
                    }

                    bootx64_efi_file->write(bootx64_efi_file, (uint8_t*)&buf[0], r);

                }

                fclose(fp_efi_boot);

                bootx64_efi_file->close(bootx64_efi_file);
            } else {
                bootx64_efi_path->close(bootx64_efi_path);
            }

            boot_dir->close(boot_dir);

            res = 0;
        } else {
            boot_path->close(boot_path);
        }

        efi_dir->close(efi_dir);
    } else{
        print_error("efi dir can not created");
        efi_path->close(efi_path);
    }

    root_dir->close(root_dir);

    fs->close(fs);

    d->close(d);

    if(res == 0) {
        print_success("DISK BUILDED");
    } else {
        print_error("DISK BUILD FAILED");
    }


    return res;
}

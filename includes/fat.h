/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___FAT_H
#define ___FAT_H 0

#include <types.h>
#include <fs.h>

#define FAT32_BOOT_SIGNATURE 0xAA55
#define FAT32_IDENTIFIER "FAT32   "
#define FAT32_SIGNATURE 0x29
#define FAT32_OEM_ID "hobby.os"
#define FAT32_BYTES_PER_SECTOR 0x200
#define FAT32_RESERVED_SECTORS 0x20
#define FAT32_FAT_COUNT 0x02
#define FAT32_MEDIA_DESCRIPTOR_TYPE 0xF8
#define FAT32_SECTORS_PER_TRACK 0x3F
#define FAT32_HEAD_COUNT 0x40
#define FAT32_ROOT_DIR_CLUSTER_NUMBER 0x2
#define FAT32_FSINFO_SECTOR 0x1
#define FAT32_BACKUP_BPB 0x6
#define FAT32_DRIVE_NUMBER 0x80
#define FAT32_ESP_VOLUME_LABEL "ESP EFI VOL"

#define FAT32_YEAR_START 1980


#define FAT32_CLUSTER_BAD  0x0FFFFFF7
#define FAT32_CLUSTER_END  0x0FFFFFF8
#define FAT32_CLUSTER_END2  0x0FFFFFFF

typedef struct fat32_bpb_t {
    uint8_t  jump[3]; /* 0xeb 0x58 0x90 */
    char_t   oem_id[8]; /* "hobby.os" */
    uint16_t bytes_per_sector; /* 0x200 */
    uint8_t  sectors_per_cluster; /* 0x01 */
    uint16_t reserved_sectors; /* 0x20 */
    uint8_t  fat_count; /* 0x2  */
    uint16_t dirent_count; /* 0x0 */
    uint16_t sector_count; /* 0x0 look large_sector_count*/
    uint8_t  media_descriptor_type; /* 0xf8 */
    uint16_t sectors_per_fat_unused; /* 0x0 */
    uint16_t sectors_per_track; /* 0x3f */
    uint16_t head_count; /* 0x40 */
    uint32_t hidden_sector_count; /* 0x800: partition start */
    uint32_t large_sector_count; /* 0x31fce  */
    uint32_t sectors_per_fat; /* 0x627 int((part_sector_size-reserved_sectors)/130) */
    uint16_t flags; /* 0x0 */
    uint16_t version_number; /* 0x0 */
    uint32_t root_dir_cluster_number; /* 0x2 */
    uint16_t fsinfo_sector; /* 0x1 */
    uint16_t backup_bpb; /* 0x6 */
    uint8_t  reserved0[12]; /* 0x0 */
    uint8_t  drive_number; /* 0x80 */
    uint8_t  reserved1; /* 0x0 */
    uint8_t  signature; /* 0x29 */
    uint32_t serial_number; /* rand */
    char_t   volume_label[11]; /* rand */
    char_t   identifier[8]; /* "FAT32   " */
    uint8_t  boot_code[420];
    uint16_t boot_signature; /* 0xaa55 */
}__attribute__((packed))  fat32_bpb_t;

#define FAT32_FSINFO_SIGNATURE0 0x41615252
#define FAT32_FSINFO_SIGNATURE1 0x61417272
#define FAT32_FSINFO_SIGNATURE2 0xAA550000

typedef struct fat32_fsinfo_t {
    uint32_t signature0;
    uint8_t  reserved0[480];
    uint32_t signature1;
    uint32_t free_cluster_count;
    uint32_t last_allocated_cluster;
    uint8_t  reserved1[12];
    uint32_t signature2;
}__attribute__((packed))  fat32_fsinfo_t;

#define FAT32_DIRENT_TYPE_READONLY    0x01
#define FAT32_DIRENT_TYPE_HIDDEN      0x02
#define FAT32_DIRENT_TYPE_SYSTEM      0x04
#define FAT32_DIRENT_TYPE_VOLUMEID    0x08
#define FAT32_DIRENT_TYPE_DIRECTORY   0x10
#define FAT32_DIRENT_TYPE_ARCHIVE     0x20
#define FAT32_DIRENT_TYPE_LONGNAME    0x0F /* (FAT32_DIRENT_TYPE_READONLY|FAT32_DIRENT_TYPE_HIDDEN|FAT32_DIRENT_TYPE_SYSTEM|FAT32_DIRENT_TYPE_VOLUMEID) */
#define FAT32_DIRENT_TYPE_UNUSED      0xE5

typedef struct fat32_dirent_time {
    uint16_t seconds : 5;
    uint16_t minutes : 6;
    uint16_t hours   : 5;
}__attribute__((packed)) fat32_dirent_time;

typedef struct fat32_dirent_date {
    uint16_t day   : 5;
    uint16_t month : 4;
    uint16_t year  : 7;
}__attribute__((packed)) fat32_dirent_date;

typedef struct fat32_dirent_shortname_t {
    char_t            name[11];
    uint8_t           attributes;
    uint8_t           reserved;
    uint8_t           create_time_seconds;
    fat32_dirent_time create_time;
    fat32_dirent_date create_date;
    fat32_dirent_date last_accessed_date;
    uint16_t          fat_number_high;
    fat32_dirent_time last_modification_time;
    fat32_dirent_date last_modification_date;
    uint16_t          fat_number_low;
    uint32_t          file_size;
}__attribute__((packed)) fat32_dirent_shortname_t;

typedef struct fat32_dirent_longname_t {
    uint8_t  order;
    wchar_t  name_part1[5];
    uint8_t  attributes;
    uint8_t  longentry_type_zero;
    uint8_t  checksum;
    wchar_t  name_part2[6];
    uint16_t zero;
    wchar_t  name_part3[2];
}__attribute__((packed)) fat32_dirent_longname_t;

filesystem_t* fat32_get_or_create_fs(disk_or_partition_t* d, const char_t* volname);

#endif

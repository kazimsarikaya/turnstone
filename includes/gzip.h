/**
 * @file gzip.h
 * @brief gzip related definitions and declarations
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___GZIP_H
#define ___GZIP_H

#include <types.h>
#include <buffer.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GZIP_HEADER_ID1 0x1f
#define GZIP_HEADER_ID2 0x8b

typedef enum gzip_compression_method_t : uint8_t {
    GZIP_COMPRESSION_METHOD_NONE,
    GZIP_COMPRESSION_METHOD_DEFLATE = 8,
    GZIP_COMPRESSION_METHOD_END,
} gzip_compression_method_t;

typedef enum gzip_flag_t : uint8_t {
    GZIP_FLAG_NONE     = 0,
    GZIP_FLAG_FTEXT    = 1 << 0,
    GZIP_FLAG_FHCRC    = 1 << 1,
    GZIP_FLAG_FEXTRA   = 1 << 2,
    GZIP_FLAG_FNAME    = 1 << 3,
    GZIP_FLAG_FCOMMENT = 1 << 4,
} gzip_flag_t;

typedef enum gzip_extra_flag_t : uint8_t {
    GZIP_EXTRA_FLAG_NONE,
    GZIP_EXTRA_FLAG_MAXIMUM_COMPRESSION = 2,
    GZIP_EXTRA_FLAG_FASTEST             = 4,
} gzip_extra_flag_t;

typedef enum gzip_os_type_t : uint8_t {
    GZIP_OS_TYPE_FAT_FILESYSTEM,
    GZIP_OS_TYPE_AMIGA,
    GZIP_OS_TYPE_VMS,
    GZIP_OS_TYPE_UNIX,
    GZIP_OS_TYPE_VM_CMS,
    GZIP_OS_TYPE_ATARI,
    GZIP_OS_TYPE_HPFS_FILESYSTEM,
    GZIP_OS_TYPE_MACINTOSH,
    GZIP_OS_TYPE_Z_SYSTEM,
    GZIP_OS_TYPE_CPM,
    GZIP_OS_TYPE_TOPS20,
    GZIP_OS_TYPE_NTFS_FILESYSTEM,
    GZIP_OS_TYPE_QDOS,
    GZIP_OS_TYPE_ACORN_RISCOS,
    GZIP_OS_TYPE_UNKNOWN = 255,
} gzip_os_type_t;

typedef struct gzip_header_t {
    uint8_t                   id1;
    uint8_t                   id2;
    gzip_compression_method_t compression_method;
    gzip_flag_t               flags;
    uint32_t                  mtime;
    gzip_extra_flag_t         extra_flags;
    gzip_os_type_t            os_type;
} __attribute__((packed)) gzip_header_t;

typedef struct gzip_footer_t {
    uint32_t crc32;
    uint32_t isize;
} __attribute__((packed)) gzip_footer_t;

int8_t gzip_pack(buffer_t* in, buffer_t* out);
int8_t gzip_unpack(buffer_t* in, buffer_t* out);


#ifdef __cplusplus
}
#endif

#endif

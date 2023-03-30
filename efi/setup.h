/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___SERTUP_H
#define ___SETUP_H 0

#include <types.h>
#include <logging.h>
#include <efi.h>
#include <disk.h>
#include <memory.h>

#define printf(...) video_printf(__VA_ARGS__)

disk_t* efi_disk_impl_open(efi_block_io_t* bio);

#endif

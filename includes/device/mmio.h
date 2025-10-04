/**
 * @file mmio.h
 * @brief Memory Mapped I/O header.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___MMIO_H
#define ___MMIO_H 0

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

uint64_t mmio_read(uint64_t addr, uint8_t size);
void     mmio_write(uint64_t addr, uint64_t val, uint8_t size);

#ifdef __cplusplus
}
#endif

#endif

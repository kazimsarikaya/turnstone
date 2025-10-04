/**
 * @file 461.c
 * @brief
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <device/mmio.h>

MODULE("turnstone.kernel.hw");

__attribute__((target("general-regs-only")))
uint64_t mmio_read(uint64_t addr, uint8_t size) {
    uint64_t ret = 0;
    switch(size) {
    case 1:
        ret = *((volatile uint8_t*)addr);
        break;
    case 2:
        ret = *((volatile uint16_t*)addr);
        break;
    case 4:
        ret = *((volatile uint32_t*)addr);
        break;
    case 8:
        ret = *((volatile uint64_t*)addr);
        break;
    default:
        return 0;
    }
    return ret;
}

__attribute__((target("general-regs-only")))
void mmio_write(uint64_t addr, uint64_t val, uint8_t size){
    switch(size) {
    case 1:
        *((volatile uint8_t*)addr) = (uint8_t)val;
        break;
    case 2:
        *((volatile uint16_t*)addr) = (uint16_t)val;
        break;
    case 4:
        *((volatile uint32_t*)addr) = (uint32_t)val;
        break;
    case 8:
        *((volatile uint64_t*)addr) = (uint64_t)val;
        break;
    default:
        return;
    }
}

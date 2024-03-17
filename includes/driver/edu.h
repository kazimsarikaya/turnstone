/**
 * @file edu.h
 * @brief PCI EDU (Education) Device Driver
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */


#ifndef ___EDU_H
#define ___EDU_H

#include <types.h>
#include <utils.h>

typedef struct {
    uint32_t identification;
    uint32_t liveness_check;
    uint32_t factorial_computation;
    uint32_t reserved0[5];
    uint32_t status;
    uint32_t interrupt_status;
    uint32_t reserved1[14];
    uint32_t interrupt_raise;
    uint32_t interrupt_acknowledge;
    uint32_t reserved2[6];
    uint64_t dma_source;
    uint64_t dma_destination;
    uint64_t dma_length;
    uint64_t dma_command;
} __attribute__((packed)) edu_t;

_Static_assert(offsetof_field(edu_t, status) == 0x20, "EDU: status offset is not correct");

_Static_assert(offsetof_field(edu_t, interrupt_raise) == 0x60, "EDU: interrupt_raise offset is not correct");

_Static_assert(offsetof_field(edu_t, dma_source) == 0x80, "EDU: interrupt_raise offset is not correct");

_Static_assert(sizeof(edu_t) == 0xA0, "EDU: size is not correct");

#endif

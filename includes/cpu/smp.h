/**
 * @file smp.h
 * @brief smp interface
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 **/

#ifndef ___SMP_H
/*! prevent duplicate header error macro */
#define ___SMP_H 0


#include <types.h>
#include <cpu/crx.h>
#include <cpu/descriptor.h>
#include <memory/paging.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct smp_data_t {
    uint64_t               stack_base;
    uint64_t               stack_size;
    cpu_reg_cr0_t          cr0;
    memory_page_table_t*   cr3;
    cpu_reg_cr4_t          cr4;
    descriptor_register_t* idt;
    uint64_t               gs_base;
    uint64_t               gs_base_size;
} smp_data_t;


int8_t smp_init(void);

#ifdef __cplusplus
}
#endif

#endif

/**
 * @file cpu.h
 * @brief cpu commands which needs assembly codes.
 *
 */
#ifndef ___CPU_H
/*! prevent duplicate header error macro */
#define ___CPU_H 0

#include <types.h>

/**
 * @brief stops cpu.
 *
 * This command stops cpu using hlt assembly command inside for.
 */
void cpu_hlt();

/**
 * @brief disables interrupts
 *
 * This command disables interrupts with cli assembly command.
 */
void cpu_cli();

/**
 * @brief enables interrupts
 *
 * This command enables interrupts with sti assembly command.
 */
void cpu_sti();

/**
 * @brief reads data segment (ds) value from cpu.
 * @return ds value
 *
 * This function is meaningful at real mode.
 * At long mode is does nothing
 */
uint16_t cpu_read_data_segment();

/**
 * @brief checks long mode is supported.
 * @return 0 if long mode supported
 *
 * At real mode this command uses several cpuid assembly command to learn
 * long mode is supported by cpu with several properties.
 */
uint8_t cpu_check_longmode();

#endif

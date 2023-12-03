/**
 * @file debug.h
 * @brief debug functions
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___DEBUG_H
/*! Prevent multiple inclusion */
#define ___DEBUG_H 0

#include <types.h>

/**
 * @brief initializes debug
 * @return 0 if success, -1 if error
 */
int8_t debug_init(void);

/**
 * @brief gets function address by function name
 * @param function_name function name
 * @return function address
 */
uint64_t debug_get_symbol_address(const char_t* function_name);

/**
 * @brief puts 0xcc (int3) instruction to address
 * @param address address to put breakpoint
 */
void debug_put_single_time_breakpoint_at_address(uint64_t address);

/**
 * @brief removes 0xcc (int3) instruction from address
 * @param address address to remove breakpoint
 */
void debug_remove_single_time_breakpoint_from_address(uint64_t address);

/**
 * @brief puts 0xcc (int3) instruction to function
 * @param function_name function name to put breakpoint
 */
void debug_put_breakpoint_at_symbol(const char_t* function_name);

/**
 * @brief removes 0xcc (int3) instruction from function
 * @param function_name function name to remove breakpoint
 */
void debug_remove_breakpoint_from_symbol(const char_t* function_name);

/**
 * @brief gets original byte from address
 * @param address address to get original byte
 */
uint8_t debug_get_original_byte(uint64_t address);

/**
 * @brief sets original byte to address
 * @param address address to set original byte
 */
void debug_revert_original_byte_at_address(uint64_t address);

#endif

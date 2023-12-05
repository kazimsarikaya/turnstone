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
 * @union debug_register_dr6_t
 * @brief debug register dr6
 */
typedef union debug_register_dr6_t {
    uint64_t value; ///< value of debug register dr6
    struct {
        uint64_t b0         : 1; ///< breakpoint condition detected for debug register dr0
        uint64_t b1         : 1; ///< breakpoint condition detected for debug register dr1
        uint64_t b2         : 1; ///< breakpoint condition detected for debug register dr2
        uint64_t b3         : 1; ///< breakpoint condition detected for debug register dr3
        uint64_t reserved1  : 7; ///< reserved
        uint64_t bld        : 1; ///< bus logic trap detected
        uint64_t bk_or_smms : 1; ///< ssm or ice bp detected
        uint64_t bd         : 1; ///< debug register access detected
        uint64_t bs         : 1; ///< single step
        uint64_t bt         : 1; ///< task switch
        uint64_t rtm        : 1; ///< restricted transactional memory
        uint64_t reserved2  : 47; ///< reserved
    } __attribute__((packed));
} __attribute__((packed)) debug_register_dr6_t; ///< debug register dr6

#define DEBUG_REGISTER_DR6_DEFAULT_VALUE 0xFFFF0FF0

/**
 * @union debug_register_dr7_t
 * @brief debug register dr7
 */
typedef union debug_register_dr7_t {
    uint64_t value; ///< value of debug register dr7
    struct {
        uint64_t l0         : 1; ///< local breakpoint enable for debug register dr0
        uint64_t g0         : 1; ///< global breakpoint enable for debug register dr0
        uint64_t l1         : 1; ///< local breakpoint enable for debug register dr1
        uint64_t g1         : 1; ///< global breakpoint enable for debug register dr1
        uint64_t l2         : 1; ///< local breakpoint enable for debug register dr2
        uint64_t g2         : 1; ///< global breakpoint enable for debug register dr2
        uint64_t l3         : 1; ///< local breakpoint enable for debug register dr3
        uint64_t g3         : 1; ///< global breakpoint enable for debug register dr3
        uint64_t le         : 1; ///< local exact breakpoint enable
        uint64_t ge         : 1; ///< global exact breakpoint enable
        uint64_t reserved1  : 1; ///< reserved writting 1
        uint64_t rtms       : 1; ///< restricted transactional memory state
        uint64_t ir_or_smie : 1; ///< ssm or ice bp enable
        uint64_t gd         : 1; ///< general detect enable
        uint64_t reserved2  : 2; ///< reserved
        uint64_t rw0        : 2; ///< read/write for debug register dr0
        uint64_t len0       : 2; ///< length for debug register dr0
        uint64_t rw1        : 2; ///< read/write for debug register dr1
        uint64_t len1       : 2; ///< length for debug register dr1
        uint64_t rw2        : 2; ///< read/write for debug register dr2
        uint64_t len2       : 2; ///< length for debug register dr2
        uint64_t rw3        : 2; ///< read/write for debug register dr3
        uint64_t len3       : 2; ///< length for debug register dr3
        uint64_t reserved3  : 32; ///< reserved
    } __attribute__((packed));
} __attribute__((packed)) debug_register_dr7_t; ///< debug register dr7

#define DEBUG_REGISTER_DR7_DEFAULT_VALUE 0x00000400

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

/**
 * @brief sets dr0 to address and enables dr0
 * @param address address to set dr0
 */
void debug_put_debug_for_address(uint64_t address);

/**
 * @brief disables dr0
 * @param address address to disable dr0
 */
void debug_remove_debug_for_address(uint64_t address);

/**
 * @brief sets dr0 to function and enables dr0
 * @param function_name function name to set dr0
 */
void debug_put_debug_for_symbol(const char_t* function_name);


#endif

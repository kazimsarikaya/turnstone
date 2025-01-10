/**
 * @file task.h
 * @brief cpu task definitons
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#ifndef ___CPU_TASK_H
/*! prevent duplicate header error macro */
#define ___CPU_TASK_H 0

#include <cpu/descriptor.h>
#include <cpu/interrupt.h>
#include <memory.h>
#include <memory/paging.h>
#include <list.h>
#include <buffer.h>
#include <utils.h>

/*! maximum tick count of a task without yielding */
#define TASK_MAX_TICK_COUNT 10

#define TASK_IDLE_TASK_ID 1
/*! kernel task id*/
#define TASK_KERNEL_TASK_ID 2

/**
 * @struct descriptor_tss_t
 * @brief 64 bit tss descriptor
 */
typedef struct descriptor_tss_t {
    uint16_t segment_limit1 : 16; ///< segment limit bits 0-15
    uint32_t base_address1  : 24; ///< base address bits 0-23
    uint8_t  type           : 4; ///< tss type 0x9 in bits
    uint8_t  always0_1      : 1; ////< always zero
    uint8_t  dpl            : 2; ///<  privilage level
    uint8_t  present        : 1; ///< 2/15 aka 47 is always 1
    uint8_t  segment_limit2 : 4; ///< segment limit bits 16-19
    uint8_t  unused1        : 1; ///< unused bit os can use it
    uint8_t  always0_2      : 2; ////< always zero
    uint8_t  long_mode      : 1; ///< this bit is always 1 for long mode aka granularity
    uint64_t base_address2  : 40; ///< base address bits 24-63
    uint8_t  reserved0      : 8; ///< reserved bits
    uint16_t always0_3      : 5; ///< always zero
    uint32_t reserved1      : 19; ///< reserved bits
}__attribute__((packed)) descriptor_tss_t;

/**
 * @brief initialize tss
 * @param  tss   address of tss slot in gdt
 * @param  base  base address of tss struct
 * @param  limit length of tss - 1
 * @param  DPL   privilage level
 */
#define DESCRIPTOR_BUILD_TSS_SEG(tss, base, limit, DPL) { \
            tss->type = SYSTEM_SEGMENT_TYPE_TSS_A; \
            tss->present = 1; \
            tss->long_mode = 1; \
            tss->dpl = DPL; \
            tss->segment_limit1 = limit & 0xFFFF; \
            tss->segment_limit2 = (limit >> 16) & 0xF; \
            tss->base_address1 = base & 0xFFFFFF; \
            tss->base_address2 = (base >> 24); \
}

/**
 * @struct tss_s
 * @brief tss descriptor values
 */
typedef struct tss_s {
    uint32_t reserved0; ///< at long mode this value not used
    uint64_t rsp0; ///< stack pointer for ring 0
    uint64_t rsp1; ///< stack pointer for ring 1
    uint64_t rsp2; ///< stack pointer for ring 2
    uint64_t reserved1; ///< not used at long mode
    uint64_t ist1; ///< stack pointer for interrupts with ist 1
    uint64_t ist2; ///< stack pointer for interrupts with ist 2
    uint64_t ist3; ///< stack pointer for interrupts with ist 3
    uint64_t ist4; ///< stack pointer for interrupts with ist 4
    uint64_t ist5; ///< stack pointer for interrupts with ist 5
    uint64_t ist6; ///< stack pointer for interrupts with ist 6
    uint64_t ist7; ///< stack pointer for interrupts with ist 7
    uint64_t reserved2; ///< not used at long mode
    uint16_t reserved3; ///< not used at long mode
    uint16_t iomap_base_address; ///< iomap permission bit array
}__attribute__((packed)) tss_t; ///< short hand for struct

/**
 * @enum task_state_t
 * @brief task states
 */
typedef enum task_state_t {
    TASK_STATE_NULL, ///< task state not known
    TASK_STATE_CREATED, ///< task created but never runned
    TASK_STATE_STARTING, ///< task is starting
    TASK_STATE_RUNNING, ///< task is running
    TASK_STATE_SUSPENDED, ///< task is at wait queue
    TASK_STATE_SLEEPING, ///< task is sleeping
    TASK_STATE_MESSAGE_WAITING, ///< task is waiting for message
    TASK_STATE_INTERRUPT_RECEIVED, ///< task is waiting for interrupt received
    TASK_STATE_FUTURE_WAITING, ///< task is waiting for future event
    TASK_STATE_LOCKED, ///< task is locked
    TASK_STATE_ENDED, ///< task is ended
} task_state_t; ///< short hand for enum

/**
 * @enum task_attribute_t
 * @brief task attributes
 */
typedef enum task_attribute_t {
    TASK_ATTRIBUTE_NONE = 0x0, ///< no attribute
    TASK_ATTRIBUTE_INTERRUPTIBLE = 0x1, ///< task is interruptible
} task_attribute_t; ///< short hand for enum

typedef struct task_registers_t {
    uint64_t rax; ///< register
    uint64_t rbx; ///< register
    uint64_t rcx; ///< register
    uint64_t rdx; ///< register
    uint64_t r8; ///< register
    uint64_t r9; ///< register
    uint64_t r10; ///< register
    uint64_t r11; ///< register
    uint64_t r12; ///< register
    uint64_t r13; ///< register
    uint64_t r14; ///< register
    uint64_t r15; ///< register
    uint64_t rsi; ///< register
    uint64_t rdi; ///< register
    uint64_t rsp; ///< register
    uint64_t rbp; ///< register
    uint64_t rflags; ///< register
    uint64_t cr3; ///< register
    uint8_t  sse[512]; ///< register
} task_registers_t;

_Static_assert(sizeof(task_registers_t) == 0x290, "task_registers_t size must be 0x290");
_Static_assert((offsetof_field(task_registers_t, sse) % 0x10) == 0x0, "task_registers_t sse offset must be aligned 0x10");

typedef struct task_t {
    memory_heap_t*               creator_heap; ///< the heap which task struct is at
    memory_heap_t*               heap; ///< task's heap
    uint64_t                     heap_size; ///< task's heap size
    uint64_t                     task_id; ///< task's id
    uint64_t                     cpu_id; ///< cpu id which task is running
    uint64_t                     last_tick_count; ///< tick count when task removes from executing, used for scheduling
    uint64_t                     task_switch_count; ///< task switch count
    task_state_t                 state; ///< task state
    task_attribute_t             attributes; ///< task attributes
    void*                        entry_point; ///< entry point address
    uint64_t                     arguments_count; ///< argument count
    void**                       arguments; ///< argument list
    void*                        stack; ///< stack pointer
    uint64_t                     stack_size; ///< stack size of task
    list_t*                      message_queues; ///< task's listining queues.
    uint64_t                     wake_tick; ///< tick value when task wakes up
    const char*                  task_name; ///< task name
    memory_page_table_context_t* page_table; ///< page table
    buffer_t*                    input_buffer; ///< input buffer
    buffer_t*                    output_buffer; ///< output buffer
    buffer_t*                    error_buffer; ///< error buffer
    uint64_t                     vmcs_physical_address; ///< vmcs physical address
    void*                        vm; ///< vm
    task_registers_t*            registers; ///< task registers
} task_t; ///< short hand for struct

/**
 * @brief inits kernel tasking, configures tss and kernel task
 * @param[in] heap the heap of kernel task and tasking related variables.
 */
int8_t task_init_tasking_ext(memory_heap_t* heap);

/*! inits tasking with default heap*/
#define task_init_tasking() task_init_tasking_ext(NULL)

/**
 * @brief sets task switch parameters
 * @param[in] need_eoi if task switching needs notify local apic this field should be true
 * @param[in] need_sti if task switching needs enable interrupts this field should be true
 */
void task_task_switch_set_parameters(boolean_t need_eoi, boolean_t need_sti);

/**
 * @brief switches current task to a new one.
 */
void task_switch_task(void);

/**
 * @brief yields task for waiting other tasks.
 */
void task_yield(void);

/**
 * @brief returns current task's id
 * @return task id
 */
uint64_t task_get_id(void);

/**
 * @brief returns current task
 * @return current task
 */
task_t* task_get_current_task(void);

/**
 * @brief sets current task's message waiting flag
 */
void task_set_message_waiting(void);

/**
 * @brief clears current task's message waiting flag
 * @param[in] task_id task id
 */
void task_clear_message_waiting(uint64_t task_id);

/**
 * @brief sets current task's interruptible flag
 */
void task_set_interruptible(void);

/**
 * @brief sets current task's interrupt received flag
 * @param[in] task_id task id
 */
void task_set_interrupt_received(uint64_t task_id);


void task_set_message_received(uint64_t tid);

/**
 * @brief adds a queue to task
 * @param[in] queue queue which task will have. tasks consumes these queues
 */
void task_add_message_queue(list_t* queue);

list_t* task_get_message_queue(uint64_t task_id, uint64_t queue_number);
#define task_get_current_task_message_queue(queue_number) task_get_message_queue(task_get_id(), queue_number)

/**
 * @brief creates a task and apends it to wait queue
 * @param[in] heap creator heap
 * @param[in] heap_size task's heap size, heap allocated with frame allocator
 * @param[in] stack_size task's stack size, stack allocated with frame allocator
 * @param[in] entry_point task's entry point
 * @param[in] args_cnt argument count
 * @param[in] args argument list
 * @param[in] task_name task's name
 */
uint64_t task_create_task(memory_heap_t* heap, uint64_t heap_size, uint64_t stack_size, void* entry_point, uint64_t args_cnt, void** args, const char_t* task_name);

/**
 * @brief idle task checks if there is any task neeeds to run. it speeds up task running
 */
boolean_t task_idle_check_need_yield(void);

/**
 * @brief sleep for current task for ticks
 * @param[in] wake_tick tick count for sleep
 */
void task_current_task_sleep(uint64_t wake_tick);

void task_end_task(void);
void task_kill_task(uint64_t task_id, boolean_t force);

typedef struct task_list_item_t {
    const char_t* task_name;
    uint64_t      task_id;
    uint64_t      task_address;
    uint64_t      cpu_id;
    uint64_t      task_switch_count;
    uint64_t      rsp;
    uint64_t      rbp;
    uint64_t      heap_address;
    uint64_t      heap_size;
    uint64_t      stack_address;
    uint64_t      stack_size;
    uint64_t      state;
    uint64_t      attributes;
    uint64_t      message_queues;
    uint64_t      messages;
    uint64_t      malloc_count;
    uint64_t      free_count;
    uint64_t      heap_diff;
} task_list_item_t;

void      task_print_all(buffer_t* buffer);
buffer_t* task_build_task_list(void);

buffer_t* task_get_task_input_buffer(uint64_t tid);
buffer_t* task_get_task_output_buffer(uint64_t tid);
buffer_t* task_get_task_error_buffer(uint64_t tid);
buffer_t* task_get_input_buffer(void);
buffer_t* task_get_output_buffer(void);
buffer_t* task_get_error_buffer(void);
int8_t    task_set_input_buffer(buffer_t* buffer);
int8_t    task_set_output_buffer(buffer_t * buffer);
int8_t    task_set_error_buffer(buffer_t * buffer);

void     task_set_vmcs_physical_address(uint64_t vmcs_physical_address);
uint64_t task_get_vmcs_physical_address(void);
void     task_set_vm(void* vm);
void*    task_get_vm(void);

void task_remove_task_after_fault(uint64_t task_id);

int8_t task_set_current_and_idle_task(void* entry_point, uint64_t stack_base, uint64_t stack_size);

void task_toggle_wait_for_future(uint64_t task_id);

#endif

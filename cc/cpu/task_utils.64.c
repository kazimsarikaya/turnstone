/**
 * @file task.64.c
 * @brief cpu task methods
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#include <apic.h>
#include <cpu.h>
#include <cpu/cpu_state.h>
#include <cpu/interrupt.h>
#include <cpu/task.h>
#include <cpu/crx.h>
#include <cpu/sync.h>
#include <memory/paging.h>
#include <memory/frame.h>
#include <list.h>
#include <time/timer.h>
#include <logging.h>
#include <systeminfo.h>
#include <linker.h>
#include <utils.h>
#include <hashmap.h>
#include <stdbufs.h>
#include <hypervisor/hypervisor_vmx_ops.h>
#include <hypervisor/hypervisor_vm.h>
#include <hypervisor/hypervisor_vmx_macros.h>
#include <strings.h>

MODULE("turnstone.kernel.cpu.task.utils");

void video_text_print(const char_t* str);

extern volatile cpu_state_t __seg_gs * cpu_state;
extern hashmap_t* task_map;

uint64_t task_get_id(void) {
    uint64_t id = apic_get_local_apic_id() + 1;

    task_t* current_task = task_get_current_task();

    if(current_task) {
        id = current_task->task_id;
    }

    return id;
}

uint64_t task_get_cpu_id(void) {
    return apic_get_local_apic_id();
}

void task_current_task_sleep(uint64_t wake_tick) {
    task_t* current_task = task_get_current_task();

    if(current_task) {
        current_task->wake_tick = wake_tick;
        current_task->state = TASK_STATE_SLEEPING;
        task_yield();
    }
}

void task_clear_message_waiting(uint64_t tid) {
    task_t* task = (task_t*)hashmap_get(task_map, (void*)tid);

    if(task) {
        task->state = TASK_STATE_SUSPENDED;
    } else {
        PRINTLOG(TASKING, LOG_ERROR, "task not found 0x%llx", tid);
    }
}

void task_set_interrupt_received(uint64_t tid) {
    task_t* task = (task_t*)hashmap_get(task_map, (void*)tid);
    task_t* current_task = cpu_state->current_task;

    if(task) {
        task->state = TASK_STATE_INTERRUPT_RECEIVED;

        if(current_task->cpu_id != task->cpu_id) {
            apic_send_ipi(task->cpu_id, 0xFE, false);
        }

    } else {
        video_text_print("int recv: task not found\n");
        PRINTLOG(TASKING, LOG_ERROR, "task not found 0x%llx", tid);
    }
}

void task_set_message_received(uint64_t tid) {
    task_t* task = (task_t*)hashmap_get(task_map, (void*)tid);
    task_t* current_task = cpu_state->current_task;

    if(task) {
        task->state = TASK_STATE_SUSPENDED;

        if(current_task->cpu_id != task->cpu_id) {
            apic_send_ipi(task->cpu_id, 0xFE, false);
        }

    } else {
        video_text_print("msg recv: task not found\n");
        PRINTLOG(TASKING, LOG_ERROR, "task not found 0x%llx", tid);
    }
}

void task_set_interruptible(void) {
    task_t* current_task = task_get_current_task();

    if(current_task) {
        current_task->attributes |= TASK_ATTRIBUTE_INTERRUPTIBLE;
    }
}

void task_print_all(buffer_t* buffer) {
    iterator_t* it = hashmap_iterator_create(task_map);

    while(it->end_of_iterator(it) != 0) {
        const task_t* task = it->get_item(it);


        uint64_t msgcount = 0;

        if(task->message_queues) {
            for(uint64_t q_idx = 0; q_idx < list_size(task->message_queues); q_idx++) {
                list_t* q = (list_t*)list_get_data_at_position(task->message_queues, q_idx);

                msgcount += list_size(q);
            }
        }

        memory_heap_stat_t stat = {0};
        memory_get_heap_stat_ext(task->heap, &stat);

        buffer_printf(buffer,
                      "\ttask %s 0x%llx 0x%p on cpu 0x%llx switched 0x%llx\n"
                      "\t\tstack at 0x%llx-0x%llx heap at 0x%p[0x%llx] stack 0x%p[0x%llx]\n"
                      "\t\tstate %d attributes 0x%x\n"
                      "\t\tmessage queues %lli messages %lli\n"
                      "\t\theap malloc 0x%llx free 0x%llx diff 0x%llx\n",
                      task->task_name, task->task_id, task, task->cpu_id, task->task_switch_count,
                      task->registers->rsp, task->registers->rbp, task->heap, task->heap_size,
                      task->stack, task->stack_size,
                      task->state, task->attributes, list_size(task->message_queues), msgcount,
                      stat.malloc_count, stat.free_count, stat.malloc_count - stat.free_count
                      );

        it = it->next(it);
    }

    it->destroy(it);
}

buffer_t* task_build_task_list(void) {
    uint64_t task_count = hashmap_size(task_map) + 1;

    buffer_t* buffer = buffer_new_with_capacity(NULL, task_count * sizeof(task_list_item_t));

    if(!buffer) {
        return NULL;
    }

    iterator_t* it = hashmap_iterator_create(task_map);

    if(!it) {
        buffer_destroy(buffer);
        return NULL;
    }

    while(it->end_of_iterator(it) != 0) {
        const task_t* task = it->get_item(it);


        uint64_t msgcount = 0;

        if(task->message_queues) {
            for(uint64_t q_idx = 0; q_idx < list_size(task->message_queues); q_idx++) {
                list_t* q = (list_t*)list_get_data_at_position(task->message_queues, q_idx);

                msgcount += list_size(q);
            }
        }

        memory_heap_stat_t stat = {0};

        memory_get_heap_stat_ext(task->heap, &stat);

        task_list_item_t item = {
            .task_name = task->task_name,
            .task_address = (uint64_t)task,
            .task_id = task->task_id,
            .cpu_id = task->cpu_id,
            .task_switch_count = task->task_switch_count,
            .rsp = task->registers->rsp,
            .rbp = task->registers->rbp,
            .heap_address = (uint64_t)task->heap,
            .heap_size = task->heap_size,
            .stack_address = (uint64_t)task->stack,
            .stack_size = task->stack_size,
            .state = task->state,
            .attributes = task->attributes,
            .message_queues = list_size(task->message_queues),
            .messages = msgcount,
            .malloc_count = stat.malloc_count,
            .free_count = stat.free_count,
            .heap_diff = stat.malloc_count - stat.free_count,
            .has_virtual_machine = task->vm != NULL,
        };

        if(buffer_append_bytes(buffer, (uint8_t*)&item, sizeof(task_list_item_t)) == NULL) {
            buffer_destroy(buffer);
            it->destroy(it);
            return NULL;
        }

        it = it->next(it);
    }

    it->destroy(it);

    return buffer;
}

buffer_t* task_get_task_input_buffer(uint64_t tid) {
    task_t* task = (task_t*)hashmap_get(task_map, (void*)tid);

    if(task) {
        return task->input_buffer;
    }

    return NULL;
}

buffer_t* task_get_task_output_buffer(uint64_t tid) {
    task_t* task = (task_t*)hashmap_get(task_map, (void*)tid);

    if(task) {
        return task->output_buffer;
    }

    return NULL;
}

buffer_t* task_get_task_error_buffer(uint64_t tid) {
    task_t* task = (task_t*)hashmap_get(task_map, (void*)tid);

    if(task) {
        return task->error_buffer;
    }

    return NULL;
}

buffer_t* task_get_input_buffer(void) {
    buffer_t* buffer = NULL;

    task_t* current_task = task_get_current_task();

    if(current_task) {
        buffer = current_task->input_buffer;
    }

    return buffer;
}

buffer_t* task_get_output_buffer(void) {
    buffer_t* buffer = NULL;

    task_t* current_task = task_get_current_task();

    if(current_task) {
        buffer = current_task->output_buffer;
    }

    return buffer;
}

buffer_t* task_get_error_buffer(void) {
    buffer_t* buffer = NULL;

    task_t* current_task = task_get_current_task();

    if(current_task) {
        buffer = current_task->error_buffer;
    }

    return buffer;
}

int8_t task_set_input_buffer(buffer_t* buffer) {
    if(!buffer) {
        return -1;
    }

    task_t* current_task = task_get_current_task();

    if(!current_task) {
        return -1;
    }

    current_task->input_buffer = buffer;

    return 0;
}

int8_t task_set_output_buffer(buffer_t * buffer) {
    if(!buffer) {
        return -1;
    }

    task_t* current_task = task_get_current_task();

    if(!current_task) {
        return -1;
    }

    current_task->output_buffer = buffer;

    return 0;
}

int8_t task_set_error_buffer(buffer_t * buffer) {
    if(!buffer) {
        return -1;
    }

    task_t* current_task = task_get_current_task();

    if(!current_task) {
        return -1;
    }

    current_task->error_buffer = buffer;

    return 0;
}

void task_set_vmcs_physical_address(uint64_t vmcs_physical_address) {
    task_t* current_task = task_get_current_task();

    if(current_task) {
        current_task->vmcs_physical_address = vmcs_physical_address;
    }
}

uint64_t task_get_vmcs_physical_address(void) {
    uint64_t vmcs_physical_address = 0;

    task_t* current_task = task_get_current_task();

    if(current_task) {
        vmcs_physical_address = current_task->vmcs_physical_address;
    }

    return vmcs_physical_address;
}

void task_set_vm(void* vm) {
    task_t* current_task = task_get_current_task();

    if(current_task) {
        current_task->vm = vm;
    }
}

void* task_get_vm(void) {
    void* vm = NULL;

    task_t* current_task = task_get_current_task();

    if(current_task) {
        vm = current_task->vm;
    }

    return vm;
}

void task_add_message_queue(list_t* queue){
    task_t* current_task = task_get_current_task();

    if(!current_task) {
        return;
    }

    if(current_task->message_queues == NULL) {
        current_task->message_queues = list_create_list();
    }

    list_list_insert(current_task->message_queues, queue);
}

list_t* task_get_message_queue(uint64_t task_id, uint64_t queue_number) {
    const task_t* task = hashmap_get(task_map, (void*)task_id);

    if(task == NULL) {
        return NULL;
    }

    if(task->message_queues == NULL) {
        return NULL;
    }

    return (list_t*)list_get_data_at_position(task->message_queues, queue_number);
}

void task_set_message_waiting(void){
    task_t* current_task = task_get_current_task();

    if(current_task) {
        current_task->state = TASK_STATE_MESSAGE_WAITING;
    }
}

void task_toggle_wait_for_future(uint64_t tid) {
    if(tid == 0 || tid == apic_get_local_apic_id() + 1) {
        return;
    }

    task_t* task = (task_t*)hashmap_get(task_map, (void*)tid);

    if(task) {
        char_t buf[64] = {0};
        itoa_with_buffer(buf, task->task_id);
        video_text_print("task id: ");
        video_text_print(buf);
        video_text_print(" wait for future: ");
        video_text_print(task->state == TASK_STATE_FUTURE_WAITING ? "true" : "false");
        video_text_print("\n");

        task->state = task->state == TASK_STATE_FUTURE_WAITING ? TASK_STATE_SUSPENDED : TASK_STATE_FUTURE_WAITING;
    }
}

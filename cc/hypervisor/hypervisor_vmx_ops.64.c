/**
 * @file hypervisor_vmx_ops.64.c
 * @brief VMX operations for 64-bit x86 architecture.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <hypervisor/hypervisor_vmx_ops.h>
#include <hypervisor/hypervisor_vmx_macros.h>
#include <logging.h>

MODULE("turnstone.hypervisor");

int8_t vmx_write(uint64_t field, uint64_t value) {
    uint8_t err = 0;

    asm volatile ("vmwrite %[value], %[field]\n"
                  "setna %[err]\n"
                  : [err] "=rm" (err)
                  : [value] "rm" (value), [field] "rm" (field)
                  : "cc");

    return err;
}


uint64_t vmx_read(uint64_t field) {
    uint64_t value = 0;

    asm volatile ("vmread %[field], %[value]\n"
                  : [value] "=rm" (value)
                  : [field] "rm" (field)
                  : "cc");

    return value;
}

int8_t vmxon(uint64_t vmxon_pa) {
    uint8_t err = 0;

    asm volatile ("vmxon %[pa]\n"
                  "setna %[err]\n"
                  : [err] "=rm" (err)
                  : [pa] "m" (vmxon_pa)
                  : "cc", "memory");

    return err;
}

int8_t vmptrld(uint64_t vmcs_pa) {
    uint8_t err = 0;

    asm volatile ("vmptrld %[pa]\n"
                  "setna %[err]\n"
                  : [err] "=rm" (err)
                  : [pa] "m" (vmcs_pa)
                  : "cc", "memory");

    return err;
}

int8_t vmclear(uint64_t vmcs_pa) {
    uint8_t err = 0;

    asm volatile ("vmclear %[pa]\n"
                  "setna %[err]\n"
                  : [err] "=rm" (err)
                  : [pa] "m" (vmcs_pa)
                  : "cc", "memory");

    return err;
}

int8_t vmlaunch(void) {
    uint8_t err = 0;

    asm volatile ("vmlaunch\n"
                  "setna %[err]\n"
                  : [err] "=rm" (err)
                  :
                  : "cc");

    if(err) {
        uint64_t vm_instruction_error = vmx_read(VMX_VM_INSTRUCTION_ERROR);
        PRINTLOG(HYPERVISOR, LOG_ERROR, "vmlaunch failed: %llx", vm_instruction_error);
    }

    return err;
}

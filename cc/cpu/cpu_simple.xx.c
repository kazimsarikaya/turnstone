/**
 * @file cpu_simple.xx.c
 * @brief cpu commands implementation that supports both real and long mode.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#include <cpu.h>
#include <logging.h>
#include <strings.h>

MODULE("turnstone.kernel.cpu");

void cpu_hlt(void) {
    for(;;) {
        cpu_cli();
        __asm__ __volatile__ ("hlt");
    }
}

void cpu_idle(void) {
    __asm__ __volatile__ ("hlt");
}


uint16_t cpu_read_data_segment(void){
    uint16_t ds;
    __asm__ __volatile__ ("mov %%ds, %0" : "=r" (ds));
    return ds;
}

int8_t cpu_check_rdrand(void){
    cpu_cpuid_regs_t query = {0x00000001, 0, 0, 0};
    cpu_cpuid_regs_t answer = {0, 0, 0, 0};

    if(cpu_cpuid(query, &answer) != 0) {
        return -1;
    }

    if(((answer.ecx >> 30) & 1) == 1) {
        return 0;
    }

    return -1;
}


uint8_t cpu_cpuid(cpu_cpuid_regs_t query, cpu_cpuid_regs_t* answer){
    __asm__ __volatile__ ("cpuid\n"
                          : "=a" (answer->eax), "=b" (answer->ebx), "=c" (answer->ecx), "=d" (answer->edx)
                          : "a" (query.eax), "b" (query.ebx), "c" (query.ecx), "d" (query.edx)
                          );
    if(answer->eax == 0 && answer->ebx == 0 && answer->ecx == 0 && answer->edx == 0) {
        PRINTLOG(KERNEL, LOG_FATAL, "CPU: Fatal cpuid failed");

        return -1;
    }

    return 0;
}

boolean_t cpu_is_interrupt_enabled(void) {
    uint64_t rflags;
    __asm__ __volatile__ ("pushfq\n"
                          "popq %0\n"
                          : "=r" (rflags)
                          :
                          : "memory"
                          );

    return (rflags & 0x200) == 0x200;
}

static cpu_type_t cpu_type = CPU_TYPE_UNKNOWN;
static boolean_t cpu_type_initialized = false;

cpu_type_t cpu_get_type(void) {
    if(cpu_type_initialized) {
        return cpu_type;
    }

    cpu_cpuid_regs_t query = {0};
    cpu_cpuid_regs_t result;

    query.eax = 0x0;
    cpu_cpuid(query, &result);

    char_t vendor_id[13] = {0};

    *(uint32_t*)&vendor_id[0] = result.ebx;
    *(uint32_t*)&vendor_id[4] = result.edx;
    *(uint32_t*)&vendor_id[8] = result.ecx;

    PRINTLOG(KERNEL, LOG_TRACE, "Vendor ID: %s", vendor_id);

    if(strcmp(vendor_id, "GenuineIntel") == 0) {
        cpu_type = CPU_TYPE_INTEL;
    } else if(strcmp(vendor_id, "AuthenticAMD") == 0) {
        cpu_type = CPU_TYPE_AMD;
    } else {
        cpu_type = CPU_TYPE_UNKNOWN;
    }

    cpu_type_initialized = true;

    return cpu_type;
}

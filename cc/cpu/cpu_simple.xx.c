/**
 * @file cpu_simple.xx.c
 * @brief cpu commands implementation that supports both real and long mode.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#include <cpu.h>
#include <video.h>

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
        printf("CPU: Fatal cpuid failed\n");

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

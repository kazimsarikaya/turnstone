/**
 * @file cpu.64.c
 * @brief 64 bit cpu operations
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <cpu.h>
#include <cpu/crx.h>

MODULE("turnstone.kernel.cpu");

uint64_t cpu_read_msr(uint32_t msr_address){
    uint64_t low_part, high_part;
    asm volatile ("rdmsr" : "=a" (low_part), "=d" (high_part) : "c" (msr_address));
    return (high_part << 32) | low_part;
}

int8_t cpu_write_msr(uint32_t msr_address, uint64_t value) {
    uint64_t low_part = (value & 0xFFFFFFFF), high_part = ((value >> 32) & 0xFFFFFFFF);
    asm volatile ("wrmsr" : : "a" (low_part), "d" (high_part), "c" (msr_address));
    return 0;
}

uint64_t cpu_read_cr2(void) {
    uint64_t cr2 = 0;
    asm volatile ("mov %%cr2, %0\n"
                  : "=r" (cr2));
    return cr2;
}

uint64_t cpu_read_cr3(void) {
    uint64_t cr3 = 0;
    asm volatile ("mov %%cr3, %0\n"
                  : "=r" (cr3));
    return cr3;
}


cpu_reg_cr4_t cpu_read_cr4(void) {
    cpu_reg_cr4_t cr4;
    asm volatile ("mov %%cr4, %0\n"
                  : "=r" (cr4));
    return cr4;
}

void cpu_write_cr4(cpu_reg_cr4_t cr4) {
    asm volatile ("mov %0, %%cr4\n"
                  : : "r" (cr4));
}


cpu_reg_cr0_t cpu_read_cr0(void) {
    cpu_reg_cr0_t cr0;
    asm volatile ("mov %%cr0, %0\n"
                  : "=r" (cr0));
    return cr0;
}

void cpu_write_cr0(cpu_reg_cr0_t cr0) {
    asm volatile ("mov %0, %%cr0\n"
                  : : "r" (cr0));
}

void cpu_toggle_cr0_wp(void) {
    cpu_reg_cr0_t cr0 = cpu_read_cr0();

    cr0.fields.write_protect = ~cr0.fields.write_protect;

    cpu_write_cr0(cr0);
}

void cpu_cr0_disable_wp(void) {
    cpu_reg_cr0_t cr0 = cpu_read_cr0();

    cr0.fields.write_protect = 0;

    cpu_write_cr0(cr0);
}

void cpu_cr0_enable_wp(void) {
    cpu_reg_cr0_t cr0 = cpu_read_cr0();

    cr0.fields.write_protect = 1;

    cpu_write_cr0(cr0);
}

__attribute__((target("general-regs-only"))) static inline uint64_t xgetbv(uint32_t index) {
    uint32_t eax, edx;
    asm volatile ("xgetbv" : "=a" (eax), "=d" (edx) : "c" (index));
    return ((uint64_t)edx << 32) | eax;
}

__attribute__((target("general-regs-only"))) static inline void xsetbv(uint32_t index, uint64_t value) {
    uint32_t eax = (uint32_t)value;
    uint32_t edx = (uint32_t)(value >> 32);
    asm volatile ("xsetbv" : : "c" (index), "a" (eax), "d" (edx));
}

__attribute__((target("general-regs-only"))) static inline void cpu_enable_avx(void) {
    uint64_t xcr0 = xgetbv(0);

    cpu_cpuid_regs_t query  = {.eax = 0xd};
    cpu_cpuid_regs_t answer = {0};

    cpu_cpuid(query, &answer);
    uint64_t supported_xcr0 = answer.eax | (uint64_t)answer.edx << 32;

    if (supported_xcr0 & (1 << 0)) {
        xcr0 |= (1 << 0); // x87
    }
    if (supported_xcr0 & (1 << 1)) {
        xcr0 |= (1 << 1); // SSE
    }
    if (supported_xcr0 & (1 << 2)) {
        xcr0 |= (1 << 2); // AVX
    }
    if (supported_xcr0 & (1 << 5)) {
        xcr0 |= (1 << 5); // opmask
    }
    if (supported_xcr0 & (1 << 6)) {
        xcr0 |= (1 << 6); // ZMM_Hi256
    }
    if (supported_xcr0 & (1 << 7)) {
        xcr0 |= (1 << 7); // Hi16_ZMM

    }
    xsetbv(0, xcr0);
}

__attribute__((target("general-regs-only"))) void cpu_enable_sse(void) {
    cpu_reg_cr0_t cr0 = cpu_read_cr0();
    cr0.fields.monitor_coprocessor = 1;
    cr0.fields.emulation           = 0;
    cr0.fields.task_switched       = 0;
    cr0.fields.numeric_error       = 1;
    cr0.fields.write_protect       = 1;
    cpu_write_cr0(cr0);

    cpu_reg_cr4_t cr4 = cpu_read_cr4();
    cr4.fields.os_fx_support                 = 1;
    cr4.fields.os_unmasked_exception_support = 1;
    cr4.fields.os_xsave_enable               = 1;
    cpu_write_cr4(cr4);

    cpu_enable_avx();
}

void cpu_clear_segments(void) {
    asm volatile (
        "mov $0x0, %ax\n"
        "mov %ax, %ds\n"
        "mov %ax, %es\n"
        "mov %ax, %fs\n"
        "mov %ax, %gs\n"
        );
}

uint64_t cpu_read_fs_base(void) {
    uint64_t fs_base = cpu_read_msr(CPU_MSR_IA32_FS_BASE);
    return fs_base;
}

uint64_t cpu_read_gs_base(void) {
    uint64_t gs_base = cpu_read_msr(CPU_MSR_IA32_GS_BASE);
    return gs_base;
}

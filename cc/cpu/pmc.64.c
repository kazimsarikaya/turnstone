/**
 * @file pmc.64.c
 * @brief peformance monitoring counter (PMC) implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */


#include <cpu/pmc.h>
#include <cpu.h>
#include <stdbufs.h>
#include <logging.h>

MODULE("turnstone.kernel.cpu.pmc");


static void cpu_amd_pmc_start(void) {
    // Clean slate for the 4 reliable counters
    for (int32_t i = 0; i < 4; i++) {
        cpu_write_msr(MSR_AMD_PERF_EVTSEL0 + i, 0);
        cpu_write_msr(MSR_AMD_PMC0 + i, 0);
    }

    uint64_t flags = (1ULL << 16) | (1ULL << 17) | (1ULL << 22);

    // PMC0: Instructions Retired (The baseline)
    cpu_write_msr(MSR_AMD_PERF_EVTSEL0, (0xC0 | flags));

    // PMC1: L1 Data Cache Misses (Reliable on KVM)
    cpu_write_msr(MSR_AMD_PERF_EVTSEL1, (0x43 | (0x01 << 8) | flags));

    // PMC2: DTLB Misses (The "Stress" metric for your USB buffers)
    cpu_write_msr(MSR_AMD_PERF_EVTSEL2, (0x45 | (0xFF << 8) | flags));

    // PMC3: ITLB Misses (The "Stress" metric for your Randomized Modules)
    cpu_write_msr(MSR_AMD_PERF_EVTSEL3, (0x84 | flags));
}

void cpu_pmc_start(void) {
    if(cpu_get_type() == CPU_TYPE_AMD) {
        cpu_amd_pmc_start();
    } else {
        PRINTLOG(KERNEL, LOG_ERROR, "Unsupported CPU type for PMC");
    }
}

static void cpu_amd_pmc_print_results(void) {
    // Reading only the 4 MSRs that KVM whitelists on your host
    uint64_t instr     = cpu_read_msr(MSR_AMD_PMC0); // Retired Instructions
    uint64_t l1d_miss  = cpu_read_msr(MSR_AMD_PMC1); // L1 Data Misses
    uint64_t dtlb_miss = cpu_read_msr(MSR_AMD_PMC2); // DTLB Misses
    uint64_t itlb_miss = cpu_read_msr(MSR_AMD_PMC3); // ITLB Misses

    // Using float64_t as per your Turnstone OS type definitions
    float64_t l1d_mpki  = 0.0;
    float64_t dtlb_mpki = 0.0;
    float64_t itlb_mpki = 0.0;

    if (instr > 0) {
        // MPKI = (Misses * 1000) / Instructions
        float64_t instr_k = (float64_t)instr / 1000.0;

        l1d_mpki  = (float64_t)l1d_miss  / instr_k;
        dtlb_mpki = (float64_t)dtlb_miss / instr_k;
        itlb_mpki = (float64_t)itlb_miss / instr_k;
    }

    printf("\n[Turnstone OS] PMC Analysis (KVM-Safe)\n");
    printf("----------------------------------------\n");
    printf("Instructions Retired : %llu\n", instr);
    printf("L1 Data Misses       : %llu (MPKI: %.3f)\n", l1d_miss, l1d_mpki);
    printf("DTLB Misses          : %llu (MPKI: %.3f)\n", dtlb_miss, dtlb_mpki);
    printf("ITLB Misses          : %llu (MPKI: %.3f)\n", itlb_miss, itlb_mpki);
    printf("----------------------------------------\n");

    // Architect's Note: If ITLB MPKI > 0.5, randomization is hurting locality.
    if (itlb_mpki > 0.5) {
        printf("Warning: High ITLB pressure detected.\n");
    }

    for (int32_t i = 0; i < 4; i++) {
        // Reset the counters for the next measurement interval
        cpu_write_msr(MSR_AMD_PMC0 + i, 0);
    }
}

void cpu_pmc_print_results(void) {
    if(cpu_get_type() == CPU_TYPE_AMD) {
        cpu_amd_pmc_print_results();
    } else {
        PRINTLOG(KERNEL, LOG_ERROR, "Unsupported CPU type for PMC");
    }
}

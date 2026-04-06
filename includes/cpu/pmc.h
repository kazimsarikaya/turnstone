/**
 * @file pmc.h
 * @brief peformance monitoring counter (PMC) interface
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___PMC_H
/*! prevent duplicate header error macro */
#define ___PMC_H 0

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Modern AMD Core Performance Monitor Counters (MSRs 0xC0010200 - 0xC001020B) */

// Event Selection Registers (Control - Even Addresses)
#define MSR_AMD_PERF_EVTSEL0    0xC0010200
#define MSR_AMD_PERF_EVTSEL1    0xC0010202
#define MSR_AMD_PERF_EVTSEL2    0xC0010204
#define MSR_AMD_PERF_EVTSEL3    0xC0010206
#define MSR_AMD_PERF_EVTSEL4    0xC0010208
#define MSR_AMD_PERF_EVTSEL5    0xC001020A

// Performance Counter Registers (Data - Odd Addresses)
#define MSR_AMD_PMC0            0xC0010201
#define MSR_AMD_PMC1            0xC0010203
#define MSR_AMD_PMC2            0xC0010205
#define MSR_AMD_PMC3            0xC0010207
#define MSR_AMD_PMC4            0xC0010209
#define MSR_AMD_PMC5            0xC001020B

void cpu_pmc_start(void);
void cpu_pmc_print_results(void);

#ifdef __cplusplus
}
#endif

#endif

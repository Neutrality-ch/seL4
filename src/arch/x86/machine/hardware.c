/*
 * Copyright 2014, General Dynamics C4 Systems
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(GD_GPL)
 */

#include <types.h>
#include <machine/registerset.h>
#include <model/statedata.h>
#include <object/structures.h>
#include <arch/machine.h>
#include <arch/machine/hardware.h>
#include <arch/machine/registerset.h>
#include <arch/linker.h>

/* initialises MSRs required to setup sysenter and sysexit */
BOOT_CODE void
init_sysenter_msrs(void)
{
    x86_wrmsr(IA32_SYSENTER_CS_MSR,  (uint64_t)(word_t)SEL_CS_0);
    x86_wrmsr(IA32_SYSENTER_EIP_MSR, (uint64_t)(word_t)&handle_syscall);
    x86_wrmsr(IA32_SYSENTER_ESP_MSR, (uint64_t)(word_t)&x86KStss.words[1]);
}

word_t PURE getRestartPC(tcb_t *thread)
{
    return getRegister(thread, FaultIP);
}

void setNextPC(tcb_t *thread, word_t v)
{
    setRegister(thread, NextIP, v);
}

/* Returns the size of CPU's cacheline */
BOOT_CODE uint32_t CONST
getCacheLineSizeBits(void)
{
    uint32_t line_size;
    uint32_t n;

    line_size = getCacheLineSize();
    if (line_size == 0) {
        printf("Cacheline size must be >0\n");
        return 0;
    }

    /* determine size_bits */
    n = 0;
    while (!(line_size & 1)) {
        line_size >>= 1;
        n++;
    }

    if (line_size != 1) {
        printf("Cacheline size must be a power of two\n");
        return 0;
    }

    return n;
}

/* Flushes a specific memory range from the CPU cache */

void flushCacheRange(void* vaddr, uint32_t size_bits)
{
    uint32_t v;

    assert(size_bits < WORD_BITS);
    assert(IS_ALIGNED((uint32_t)vaddr, size_bits));

    x86_mfence();

    for (v = ROUND_DOWN((uint32_t)vaddr, x86KScacheLineSizeBits);
            v < (uint32_t)vaddr + BIT(size_bits);
            v += BIT(x86KScacheLineSizeBits)) {
        flushCacheLine((void*)v);
    }
    x86_mfence();
}

/* Disables as many prefetchers as possible */
BOOT_CODE bool_t
disablePrefetchers()
{
    uint32_t version_info;
    uint32_t low, high;
    word_t i;

    uint32_t valid_models[] = { BROADWELL_MODEL_ID, HASWELL_MODEL_ID, IVY_BRIDGE_MODEL_ID,
                                SANDY_BRIDGE_1_MODEL_ID, SANDY_BRIDGE_2_MODEL_ID, WESTMERE_1_MODEL_ID, WESTMERE_2_MODEL_ID,
                                WESTMERE_3_MODEL_ID, NEHALEM_1_MODEL_ID, NEHALEM_2_MODEL_ID, NEHALEM_3_MODEL_ID
                              };

    version_info = x86_cpuid_eax(0x1, 0x0);

    for (i = 0; i < ARRAY_SIZE(valid_models); ++i) {
        if (MODEL_ID(version_info) == valid_models[i]) {
            low = x86_rdmsr_low(IA32_PREFETCHER_MSR);
            high = x86_rdmsr_high(IA32_PREFETCHER_MSR);

            low |= IA32_PREFETCHER_MSR_L2;
            low |= IA32_PREFETCHER_MSR_L2_ADJACENT;
            low |= IA32_PREFETCHER_MSR_DCU;
            low |= IA32_PREFETCHER_MSR_DCU_IP;

            x86_wrmsr(IA32_PREFETCHER_MSR, ((uint64_t)high) << 32 | low);

            return true;
        }
    }

    printf("Disabling prefetchers not implemented for CPU model: %x\n", MODEL_ID(version_info));
    return false;
}

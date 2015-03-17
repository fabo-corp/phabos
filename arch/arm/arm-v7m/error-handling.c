/*
 * Copyright (C) 2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#include <phabos/kprintf.h>
#include <asm/hwio.h>
#include <stdlib.h>

#define CFSR  0xe000ed28
#define MMFSR 0xe000ed28
#define BFSR  0xe000ed29
#define UFSR  0xe000ed2a
#define HFSR  0xe000ed2c
#define MMAR  0xe000ed34
#define BFAR  0xe000ed38
#define AFSR  0xe000ed3c

#define BFSR_BFARVALID      (1 << 7)
#define MMFSR_MMARVALID     (1 << 7)
#define HFSR_VECTTBL        (1 << 1)

static void explain_register(const char *name, const char *fullname,
                             uint32_t addr, const char *description)
{
    kprintf("%s (%s): %#X\n", name, fullname, addr);
    if (description)
        kprintf("\t-> %s.\n", description);
}

static void  explain_bit(uint32_t value, unsigned bit,
                         const char *name, const char *description)
{
    if (value & (1 << bit)) {
        kprintf("\t* [%u] %s  = 1\n", bit, name);
        kprintf("\t\t-> %s.\n", description);
    }
}

static void analyze_bfsr(void)
{
    uint8_t bfsr = read8(BFSR);
    uint32_t bfar = read32(BFAR);

    if (!bfsr)
        return;

    explain_register("BFSR", "Bus Fault Status Register", bfsr, NULL);
    explain_bit(bfsr, 7, "BFARVALID", "BFAR is a valid address");
    explain_bit(bfsr, 4, "STKERR", "Faulted from stacking something from a "
                         "exception entry");
    explain_bit(bfsr, 3, "UNSTKERR", "Faulted from unstacking something for a "
                         "return from an exception");
    explain_bit(bfsr, 2, "IMPRECISERR", "Faulted when reading/writing data. No "
                         "way to know for sure where it happened");
    explain_bit(bfsr, 1, "PRECISERR", "See BFAR to get the address that "
                         "generated the bus fault");
    explain_bit(bfsr, 0, "IBUSERR", "Bus fault while issuing the faulting "
                         "instruction");

    if (bfsr & BFSR_BFARVALID) {
        kprintf("\n");
        explain_register("BFAR", "BusFault Address Register", bfar,
                         "Address that generated the Bus Fault");
    }
}

static void analyze_hfsr(void)
{
    uint32_t hfsr = read32(HFSR);

    if (!hfsr)
        return;

    explain_register("HFSR", "Hard Fault Status Register", hfsr, NULL);
    explain_bit(hfsr, 30, "FORCED", "HardFaulted because a fault occured while "
                                    "processing another interrupt");
    explain_bit(hfsr, 1, "VECTTBL", "BusFault on a vector table read during "
                                    "exception processing");

    if (hfsr & HFSR_VECTTBL) {
        kprintf("\n");
        analyze_bfsr();
    }
}

static void analyze_mmfsr(void)
{
    uint8_t mmfsr = read8(MMFSR);
    uint32_t mmar = read32(MMAR);

    if (!mmfsr)
        return;

    explain_register("MMFSR", "Memory Manage Fault Status Register",
                     mmfsr, NULL);
    explain_bit(mmfsr, 7, "MMARVALID", "MMAR holds a valid fault address");
    explain_bit(mmfsr, 4, "MSTKERR", "Stacking on exception entry caused "
                          "memory access violation");
    explain_bit(mmfsr, 3, "MUNSTKERR", "Unstacking on exception return caused "
                          "memory access violation");
    explain_bit(mmfsr, 1, "DACCVIOL", "LDR/STR to a memory location that does "
                          "not permit that operation");
    explain_bit(mmfsr, 0, "IACCVIOL", "Instruction fetch of a memory location "
                          "that does not permit execution");

    if (mmfsr & MMFSR_MMARVALID) {
        kprintf("\n");
        explain_register("MMAR", "MemoryManage Address Register", mmar,
                         "Address that generated the MemoryManage Fault");
    }
}

static void analyze_ufsr(void)
{
    uint16_t ufsr = read16(UFSR);

    if (!ufsr)
        return;

    explain_register("UFSR", "Usage Fault Status Register", ufsr, NULL);
    explain_bit(ufsr, 9, "DIVBYZERO", "Fault because of a divide by zero");
    explain_bit(ufsr, 8, "UNALIGNED", "Fault because of an unaligned memory "
                         "access");
    explain_bit(ufsr, 3, "NOCP", "Fault because CPU does not support "
                                 "Coprocessor instructions");
    explain_bit(ufsr, 2, "INVPC", "Invalid PC load by EXC_RETURN (from "
                                  "interrupt return)");
}

void analyze_status_registers(void)
{
    analyze_hfsr();
    analyze_bfsr();
    analyze_mmfsr();
    analyze_ufsr();
}

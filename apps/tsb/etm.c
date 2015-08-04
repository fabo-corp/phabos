/**
 * Copyright (c) 2014-2015 Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <config.h>

#include <asm/hwio.h>

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define TSB_PIN_ETM             (1 << 4)

#define DRIVE_MA_MIN         2
#define DRIVE_MA_DEFAULT     4
#define DRIVE_MA_MAX         8

#define TPIU_CPSR            0xE0040004
#define TPIU_SPPR            0xE00400F0

#define ETMCR                0xE0041000
#define ETMTRIGGER           0xE0041008

#define ETMTEEVR             0xE0041020
#define ETMTRACEIDR          0xE0041200
#define ETMLAR               0xE0041FB0
#define ETMLSR               0xE0041FB4

#define ETM_UNLOCK           0xC5ACCE55 /* Magic value for ETMLAR to unlock ETM access */
#define ETM_LOCK             0x0        /* Less magic lock value for ETMLAR */

#define ETMLSR_LOCKED        0x2        /* Bit is set in ETMLSR when ETM locked */

#define ETMCR_PROGRAM        0x00022C40 /* ETM is awake and in programming mode */
#define ETMCR_OPERATE        0x00022840 /* ETM is awake and in operational mode */

#define ETM_EVENT_ALWAYS     0x6F       /* ETM decodes value as ALWAYS */
#define ETM_EVENT_NEVER      0x406F     /* ETM decodes value as !ALWAYS */

enum tsb_drivestrength {
    tsb_ds_min     = 0,
    tsb_ds_default,
    tsb_ds_max,
    tsb_ds_invalid,
};

#define DRIVESTRENGTH_OFFSET(driver) (((driver >> 22) & 0xfc))
#define DRIVESTRENGTH_SHIFT(driver)  (driver & 0x1f)
#define DRIVESTRENGTH_MASK(driver)  (0x3 << DRIVESTRENGTH_SHIFT(driver))

void tsb_set_pinshare(uint32_t pin);
void tsb_clr_pinshare(uint32_t pin);
uint32_t tsb_get_pinshare(void);
void tsb_set_drivestrength(uint32_t ds_id, enum tsb_drivestrength value);
enum tsb_drivestrength tsb_get_drivestrength(uint32_t ds_id);

#define TSB_SCM_REG0  (0 << 24)
#define TSB_SCM_REG1  (1 << 24) /* I2S / Unipro / HSIC */
#define TSB_SCM_REG2  (2 << 24) /* CDSI */

#ifdef CONFIG_TSB_CHIP_REV_ES2
#define TSB_TRACE_DRIVESTRENGTH   (TSB_SCM_REG1 | 24)
#define TSB_SPI_DRIVESTRENGTH     (TSB_SCM_REG1 | 22)
#define TSB_PWM_DRIVESTRENGTH     (TSB_SCM_REG1 | 20)
#define TSB_I2S_DRIVESTRENGTH     (TSB_SCM_REG1 | 18)
#else /* ES1 */
#define TSB_TRACE_DRIVESTRENGTH   (TSB_SCM_REG1 | 6)
#define TSB_SPI_DRIVESTRENGTH     (TSB_SCM_REG1 | 4)
#define TSB_PWM_DRIVESTRENGTH     (TSB_SCM_REG1 | 2)
#define TSB_I2S_DRIVESTRENGTH     (TSB_SCM_REG1 | 0)
#endif

static struct {
    int enabled;
    unsigned short drive_ma;
    unsigned short drive_ma_save;
    uint32_t etm_pinshare_save;
    uint32_t cpsr_save;
    uint32_t sppr_save;
    uint32_t cr_save;
    uint32_t trigger_save;
    uint32_t teevr_save;
    uint32_t traceidr_save;
    uint32_t lsr_save;
} etm = {.drive_ma = DRIVE_MA_DEFAULT};

/* set drive strength for the TRACE lines to the specified value */
void set_trace_drive_ma(unsigned short drive_ma) {
    switch(drive_ma) {
    case DRIVE_MA_MIN:
        tsb_set_drivestrength(TSB_TRACE_DRIVESTRENGTH, tsb_ds_min);
        break;
    case DRIVE_MA_DEFAULT:
        tsb_set_drivestrength(TSB_TRACE_DRIVESTRENGTH, tsb_ds_default);
        break;
    case DRIVE_MA_MAX:
        tsb_set_drivestrength(TSB_TRACE_DRIVESTRENGTH, tsb_ds_max);
        break;
    default:
        printf("Provided drive strength value of %u milliamp is invalid\n", drive_ma);
        break;
    }
}

/* get the currently configured drive strength for the TRACE lines */
unsigned short get_trace_drive_ma(void) {
    switch(tsb_get_drivestrength(TSB_TRACE_DRIVESTRENGTH)) {
    case tsb_ds_min:
        return DRIVE_MA_MIN;
        break;
    case tsb_ds_default:
        return DRIVE_MA_DEFAULT;
        break;
    case tsb_ds_max:
        return DRIVE_MA_MAX;
        break;
    default:
        printf("Unable to decode TRACE drive strength\n");
        return 0xFFFF;
    }
}

void print_usage(void) {
    printf("ETM: Usage:\n");
    printf("ETM:  etm e [2|4|8]    : enable TRACE output lines, with optional drive milliamps\n");
    printf("ETM:  etm d            : return TRACE output lines to their orignal state\n");
    printf("ETM:  etm s            : display state of TRACE output lines\n");
}

int etm_main(int argc, char *argv[])
{
    char cmd;
    long drive_ma;
    unsigned short last_drive_ma = etm.drive_ma;

    if (argc < 2) {
        print_usage();
        return EXIT_FAILURE;
    } else {
        cmd = argv[1][0];
    }

    switch (cmd) {
    case 'h':
    case '?':
        print_usage();
        break;
    case 'e':
        /* enable ETM */
        if (argc == 2) {
            etm.drive_ma = DRIVE_MA_MAX;
        } else if (argc == 3) {
            drive_ma = strtol(argv[2], NULL, 10);
            if (drive_ma != DRIVE_MA_MIN &&
                drive_ma != DRIVE_MA_DEFAULT &&
                drive_ma != DRIVE_MA_MAX) {
                printf("Invalid drive strength of %hu ma when trying to enable ETM (%u|%u|%u are valid)\n",
                       drive_ma, DRIVE_MA_MIN, DRIVE_MA_DEFAULT, DRIVE_MA_MAX);
                return EXIT_FAILURE;
            }
            etm.drive_ma = (unsigned short) drive_ma;
        } else {
            printf("Too many arguments specified when attempting to enable ETM\n");
            return EXIT_FAILURE;
        }

        if (etm.enabled) {
            /* update drive strength if it has changed */
            if (etm.drive_ma != last_drive_ma) {
                set_trace_drive_ma(etm.drive_ma);
                printf("ETM drive strength changed to %u milliamps\n", etm.drive_ma);
            } else
                printf("WARNING: ETM is already enabled\n");
        } else {
            /*
             * perhaps we ought to be recording the old value
             * but the present API doesn't expose it
             */
            etm.etm_pinshare_save = tsb_get_pinshare() & TSB_PIN_ETM;
            tsb_set_pinshare(TSB_PIN_ETM);

            /* set drive strength for the TRACE signals to the specified value */
            etm.drive_ma_save = get_trace_drive_ma();
            set_trace_drive_ma(etm.drive_ma);

            /*
             * record current value of the ETM lock status register so that we can restore
             * the original lock state if ETM TRACE is disabled
             */
            etm.lsr_save = read32(ETMLSR);

            /* unlock ETM: may need a delay right after */
            write32(ETMLAR, ETM_UNLOCK);

            /* put ETM into programming mode: may need a delay right after */
            etm.cr_save = read32(ETMCR);
            write32(ETMCR, ETMCR_PROGRAM);

            /* Don't define a trigger for TRACE (NOT Always) */
            etm.trigger_save = read32(ETMTRIGGER);
            write32(ETMTRIGGER, ETM_EVENT_NEVER);

            /* TRACE all memory */
            etm.teevr_save = read32(ETMTEEVR);
            write32(ETMTEEVR, ETM_EVENT_ALWAYS);

            /* Set identifier for trace output (match what DTRACE selects) */
            etm.traceidr_save = read32(ETMTRACEIDR);
            write32(ETMTRACEIDR, 0x2);

            /* Set trace port width to 4 (versus 1) */
            etm.cpsr_save = read32(TPIU_CPSR);
            write32(TPIU_CPSR, 0x8);

            /* Set trace output type to parallel (versus 1 == serial) */
            etm.sppr_save = read32(TPIU_SPPR);
            write32(TPIU_SPPR, 0x0);

            /* And finally, set ETM to operational mode */
            write32(ETMCR, ETMCR_OPERATE);

            etm.enabled = 1;
            printf("ETM enabled with a drive strength of %u milliamps\n", etm.drive_ma);
        }
        break;
    case 'd':
        /* Disable ETM */
        if (!etm.enabled) {
            printf("WARNING: etm is already disabled\n");
        } else {
            /* Put ETM back into programming mode */
            write32(ETMCR, ETMCR_PROGRAM);

            /* Restore registers to saved values */
            write32(TPIU_SPPR, etm.sppr_save);
            write32(TPIU_CPSR, etm.cpsr_save);
            write32(ETMTRACEIDR, etm.traceidr_save);
            write32(ETMTEEVR, etm.teevr_save);
            write32(ETMTRIGGER, etm.trigger_save);
            write32(ETMCR, etm.cr_save);

            /* Relock ETM if it was locked when we enabled TRACE */
            if (etm.lsr_save & ETMLSR_LOCKED)
                write32(ETMLAR, ETM_LOCK);

            /* Restore the original drive strength */
            set_trace_drive_ma(etm.drive_ma_save);

            /* Clear the ETM pinshare if it wasn't set on entry */
            if (!etm.etm_pinshare_save)
                tsb_clr_pinshare(TSB_PIN_ETM);

            etm.enabled = 0;
            etm.drive_ma = 0;
        }
        break;
    case 's':
        /* Get the ETM state */
        if (etm.enabled) {
            printf("ETM state = enabled, with a drive strength of %u milliamps\n", etm.drive_ma);
        } else {
            printf("ETM state = either never enabled or returned to original state\n");
        }
        break;
    default:
        printf("ETM: Unknown command\n");
        print_usage();
        return EXIT_FAILURE;
    }

    return 0;
}

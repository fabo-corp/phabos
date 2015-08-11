/*
 * Copyright (c) 2015 Google Inc.
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

/**
 * @author: Perry Hung
 */

#define DBG_COMP DBG_SVC     /* DBG_COMP macro of the component */

#include <asm/hwio.h>
#include <asm/delay.h>
#include <asm/gpio.h>
#include <phabos/gpio.h>
#include <phabos/utils.h>

#include "up_debug.h"
#include "ara_board.h"
#include "interface.h"
#include "tsb_switch_driver_es1.h"

#define SWITCH_I2C_BUS      (2)

#define SVC_LED_RED         (GPIO_OUTPUT | GPIO_PUSHPULL | GPIO_PORTA | \
                             GPIO_OUTPUT_SET | GPIO_PIN7)

/*
 * How long to leave hold each regulator before the next.
 */
#define HOLD_TIME_1P1                   (50000) // 0-100ms before 1p2, 1p8
#define HOLD_TIME_1P8                   (0)
#define HOLD_TIME_1P2                   (0)

#define HOLD_TIME_SW_1P1                (50000) // 50ms for 1P1
#define HOLD_TIME_SW_1P8                (10000) // 10ms for 1P8
#define POWER_SWITCH_OFF_STAB_TIME_US   (10000) // 10ms switch off

#define WAKEOUT_APB1    (GPIO_FLOAT | GPIO_PORTE | GPIO_PIN8)
#define WAKEOUT_APB2    (GPIO_FLOAT | GPIO_PORTE | GPIO_PIN12)
#define WAKEOUT_APB3    (GPIO_FLOAT | GPIO_PORTA | GPIO_PIN4)
#define WAKEOUT_GPB1    (GPIO_FLOAT | GPIO_PORTH | GPIO_PIN11)
#define WAKEOUT_GPB2    (GPIO_FLOAT | GPIO_PORTH | GPIO_PIN12)
#define WAKEOUT_SPRING1 (GPIO_FLOAT | GPIO_PORTH | GPIO_PIN13)
#define WAKEOUT_SPRING2 (GPIO_FLOAT | GPIO_PORTE | GPIO_PIN10)
#define WAKEOUT_SPRING3 (GPIO_FLOAT | GPIO_PORTH | GPIO_PIN14)
#define WAKEOUT_SPRING4 (GPIO_FLOAT | GPIO_PORTC | GPIO_PIN15)
#define WAKEOUT_SPRING5 (GPIO_FLOAT | GPIO_PORTG | GPIO_PIN8)
#define WAKEOUT_SPRING6 (GPIO_FLOAT | GPIO_PORTB | GPIO_PIN15)
#define WAKEOUT_SPRING7 (GPIO_FLOAT | GPIO_PORTH | GPIO_PIN10)
#define WAKEOUT_SPRING8 (GPIO_FLOAT | GPIO_PORTH | GPIO_PIN15)

/* ADC Channels and GPIOs used for Spring Current Measurements */
#define SPRING_COUNT            8

#if 0 // XXX phabos
#define SPRING1_ADC             ADC1
#define SPRING2_ADC             ADC1
#define SPRING3_ADC             ADC1
#define SPRING4_ADC             ADC3
#define SPRING5_ADC             ADC3
#define SPRING6_ADC             ADC3
#define SPRING7_ADC             ADC3
#define SPRING8_ADC             ADC1

#define SPRING1_SENSE_CHANNEL   14    /* PC4 is ADC1_IN14 */
#define SPRING2_SENSE_CHANNEL   10    /* PC0 is ADC1_IN10 */
#define SPRING3_SENSE_CHANNEL   15    /* PC5 is ADC1_IN15 */
#define SPRING4_SENSE_CHANNEL   7     /* PF9 is ADC3_IN7 */
#define SPRING5_SENSE_CHANNEL   6     /* PF8 is ADC3_IN6 */
#define SPRING6_SENSE_CHANNEL   14    /* PF4 is ADC3_IN14 */
#define SPRING7_SENSE_CHANNEL   15    /* PF5 is ADC3_IN15 */
#define SPRING8_SENSE_CHANNEL   11    /* PC1 is ADC1_IN11 */
#endif

#define SPRING1_SIGN_PIN (GPIO_INPUT | GPIO_PORTD | GPIO_PIN0)  /* PD0 */
#define SPRING2_SIGN_PIN (GPIO_INPUT | GPIO_PORTG | GPIO_PIN0)  /* PG0 */
#define SPRING3_SIGN_PIN (GPIO_INPUT | GPIO_PORTD | GPIO_PIN1)  /* PD1 */
#define SPRING4_SIGN_PIN (GPIO_INPUT | GPIO_PORTG | GPIO_PIN5)  /* PG5 */
#define SPRING5_SIGN_PIN (GPIO_INPUT | GPIO_PORTG | GPIO_PIN1)  /* PG1 */
#define SPRING6_SIGN_PIN (GPIO_INPUT | GPIO_PORTF | GPIO_PIN2)  /* PF2 */
#define SPRING7_SIGN_PIN (GPIO_INPUT | GPIO_PORTF | GPIO_PIN13) /* PF13 */
#define SPRING8_SIGN_PIN (GPIO_INPUT | GPIO_PORTA | GPIO_PIN8)  /* PA8 */

/*
 * Built-in bridge voltage regulator list
 */
static struct vreg_data apb1_vreg_data[] = {
    INIT_ACTIVE_HIGH_VREG_DATA(STM32_GPIO_PIN(GPIO_PORTD | GPIO_PIN4),
                               HOLD_TIME_1P1),
    INIT_ACTIVE_HIGH_VREG_DATA(STM32_GPIO_PIN(GPIO_PORTD | GPIO_PIN5),
                               HOLD_TIME_1P8),
    INIT_ACTIVE_HIGH_VREG_DATA(STM32_GPIO_PIN(GPIO_PORTD | GPIO_PIN6),
                               HOLD_TIME_1P2),
};

static struct vreg_data apb2_vreg_data[] = {
    INIT_ACTIVE_HIGH_VREG_DATA(STM32_GPIO_PIN(GPIO_PORTD | GPIO_PIN11),
                               HOLD_TIME_1P1),
    INIT_ACTIVE_HIGH_VREG_DATA(STM32_GPIO_PIN(GPIO_PORTD | GPIO_PIN10),
                               HOLD_TIME_1P8),
    INIT_ACTIVE_HIGH_VREG_DATA(STM32_GPIO_PIN(GPIO_PORTE | GPIO_PIN7),
                               HOLD_TIME_1P2),
    INIT_ACTIVE_HIGH_VREG_DATA(STM32_GPIO_PIN(GPIO_PORTC | GPIO_PIN6),
                               0),  // 2p8_tp
};

static struct vreg_data apb3_vreg_data[] = {
    INIT_ACTIVE_HIGH_VREG_DATA(STM32_GPIO_PIN(GPIO_PORTD | GPIO_PIN12),
                               HOLD_TIME_1P1),
    INIT_ACTIVE_HIGH_VREG_DATA(STM32_GPIO_PIN(GPIO_PORTE | GPIO_PIN15),
                               HOLD_TIME_1P8),
    INIT_ACTIVE_HIGH_VREG_DATA(STM32_GPIO_PIN(GPIO_PORTE | GPIO_PIN13),
                               HOLD_TIME_1P2),
    INIT_ACTIVE_HIGH_VREG_DATA(STM32_GPIO_PIN(GPIO_PORTC | GPIO_PIN7),
                               0),  // 2p8
};

static struct vreg_data gpb1_vreg_data[] =  {
    INIT_ACTIVE_HIGH_VREG_DATA(STM32_GPIO_PIN(GPIO_PORTB | GPIO_PIN5),
                               HOLD_TIME_1P1),
    INIT_ACTIVE_HIGH_VREG_DATA(STM32_GPIO_PIN(GPIO_PORTC | GPIO_PIN3),
                               HOLD_TIME_1P8),
    INIT_ACTIVE_HIGH_VREG_DATA(STM32_GPIO_PIN(GPIO_PORTC | GPIO_PIN2),
                               HOLD_TIME_1P2),
};

static struct vreg_data gpb2_vreg_data[] = {
    INIT_ACTIVE_HIGH_VREG_DATA(STM32_GPIO_PIN(GPIO_PORTD | GPIO_PIN7),
                               HOLD_TIME_1P1),
    INIT_ACTIVE_HIGH_VREG_DATA(STM32_GPIO_PIN(GPIO_PORTD | GPIO_PIN9),
                               HOLD_TIME_1P8),
    INIT_ACTIVE_HIGH_VREG_DATA(STM32_GPIO_PIN(GPIO_PORTD | GPIO_PIN8),
                               HOLD_TIME_1P2),
};

/*
 * Interfaces on this board
 */
DECLARE_INTERFACE(apb1, apb1_vreg_data, 0, WAKEOUT_APB1);
DECLARE_INTERFACE(apb2, apb2_vreg_data, 1, WAKEOUT_APB2);
DECLARE_INTERFACE(apb3, apb3_vreg_data, 2, WAKEOUT_APB3);
DECLARE_INTERFACE(gpb1, gpb1_vreg_data, 3, WAKEOUT_GPB1);
DECLARE_INTERFACE(gpb2, gpb2_vreg_data, 4, WAKEOUT_GPB2);

#define SPRING_INTERFACES_COUNT     8
#if 0 // phabos
DECLARE_SPRING_INTERFACE(1, STM32_GPIO_PIN(GPIO_PORTI | GPIO_PIN2), 9,
                         SPRING1_ADC, SPRING1_SENSE_CHANNEL, SPRING1_SIGN_PIN);
DECLARE_SPRING_INTERFACE(2, STM32_GPIO_PIN(GPIO_PORTF | GPIO_PIN14), 10,
                         SPRING2_ADC, SPRING2_SENSE_CHANNEL, SPRING2_SIGN_PIN);
DECLARE_SPRING_INTERFACE(3, STM32_GPIO_PIN(GPIO_PORTF | GPIO_PIN13), 11,
                         SPRING3_ADC, SPRING3_SENSE_CHANNEL, SPRING3_SIGN_PIN);
DECLARE_SPRING_INTERFACE(4, STM32_GPIO_PIN(GPIO_PORTF | GPIO_PIN11), 6,
                         SPRING4_ADC, SPRING4_SENSE_CHANNEL, SPRING4_SIGN_PIN);
DECLARE_SPRING_INTERFACE(5, STM32_GPIO_PIN(GPIO_PORTG | GPIO_PIN15), 7,
                         SPRING5_ADC, SPRING5_SENSE_CHANNEL, SPRING5_SIGN_PIN);
DECLARE_SPRING_INTERFACE(6, STM32_GPIO_PIN(GPIO_PORTG | GPIO_PIN12), 8,
                         SPRING6_ADC, SPRING6_SENSE_CHANNEL, SPRING6_SIGN_PIN);
DECLARE_SPRING_INTERFACE(7, STM32_GPIO_PIN(GPIO_PORTG | GPIO_PIN13), 5,
                         SPRING7_ADC, SPRING7_SENSE_CHANNEL, SPRING7_SIGN_PIN);
DECLARE_SPRING_INTERFACE(8, STM32_GPIO_PIN(GPIO_PORTG | GPIO_PIN15), 13,
                         SPRING8_ADC, SPRING8_SENSE_CHANNEL, SPRING8_SIGN_PIN);
#endif

/*
 * NB: always declare first the interfaces, then the spring interfaces.
 * Assumed by Spring Power Measurement Library (up_spring_pm.c).
 */
static struct interface *bdb1b_interfaces[] = {
    &apb1_interface,
    &apb2_interface,
    &apb3_interface,
    &gpb1_interface,
    &gpb2_interface,
#if 0
    &bb1_interface,
    &bb2_interface,
    &bb3_interface,
    &bb4_interface,
    &bb5_interface,
    &bb6_interface,
    &bb7_interface,
    &bb8_interface,
#endif
};

/*
 * Switch power supplies.
 * Note: 1P8 is also used by the I/O Expanders.
 */
static struct vreg_data sw_vreg_data[] = {
    // Switch 1P1
    INIT_ACTIVE_HIGH_VREG_DATA(STM32_GPIO_PIN(GPIO_PORTH | GPIO_PIN9),
                               HOLD_TIME_SW_1P1),
    // Switch 1P8
    INIT_ACTIVE_HIGH_VREG_DATA(STM32_GPIO_PIN(GPIO_PORTH | GPIO_PIN6),
                               HOLD_TIME_SW_1P8),
};
DECLARE_VREG(sw, sw_vreg_data);

static struct ara_board_info bdb1b_board_info = {
    .interfaces = bdb1b_interfaces,
    .nr_interfaces = ARRAY_SIZE(bdb1b_interfaces),
    .nr_spring_interfaces = SPRING_INTERFACES_COUNT,

    .sw_data = {
        .vreg = &sw_vreg,
        .gpio_reset = (GPIO_OUTPUT | GPIO_OUTPUT_CLEAR |
                       GPIO_PORTE | GPIO_PIN14),
        .gpio_irq   = (GPIO_PORTI | GPIO_PIN9),
        .rev        = SWITCH_REV_ES1,
        .bus        = SWITCH_I2C_BUS,
    },
};

/*
* Bridge on BDB1B are already powered on POR.
* Remove the power while the bridges and other components
* are starting can cause some troubles.
* Update vreg.def_val to keep the bridge powered during the config
* if the reset was caused by a POR (ie we plug power on board).
*/
void interfaces_por_update(struct interface **ints, size_t nr_ints)
{
    int por;
    int i, j;
    struct vreg *vreg;

// FIXME port to phabos
#define STM32_RCC_BASE          0x40023800
#define STM32_RCC_CSR           (STM32_RCC_BASE + 0x74)
#define RCC_CSR_PORRSTF         (1 << 27)
#define RCC_CSR_RMVF            (1 << 24)

    por = read32(STM32_RCC_CSR) & RCC_CSR_PORRSTF;
    read32(STM32_RCC_CSR) |= RCC_CSR_RMVF;
    if (!por) {
        /* Firmware reset was not cause by POR,
         * let SVC power off and power on bridges
         */
        return;
    }

    for (i = 0; i < nr_ints; i++) {
        vreg = ints[i]->vreg;
        if (!vreg) {
            continue;
        }

        for (j = 0; j < vreg->nr_vregs; j++) {
            if (!&vreg->vregs[j]) {
                break;
            }
            /* Ensure vreg_config will not power off the bridges */
            vreg->vregs[j].def_val = 1;
        }
    }
}

struct ara_board_info *board_init(void)
{
    /* Pretty lights */
    stm32_configgpio(SVC_LED_RED);
    stm32_gpiowrite(SVC_LED_RED, true);

    /*
     * Configure the switch reset and power supply lines.
     * Hold all the lines low while we turn on the power rails.
     */
    vreg_config(&sw_vreg);
    stm32_configgpio(bdb1b_board_info.sw_data.gpio_reset);
    udelay(POWER_SWITCH_OFF_STAB_TIME_US);

    /*
     * Enable 1P1 and 1P8, used by the I/O Expanders.
     * This also enables the switch power supplies.
     */
    vreg_get(&sw_vreg);

    interfaces_por_update(bdb1b_board_info.interfaces,
                          bdb1b_board_info.nr_interfaces);

    return &bdb1b_board_info;
}

void board_exit(void)
{
    /* Disable 1V1 and 1V8, used by the I/O Expanders */
    vreg_put(&sw_vreg);
}

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
 * @author: Jean Pihet
 * @author: Perry Hung
 */

#define DBG_COMP DBG_SVC     /* DBG_COMP macro of the component */

#include <nuttx/config.h>
#include <nuttx/arch.h>
#include <nuttx/util.h>
#include <nuttx/i2c.h>
#include <nuttx/gpio.h>

#include "nuttx/gpio/stm32_gpio_chip.h"
#include "nuttx/gpio/tca64xx.h"

#include "up_debug.h"
#include "ara_board.h"
#include "interface.h"
#include "tsb_switch_driver_es2.h"
#include "stm32.h"

#define SVC_LED_GREEN       (GPIO_OUTPUT | GPIO_PUSHPULL | GPIO_PORTB | \
                             GPIO_PIN0 | GPIO_OUTPUT_SET)

/* U701 I/O Expander reset */
#define SVC_RST_IOEXP_PIN   STM32_GPIO_PIN(GPIO_PORTE | GPIO_PIN0)
#define SVC_RST_IOEXP       (GPIO_OUTPUT | GPIO_OPENDRAIN | GPIO_PULLUP | \
                             GPIO_PORTE | GPIO_PIN0)

/* I/O Expander IRQ to SVC */
#define IO_EXP_IRQ          STM32_GPIO_PIN(GPIO_PORTA | GPIO_PIN0)

/* USB_HUB_RESET to USB UARTs transceiver */
#define USB_HUB_RESET       STM32_GPIO_PIN(GPIO_PORTA | GPIO_PIN1)

/* I/O Expander: I2C bus and addresses */
#define IOEXP_I2C_BUS       2
#define IOEXP_U701_I2C_ADDR 0x21

/*
 * How long to leave hold each regulator before the next.
 */
#define HOLD_TIME_1P1                   (50000) // 0-100ms before 1p2, 1p8
#define HOLD_TIME_1P8                   (60)
#define HOLD_TIME_1P2                   (0)

#define HOLD_TIME_SW_1P1                (50000) // 50ms for 1P1
#define HOLD_TIME_SW_1P8                (10000) // 10ms for 1P8
#define POWER_SWITCH_OFF_STAB_TIME_US   (10000) // 10ms switch off

#define WAKEOUT_APB1    (GPIO_FLOAT | GPIO_PORTE | GPIO_PIN8)
#define WAKEOUT_APB2    (GPIO_FLOAT | GPIO_PORTE | GPIO_PIN12)
#define WAKEOUT_APB3    (GPIO_FLOAT | GPIO_PORTF | GPIO_PIN6)
#define WAKEOUT_GPB1    (GPIO_FLOAT | GPIO_PORTH | GPIO_PIN11)
#define WAKEOUT_GPB2    (GPIO_FLOAT | GPIO_PORTH | GPIO_PIN12)
#define WAKEOUT_SMA1    (GPIO_FLOAT | GPIO_PORTH | GPIO_PIN13)
#define WAKEOUT_SMA2    (GPIO_FLOAT | GPIO_PORTH | GPIO_PIN14)

/*
 * Bridges reset lines
 */
#define SVC_RST_APB1    STM32_GPIO_PIN(GPIO_PORTF | GPIO_PIN0)
#define SVC_RST_APB2    STM32_GPIO_PIN(GPIO_PORTF | GPIO_PIN1)
#define SVC_RST_APB3    STM32_GPIO_PIN(GPIO_PORTF | GPIO_PIN2)
#define SVC_RST_GPB1    STM32_GPIO_PIN(GPIO_PORTF | GPIO_PIN3)
#define SVC_RST_GPB2    STM32_GPIO_PIN(GPIO_PORTF | GPIO_PIN4)
#define SVC_RST_SMA1    STM32_GPIO_PIN(GPIO_PORTF | GPIO_PIN5)
#define SVC_RST_SMA2    STM32_GPIO_PIN(GPIO_PORTF | GPIO_PIN7)

/*
 * Bridges reset pins: active low.
 * Asserted at start-up and de-asserted after the regulators are enabled.
 */
#define INIT_SVC_RST_DATA(g)        \
    {                               \
        .gpio = g,                  \
        .hold_time = 0,             \
        .active_high = 1,           \
        .def_val = 0,               \
    }

/*
 * BOOTRET pins
 */
#define BOOTRET_APB1    STM32_GPIO_PIN(GPIO_PORTF | GPIO_PIN12)
#define BOOTRET_APB2    STM32_GPIO_PIN(GPIO_PORTG | GPIO_PIN3)
#define BOOTRET_APB3    STM32_GPIO_PIN(GPIO_PORTG | GPIO_PIN4)
#define BOOTRET_GPB1    STM32_GPIO_PIN(GPIO_PORTG | GPIO_PIN6)
#define BOOTRET_GPB2    STM32_GPIO_PIN(GPIO_PORTG | GPIO_PIN7)
#define BOOTRET_SMA1    STM32_GPIO_PIN(GPIO_PORTF | GPIO_PIN13)
#define BOOTRET_SMA2    STM32_GPIO_PIN(GPIO_PORTF | GPIO_PIN14)

/* Bootret pins: active low, enabled by default */
#define INIT_BOOTRET_DATA(g)        \
    {                               \
        .gpio = g,                  \
        .hold_time = 0,             \
        .active_high = 0,           \
        .def_val = 0,               \
    }

/*
 * DETECT_IN lines directly connected to the SVC, for direct
 * control and/or simulation.
 * Configured by default as output, inactive, with pull-up.
 */
#define SMA1_DET_IN     (GPIO_OUTPUT | GPIO_OPENDRAIN | GPIO_PULLUP | \
                         GPIO_PORTC | GPIO_PIN0)
#define SMA2_DET_IN     (GPIO_OUTPUT | GPIO_OPENDRAIN | GPIO_PULLUP | \
                         GPIO_PORTC | GPIO_PIN1)

/*
 * Built-in bridge voltage regulator list
 */
static struct vreg_data apb1_vreg_data[] = {
    INIT_BOOTRET_DATA(BOOTRET_APB1),
    INIT_ACTIVE_HIGH_VREG_DATA(STM32_GPIO_PIN(GPIO_PORTD | GPIO_PIN4),
                               HOLD_TIME_1P1),
    INIT_ACTIVE_HIGH_VREG_DATA(STM32_GPIO_PIN(GPIO_PORTD | GPIO_PIN6),
                               HOLD_TIME_1P8),
    INIT_ACTIVE_HIGH_VREG_DATA(STM32_GPIO_PIN(GPIO_PORTD | GPIO_PIN5),
                               HOLD_TIME_1P2),
    INIT_SVC_RST_DATA(SVC_RST_APB1),
};

static struct vreg_data apb2_vreg_data[] = {
    INIT_BOOTRET_DATA(BOOTRET_APB2),
    INIT_ACTIVE_HIGH_VREG_DATA(STM32_GPIO_PIN(GPIO_PORTD | GPIO_PIN11),
                               HOLD_TIME_1P1),
    INIT_ACTIVE_HIGH_VREG_DATA(STM32_GPIO_PIN(GPIO_PORTD | GPIO_PIN10),
                               HOLD_TIME_1P8),
    INIT_ACTIVE_HIGH_VREG_DATA(STM32_GPIO_PIN(GPIO_PORTE | GPIO_PIN7),
                               HOLD_TIME_1P2),
    INIT_ACTIVE_HIGH_VREG_DATA(STM32_GPIO_PIN(GPIO_PORTC | GPIO_PIN6),
                               0),  // 2p8_tp
    INIT_SVC_RST_DATA(SVC_RST_APB2),
};

static struct vreg_data apb3_vreg_data[] = {
    INIT_BOOTRET_DATA(BOOTRET_APB3),
    INIT_ACTIVE_HIGH_VREG_DATA(STM32_GPIO_PIN(GPIO_PORTD | GPIO_PIN12),
                               HOLD_TIME_1P1),
    INIT_ACTIVE_HIGH_VREG_DATA(STM32_GPIO_PIN(GPIO_PORTE | GPIO_PIN15),
                               HOLD_TIME_1P8),
    INIT_ACTIVE_HIGH_VREG_DATA(STM32_GPIO_PIN(GPIO_PORTE | GPIO_PIN13),
                               HOLD_TIME_1P2),
    INIT_ACTIVE_HIGH_VREG_DATA(STM32_GPIO_PIN(GPIO_PORTC | GPIO_PIN7),
                               0),  // 2p8
    INIT_SVC_RST_DATA(SVC_RST_APB3),
};

static struct vreg_data gpb1_vreg_data[] =  {
    INIT_BOOTRET_DATA(BOOTRET_GPB1),
    INIT_ACTIVE_HIGH_VREG_DATA(STM32_GPIO_PIN(GPIO_PORTB | GPIO_PIN5),
                               HOLD_TIME_1P1),
    INIT_ACTIVE_HIGH_VREG_DATA(STM32_GPIO_PIN(GPIO_PORTC | GPIO_PIN3),
                               HOLD_TIME_1P8),
    INIT_ACTIVE_HIGH_VREG_DATA(STM32_GPIO_PIN(GPIO_PORTC | GPIO_PIN2),
                               HOLD_TIME_1P2),
    INIT_SVC_RST_DATA(SVC_RST_GPB1),
};

static struct vreg_data gpb2_vreg_data[] = {
    INIT_BOOTRET_DATA(BOOTRET_GPB2),
    INIT_ACTIVE_HIGH_VREG_DATA(STM32_GPIO_PIN(GPIO_PORTD | GPIO_PIN7),
                               HOLD_TIME_1P1),
    INIT_ACTIVE_HIGH_VREG_DATA(STM32_GPIO_PIN(GPIO_PORTD | GPIO_PIN9),
                               HOLD_TIME_1P8),
    INIT_ACTIVE_HIGH_VREG_DATA(STM32_GPIO_PIN(GPIO_PORTD | GPIO_PIN8),
                               HOLD_TIME_1P2),
    INIT_SVC_RST_DATA(SVC_RST_GPB2),
};

static struct vreg_data sma1_vreg_data[] = {
    INIT_BOOTRET_DATA(BOOTRET_SMA1),
    INIT_ACTIVE_HIGH_VREG_DATA(STM32_GPIO_PIN(GPIO_PORTD | GPIO_PIN1),
                               HOLD_TIME_1P1),
    INIT_ACTIVE_HIGH_VREG_DATA(STM32_GPIO_PIN(GPIO_PORTD | GPIO_PIN3),
                               HOLD_TIME_1P8),
    INIT_ACTIVE_HIGH_VREG_DATA(STM32_GPIO_PIN(GPIO_PORTD | GPIO_PIN2),
                               HOLD_TIME_1P2),
    INIT_SVC_RST_DATA(SVC_RST_SMA1),
};

static struct vreg_data sma2_vreg_data[] = {
    INIT_BOOTRET_DATA(BOOTRET_SMA2),
    INIT_ACTIVE_HIGH_VREG_DATA(STM32_GPIO_PIN(GPIO_PORTD | GPIO_PIN13),
                               HOLD_TIME_1P1),
    INIT_ACTIVE_HIGH_VREG_DATA(STM32_GPIO_PIN(GPIO_PORTD | GPIO_PIN15),
                               HOLD_TIME_1P8),
    INIT_ACTIVE_HIGH_VREG_DATA(STM32_GPIO_PIN(GPIO_PORTD | GPIO_PIN14),
                               HOLD_TIME_1P2),
    INIT_SVC_RST_DATA(SVC_RST_SMA2),
};

/*
 * Interfaces on this board
 */
DECLARE_INTERFACE(apb1, apb1_vreg_data, 4, WAKEOUT_APB1,
                  U701_GPIO_PIN(1), ARA_IFACE_WD_ACTIVE_HIGH,
                  U701_GPIO_PIN(0), ARA_IFACE_WD_ACTIVE_LOW);
DECLARE_INTERFACE(apb2, apb2_vreg_data, 12, WAKEOUT_APB2,
                  U701_GPIO_PIN(3), ARA_IFACE_WD_ACTIVE_HIGH,
                  U701_GPIO_PIN(2), ARA_IFACE_WD_ACTIVE_LOW);
DECLARE_INTERFACE(apb3, apb3_vreg_data, 7, WAKEOUT_APB3,
                  U701_GPIO_PIN(5), ARA_IFACE_WD_ACTIVE_HIGH,
                  U701_GPIO_PIN(4), ARA_IFACE_WD_ACTIVE_LOW);
DECLARE_INTERFACE(gpb1, gpb1_vreg_data, 0, WAKEOUT_GPB1,
                  U701_GPIO_PIN(7), ARA_IFACE_WD_ACTIVE_HIGH,
                  U701_GPIO_PIN(6), ARA_IFACE_WD_ACTIVE_LOW);
DECLARE_INTERFACE(gpb2, gpb2_vreg_data, 2, WAKEOUT_GPB2,
                  U701_GPIO_PIN(9), ARA_IFACE_WD_ACTIVE_HIGH,
                  U701_GPIO_PIN(8), ARA_IFACE_WD_ACTIVE_LOW);

DECLARE_EXPANSION_INTERFACE(sma1, sma1_vreg_data, 9, WAKEOUT_SMA1,
                            U701_GPIO_PIN(11), ARA_IFACE_WD_ACTIVE_HIGH,
                            U701_GPIO_PIN(13), ARA_IFACE_WD_ACTIVE_LOW);
DECLARE_EXPANSION_INTERFACE(sma2, sma2_vreg_data, 10, WAKEOUT_SMA2,
                            U701_GPIO_PIN(12), ARA_IFACE_WD_ACTIVE_HIGH,
                            U701_GPIO_PIN(15), ARA_IFACE_WD_ACTIVE_LOW);

/*
 * Important note: Always declare the spring interfaces last.
 * Assumed by Spring Power Measurement Library (up_spring_pm.c).
 */
static struct interface *sdb_interfaces[] = {
    &apb1_interface,
    &apb2_interface,
    &apb3_interface,
    &gpb1_interface,
    &gpb2_interface,
    &sma1_interface,
    &sma2_interface,
};

/*
 * Switch power supplies.
 */
static struct vreg_data sw_vreg_data[] = {
    // Switch 1P1
    INIT_ACTIVE_HIGH_VREG_DATA(STM32_GPIO_PIN(GPIO_PORTH | GPIO_PIN9),
                               HOLD_TIME_SW_1P1),
    // Switch 1P8
    INIT_ACTIVE_HIGH_VREG_DATA(STM32_GPIO_PIN(GPIO_PORTH | GPIO_PIN7),
                               HOLD_TIME_SW_1P8),
};
DECLARE_VREG(sw, sw_vreg_data);

/*
 * I/O Expander power supplies.
 */
static struct vreg_data ioexp_vreg_data[] = {
    // 1P8
    INIT_ACTIVE_HIGH_VREG_DATA(STM32_GPIO_PIN(GPIO_PORTA | GPIO_PIN2), 0),
};
DECLARE_VREG(ioexp, ioexp_vreg_data);

static struct io_expander_info sdb_io_expanders[] = {
        {
            .part       = TCA6416_PART,
            .i2c_bus    = IOEXP_I2C_BUS,
            .i2c_addr   = IOEXP_U701_I2C_ADDR,
            .reset      = SVC_RST_IOEXP_PIN,
            .irq        = IO_EXP_IRQ,
            .gpio_base  = U701_GPIO_CHIP_START,
        },
};

static struct ara_board_info sdb_board_info = {
    .interfaces = sdb_interfaces,
    .nr_interfaces = ARRAY_SIZE(sdb_interfaces),
    .nr_spring_interfaces = 0,

    .sw_data = {
        .vreg = &sw_vreg,
        .gpio_reset = (GPIO_OUTPUT | GPIO_OPENDRAIN | GPIO_PULLUP |
                       GPIO_OUTPUT_CLEAR | GPIO_PORTE | GPIO_PIN14),
        .gpio_irq   = (GPIO_INPUT | GPIO_FLOAT | GPIO_EXTI | \
                       GPIO_PORTH | GPIO_PIN3),
        .rev        = SWITCH_REV_ES2,
        .bus        = SW_SPI_PORT,
        .spi_cs     = (GPIO_OUTPUT | GPIO_PUSHPULL | GPIO_OUTPUT_SET | \
                       GPIO_PORTA | GPIO_PIN4)
    },

    .io_expanders = sdb_io_expanders,
    .nr_io_expanders = ARRAY_SIZE(sdb_io_expanders),
};

struct ara_board_info *board_init(void) {
    int i;

    /* Pretty lights */
    stm32_configgpio(SVC_LED_GREEN);
    stm32_gpiowrite(SVC_LED_GREEN, false);

    /* Disable these for now */
    stm32_configgpio(SVC_RST_IOEXP);
    stm32_gpiowrite(SVC_RST_IOEXP, false);

    /*
     * Register the STM32 GPIOs to Gpio Chip
     *
     * This needs to happen before the I/O Expanders registration, which
     * uses some STM32 pins
     */
    stm32_gpio_init();

    /*
     * Configure the switch and I/O Expander reset and power supply lines.
     * Hold all the lines low while we turn on the power rails.
     */
    vreg_config(&ioexp_vreg);
    vreg_config(&sw_vreg);
    stm32_configgpio(sdb_board_info.sw_data.gpio_reset);
    up_udelay(POWER_SWITCH_OFF_STAB_TIME_US);

    /*
     * Configure the SVC DETECT_IN lines
     */
    stm32_configgpio(SMA1_DET_IN);
    stm32_configgpio(SMA2_DET_IN);

    /*
     * Enable 1P1 and 1P8, used by the I/O Expanders
     */
    vreg_get(&ioexp_vreg);

    /* Register the TCA64xx I/O Expanders GPIOs to Gpio Chip */
    for (i = 0; i < sdb_board_info.nr_io_expanders; i++) {
        struct io_expander_info *io_exp = &sdb_board_info.io_expanders[i];

        io_exp->i2c_dev = up_i2cinitialize(io_exp->i2c_bus);
        if (!io_exp->i2c_dev) {
            dbg_error("%s(): Failed to get I/O Expander I2C bus %u\n",
                      __func__, io_exp->i2c_bus);
        } else {
            if (tca64xx_init(&io_exp->io_exp_driver_data,
                             io_exp->part,
                             io_exp->i2c_dev,
                             io_exp->i2c_addr,
                             io_exp->reset,
                             io_exp->irq,
                             io_exp->gpio_base) < 0) {
                dbg_error("%s(): Failed to register I/O Expander(0x%02x)\n",
                          __func__, io_exp->i2c_addr);
                up_i2cuninitialize(io_exp->i2c_dev);
            }
        }
    }

    /* Hold USB_HUB_RESET high */
    gpio_direction_out(USB_HUB_RESET, 1);

    return &sdb_board_info;
}

void board_exit(void) {
    int i;
    /*
     * First unregister the TCA64xx I/O Expanders and associated I2C bus(ses).
     * Done in reverse order from registration to account for IRQ chaining
     * between I/O Expander chips.
     */
    for (i = sdb_board_info.nr_io_expanders - 1; i >= 0; i--) {
        struct io_expander_info *io_exp = &sdb_board_info.io_expanders[i];

        if (io_exp->io_exp_driver_data)
            tca64xx_deinit(io_exp->io_exp_driver_data);

        if (io_exp->i2c_dev)
            up_i2cuninitialize(io_exp->i2c_dev);
    }

    /* Disable the I/O Expanders power */
    vreg_put(&ioexp_vreg);

    /* Lastly unregister the GPIO Chip driver */
    stm32_gpio_deinit();
}

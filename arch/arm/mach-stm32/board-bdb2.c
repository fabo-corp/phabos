#include "stm32f4xx.h"
#include "rcc.h"
#include "bdb.h"

#include <asm/hwio.h>
#include <asm/delay.h>
#include <asm/gpio.h>

#include <phabos/kprintf.h>
#include <phabos/driver.h>
#include <phabos/serial/uart.h>
#include <phabos/gpio.h>
#include <phabos/gpio/tca64xx.h>
#include <phabos/i2c.h>
#include <phabos/i2c/stm32-i2c.h>
#include <phabos/spi.h>
#include <phabos/spi/spi-stm32.h>

#define STM32_USART1_BRR    (STM32_USART1_BASE + 0x08)
#define STM32_USART1_CR1    (STM32_USART1_BASE + 0x0c)

#define STM32_USART1_CR1_UE (1 << 13)
#define STM32_USART1_CR1_TE (1 << 3)

#define STM32_USART1_BRR_APB2_84MHZ_B115200_MANTISSA (45 << 4)
#define STM32_USART1_BRR_APB2_84MHZ_B115200_FRACTION 9
#define STM32_USART1_BRR_APB2_84MHZ_B115200 \
    (STM32_USART1_BRR_APB2_84MHZ_B115200_MANTISSA | \
     STM32_USART1_BRR_APB2_84MHZ_B115200_FRACTION)

#define STM32_USART1_BRR_APB2_80MHZ_B115200_MANTISSA (43 << 4)
#define STM32_USART1_BRR_APB2_80MHZ_B115200_FRACTION 7
#define STM32_USART1_BRR_APB2_80MHZ_B115200 \
    (STM32_USART1_BRR_APB2_80MHZ_B115200_MANTISSA | \
     STM32_USART1_BRR_APB2_80MHZ_B115200_FRACTION)

#if defined(CONFIG_BOOT_FLASH)
#   define PLLCFGR RCC_PLLCFGR_PLLSRC_HSI | (320 << RCC_PLLCFGR_PLLN_OFFSET) | \
                   RCC_PLLCFGR_PLLP4 | (8 << RCC_PLLCFGR_PLLM_OFFSET)
#   define MAINPLL_FREQ     160000000 // 160 MHz
#   define APB1_FREQ        40000000 // 40 MHz
#   define APB2_FREQ        80000000 // 80 MHz
#   define USART1_B115200   STM32_USART1_BRR_APB2_80MHZ_B115200
#else
#   define PLLCFGR RCC_PLLCFGR_PLLSRC_HSI | (336 << RCC_PLLCFGR_PLLN_OFFSET) | \
                   RCC_PLLCFGR_PLLP4 | (8 << RCC_PLLCFGR_PLLM_OFFSET)
#   define MAINPLL_FREQ     168000000 // 168 MHz
#   define APB1_FREQ        42000000 // 42 MHz
#   define APB2_FREQ        84000000 // 84 MHz
#   define USART1_B115200   STM32_USART1_BRR_APB2_84MHZ_B115200
#endif

#define SYSCLOCK_FREQ   MAINPLL_FREQ
#define AHB_FREQ        MAINPLL_FREQ

#define FLASH_ACR (STM32_FLASH_BASE + 0x00)

#define FLASH_ACR_LATENCY_0WS   (0 << 0)
#define FLASH_ACR_LATENCY_1WS   (1 << 0)
#define FLASH_ACR_LATENCY_2WS   (2 << 0)
#define FLASH_ACR_LATENCY_3WS   (3 << 0)
#define FLASH_ACR_LATENCY_4WS   (4 << 0)
#define FLASH_ACR_LATENCY_5WS   (5 << 0)
#define FLASH_ACR_LATENCY_6WS   (6 << 0)
#define FLASH_ACR_LATENCY_7WS   (7 << 0)
#define FLASH_ACR_ICEN          (1 << 9)
#define FLASH_ACR_DCEN          (1 << 19)

struct gpio_device gpio_port[] = {
    {
        .base = 0,
        .count = 16,
        .device = {
            .name = "stm32-gpio-a",
            .description = "STM32 GPIO Port A",
            .driver = "stm32-gpio",
            .reg_base = STM32_GPIOA_BASE,
        },
    },
    {
        .base = 16,
        .count = 16,
        .device = {
            .name = "stm32-gpio-b",
            .description = "STM32 GPIO Port B",
            .driver = "stm32-gpio",
            .reg_base = STM32_GPIOB_BASE,
        },
    },
    {
        .base = 32,
        .count = 16,
        .device = {
            .name = "stm32-gpio-c",
            .description = "STM32 GPIO Port C",
            .driver = "stm32-gpio",
            .reg_base = STM32_GPIOC_BASE,
        },
    },
    {
        .base = 48,
        .count = 16,
        .device = {
            .name = "stm32-gpio-d",
            .description = "STM32 GPIO Port D",
            .driver = "stm32-gpio",
            .reg_base = STM32_GPIOD_BASE,
        },
    },
    {
        .base = 64,
        .count = 16,
        .device = {
            .name = "stm32-gpio-e",
            .description = "STM32 GPIO Port E",
            .driver = "stm32-gpio",
            .reg_base = STM32_GPIOE_BASE,
        },
    },
    {
        .base = 80,
        .count = 16,
        .device = {
            .name = "stm32-gpio-f",
            .description = "STM32 GPIO Port F",
            .driver = "stm32-gpio",
            .reg_base = STM32_GPIOF_BASE,
        },
    },
    {
        .base = 96,
        .count = 16,
        .device = {
            .name = "stm32-gpio-g",
            .description = "STM32 GPIO Port G",
            .driver = "stm32-gpio",
            .reg_base = STM32_GPIOG_BASE,
        },
    },
    {
        .base = 112,
        .count = 16,
        .device = {
            .name = "stm32-gpio-h",
            .description = "STM32 GPIO Port H",
            .driver = "stm32-gpio",
            .reg_base = STM32_GPIOH_BASE,
        },
    },
    {
        .base = 128,
        .count = 12,
        .device = {
            .name = "stm32-gpio-i",
            .description = "STM32 GPIO Port I",
            .driver = "stm32-gpio",
            .reg_base = STM32_GPIOI_BASE,
        },
    },
};

static struct uart_device stm32_usart_device = {
    .device = {
        .name = "usart-1",
        .description = "STM32 USART-1",
        .driver = "stm32-usart",

        .reg_base = STM32_USART1_BASE,
        .irq = STM32_IRQ_USART1,
    },
};

static struct stm32_i2c_adapter_platform stm32_i2c_pdata = {
    .evt_irq = STM32_IRQ_I2C2_EV,
    .err_irq = STM32_IRQ_I2C2_ER,
    .clk = APB1_FREQ,
};

struct i2c_adapter stm32_i2c_adapter = {
    .device = {
        .name = "i2c-2",
        .description = "STM32 I2C-2",
        .driver = "stm32-i2c",

        .reg_base = STM32_I2C2_BASE,
        .pdata = &stm32_i2c_pdata,
    },
};

static struct stm32_spi_master_platform stm32_spi_pdata = {
    .clk = APB1_FREQ,
};

struct spi_master stm32_spi_master = {
    .device = {
        .name = "spi-1",
        .description = "STM32 SPI-1",
        .driver = "stm32-spi",

        .reg_base = STM32_SPI1_BASE,
        .pdata = &stm32_spi_pdata,
    },
};

static struct tca64xx_platform tca64xx_io_expander_pdata[] = {
    {
        .part = TCA6416_PART,
        .adapter = &stm32_i2c_adapter,
        .addr = 0x20,
        .reset_gpio = GPIO_PORTE | GPIO_PIN0,
        .irq = GPIO_PORTA | GPIO_PIN0,
    },
    {
        .part = TCA6416_PART,
        .adapter = &stm32_i2c_adapter,
        .addr = 0x21,
        .reset_gpio = GPIO_PORTE | GPIO_PIN1,
        .irq = U96_GPIO_CHIP_START + 7,
    },
    {
        .part = TCA6424_PART,
        .adapter = &stm32_i2c_adapter,
        .addr = 0x23,
        .reset_gpio = TCA64XX_IO_UNUSED,
        .irq = TCA64XX_IO_UNUSED,
    },
};

static int tca64xx_power_on(struct device *device)
{
    struct tca64xx_platform *pdata = device->pdata;

    if (pdata->reset_gpio == TCA64XX_IO_UNUSED)
        return 0;

    stm32_configgpio(GPIO_OUTPUT | GPIO_OPENDRAIN | GPIO_PULLUP |
                     pdata->reset_gpio);
    stm32_gpiowrite(pdata->reset_gpio, false);
    udelay(1); // T_reset = 600ns
    stm32_gpiowrite(pdata->reset_gpio, true);

    return 0;
}

static struct gpio_device tca64xx_io_expander[] = {
    {
        .base = U96_GPIO_CHIP_START,
        .count = 16,
        .device = {
            .name = "tca6416",
            .description = "TCA6416 U96",
            .driver = "tca64xx",
            .pdata = &tca64xx_io_expander_pdata[0],
            .power_on = tca64xx_power_on,
        },
    },
    {
        .base = U90_GPIO_CHIP_START,
        .count = 16,
        .device = {
            .name = "tca6416",
            .description = "TCA6416 U90",
            .driver = "tca64xx",
            .pdata = &tca64xx_io_expander_pdata[1],
            .power_on = tca64xx_power_on,
        },
    },
    {
        .base = U135_GPIO_CHIP_START,
        .count = 24,
        .device = {
            .name = "tca6424",
            .description = "TCA6424 U135",
            .driver = "tca64xx",
            .pdata = &tca64xx_io_expander_pdata[2],
        },
    },
};

static void uart_init(void)
{
    stm32_clk_enable(STM32_CLK_USART1);
    stm32_reset(STM32_RST_USART1);

    stm32_configgpio(GPIO_PORTB | GPIO_PIN6 | GPIO_AF7 | GPIO_ALT_FCT |
                     GPIO_PULLUP);
    stm32_configgpio(GPIO_PORTB | GPIO_PIN7 | GPIO_AF7 | GPIO_ALT_FCT |
                     GPIO_PULLUP);

    write32(STM32_USART1_CR1, STM32_USART1_CR1_UE);
    write32(STM32_USART1_BRR, USART1_B115200);
    write32(STM32_USART1_CR1, STM32_USART1_CR1_UE | STM32_USART1_CR1_TE);
}

static void i2c_init(void)
{
    stm32_clk_enable(STM32_CLK_I2C2);
    stm32_reset(STM32_RST_I2C2);

    stm32_configgpio(GPIO_PORTH | GPIO_PIN4 | GPIO_AF4 | GPIO_ALT_FCT |
                     GPIO_OPENDRAIN | GPIO_SPEED_FAST);
    stm32_configgpio(GPIO_PORTH | GPIO_PIN5 | GPIO_AF4 | GPIO_ALT_FCT |
                     GPIO_OPENDRAIN | GPIO_SPEED_FAST);
}

static void gpio_init(void)
{
    for (int i = 0; i < ARRAY_SIZE(gpio_port); i++) {
        stm32_clk_enable(STM32_CLK_GPIOA + i);
        stm32_reset(STM32_RST_GPIOA + i);
    }
}

static void spi_init(void)
{
    stm32_clk_enable(STM32_CLK_SPI1);
    stm32_reset(STM32_RST_SPI1);

    stm32_configgpio(GPIO_PORTA | GPIO_PIN4 | GPIO_AF5 | GPIO_ALT_FCT |
                     GPIO_SPEED_HIGH);
    stm32_configgpio(GPIO_PORTA | GPIO_PIN5 | GPIO_AF5 | GPIO_ALT_FCT |
                     GPIO_SPEED_HIGH);
    stm32_configgpio(GPIO_PORTA | GPIO_PIN6 | GPIO_AF5 | GPIO_ALT_FCT |
                     GPIO_SPEED_HIGH);
    stm32_configgpio(GPIO_PORTA | GPIO_PIN7 | GPIO_AF5 | GPIO_ALT_FCT |
                     GPIO_SPEED_HIGH);
}

__attribute__((unused)) static void display_temperature(void)
{
    stm32_clk_enable(STM32_CLK_ADC);
    stm32_reset(STM32_RST_ADC);

#define ADC_CR2     0x08
#define ADC_SMPR1   0x0c
#define ADC_SQR3    0x34
#define ADC_DR      0x4c
#define ADC_CCR     (0x300 + 0x04)

#define ADC_CR2_ADON    (1 << 0)
#define ADC_CR2_SWSTART (1 << 30)
#define ADC_CR2_CONT    (1 << 1)

#define ADC_CCR_ADCPRE_DIV2 (0 << 16)
#define ADC_CCR_ADCPRE_DIV4 (1 << 16)
#define ADC_CCR_ADCPRE_DIV6 (2 << 16)
#define ADC_CCR_ADCPRE_DIV8 (3 << 16)
#define ADC_CCR_TSVREFE     (1 << 23)

    write32(STM32_ADC_BASE + ADC_CR2, ADC_CR2_ADON);
    write32(STM32_ADC_BASE + ADC_SMPR1, 7 << 18);
    write32(STM32_ADC_BASE + ADC_SQR3, 16);
//    write32(STM32_ADC_BASE + ADC_SMPR1, 4 << 24);
//    write32(STM32_ADC_BASE + ADC_SQR3, 18);
    write32(STM32_ADC_BASE + ADC_CCR, ADC_CCR_TSVREFE | (1 << 22) | ADC_CCR_ADCPRE_DIV6);
    write32(STM32_ADC_BASE + ADC_CR2, ADC_CR2_SWSTART | ADC_CR2_ADON | ADC_CR2_CONT);

    for (int i = 0;; i++) {
        uint16_t temperature = read32(STM32_ADC_BASE + ADC_DR);
        int32_t vsense = (temperature * 1800) >> 12;
        vsense = ((vsense - 760) * 10) / 25 + 25;

        kprintf("Vsense = %u\n", temperature);
        kprintf("Temperature = %d°C\n", vsense);
        mdelay(2500);
    }
}

void machine_init(void)
{
    /*
     * Configure clocks to the following:
     *
     * Boot from flash:
     *     PLL: 160MHz
     *     AHB: 160MHz
     *     APB1: 40MHz
     *     APB2: 80Mhz
     *
     * Warning: we cannot go up to 168Mhz when running from flash when
     *          VDD = 1.8V.
     *
     * Boot from flash and copy to ram or boot from ram:
     *     PLL: 164MHz
     *     AHB: 164MHz
     *     APB1: 42MHz
     *     APB2: 84Mhz
     */
    write32(RCC_PLLCFGR, PLLCFGR);
    write32(RCC_CFGR, RCC_CFGR_SW_PLL | RCC_CFGR_PPRE1_DIV4 |
                      RCC_CFGR_PPRE2_DIV2);
    read32(RCC_CR) |= RCC_CR_PLLON;

    /*
     * Enable Flash I-Cache and D-Cache + set the latency to 7 wait states
     */
    write32(FLASH_ACR, FLASH_ACR_LATENCY_7WS | FLASH_ACR_ICEN | FLASH_ACR_DCEN);

    /*
     * FIXME These doesn't belong here and should go away in the near future
     */
    gpio_init(); // XXX: Enable all GPIOs for now
    uart_init(); // XXX: Enable USART1
    i2c_init();  // XXX: Enable I2C2
    spi_init();

    //display_temperature();

    for (int i = 0; i < ARRAY_SIZE(gpio_port); i++)
        device_register(&gpio_port[i].device);
    device_register(&stm32_usart_device.device);
    device_register(&stm32_i2c_adapter.device);
    device_register(&stm32_spi_master.device);
    for (int i = 0; i < ARRAY_SIZE(tca64xx_io_expander); i++)
        device_register(&tca64xx_io_expander[i].device);
}

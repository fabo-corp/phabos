#include "stm32f4xx.h"
#include "rcc.h"

#include <asm/hwio.h>
#include <asm/delay.h>
#include <asm/gpio.h>

#include <phabos/kprintf.h>
#include <phabos/driver.h>
#include <phabos/serial/uart.h>
#include <phabos/gpio.h>
#include <phabos/i2c.h>
#include <phabos/i2c/stm32-i2c.h>

#define STM32_USART1_BRR    (STM32_USART1_BASE + 0x08)
#define STM32_USART1_CR1    (STM32_USART1_BASE + 0x0c)

#define MAINPLL_FREQ    168000000 // 168 MHz
#define SYSCLOCK_FREQ   MAINPLL_FREQ
#define AHB_FREQ        MAINPLL_FREQ
#define APB1_FREQ       42000000 // 42 MHz
#define APB2_FREQ       84000000 // 84 MHz

#define STM32_USART1_CR1_UE (1 << 13)
#define STM32_USART1_CR1_TE (1 << 3)

#define STM32_USART1_BRR_APB2_84MHZ_B115200_MANTISSA (45 << 4)
#define STM32_USART1_BRR_APB2_84MHZ_B115200_FRACTION 9
#define STM32_USART1_BRR_APB2_84MHZ_B115200 \
    (STM32_USART1_BRR_APB2_84MHZ_B115200_MANTISSA | \
     STM32_USART1_BRR_APB2_84MHZ_B115200_FRACTION)

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
        .count = 16,
        .device = {
            .name = "stm32-gpio-a",
            .description = "STM32 GPIO Port A",
            .driver = "stm32-gpio",
            .reg_base = STM32_GPIOA_BASE,
        },
    },
    {
        .count = 16,
        .device = {
            .name = "stm32-gpio-b",
            .description = "STM32 GPIO Port B",
            .driver = "stm32-gpio",
            .reg_base = STM32_GPIOB_BASE,
        },
    },
    {
        .count = 16,
        .device = {
            .name = "stm32-gpio-c",
            .description = "STM32 GPIO Port C",
            .driver = "stm32-gpio",
            .reg_base = STM32_GPIOC_BASE,
        },
    },
    {
        .count = 16,
        .device = {
            .name = "stm32-gpio-d",
            .description = "STM32 GPIO Port D",
            .driver = "stm32-gpio",
            .reg_base = STM32_GPIOD_BASE,
        },
    },
    {
        .count = 16,
        .device = {
            .name = "stm32-gpio-e",
            .description = "STM32 GPIO Port E",
            .driver = "stm32-gpio",
            .reg_base = STM32_GPIOE_BASE,
        },
    },
    {
        .count = 16,
        .device = {
            .name = "stm32-gpio-f",
            .description = "STM32 GPIO Port F",
            .driver = "stm32-gpio",
            .reg_base = STM32_GPIOF_BASE,
        },
    },
    {
        .count = 16,
        .device = {
            .name = "stm32-gpio-g",
            .description = "STM32 GPIO Port G",
            .driver = "stm32-gpio",
            .reg_base = STM32_GPIOG_BASE,
        },
    },
    {
        .count = 16,
        .device = {
            .name = "stm32-gpio-h",
            .description = "STM32 GPIO Port H",
            .driver = "stm32-gpio",
            .reg_base = STM32_GPIOH_BASE,
        },
    },
    {
        .count = 16,
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
        .name = "stm32-usart1",
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

static struct i2c_adapter stm32_i2c_adapter = {
    .device = {
        .name = "stm32-i2c2",
        .description = "STM32 I2C-2",
        .driver = "stm32-i2c",

        .reg_base = STM32_I2C2_BASE,
        .pdata = &stm32_i2c_pdata,
    },
};

#if 0
static struct tca64xx_platform tca64xx_io_expander_pdata[] = {
    {
        .part = TCA6416_PART,
        .i2c_adapter = &stm32_i2c_adapter,
        .i2c_addr = 0x21,
        .reset = IO_RESET,
    },
    {
        .part = TCA6416_PART,
        .i2c_adapter = &stm32_i2c_adapter,
        .i2c_addr = 0x20,
        .reset = IO_RESET,
    },
    {
        .part = TCA6416_PART,
        .i2c_adapter = &stm32_i2c_adapter,
        .i2c_addr = 0x23,
        .reset = TCA64XX_IO_UNUSED,
    },
};

static struct gpio_device tca64xx_io_expander[] = {
    {
        .count = 16,
        .device = {
            .name = "tca6416",
            .description = "TCA6416 U90",
            .driver = "tca64xx",

            .irq = 
            .pdata = &tca64xx_io_expander_pdata[0],
        },
    },
    {
        .count = 16,
        .device = {
            .name = "tca6416",
            .description = "TCA6416 U96",
            .driver = "tca64xx",

            .irq = 
            .pdata = &tca64xx_io_expander_pdata[1],
        },
    },
    {
        .count = 16,
        .device = {
            .name = "tca6416",
            .description = "TCA6416 U135",
            .driver = "tca64xx",

            .irq = 
            .pdata = &tca64xx_io_expander_pdata[2],
        },
    },
};
#endif

static void uart_init(void)
{
    stm32_clk_enable(STM32_CLK_USART1);
    stm32_reset(STM32_RST_USART1);

    stm32_configgpio(GPIO_PORTB | GPIO_PIN6 | GPIO_AF7 | GPIO_ALT_FCT |
                     GPIO_PULLUP);
    stm32_configgpio(GPIO_PORTB | GPIO_PIN7 | GPIO_AF7 | GPIO_ALT_FCT |
                     GPIO_PULLUP);

    write32(STM32_USART1_CR1, STM32_USART1_CR1_UE);
    write32(STM32_USART1_BRR, STM32_USART1_BRR_APB2_84MHZ_B115200);
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

static void display_temperature(void)
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
     *     PLL: 168MHz
     *     AHB: 168MHz
     *     APB1: 42MHz
     *     APB2: 84Mhz
     */
    write32(RCC_PLLCFGR, RCC_PLLCFGR_PLLSRC_HSI |
                         (336 << RCC_PLLCFGR_PLLN_OFFSET) | RCC_PLLCFGR_PLLP4 |
                         (8 << RCC_PLLCFGR_PLLM_OFFSET));
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

    display_temperature();

    for (int i = 0; i < ARRAY_SIZE(gpio_port); i++)
        device_register(&gpio_port[i].device);
    device_register(&stm32_usart_device.device);
    device_register(&stm32_i2c_adapter.device);
}

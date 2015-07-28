#include <asm/hwio.h>
#include <asm/delay.h>
#include <asm/gpio.h>

#include <phabos/kprintf.h>
#include <phabos/driver.h>
#include <phabos/serial/uart.h>

#define STM32_USART1_BASE   0x40011000
#define STM32_GPIOB_BASE    0x40020400
#define STM32_RCC_BASE      0x40023800

#define STM32_GPIOB_MODER   (STM32_GPIOB_BASE + 0x00)
#define STM32_GPIOB_PUPDR   (STM32_GPIOB_BASE + 0x0c)
#define STM32_GPIOB_BSRR    (STM32_GPIOB_BASE + 0x18)
#define STM32_GPIOB_AFRL    (STM32_GPIOB_BASE + 0x20)

#define STM32_RCC_CR            (STM32_RCC_BASE + 0x00)
#define STM32_RCC_CFGR          (STM32_RCC_BASE + 0x08)
#define STM32_RCC_CIR           (STM32_RCC_BASE + 0x0c)
#define STM32_RCC_AHB1RSTR      (STM32_RCC_BASE + 0x10)
#define STM32_RCC_APB2RSTR      (STM32_RCC_BASE + 0x24)
#define STM32_RCC_AHB1ENR      (STM32_RCC_BASE + 0x30)
#define STM32_RCC_APB2ENR       (STM32_RCC_BASE + 0x44)

#define STM32_USART1_SR     (STM32_USART1_BASE + 0x00)
#define STM32_USART1_DR     (STM32_USART1_BASE + 0x04)
#define STM32_USART1_BRR    (STM32_USART1_BASE + 0x08)
#define STM32_USART1_CR1    (STM32_USART1_BASE + 0x0c)

#define STM32_IRQ_USART1    37

static int stm32_usart_power_on(struct device *device)
{
    return 0;
}

static struct uart_device stm32_usart_device = {
    .device = {
        .name = "stm32-usart1",
        .description = "STM32 USART 1",
        .driver = "stm32-usart",

        .reg_base = STM32_USART1_BASE,
        .irq = STM32_IRQ_USART1,

        .power_on = stm32_usart_power_on,
    //    .power_off = tsb_hcd_power_off,
    },
};

#define RCC_CR      (STM32_RCC_BASE + 0x00)
#define RCC_PLLCFGR (STM32_RCC_BASE + 0x04)
#define RCC_CFGR    (STM32_RCC_BASE + 0x08)

#define RCC_CR_HSEON    (1 << 16)
#define RCC_CR_HSERDY   (1 << 17)
#define RCC_CR_HSEBYP   (1 << 18)
#define RCC_CR_PLLON    (1 << 24)
#define RCC_CR_PLLRDY   (1 << 25)

#define RCC_PLLCFGR_PLLP2       (0 << 16)
#define RCC_PLLCFGR_PLLP4       (1 << 16)
#define RCC_PLLCFGR_PLLP6       (2 << 16)
#define RCC_PLLCFGR_PLLP8       (3 << 16)
#define RCC_PLLCFGR_PLLSRC_HSI  (0 << 22)
#define RCC_PLLCFGR_PLLSRC_HSE  (1 << 22)
#define RCC_PLLCFGR_PLLM_OFFSET 0
#define RCC_PLLCFGR_PLLN_OFFSET 6

#define RCC_CFGR_SW_HSI         (0 << 0)
#define RCC_CFGR_SW_HSE         (1 << 0)
#define RCC_CFGR_SW_PLL         (2 << 0)
#define RCC_CFGR_HPRE_DIV1      (0 << 4)
#define RCC_CFGR_HPRE_DIV2      (8 << 4)
#define RCC_CFGR_HPRE_DIV4      (9 << 4)
#define RCC_CFGR_HPRE_DIV8      (10 << 4)
#define RCC_CFGR_HPRE_DIV16     (11 << 4)
#define RCC_CFGR_HPRE_DIV64     (12 << 4)
#define RCC_CFGR_HPRE_DIV128    (13 << 4)
#define RCC_CFGR_HPRE_DIV256    (14 << 4)
#define RCC_CFGR_HPRE_DIV512    (15 << 4)
#define RCC_CFGR_PPRE1_DIV1     (0 << 10)
#define RCC_CFGR_PPRE1_DIV2     (4 << 10)
#define RCC_CFGR_PPRE1_DIV4     (5 << 10)
#define RCC_CFGR_PPRE1_DIV8     (6 << 10)
#define RCC_CFGR_PPRE1_DIV16    (7 << 10)
#define RCC_CFGR_PPRE2_DIV1     (0 << 13)
#define RCC_CFGR_PPRE2_DIV2     (4 << 13)
#define RCC_CFGR_PPRE2_DIV4     (5 << 13)
#define RCC_CFGR_PPRE2_DIV8     (6 << 13)
#define RCC_CFGR_PPRE2_DIV16    (7 << 13)

void machine_init(void)
{
    /*
     * PLL: 168MHz
     * AHB: 168MHz
     * APB1: 42MHz
     * APB2: 84Mhz
     */
    write32(RCC_PLLCFGR, RCC_PLLCFGR_PLLSRC_HSI |
                         (336 << RCC_PLLCFGR_PLLN_OFFSET) | RCC_PLLCFGR_PLLP4 |
                         (8 << RCC_PLLCFGR_PLLM_OFFSET));
    write32(RCC_CFGR, RCC_CFGR_SW_PLL | RCC_CFGR_PPRE1_DIV4 |
                      RCC_CFGR_PPRE2_DIV2);
    read32(RCC_CR) |= RCC_CR_PLLON;

    // XXX: Enable all GPIOs for now
    read32(STM32_RCC_AHB1ENR) |= 0x1ff;
    read32(STM32_RCC_AHB1RSTR) |= 0x1ff;
    read32(STM32_RCC_AHB1RSTR) &= ~0x1ff;

    // XXX: Enable USART1
    read32(STM32_GPIOB_MODER) |= 0x2 << 12 | 0x2 << 14;
    read32(STM32_GPIOB_PUPDR) |= 0x1 << 12 | 0x1 << 14;
    read32(STM32_GPIOB_AFRL) |= 0x7 << 24 | 0x7 << 28;

    read32(STM32_RCC_APB2ENR) |= (1 << 4);
    read32(STM32_RCC_APB2RSTR) |= (1 << 4);
    read32(STM32_RCC_APB2RSTR) &= ~(1 << 4);

    write32(STM32_USART1_CR1, (1 << 13));
    write32(STM32_USART1_BRR, (45 << 4) | 9);
    read32(STM32_USART1_CR1) |= (1 << 3) | (1 << 2);

    device_register(&stm32_usart_device.device);
}

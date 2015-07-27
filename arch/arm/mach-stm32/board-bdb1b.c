#include <asm/hwio.h>
#include <asm/delay.h>

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

static struct uart_device stm32_usart_device = {
    .device = {
        .name = "stm32-usart1",
        .description = "STM32 USART 1",
        .driver = "stm32-usart",

        .reg_base = STM32_USART1_BASE,
        .irq = STM32_IRQ_USART1,

    //    .power_on = tsb_hcd_power_on,
    //    .power_off = tsb_hcd_power_off,
    },
};

void machine_init(void)
{
//    write32(STM32_RCC_CR, (1 << 24) | 1);
//    write32(STM32_RCC_CFGR, 2);
//    write32(STM32_RCC_CIR, 0);

    read32(STM32_RCC_AHB1ENR) |= 0x3;  // Enable GPIOA and GPIOB
//    read32(STM32_RCC_AHB1RSTR) |= 0x3; // Reset GPIOA and GPIOB
//    mdelay(300);


    read32(STM32_GPIOB_MODER) &= ~(3 << 0 | 3 << 2);
    read32(STM32_GPIOB_MODER) |= (0x1 << 0) | (0x1 << 2);
//    read32(STM32_GPIOB_PUPDR) |= 0x1 << 0;
//    read32(STM32_GPIOB_PUPDR) &= ~(3 << 2);
    write32(STM32_GPIOB_BSRR, 1 << 16);
    write32(STM32_GPIOB_BSRR, 1 << 1);

    read32(STM32_GPIOB_MODER) |= 0x2 << 12 | 0x2 << 14;
    read32(STM32_GPIOB_PUPDR) |= 0x1 << 12 | 0x1 << 14;
    read32(STM32_GPIOB_AFRL) |= 0x7 << 24 | 0x7 << 28;

#if 1
    read32(STM32_RCC_APB2ENR) |= (1 << 4);
    read32(STM32_RCC_APB2RSTR) |= (1 << 4);

    read32(STM32_RCC_APB2RSTR) &= ~(1 << 4);

    write32(STM32_USART1_CR1, (1 << 13));
    write32(STM32_USART1_BRR, (8 << 4) | 11);
    read32(STM32_USART1_CR1) |= (1 << 3) | (1 << 2);
#endif

    device_register(&stm32_usart_device.device);
}

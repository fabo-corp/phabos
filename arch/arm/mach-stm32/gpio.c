#include <asm/gpio.h>
#include <asm/hwio.h>
#include <asm/irq.h>

#include <phabos/utils.h>

#include "stm32f4xx.h"

#include <errno.h>
#include <stdlib.h>

#define GPIO_BASE   0x40020000
#define GPIO_SIZE   0x400

#define SYSCONFIG_BASE  STM32_SYSCFG_BASE
#define EXTI_BASE       STM32_EXTI_BASE

#define GPIO_MODER      0x00
#define GPIO_OTYPER     0x04
#define GPIO_OSPEEDR    0x08
#define GPIO_PUPDR      0x0c
#define GPIO_BSRR       0x18
#define GPIO_AFRL       0x20
#define GPIO_AFRH       0x24

#define SYSCFG_EXTICR1  0x08

#define EXTI_IMR        0x00
#define EXTI_EMR        0x04
#define EXTI_RTSR       0x08
#define EXTI_FTSR       0x0c
#define EXTI_SWIER      0x10
#define EXTI_PR         0x14

struct exti_handler {
    irq_handler_t handler;
    void *priv;
};

static struct exti_handler exti_handlers[16] = {{NULL,},};

static inline unsigned config_to_port(unsigned long config)
{
    return (config >> GPIO_PORT_OFFSET) & GPIO_PORT_MASK;
}

static inline uintptr_t config_to_regbase(unsigned long config)
{
    return GPIO_BASE + config_to_port(config) * GPIO_SIZE;
}

static inline int config_to_pin(unsigned long config)
{
    return (config >> GPIO_PIN_OFFSET) & GPIO_PIN_MASK;
}

static unsigned config_to_af(unsigned long config)
{
    return (config >> GPIO_AF_OFFSET) & GPIO_AF_MASK;
}

static void gpio_update(uint32_t reg, unsigned offset, unsigned long mask,
                        unsigned long config, unsigned long config_offset)
{
    irq_disable();

    uintptr_t base = config_to_regbase(config);

    uint32_t tmp = read32(base + reg);
    tmp &= ~(mask << offset);
    tmp |= ((config >> config_offset) & mask) << offset;
    write32(base + reg, tmp);

    irq_enable();
}

int stm32_configgpio(unsigned long config)
{
    int pin = config_to_pin(config);
    uint32_t mode = config & (GPIO_MODE_MASK << GPIO_MODE_OFFSET);

    if (config & GPIO_OUTPUT_CLEAR)
        stm32_gpiowrite(config, false);

    if (config & GPIO_OUTPUT_SET)
        stm32_gpiowrite(config, true);

    gpio_update(GPIO_OTYPER, pin, GPIO_TYPE_MASK, config, GPIO_TYPE_OFFSET);
    gpio_update(GPIO_PUPDR, pin << 1, GPIO_PUSHPULL_MASK, config,
                GPIO_PUSHPULL_OFFSET);
    gpio_update(GPIO_OSPEEDR, pin << 1, GPIO_SPEED_MASK, config,
                GPIO_SPEED_OFFSET);

    if (mode == GPIO_ALT_FCT) {
        unsigned af = config_to_af(config);

        if (af < 8) {
            gpio_update(GPIO_AFRL, pin << 2, GPIO_AF_MASK, config,
                        GPIO_AF_OFFSET);
        } else {
            gpio_update(GPIO_AFRH, (pin - 8) << 2, GPIO_AF_MASK,
                        config - (GPIO_AF8 << GPIO_AF_OFFSET), GPIO_AF_OFFSET);
        }
    }

    if (config & GPIO_EXTI) {
        irq_disable();
        uint32_t reg = SYSCONFIG_BASE + SYSCFG_EXTICR1 + 4 * (pin / 4);
        kprintf("reg = %x\n", reg);
        uint32_t exti = read32(reg) & ~(GPIO_PORT_MASK << (pin % 4));
        kprintf("exti = %x\n", exti | (config_to_port(config) << ((pin % 4) * 4)));
        write32(reg, exti | (config_to_port(config) << (pin % 4)));
        irq_enable();
    }

    gpio_update(GPIO_MODER, pin << 1, GPIO_MODE_MASK, config, GPIO_MODE_OFFSET);

    return 0;
}

int stm32_gpiowrite(unsigned long port, bool high_level)
{
    uintptr_t base = config_to_regbase(port);

    if (high_level)
        write32(base + GPIO_BSRR, 1 << config_to_pin(port));
    else
        write32(base + GPIO_BSRR, (1 << config_to_pin(port)) << 16);

    return 0;
}

static void update_bit(uint32_t addr, int bit, bool value)
{
    irq_disable();

    uint32_t val = read32(addr);

    val &= ~(1 << bit);
    val |= value << bit;

    write32(addr, val);

    irq_enable();
}

static void exti_interrupt(int irq, void *priv)
{
    unsigned pin;
    uint32_t pr;

    switch (irq) {
    case STM32_IRQ_EXTI0 ... STM32_IRQ_EXTI4:
        pin = irq - STM32_IRQ_EXTI0;
        break;

    case STM32_IRQ_EXTI9_5:
        pr = (read32(EXTI_BASE + EXTI_PR) >> 5) & 0x1f;
        if (!pr)
            return;

        pin = 5;
        for (; !(pr & 1); pr >>= 1)
            pin++;
        break;

    case STM32_IRQ_EXTI15_10:
        pr = (read32(EXTI_BASE + EXTI_PR) >> 10) & 0x3f;
        if (!pr)
            return;

        pin = 10;
        for (; !(pr & 1); pr >>= 1)
            pin++;
        break;
    }

    write32(EXTI_BASE + EXTI_PR, 1 << pin);

    if (exti_handlers[pin].handler)
        exti_handlers[pin].handler(pin, exti_handlers[pin].priv);
}

int stm32_gpiosetevent_priv(unsigned long config, bool rising_edge,
                            bool falling_edge, bool event,
                            irq_handler_t handler, void *priv)
{
    static bool is_initialized = false;
    int pin = config_to_pin(config);

    if (!handler)
        return -EINVAL;

    update_bit(EXTI_BASE + EXTI_IMR, pin, true);
    update_bit(EXTI_BASE + EXTI_EMR, pin, event);
    update_bit(EXTI_BASE + EXTI_RTSR, pin, rising_edge);
    update_bit(EXTI_BASE + EXTI_FTSR, pin, falling_edge);

    exti_handlers[pin].handler = handler;
    exti_handlers[pin].priv = priv;

    if (!is_initialized) {
        static int irqs[] = {
            STM32_IRQ_EXTI0, STM32_IRQ_EXTI1, STM32_IRQ_EXTI2,
            STM32_IRQ_EXTI3, STM32_IRQ_EXTI4, STM32_IRQ_EXTI9_5,
            STM32_IRQ_EXTI15_10 };

        is_initialized = true;
        for (int i = 0; i < ARRAY_SIZE(irqs); i++) {
            irq_attach(irqs[i], exti_interrupt, NULL);
            irq_enable_line(irqs[i]);
        }
    }

    stm32_configgpio(config);

    return 0;
}

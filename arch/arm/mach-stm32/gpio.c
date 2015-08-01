#include <asm/gpio.h>
#include <asm/hwio.h>

#define GPIO_MODER_INPUT        0
#define GPIO_MODER_OUTPUT       1
#define GPIO_MODER_ALT_FUNCTION 2 

#define GPIO_BASE   0x40020000
#define GPIO_SIZE   0x400

#define GPIO_MODER   0x00
#define GPIO_OTYPER  0x04
#define GPIO_PUPDR   0x0c
#define GPIO_BSRR    0x18
#define GPIO_AFRL    0x20

static inline uintptr_t config_to_regbase(unsigned long config)
{
    return GPIO_BASE + ((config >> 4) & 0xff) * 0x400;
}

static inline int config_to_pin(unsigned long config)
{
    return config & 0xf;
}

int stm32_configgpio(unsigned long config)
{
    uintptr_t base = config_to_regbase(config);
    int pin = config_to_pin(config);
    uint32_t moder;

    if (config & GPIO_OUTPUT_CLEAR)
        stm32_gpiowrite(config, false);

    if (config & GPIO_OUTPUT_SET)
        stm32_gpiowrite(config, true);

    moder = read32(base + GPIO_MODER);
    moder &= ~(3 << (pin << 1));

    if (config & GPIO_OUTPUT)
        moder |= GPIO_MODER_OUTPUT << (pin << 1);

    write32(base + GPIO_MODER, moder);

    if (config & GPIO_OPENDRAIN) {
        read32(base + GPIO_OTYPER) |= 1 << pin;
    } else {
        read32(base + GPIO_OTYPER) &= ~(1 << pin);
    }

    read32(base + GPIO_PUPDR) &= ~(3 << (pin << 1));

    if ((config & GPIO_PULLDOWN) == GPIO_PULLDOWN)
        read32(base + GPIO_PUPDR) |= GPIO_PULLDOWN << (pin << 1);
    else if ((config & GPIO_PULLUP) == GPIO_PULLUP)
        read32(base + GPIO_PUPDR) |= GPIO_PULLUP << (pin << 1);

    return 0;
}

int stm32_gpiowrite(unsigned long port, bool high_level)
{
    uintptr_t base = config_to_regbase(port);

//    stm32_configgpio(port);

    if (high_level) {
        write32(base + GPIO_BSRR, 1 << config_to_pin(port));
    } else {
        write32(base + GPIO_BSRR, (1 << config_to_pin(port)) << 16);
    }

    return 0;
}

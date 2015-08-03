#include <asm/gpio.h>
#include <asm/hwio.h>

#define GPIO_BASE   0x40020000
#define GPIO_SIZE   0x400

#define GPIO_MODER      0x00
#define GPIO_OTYPER     0x04
#define GPIO_OSPEEDR    0x08
#define GPIO_PUPDR      0x0c
#define GPIO_BSRR       0x18
#define GPIO_AFRL       0x20
#define GPIO_AFRH       0x24

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
    uintptr_t base = config_to_regbase(config);

    uint32_t tmp = read32(base + reg);
    tmp &= ~(mask << offset);
    tmp |= ((config >> config_offset) & mask) << offset;
    write32(base + reg, tmp);
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

#ifndef __STM32_GPIO_H__
#define __STM32_GPIO_H__

#include <stdbool.h>

#define GPIO_PORTA  (0 << 0)
#define GPIO_PORTB  (1 << 0)
#define GPIO_PORTC  (2 << 0)
#define GPIO_PORTD  (3 << 0)
#define GPIO_PORTE  (4 << 0)
#define GPIO_PORTF  (5 << 0)
#define GPIO_PORTG  (6 << 0)
#define GPIO_PORTH  (7 << 0)
#define GPIO_PORTI  (8 << 0)

#define GPIO_PIN0   (0 << 8)
#define GPIO_PIN1   (1 << 8)
#define GPIO_PIN2   (2 << 8)
#define GPIO_PIN3   (3 << 8)
#define GPIO_PIN4   (4 << 8)
#define GPIO_PIN5   (5 << 8)
#define GPIO_PIN6   (6 << 8)
#define GPIO_PIN7   (7 << 8)
#define GPIO_PIN8   (8 << 8)
#define GPIO_PIN9   (9 << 8)
#define GPIO_PIN10  (10 << 8)
#define GPIO_PIN11  (11 << 8)
#define GPIO_PIN12  (12 << 8)
#define GPIO_PIN13  (13 << 8)
#define GPIO_PIN14  (14 << 8)
#define GPIO_PIN15  (15 << 8)

#define GPIO_INPUT      (0 << 16)
#define GPIO_OUTPUT     (1 << 16)

#define GPIO_OUTPUT_CLEAR (1 << 21)
#define GPIO_OUTPUT_SET   (1 << 17)

#define GPIO_PUSHPULL   (0 << 18)
#define GPIO_OPENDRAIN  (1 << 18)

#define GPIO_FLOAT      (0 << 19)
#define GPIO_PULLUP     (1 << 19)
#define GPIO_PULLDOWN   (2 << 19)

int stm32_configgpio(unsigned long config);
int stm32_gpiowrite(unsigned long port, bool high_level);

#endif /* __STM32_GPIO_H__ */


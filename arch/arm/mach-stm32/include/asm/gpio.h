#ifndef __STM32_GPIO_H__
#define __STM32_GPIO_H__

#define GPIO_PORTA  0
#define GPIO_PORTB  1
#define GPIO_PORTC  2
#define GPIO_PORTD  3
#define GPIO_PORTE  4
#define GPIO_PORTF  5

#define GPIO_PIN0   0
#define GPIO_PIN1   1
#define GPIO_PIN2   2
#define GPIO_PIN3   3
#define GPIO_PIN4   4
#define GPIO_PIN5   5
#define GPIO_PIN6   6
#define GPIO_PIN7   7
#define GPIO_PIN8   8
#define GPIO_PIN9   9
#define GPIO_PIN10  10
#define GPIO_PIN11  11
#define GPIO_PIN12  12
#define GPIO_PIN13  13
#define GPIO_PIN14  14
#define GPIO_PIN15  15

#define GPIO_OPENDRAIN 1

#define GPIO_INPUT  0
#define GPIO_OUTPUT 1

#define GPIO_OUTPUT_SET (1 << 8)

int stm32_configgpio(unsigned int config);
int stm32_gpiowrite(unsigned int port, unsigned int value);

#endif /* __STM32_GPIO_H__ */


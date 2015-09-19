/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#ifndef __STM32_GPIO_H__
#define __STM32_GPIO_H__

#include <stdbool.h>
#include <asm/irq.h>

/*
 * config[21:21] = EXTI
 * config[20:20] = GPIO Output set
 * config[19:19] = GPIO Output clear
 * config[18:17] = Floating/Pull up/Pull down
 * config[16:16] = Output type
 * config[15:14] = Mode
 * config[13:12] = Speed
 * config[11:8]  = Alternate Function
 * config[7:4]   = Port
 * config[3:0]   = Pin
 */
#define GPIO_PIN_OFFSET         0
#define GPIO_PORT_OFFSET        4
#define GPIO_AF_OFFSET          8
#define GPIO_SPEED_OFFSET       12
#define GPIO_MODE_OFFSET        14
#define GPIO_TYPE_OFFSET        16
#define GPIO_PUSHPULL_OFFSET    17

#define GPIO_PIN_MASK           0xf
#define GPIO_PORT_MASK          0xf
#define GPIO_AF_MASK            0xf
#define GPIO_SPEED_MASK         0x3
#define GPIO_MODE_MASK          0x3
#define GPIO_TYPE_MASK          0x1
#define GPIO_PUSHPULL_MASK      0x3

#define GPIO_PORTA  (0 << 4)
#define GPIO_PORTB  (1 << 4)
#define GPIO_PORTC  (2 << 4)
#define GPIO_PORTD  (3 << 4)
#define GPIO_PORTE  (4 << 4)
#define GPIO_PORTF  (5 << 4)
#define GPIO_PORTG  (6 << 4)
#define GPIO_PORTH  (7 << 4)
#define GPIO_PORTI  (8 << 4)

#define GPIO_AF0    (0 << 8)
#define GPIO_AF1    (1 << 8)
#define GPIO_AF2    (2 << 8)
#define GPIO_AF3    (3 << 8)
#define GPIO_AF4    (4 << 8)
#define GPIO_AF5    (5 << 8)
#define GPIO_AF6    (6 << 8)
#define GPIO_AF7    (7 << 8)
#define GPIO_AF8    (8 << 8)
#define GPIO_AF9    (9 << 8)
#define GPIO_AF10   (10 << 8)
#define GPIO_AF11   (11 << 8)
#define GPIO_AF12   (12 << 8)
#define GPIO_AF13   (13 << 8)
#define GPIO_AF14   (14 << 8)
#define GPIO_AF15   (15 << 8)

#define GPIO_PIN0   (0 << 0)
#define GPIO_PIN1   (1 << 0)
#define GPIO_PIN2   (2 << 0)
#define GPIO_PIN3   (3 << 0)
#define GPIO_PIN4   (4 << 0)
#define GPIO_PIN5   (5 << 0)
#define GPIO_PIN6   (6 << 0)
#define GPIO_PIN7   (7 << 0)
#define GPIO_PIN8   (8 << 0)
#define GPIO_PIN9   (9 << 0)
#define GPIO_PIN10  (10 << 0)
#define GPIO_PIN11  (11 << 0)
#define GPIO_PIN12  (12 << 0)
#define GPIO_PIN13  (13 << 0)
#define GPIO_PIN14  (14 << 0)
#define GPIO_PIN15  (15 << 0)

#define GPIO_SPEED_LOW      (0 << 12)
#define GPIO_SPEED_MEDIUM   (1 << 12)
#define GPIO_SPEED_FAST     (2 << 12)
#define GPIO_SPEED_HIGH     (3 << 12)

#define GPIO_INPUT      (0 << 14)
#define GPIO_OUTPUT     (1 << 14)
#define GPIO_ALT_FCT    (2 << 14)

#define GPIO_PUSHPULL   (0 << 16)
#define GPIO_OPENDRAIN  (1 << 16)

#define GPIO_FLOAT      (0 << 17)
#define GPIO_PULLUP     (1 << 17)
#define GPIO_PULLDOWN   (2 << 17)

#define GPIO_OUTPUT_CLEAR (1 << 19)
#define GPIO_OUTPUT_SET   (1 << 20)
#define GPIO_EXTI         (1 << 21)

int stm32_configgpio(unsigned long config);
int stm32_gpiowrite(unsigned long port, bool high_level);

int stm32_gpiosetevent_priv(unsigned long config, bool rising_edge,
                            bool falling_edge, bool event,
                            irq_handler_t handler, void *priv);

#endif /* __STM32_GPIO_H__ */


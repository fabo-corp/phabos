/*
 * Copyright (c) 2015 Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef  __BDB_H__
#define  __BDB_H__

#define STM32_GPIO_CHIP_BASE 0 // XXX phabos porting

/*
 * GPIO Chip pins
 *
 * STM32 GPIO starts at STM32_GPIO_CHIP_BASE
 */
#define STM32_GPIO_CHIP_NR      140

/* BDB1B/2A */
#define U96_GPIO_CHIP_START     (STM32_GPIO_CHIP_BASE + STM32_GPIO_CHIP_NR)
#define U96_GPIO_CHIP_NR        16
#define U90_GPIO_CHIP_START     (U96_GPIO_CHIP_START + U96_GPIO_CHIP_NR)
#define U90_GPIO_CHIP_NR        16
#define U135_GPIO_CHIP_START    (U90_GPIO_CHIP_START + U90_GPIO_CHIP_NR)
#define U135_GPIO_CHIP_NR       24

/* SDB */
#define U701_GPIO_CHIP_START    (STM32_GPIO_CHIP_BASE + STM32_GPIO_CHIP_NR)
#define U701_GPIO_CHIP_NR       16

/* GPIO Chip pin number macro */
#define STM32_GPIO_PIN(p)       (STM32_GPIO_CHIP_BASE + (p))
#define U96_GPIO_PIN(p)         (U96_GPIO_CHIP_START + (p))
#define U90_GPIO_PIN(p)         (U90_GPIO_CHIP_START + (p))
#define U135_GPIO_PIN(p)        (U135_GPIO_CHIP_START + (p))
#define U701_GPIO_PIN(p)        (U701_GPIO_CHIP_START + (p))

#endif

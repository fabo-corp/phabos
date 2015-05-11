/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#ifndef __ARM_HWIO_H__
#define __ARM_HWIO_H__

#include <stdint.h>

#define write8(x, y) *((volatile uint8_t*) (x)) = (uint8_t)y
#define write16(x, y) *((volatile uint16_t*) (x)) = (uint16_t)y
#define write32(x, y) *((volatile uint32_t*) (x)) = (uint32_t)y

#define read8(x) *((volatile uint8_t*) (x))
#define read16(x) *((volatile uint16_t*) (x))
#define read32(x) *((volatile uint32_t*) (x))

#endif /* __ARM_HWIO_H__ */


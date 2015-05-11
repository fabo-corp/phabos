/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#ifndef __LINKER_UTILS_H__
#define __LINKER_UTILS_H__

#include <config.h>

#define BOOT_STORAGE > FLASH
#define CODE_STORAGE > FLASH
#define DATA_STORAGE > SRAM AT> FLASH

#ifdef CONFIG_BOOT_COPYTORAM
#undef CODE_STORAGE
#define CODE_STORAGE > SRAM AT> FLASH
#endif

#ifdef CONFIG_BOOT_RAM
#undef BOOT_STORAGE
#undef CODE_STORAGE
#undef DATA_STORAGE

#define BOOT_STORAGE > SRAM
#define CODE_STORAGE > SRAM
#define DATA_STORAGE > SRAM
#endif

#endif /* __LINKER_UTILS_H__ */


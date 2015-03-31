/*
 * Copyright (C) 2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#ifndef __KPRINTF_H__
#define __KPRINTF_H__

void kputc(char c);
int kputs(const char* str);
int kprintf(const char* format, ...) __attribute__((format(printf, 1, 2)));

#endif /* __KPRINTF_H__ */


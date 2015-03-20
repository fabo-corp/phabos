/*
 * Copyright (C) 2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#ifndef __ARM_BYTEORDERING_H__
#define __ARM_BYTEORDERING_H__

#include <config.h>
#include <stdint.h>

typedef uint32_t le32_t;
typedef uint32_t be32_t;
typedef uint16_t le16_t;
typedef uint16_t be16_t;

#ifdef CONFIG_LITTLE_ENDIAN

static inline le32_t cpu_to_le32(uint32_t x)
{
    return x;
}

static inline le16_t cpu_to_le16(uint16_t x)
{
    return x;
}

static inline uint32_t le32_to_cpu(le32_t x)
{
    return x;
}

static inline uint16_t le16_to_cpu(le16_t x)
{
    return x;
}

static inline be32_t cpu_to_be32(uint32_t x)
{
    be32_t y;
    asm volatile("rev %0, %1" : "=r"(y) : "r"(x));
    return y;
}

static inline be16_t cpu_to_be16(uint16_t x)
{
    be16_t y;
    asm volatile("rev16 %0, %1" : "=r"(y) : "r"(x));
    return y;
}

static inline uint32_t be32_to_cpu(be32_t x)
{
    uint32_t y;
    asm volatile("rev %0, %1" : "=r"(y) : "r"(x));
    return y;
}

static inline uint16_t be16_to_cpu(be16_t x)
{
    uint16_t y;
    asm volatile("rev16 %0, %1" : "=r"(y) : "r"(x));
    return y;
}

#elif CONFIG_BIG_ENDIAN

static inline le32_t cpu_to_le32(uint32_t x)
{
    uint32_t y;
    asm volatile("rev %0, %1" : "=r"(y) : "r"(x));
    return y;
}

static inline le16_t cpu_to_le16(uint16_t x)
{
    le16_t y;
    asm volatile("rev16 %0, %1" : "=r"(y) : "r"(x));
    return y;
}

static inline uint32_t le32_to_cpu(le32_t x)
{
    le32_t y;
    asm volatile("rev %0, %1" : "=r"(y) : "r"(x));
    return y;
}

static inline uint16_t le16_to_cpu(le16_t x)
{
    uint16_t y;
    asm volatile("rev16 %0, %1" : "=r"(y) : "r"(x));
    return y;
}

static inline be32_t cpu_to_be32(uint32_t x)
{
    return x;
}

static inline be16_t cpu_to_be16(uint16_t x)
{
    return x;
}

static inline uint32_t be32_to_cpu(be32_t x)
{
    return x;
}

static inline uint16_t be16_to_cpu(be16_t x)
{
    return x;
}

#endif /* CONFIG_BIG_ENDIAN */

#endif /* __ARM_BYTEORDERING_H__ */


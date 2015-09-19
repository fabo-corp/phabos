/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#include <config.h>

#include <asm/mpu.h>
#include <asm/hwio.h>
#include <asm/irq.h>

#include <phabos/assert.h>

#include <errno.h>

#define MPU_SUBREGION_COUNT 8

#define MPU_TYPE 0xe000ed90
#define MPU_CTRL 0xe000ed94
#define MPU_RNR  0xe000ed98
#define MPU_RBAR  0xe000ed9c
#define MPU_RASR 0xe000eda0

#define SHCSR 0xe000ed24
#define SHCSR_MEMFAULTENA   (1 << 16)

#define MPU_TYPE_DREGION_OFFSET 8

#define MPU_CTRL_ENABLE         (1 << 0)
#define MPU_CTRL_HFNMIENA       (1 << 1)
#define MPU_CTRL_PRIVDEFENA     (1 << 2)

#define MPU_RBAR_VALID          (1 << 4)

#define MPU_RASR_ENABLE         (1 << 0)
#define MPU_RASR_AP_NANA        (0 << 24)
#define MPU_RASR_AP_RWNA        (1 << 24)
#define MPU_RASR_AP_RWRO        (2 << 24)
#define MPU_RASR_AP_RWRW        (3 << 24)
#define MPU_RASR_AP_RONA        (5 << 24)
#define MPU_RASR_AP_RORO        (6 << 24)

#define NR_REGION 8

static size_t mpu_region_count;
static uint8_t region_bitmap;

static void mpu_disable_all(void)
{
    mpu_disable();

    for (unsigned i = 0; i < mpu_region_count; i++) {
        mpu_disable_region(i);
    }
}

static int mpu_setup_null_region(void)
{
    int retval;

    retval = mpu_request_region(7);
    RET_IF_FAIL(!retval, retval);

    retval = mpu_setup_region(7, 0x00000000, 5, MPU_XN, 0, 0);
    RET_IF_FAIL(!retval, retval);

    retval = mpu_enable_region(7);
    RET_IF_FAIL(!retval, retval);

    return 0;
}

int mpu_init(void)
{
    mpu_region_count = (read32(MPU_TYPE) >> MPU_TYPE_DREGION_OFFSET) & 0xff;
    if (!mpu_region_count)
        return -ENODEV;

    mpu_disable_all();

    read32(SHCSR) |= SHCSR_MEMFAULTENA;
    read32(MPU_CTRL) |= MPU_CTRL_PRIVDEFENA;

#ifdef CONFIG_MPU_NULL_REGION
    mpu_setup_null_region();
#endif

    return 0;
}

void mpu_disable(void)
{
    write32(MPU_CTRL, read32(MPU_CTRL) & ~MPU_CTRL_ENABLE);
    asm volatile("dsb; isb");
}

void mpu_enable(void)
{
    read32(MPU_CTRL) |= MPU_CTRL_ENABLE;
    asm volatile("dsb; isb");
}

int mpu_disable_region(unsigned region)
{
    RET_IF_FAIL(region < mpu_region_count, -EINVAL);

    write32(MPU_RNR, region);
    read32(MPU_RASR) &= ~1;

    return 0;
}

static unsigned encode_ap(unsigned ap)
{
    if (!ap)
        return MPU_RASR_AP_NANA;

    if (ap & MPU_UA_RW)
        return MPU_RASR_AP_RWRW;

    if (ap & MPU_UA_RO) {
        if (ap & MPU_PA_RW)
            return MPU_RASR_AP_RWRO;
        return MPU_RASR_AP_RORO;
    }

    if (ap & MPU_PA_RW)
        return MPU_RASR_AP_RWNA;
    return MPU_RASR_AP_RONA;
}

int mpu_setup_region(unsigned region, uint32_t addr, size_t order,
                     unsigned long attributes, unsigned ap, unsigned tex)
{
    RET_IF_FAIL(region < mpu_region_count, -EINVAL);
    RET_IF_FAIL(!(attributes & MPU_ATTRIBUTES_MASK), -EINVAL);
    RET_IF_FAIL(order <= 32 && order >= 5, -EINVAL);
    RET_IF_FAIL(tex < 8, -EINVAL);

    write32(MPU_RNR, region | MPU_RBAR_VALID  | (addr & ~0x1f));
    write32(MPU_RASR, attributes | ((order - 1) << 1) | encode_ap(ap) |
                      (tex << 19));

    return 0;
}

int mpu_enable_region(unsigned region)
{
    RET_IF_FAIL(region < mpu_region_count, -EINVAL);

    write32(MPU_RNR, region);
    read32(MPU_RASR) |= MPU_RASR_ENABLE;

    return 0;
}

int mpu_disable_subregion(unsigned region, unsigned subregion)
{
    RET_IF_FAIL(region < mpu_region_count, -EINVAL);
    RET_IF_FAIL(subregion < MPU_SUBREGION_COUNT, -EINVAL);

    write32(MPU_RNR, region);
    read32(MPU_RASR) |= subregion << 8;

    return 0;
}

int mpu_request_region(unsigned region)
{
    int retval;

    RET_IF_FAIL(region < mpu_region_count, -EINVAL);
    RET_IF_FAIL(region < NR_REGION, -EINVAL);

    irq_disable();
    if (region_bitmap & (1 << region)) {
        retval = -EBUSY;
    } else {
        region_bitmap |= 1 << region;
        retval = 0;
    }
    irq_enable();

    return retval;
}

int mpu_release_region(unsigned region)
{
    int retval;

    RET_IF_FAIL(region < mpu_region_count, -EINVAL);
    RET_IF_FAIL(region < NR_REGION, -EINVAL);

    irq_disable();
    if (region_bitmap & (1 << region)) {
        region_bitmap &= ~(1 << region);
        retval = 0;
    } else {
        retval = -EINVAL;
    }
    irq_enable();

    return retval;
}

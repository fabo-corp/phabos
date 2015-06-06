#ifndef __ARM_MPU_H__
#define __ARM_MPU_H__

#include <stddef.h>
#include <stdint.h>

#define MPU_BUFFERABLE  (1 << 16)
#define MPU_CACHEABLE   (1 << 17)
#define MPU_SHAREABLE   (1 << 18)
#define MPU_XN          (1 << 28)
#define MPU_ATTRIBUTES_MASK     ~(MPU_BUFFERABLE | MPU_CACHEABLE | \
                                  MPU_SHAREABLE | MPU_XN)

#define MPU_PA_RO       (1 << 0)
#define MPU_PA_RW       (3 << 0)
#define MPU_UA_RO       (1 << 2)
#define MPU_UA_RW       (3 << 2)

int mpu_init(void);
void mpu_disable(void);
void mpu_enable(void);
int mpu_disable_region(unsigned region);
int mpu_setup_region(unsigned region, uint32_t addr, size_t size,
                     unsigned long attributes, unsigned ap, unsigned tex);
int mpu_enable_region(unsigned region);
int mpu_disable_subregion(unsigned region, unsigned subregion);
int mpu_enable_subregion(unsigned region, unsigned subregion);
int mpu_request_region(unsigned region);
int mpu_release_region(unsigned region);

#endif /* __ARM_MPU_H__ */


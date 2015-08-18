#ifndef __STM32_I2C_H__
#define __STM32_I2C_H__

struct stm32_i2c_master_platform {
    unsigned int evt_irq;
    unsigned int err_irq;
    unsigned long clk;
};

#endif /* __STM32_I2C_H__ */


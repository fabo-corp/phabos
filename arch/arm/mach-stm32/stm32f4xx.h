#ifndef __STM32F4XX_H__
#define __STM32F4XX_H__

#define STM32_UART7_BASE        0x40007800
#define STM32_UART8_BASE        0x40007c00
#define STM32_TIM1_BASE         0x40010000
#define STM32_TIM8_BASE         0x40010400
#define STM32_USART1_BASE       0x40011000
#define STM32_USART6_BASE       0x40011400
#define STM32_ADC_BASE          0x40012000
#define STM32_SDIO_BASE         0x40012c00
#define STM32_SPI1_BASE         0x40013000
#define STM32_SPI4_BASE         0x40013400
#define STM32_SYSCFG_BASE       0x40013800
#define STM32_EXTI_BASE         0x40013c00
#define STM32_TIM9_BASE         0x40014000
#define STM32_TIM10_BASE        0x40014400
#define STM32_TIM11_BASE        0x40014800
#define STM32_SPI5_BASE         0x40015000
#define STM32_SPI6_BASE         0x40015400
#define STM32_SAI1_BASE         0x40015800
#define STM32_LCD_BASE          0x40016800
#define STM32_GPIO_BASE         0x40020000
#define STM32_CRC_BASE          0x40023000
#define STM32_RCC_BASE          0x40023800
#define STM32_FLASH_BASE        0x40023c00
#define STM32_BKPSRAM_BASE      0x40024000
#define STM32_DMA1_BASE         0x40026000
#define STM32_DMA2_BASE         0x40026400
#define STM32_ETH_MAC_BASE      0x40028000
#define STM32_DMA2D_BASE        0x4002B000
#define STM32_USB_OTG_HS_BASE   0x40040000
#define STM32_USB_OTG_FS_BASE   0x50000000
#define STM32_DCMI_BASE         0x50050000
#define STM32_CRYP_BASE         0x50060000
#define STM32_HASH_BASE         0x50060400
#define STM32_RNG_BASE          0x50060800

#define STM32_GPIOA_BASE (STM32_GPIO_BASE + 0x0000)
#define STM32_GPIOB_BASE (STM32_GPIO_BASE + 0x0400)
#define STM32_GPIOC_BASE (STM32_GPIO_BASE + 0x0800)
#define STM32_GPIOD_BASE (STM32_GPIO_BASE + 0x0c00)
#define STM32_GPIOE_BASE (STM32_GPIO_BASE + 0x1000)
#define STM32_GPIOF_BASE (STM32_GPIO_BASE + 0x1400)
#define STM32_GPIOG_BASE (STM32_GPIO_BASE + 0x1800)
#define STM32_GPIOH_BASE (STM32_GPIO_BASE + 0x1c00)
#define STM32_GPIOI_BASE (STM32_GPIO_BASE + 0x2000)
#define STM32_GPIOJ_BASE (STM32_GPIO_BASE + 0x2400)
#define STM32_GPIOK_BASE (STM32_GPIO_BASE + 0x2800)

#define STM32_IRQ_WWDG                  0
#define STM32_IRQ_PVD                   1
#define STM32_IRQ_TAMP_STAMP            2
#define STM32_IRQ_RTC_WKUP              3
#define STM32_IRQ_FLASH                 4
#define STM32_IRQ_RCC                   5
#define STM32_IRQ_EXTI0                 6
#define STM32_IRQ_EXTI1                 7
#define STM32_IRQ_EXTI2                 8
#define STM32_IRQ_EXTI3                 9
#define STM32_IRQ_EXTI4                 10
#define STM32_IRQ_DMA1_STREAM0          11
#define STM32_IRQ_DMA1_STREAM1          12
#define STM32_IRQ_DMA1_STREAM2          13
#define STM32_IRQ_DMA1_STREAM3          14
#define STM32_IRQ_DMA1_STREAM4          15
#define STM32_IRQ_DMA1_STREAM5          16
#define STM32_IRQ_DMA1_STREAM6          17
#define STM32_IRQ_ADC                   18
#define STM32_IRQ_CAN1_TX               19
#define STM32_IRQ_CAN1_RX0              20
#define STM32_IRQ_CAN1_RX1              21
#define STM32_IRQ_CAN1_SCE              22
#define STM32_IRQ_EXTI9_5               23
#define STM32_IRQ_TIM1_BRK_TIM9         24
#define STM32_IRQ_TIM1_UP_TIM10         25
#define STM32_IRQ_TIM1_TRG_COM_TIM11    26
#define STM32_IRQ_TIM1_CC               27
#define STM32_IRQ_TIM2                  28
#define STM32_IRQ_TIM3                  29
#define STM32_IRQ_TIM4                  30
#define STM32_IRQ_I2C1_EV               31
#define STM32_IRQ_I2C1_ER               32
#define STM32_IRQ_I2C2_EV               33
#define STM32_IRQ_I2C2_ER               34
#define STM32_IRQ_SPI1                  35
#define STM32_IRQ_SPI2                  36
#define STM32_IRQ_USART1                37
#define STM32_IRQ_USART2                38
#define STM32_IRQ_USART3                39
#define STM32_IRQ_EXTI15_10             40
#define STM32_IRQ_RTC_ALARM             41
#define STM32_IRQ_OTG_FSWKUP            42
#define STM32_IRQ_TIM8_BRK_TIM12        43
#define STM32_IRQ_TIM8_UP_TIM13         44
#define STM32_IRQ_TIM8_TRG_COM_TIM14    45
#define STM32_IRQ_TIM8_CC               46
#define STM32_IRQ_DMA1_STREAM7          47
#define STM32_IRQ_FSMC                  48
#define STM32_IRQ_SDIO                  49
#define STM32_IRQ_TIM5                  50
#define STM32_IRQ_SPI3                  51
#define STM32_IRQ_UART4                 52
#define STM32_IRQ_UART5                 53
#define STM32_IRQ_TIM6_DAC              54
#define STM32_IRQ_TIM7                  55
#define STM32_IRQ_DMA2_STREAM0          56
#define STM32_IRQ_DMA2_STREAM1          57
#define STM32_IRQ_DMA2_STREAM2          58
#define STM32_IRQ_DMA2_STREAM3          59
#define STM32_IRQ_DMA2_STREAM4          60
#define STM32_IRQ_ETH                   61
#define STM32_IRQ_ETH_WKUP              62
#define STM32_IRQ_CAN2_TX               63
#define STM32_IRQ_CAN2_RX0              64
#define STM32_IRQ_CAN2_RX1              65
#define STM32_IRQ_CAN2_SCE              66
#define STM32_IRQ_OTG_FS                67
#define STM32_IRQ_DMA2_STREAM5          68
#define STM32_IRQ_DMA2_STREAM6          69
#define STM32_IRQ_DMA2_STREAM7          70
#define STM32_IRQ_USART6                71
#define STM32_IRQ_I2C3_EV               72
#define STM32_IRQ_I2C3_ER               73
#define STM32_IRQ_OTG_HS_EP1_OUT        74
#define STM32_IRQ_OTG_HS_EP1_IN         75
#define STM32_IRQ_OTG_HS_WKUP           76
#define STM32_IRQ_OTG_HS                77
#define STM32_IRQ_DCMI                  78
#define STM32_IRQ_CRYP                  79
#define STM32_IRQ_HASH_RNG              80
#define STM32_IRQ_FPU                   81

#endif /* __STM32F4XX_H__ */


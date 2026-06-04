/**
  * @file    stm32c5xx_hal_conf.h
  * @brief   HAL configuration file for TinyUF2 STM32C5 port.
  ******************************************************************************
  */

#ifndef STM32C5XX_HAL_CONF_H
#define STM32C5XX_HAL_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

/* System Configuration */
#define  USE_HAL_TICK_INT_PRIORITY              15U
#define  USE_HAL_FLASH_PREFETCH                 1U
#define  USE_HAL_MUTEX                           0U
#define  USE_HAL_CHECK_PARAM                     0U
#define  USE_HAL_SECURE_CHECK_PARAM              0U
#define  USE_HAL_CHECK_PROCESS_STATE             0U

/* Module Enables - only enable what we need */
#define USE_HAL_CORTEX_MODULE                   1U
#define USE_HAL_DMA_MODULE                      1U
#define USE_HAL_FLASH_MODULE                    1U
#define USE_HAL_FLASH_PROGRAM_BY_ADDR           1U
#define USE_HAL_FLASH_ERASE_BY_ADDR             1U
#define USE_HAL_FLASH_ERASE_PAGE                1U
#define USE_HAL_FLASH_ECC                       0U
#define USE_HAL_FLASH_OB_EDATA                  0U
#define USE_HAL_GPIO_MODULE                     1U
#define USE_HAL_PWR_MODULE                      1U
#define USE_HAL_RCC_MODULE                      1U
#define USE_HAL_UART_MODULE                     1U
#define USE_HAL_ICACHE_MODULE                   0U

/* All other modules disabled */
#define USE_HAL_ADC_MODULE                      0U
#define USE_HAL_AES_MODULE                      0U
#define USE_HAL_CCB_MODULE                      0U
#define USE_HAL_COMP_MODULE                     0U
#define USE_HAL_CORDIC_MODULE                   0U
#define USE_HAL_CRC_MODULE                      0U
#define USE_HAL_CRS_MODULE                      0U
#define USE_HAL_DAC_MODULE                      0U
#define USE_HAL_DBGMCU_MODULE                   0U
#define USE_HAL_ETH_MODULE                      0U
#define USE_HAL_EXTI_MODULE                     0U
#define USE_HAL_FDCAN_MODULE                    0U
#define USE_HAL_HASH_MODULE                     0U
#define USE_HAL_HCD_MODULE                      0U
#define USE_HAL_I2C_MODULE                      0U
#define USE_HAL_I2S_MODULE                      0U
#define USE_HAL_I3C_MODULE                      0U
#define USE_HAL_IWDG_MODULE                     0U
#define USE_HAL_LPTIM_MODULE                    0U
#define USE_HAL_OPAMP_MODULE                    0U
#define USE_HAL_PCD_MODULE                      0U
#define USE_HAL_PKA_MODULE                      0U
#define USE_HAL_RAMCFG_MODULE                   0U
#define USE_HAL_RNG_MODULE                      0U
#define USE_HAL_RTC_MODULE                      0U
#define USE_HAL_SBS_MODULE                      0U
#define USE_HAL_SMARTCARD_MODULE                0U
#define USE_HAL_SMBUS_MODULE                    0U
#define USE_HAL_SPI_MODULE                      0U
#define USE_HAL_TAMP_MODULE                     0U
#define USE_HAL_TIM_MODULE                      0U
#define USE_HAL_USART_MODULE                    0U
#define USE_HAL_WWDG_MODULE                     0U
#define USE_HAL_XSPI_MODULE                     0U

/* Oscillator Values */
#if !defined(HSE_VALUE)
  #define HSE_VALUE    8000000U
#endif

#if !defined(HSE_STARTUP_TIMEOUT)
  #define HSE_STARTUP_TIMEOUT    100U
#endif

#if !defined(LSI_VALUE)
  #define LSI_VALUE  32000UL
#endif

#if !defined(LSE_VALUE)
  #define LSE_VALUE  32768UL
#endif

#if !defined(LSE_STARTUP_TIMEOUT)
  #define LSE_STARTUP_TIMEOUT    5000UL
#endif

#define  VDD_VALUE                  3300UL

/* Assert */
#define assert_param(expr) ((void)0U)

#ifdef __cplusplus
}
#endif

#endif /* STM32C5XX_HAL_CONF_H */

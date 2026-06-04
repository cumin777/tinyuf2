/*
 * The MIT License (MIT)
 * Copyright (c) 2024 XIAO C5 board config
 */

#ifndef BOARD_H_
#define BOARD_H_

//--------------------------------------------------------------------+
// LED
//--------------------------------------------------------------------+
// XIAO C5: User LED on PA5 (adjust per actual schematic)
#define LED_PORT              GPIOA
#define LED_PIN               GPIO_PIN_5
#define LED_STATE_ON          1

//--------------------------------------------------------------------+
// Button
//--------------------------------------------------------------------+
// XIAO C5: User button on PC13 (adjust per actual schematic)
#define BUTTON_PORT           GPIOC
#define BUTTON_PIN            GPIO_PIN_13
#define BUTTON_STATE_ACTIVE   0

//--------------------------------------------------------------------+
// UART (debug, optional)
//--------------------------------------------------------------------+
// #define UART_DEV              USART2
// #define UART_CLOCK_ENABLE     __HAL_RCC_USART2_CLK_ENABLE
// #define UART_CLOCK_DISABLE    __HAL_RCC_USART2_CLK_DISABLE
// #define UART_GPIO_PORT        GPIOA
// #define UART_GPIO_AF          GPIO_AF7_USART2
// #define UART_TX_PIN           GPIO_PIN_2
// #define UART_RX_PIN           GPIO_PIN_3

//--------------------------------------------------------------------+
// Neopixel
//--------------------------------------------------------------------+
#define NEOPIXEL_NUMBER       0

//--------------------------------------------------------------------+
// Flash
//--------------------------------------------------------------------+
#define BOARD_FLASH_SIZE      FLASH_SIZE   // 1MB from CMSIS

//--------------------------------------------------------------------+
// USB UF2
//--------------------------------------------------------------------+
// TODO: Apply for official VID/PID
#define USB_VID           0x239A
#define USB_PID           0x00C5
#define USB_MANUFACTURER  "Seeed"
#define USB_PRODUCT       "XIAO C5"

#define UF2_PRODUCT_NAME  USB_MANUFACTURER " " USB_PRODUCT
#define UF2_BOARD_ID      "STM32C5A3-XIAO-C5"
#define UF2_VOLUME_LABEL  "XIAOC5BOOT"
#define UF2_INDEX_URL     "https://github.com/seeed"

//--------------------------------------------------------------------+
// RCC Clock
//--------------------------------------------------------------------+
static inline void SystemClock_Config(void)
{
  // STM32C5 clock configuration
  // Default after reset: HSI (48 MHz via HSIDIV3)
  // For bootloader: use default HSI clock, configure USB 48MHz from HSI144/3

  // Note: Full clock configuration with PLL for 144MHz can be done here.
  // For bootloader simplicity, the default HSI-derived 48MHz is sufficient
  // for USB operation.

  // Configure USB 48MHz clock source (CK48) using HSI144/3
  HAL_RCC_CK48_SetKernelClkSource(HAL_RCC_CK48_CLK_SRC_HSIDIV3);

  // Enable USB clock
  HAL_RCC_USB_EnableClock();
}

#endif

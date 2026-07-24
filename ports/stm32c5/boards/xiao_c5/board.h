/*
 * The MIT License (MIT)
 * Copyright (c) 2024 XIAO C5 board config
 */

#ifndef BOARD_H_
#define BOARD_H_

//--------------------------------------------------------------------+
// LED - XIAO C5 User LED on PA5
//--------------------------------------------------------------------+
#define LED_PORT              HAL_GPIOA
#define LED_PIN               HAL_GPIO_PIN_5
#define LED_STATE_ON          1

//--------------------------------------------------------------------+
// Button - XIAO C5 User button (if available)
//--------------------------------------------------------------------+
#define BUTTON_PORT           HAL_GPIOC
#define BUTTON_PIN            HAL_GPIO_PIN_13
#define BUTTON_STATE_ACTIVE   0

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
  // STM32C5: Default HSI-derived 48MHz is sufficient for bootloader USB operation
  // Configure USB 48MHz clock source (CK48) using HSI144/3
  HAL_RCC_CK48_SetKernelClkSource(HAL_RCC_CK48_CLK_SRC_HSIDIV3);

  // Enable USB clock
  HAL_RCC_USB_EnableClock();
}

#endif

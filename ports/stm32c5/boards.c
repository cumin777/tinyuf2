/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Ha Thach for Adafruit Industries
 * Copyright (c) 2024 STM32C5 port
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "board_api.h"
#include "stm32_hal.h"

#ifndef BUILD_NO_TINYUSB
#include "tusb.h"
#endif

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM DECLARATION
//--------------------------------------------------------------------+

#define STM32_UUID    ((volatile uint32_t *) UID_BASE)

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM
//--------------------------------------------------------------------+
#if defined(UART_DEV) && CFG_TUSB_DEBUG
  #define USE_UART 1
#else
  #define USE_UART 0
#endif

void board_init(void)
{
#ifdef BUILD_APPLICATION
  SCB->VTOR = (uint32_t) BOARD_FLASH_APP_START;
#endif

  HAL_Init();
  SystemClock_Config(); // implemented in board.h
  SystemCoreClockUpdate();

  // Enable All GPIOs clocks
  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOA);
  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOB);
  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOC);
  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOD);
  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOH);

  // LED - configure as output push-pull
  hal_gpio_config_t gpio_cfg = {
    .mode        = HAL_GPIO_MODE_OUTPUT,
    .pull        = HAL_GPIO_PULL_UP,
    .speed       = HAL_GPIO_SPEED_FREQ_HIGH,
    .output_type = HAL_GPIO_OUTPUT_PUSHPULL,
    .alternate   = 0,
    .init_state  = HAL_GPIO_PIN_RESET,
  };
  HAL_GPIO_Init(LED_PORT, LED_PIN, &gpio_cfg);

  board_led_write(false);

  // Button - configure as input
  gpio_cfg.mode = HAL_GPIO_MODE_INPUT;
  gpio_cfg.pull = BUTTON_STATE_ACTIVE ? HAL_GPIO_PULL_DOWN : HAL_GPIO_PULL_UP;
  gpio_cfg.speed = HAL_GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(BUTTON_PORT, BUTTON_PIN, &gpio_cfg);
}

void board_dfu_init(void)
{
  // Configure DM DP Pins as input
  hal_gpio_config_t gpio_cfg = {
    .mode        = HAL_GPIO_MODE_INPUT,
    .pull        = HAL_GPIO_PULL_NO,
    .speed       = HAL_GPIO_SPEED_FREQ_HIGH,
    .output_type = HAL_GPIO_OUTPUT_PUSHPULL,
    .alternate   = 0,
    .init_state  = HAL_GPIO_PIN_RESET,
  };
  HAL_GPIO_Init(HAL_GPIOA, HAL_GPIO_PIN_11 | HAL_GPIO_PIN_12, &gpio_cfg);

  // STM32C5: No VDDUSB enable needed - USB transceiver powered directly by VDD

  // Enable USB clock using STM32C5 new API
  HAL_RCC_USB_EnableClock();

  // Configure USB 48MHz clock source (CK48) - use HSI144/3
  HAL_RCC_CK48_SetKernelClkSource(HAL_RCC_CK48_CLK_SRC_HSIDIV3);
}

void board_reset(void) {
  NVIC_SystemReset();
}

void board_dfu_complete(void) {
  NVIC_SystemReset();
}

bool board_app_valid(void)
{
  volatile uint32_t const * app_vector = (volatile uint32_t const*) BOARD_FLASH_APP_START;
  uint32_t sp = app_vector[0];
  uint32_t app_entry = app_vector[1];

  TUF2_LOG1_HEX(sp);
  TUF2_LOG1_HEX(app_entry);

  // 1st word is stack pointer (must be in SRAM region)
  if ((sp & 0xff000003) != 0x20000000) return false;

  // 2nd word is App entry point (reset)
  if (app_entry < BOARD_FLASH_APP_START || app_entry > BOARD_FLASH_APP_START + BOARD_FLASH_SIZE) {
    return false;
  }

  return true;
}

void board_teardown(void) {
  // 1. De-init peripherals (GPIO, USB) while clocks are still running
#ifdef BUTTON_PIN
  HAL_GPIO_DeInit(BUTTON_PORT, BUTTON_PIN);
#endif

#ifdef LED_PIN
  HAL_GPIO_WritePin(LED_PORT, LED_PIN, HAL_GPIO_PIN_RESET);
  HAL_GPIO_DeInit(LED_PORT, LED_PIN);
#endif

  // 2. Disable USB before turning off its clock
  HAL_RCC_USB_DisableClock();
  HAL_GPIO_DeInit(HAL_GPIOA, HAL_GPIO_PIN_11 | HAL_GPIO_PIN_12); // USB

  // 3. Disable all GPIO clocks
  LL_AHB2_GRP1_DisableClock(LL_AHB2_GRP1_PERIPH_GPIOA);
  LL_AHB2_GRP1_DisableClock(LL_AHB2_GRP1_PERIPH_GPIOB);
  LL_AHB2_GRP1_DisableClock(LL_AHB2_GRP1_PERIPH_GPIOC);
  LL_AHB2_GRP1_DisableClock(LL_AHB2_GRP1_PERIPH_GPIOD);
  LL_AHB2_GRP1_DisableClock(LL_AHB2_GRP1_PERIPH_GPIOH);

  // 4. Stop SysTick explicitly
  SysTick->CTRL = 0;
  SysTick->LOAD = 0;
  SysTick->VAL = 0;

  // 5. De-init HAL state
  //    Note: intentionally NOT calling HAL_RCC_Reset() here.
  //    The app's clock driver (e.g. Zephyr stm32_clock_control_init)
  //    performs a full clock reconfiguration from scratch.
  //    HAL_RCC_Reset() can leave the RCC in a state that the app driver
  //    does not handle correctly (e.g. HSIS not running at expected freq).
  HAL_DeInit();
}

static void led_delay_cycles(uint32_t cycles) {
  for (volatile uint32_t i = 0; i < cycles; i++) {
    __NOP();
  }
}

static void led_prepare_for_blink(void) {
  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOA);

  GPIOA->MODER = (GPIOA->MODER & ~(3U << (5U * 2U))) | (1U << (5U * 2U));
  GPIOA->OTYPER &= ~(1U << 5U);
  GPIOA->OSPEEDR |= (3U << (5U * 2U));
  GPIOA->PUPDR &= ~(3U << (5U * 2U));
}

static void led_blink_blocking(uint8_t count) {
  led_prepare_for_blink();

  for (uint8_t i = 0; i < count; i++) {
    GPIOA->BSRR = (1U << 5U);
    led_delay_cycles(800000U);
    GPIOA->BSRR = (1U << (5U + 16U));
    led_delay_cycles(800000U);
  }

  led_delay_cycles(1800000U);
  GPIOA->BSRR = (1U << (5U + 16U));
}

static void led_panic_blink(uint8_t count) {
  for (uint8_t i = 0; i < 4; i++) {
    led_blink_blocking(count);
  }
}

void board_app_jump(void)
{
  typedef void (*FunctionPointer)(void);

  volatile uint32_t const * app_vector = (volatile uint32_t const*) BOARD_FLASH_APP_START;
  uint32_t sp = app_vector[0];
  uint32_t app_entry = app_vector[1];

  if ((sp & 0xff000003U) != 0x20000000U) {
    led_panic_blink(2);
  }

  if ((app_entry < BOARD_FLASH_APP_START) || (app_entry > (BOARD_FLASH_ADDR_ZERO + BOARD_FLASH_SIZE)) ||
      ((app_entry & 0x1U) == 0U)) {
    led_panic_blink(3);
  }

  // board_teardown() has already done the DeInit equivalent before this call.

  // 1. Disable all NVIC interrupts and clear pending flags
  //    This is critical: after teardown, stale pending interrupts in the NVIC
  //    could fire as soon as interrupts are enabled, but with the wrong VTOR.
  for (int i = 0; i < 8; i++) {
    NVIC->ICER[i] = 0xFFFFFFFFU;  // Disable all interrupt lines
    NVIC->ICPR[i] = 0xFFFFFFFFU;  // Clear all pending flags
  }

  // 2. Set VTOR to the application's vector table BEFORE the jump.
  //    Without this, VTOR still points to 0x08000000 (bootloader) and any
  //    interrupt between the jump and the app's early init would use the
  //    wrong vector table, causing a HardFault.
  SCB->VTOR = (uint32_t) BOARD_FLASH_APP_START;
  __DSB();
  __ISB();

  // 3. Ensure MSP is active (SPSEL = 0) before jumping
  __set_CONTROL(0);
  __ISB();

  // 4. Set MSP and jump — do NOT __enable_irq() here, let the app's
  //    startup code handle interrupt enabling at the right time.
  __set_MSP(sp);
  FunctionPointer jump_to_app = (FunctionPointer) app_entry;
  jump_to_app();

  led_panic_blink(4);
}

uint8_t board_usb_get_serial(uint8_t serial_id[16])
{
  uint8_t const len = 12;
  uint32_t* serial_id32 = (uint32_t*) (uintptr_t) serial_id;

  serial_id32[0] = STM32_UUID[0];
  serial_id32[1] = STM32_UUID[1];
  serial_id32[2] = STM32_UUID[2];

  return len;
}

//--------------------------------------------------------------------+
// LED pattern
//--------------------------------------------------------------------+

void board_led_write(uint32_t state)
{
  HAL_GPIO_WritePin(LED_PORT, LED_PIN, state ? HAL_GPIO_PIN_SET : HAL_GPIO_PIN_RESET);
}

#if NEOPIXEL_NUMBER
#define MAGIC_800_INT   900000
#define MAGIC_800_T0H  2800000
#define MAGIC_800_T1H  1350000

static inline uint8_t apply_percentage(uint8_t brightness)
{
  return (uint8_t) ((brightness*NEOPIXEL_BRIGHTNESS) >> 8);
}

void board_rgb_write(uint8_t const rgb[]) {
  uint32_t const sys_freq = HAL_RCC_GetSysClockFreq();
  uint32_t const interval = sys_freq / MAGIC_800_INT;
  uint32_t const t0 = sys_freq / MAGIC_800_T0H;
  uint32_t const t1 = sys_freq / MAGIC_800_T1H;

  uint8_t const colors[3] = {apply_percentage(rgb[1]), apply_percentage(rgb[0]),
                             apply_percentage(rgb[2])};

  __disable_irq();
  uint32_t start;
  uint32_t cyc;

  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
  DWT->CYCCNT = 0;

  for (uint32_t i = 0; i < NEOPIXEL_NUMBER; i++) {
    uint8_t const *color_pointer = colors;
    uint8_t const *const color_pointer_end = color_pointer + 3;
    uint8_t color = *color_pointer++;
    uint8_t color_mask = 0x80;

    while (true) {
      cyc = (color & color_mask) ? t1 : t0;
      start = DWT->CYCCNT;

      HAL_GPIO_WritePin(NEOPIXEL_PORT, NEOPIXEL_PIN, HAL_GPIO_PIN_SET);
      while ((DWT->CYCCNT - start) < cyc)
        ;

      HAL_GPIO_WritePin(NEOPIXEL_PORT, NEOPIXEL_PIN, HAL_GPIO_PIN_RESET);
      while ((DWT->CYCCNT - start) < interval)
        ;

      if (!(color_mask >>= 1)) {
        if (color_pointer >= color_pointer_end) {
          break;
        }
        color = *color_pointer++;
        color_mask = 0x80;
      }
    }
  }

  __enable_irq();
}

#else

void board_rgb_write(uint8_t const rgb[]) {
  (void) rgb;
}

#endif

//--------------------------------------------------------------------+
// Timer
//--------------------------------------------------------------------+
void board_timer_start(uint32_t ms) {
  SysTick_Config((SystemCoreClock / 1000) * ms);
}

void board_timer_stop(void) {
  SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;
}

void SysTick_Handler(void) {
  board_timer_handler();
}

int board_uart_write(void const * buf, int len)
{
  (void) buf; (void) len;
  return 0;
}

#ifndef BUILD_NO_TINYUSB
// Forward USB interrupt events to TinyUSB IRQ Handler
void USB_DRD_FS_IRQHandler(void) {
  tud_int_handler(0);
}
#endif

// Required by __libc_init_array in startup code if we are compiling using
// -nostdlib/-nostartfiles.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-prototypes"
void _init(void)
{

}

//--------------------------------------------------------------------+
// C runtime entry point (called by CMSIS startup Reset_Handler)
// Since we use -nostartfiles, we must provide _start ourselves.
//--------------------------------------------------------------------+
extern int main(void);
extern void __libc_init_array(void);

// Linker-defined symbols for data/bss initialization
extern uint32_t _sidata, _sdata, _edata;
extern uint32_t _sbss, _ebss;

void _start(void)
{
  // Copy .data section from flash to RAM
  uint32_t *src = &_sidata;
  uint32_t *dst = &_sdata;
  while (dst < &_edata) {
    *dst++ = *src++;
  }

  // Zero .bss section
  dst = &_sbss;
  while (dst < &_ebss) {
    *dst++ = 0;
  }

  // Call C++ constructors / __attribute__((constructor))
  __libc_init_array();

  // Call main application
  main();

  // Should never return, but loop if it does
  while (1) {}
}
#pragma GCC diagnostic pop

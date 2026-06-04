# STM32C5A3ZGT6 TinyUF2 Bootloader 适配指南

## 1. 概述

本文档记录将 TinyUF2 Bootloader 适配到 **STM32C5A3ZGT6** MCU 所需的全部工作内容。

### 1.1 什么是 UF2 Bootloader

UF2 (USB Flashing Format) Bootloader 是一种通过 USB 拖拽烧录固件的引导加载程序。它将 MCU 模拟为一个 USB 大容量存储设备（U盘），用户只需将 `.uf2` 格式的固件文件拖拽到这个虚拟U盘中即可完成固件烧录。

- **与原生 Bootloader 的关系**：STM32 原生 Bootloader（系统存储器中的 AN2606 Bootloader）和 UF2 Bootloader 处于同一层级，都是在上电后、用户应用之前运行的引导程序。但 UF2 Bootloader 提供了更友好的 USB 拖拽烧录体验。
- **工作流程**：上电 → UF2 Bootloader 启动 → 检查是否需要进入 DFU 模式（双击复位按钮 / 按住按钮 / 无有效应用）→ 是：枚举为 USB MSC 设备，等待 UF2 文件 → 否：跳转到用户应用。

### 1.2 适配策略

选择 **STM32H5** 端口作为参考模板，原因如下：
- 同为 Cortex-M33 内核
- 同为 FSDEV 类型 USB 外设
- 同为 8KB 统一 Flash 页大小
- 端口结构相似度最高

---

## 2. STM32C5A3ZGT6 关键参数

> 数据来源：STM32C5A3x6/G 数据手册 DS15137 Rev 1，参考手册 RM0522

### 2.1 核心规格

| 参数 | 值 |
|------|-----|
| 内核 | Arm Cortex-M33 (with FPU) |
| 最大主频 | 144 MHz |
| Flash 容量 | 1 MB（双 Bank，每 Bank 64 页 × 8 KB） |
| Flash 页大小 | **8 KB**（统一页大小，与 STM32H5 相同） |
| Flash 编程粒度 | Quad-word（128-bit / 16 字节），与 STM32H5 相同 |
| Flash 擦除时间 | 页擦除约 2 ms |
| Flash 编程时间 | 字编程约 20 µs |
| SRAM 总量 | 256 KB（SRAM1: 128KB + SRAM2: 128KB with ECC） |
| Data Flash | 64 KB（双 Bank，用于 EEPROM 模拟，16 × 2-KB 扇区） |
| OTP | 4.5 KB |
| 唯一 ID | 96-bit UID |
| 封装 | LQFP144 |

### 2.2 存储器映射

| 区域 | 起始地址 | 大小 |
|------|----------|------|
| Flash (Bank 1) | `0x0800_0000` | 512 KB (64 pages × 8 KB) |
| Flash (Bank 2) | `0x0808_0000` | 512 KB (64 pages × 8 KB) |
| SRAM1 | `0x2000_0000` | 128 KB |
| SRAM2 | `0x2002_0000` | 128 KB |
| System Memory | `0x1FFF_0000` | - |
| OTP | `0x1FFF_9000` | 4.5 KB |

### 2.3 USB 外设

| 参数 | 值 |
|------|-----|
| 类型 | **USB 2.0 Full-Speed Device**（FSDEV 类型） |
| 端点数量 | 可配置 1-8 个端点 |
| 包缓冲区 SRAM | 2048 字节（专用） |
| USB_DP 引脚 | **PA12** |
| USB_DM 引脚 | **PA11** |
| 内置上拉 | USB_DP 上可控嵌入式上拉 |
| USB 时钟 | 需要 48 MHz 时钟源 |
| TinyUSB 驱动路径 | `lib/tinyusb/src/portable/st/stm32_fsdev/dcd_stm32_fsdev.c` |

**关键确认**：STM32C5 的 USB 外设为 **FSDEV 类型**（与 STM32H5、STM32F0/F1/F3/L4 系列相同），**不是** DWC2/Synopsys 类型。这意味着可以直接使用 TinyUSB 的 `stm32_fsdev` 驱动。

**已确认细节**：
- USB 寄存器结构体：`USB_DRD_TypeDef`（与 STM32H5 相同）
- USB 实例名：`USB_DRD_FS`，基地址 `APB2PERIPH_BASE + 0x6000`（在 APB2 总线上）
- PMA 缓冲区：2048 字节，基地址 `APB2PERIPH_BASE + 0x6400`
- 中断向量名：`USB_DRD_FS_IRQHandler`（IRQn = 62）
- USB GPIO AF：AF13
- 无需专用 VddUSB 电源使能（USB 收发器由 VDD 直接供电）
- 48MHz 时钟源：CK48，可选 HSI144/3（默认推荐）、PSI/3 或 HSE

### 2.4 启动配置

- BOOT0 引脚 + BOOTADDR 寄存器决定启动模式
- 原生 Bootloader 位于系统存储器，支持 USART、FDCAN、USB DFU、SPI

---

## 3. 适配所需文件结构

需要创建 `ports/stm32c5/` 目录，完整结构如下：

```
ports/stm32c5/
├── CMakeLists.txt                     # 顶层 CMake 构建入口
├── family.cmake                       # 系列级构建配置（UF2_FAMILY_ID、HAL路径等）
├── boards.c                           # 板级通用实现（board_init, board_dfu_init 等）
├── board_flash.c                      # Flash 读写擦除实现
├── boards.h                           # 系列级头文件（Flash基地址、双击DFU等宏定义）
├── tusb_config.h                      # TinyUSB 配置
├── port.mk                            # Make 构建系统配置（可选，如需支持 make 构建）
├── linker/
│   ├── stm32c5_boot.ld                # Bootloader 链接脚本
│   └── stm32c5_app.ld                 # 应用程序链接脚本（可选）
└── boards/
    └── <board_name>/                  # 具体板卡配置目录
        ├── board.h                    # 板级定义（LED、Button、USB VID/PID、时钟配置）
        ├── board.cmake                # 板级 CMake（启动文件、宏定义、Flash/RAM 大小）
        └── board.mk                   # 板级 Make 配置（可选）
```

---

## 4. 各文件详细适配说明

### 4.1 `family.cmake` - 系列构建配置

参考 `ports/stm32h5/family.cmake`，需要修改：

```cmake
include_guard(GLOBAL)

#------------------------------------
# Config
#------------------------------------

# UF2_FAMILY_ID: 需要在 https://github.com/microsoft/uf2/blob/master/uf2families.md 注册
# 目前先使用临时值，后续需申请正式 ID
set(UF2_FAMILY_ID 0x<待分配>)

# HAL 库路径（需要先获取 STM32C5 HAL）
set(ST_HAL_DRIVER ${TOP}/lib/mcu/st/stm32c5xx_hal_driver)
set(ST_CMSIS ${TOP}/lib/mcu/st/cmsis_device_c5)
set(CMSIS_5 ${TOP}/lib/CMSIS_5)

include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake)

# enable LTO
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

set(CMAKE_SYSTEM_PROCESSOR cortex-m33 CACHE INTERNAL "System Processor")
set(CMAKE_TOOLCHAIN_FILE ${TOP}/cmake/toolchain/arm_${TOOLCHAIN}.cmake)

# Debug build 增大 bootloader 区域
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(LD_FLASH_BOOT_SIZE 64K)
endif ()

#------------------------------------
# BOARD_TARGET
#------------------------------------
function(family_add_board_target BOARD_TARGET)
  if (TARGET ${BOARD_TARGET})
    return()
  endif()

  add_library(${BOARD_TARGET} STATIC
    ${ST_CMSIS}/Source/Templates/system_stm32c5xx.c
    ${ST_HAL_DRIVER}/Src/stm32c5xx_hal.c
    ${ST_HAL_DRIVER}/Src/stm32c5xx_hal_cortex.c
    ${ST_HAL_DRIVER}/Src/stm32c5xx_hal_rcc.c
    ${ST_HAL_DRIVER}/Src/stm32c5xx_hal_rcc_ex.c
    ${ST_HAL_DRIVER}/Src/stm32c5xx_hal_gpio.c
    ${ST_HAL_DRIVER}/Src/stm32c5xx_hal_pwr.c
    ${ST_HAL_DRIVER}/Src/stm32c5xx_hal_pwr_ex.c
    ${ST_HAL_DRIVER}/Src/stm32c5xx_hal_flash.c
    ${ST_HAL_DRIVER}/Src/stm32c5xx_hal_flash_ex.c
    )

  target_include_directories(${BOARD_TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}
    ${CMSIS_5}/CMSIS/Core/Include
    ${ST_CMSIS}/Include
    ${ST_HAL_DRIVER}/Inc
    )

  update_board(${BOARD_TARGET})

  target_compile_definitions(${BOARD_TARGET} PUBLIC
    BOARD_UF2_FAMILY_ID=${UF2_FAMILY_ID}
    )

  target_link_options(${BOARD_TARGET} PUBLIC
    -nostartfiles
    --specs=nosys.specs --specs=nano.specs
    -Wl,--defsym=__flash_boot_size=${LD_FLASH_BOOT_SIZE}
    -Wl,--defsym=__ram_size=${LD_RAM_SIZE}
    )
endfunction()
```

### 4.2 `boards.h` - 系列级头文件

参考 `ports/stm32h5/boards.h`：

```c
#ifndef BOARDS_H_
#define BOARDS_H_

#ifdef __cplusplus
 extern "C" {
#endif

#include "stm32c5xx.h"
#include "stm32c5xx_hal_conf.h"

#include "board.h"

// Flash 起始地址
#define BOARD_FLASH_ADDR_ZERO   0x08000000

// 应用程序 Flash 起始地址（由链接脚本中的 __flash_boot_size 决定）
#ifndef BOARD_FLASH_APP_START
extern uint32_t __flash_boot_size[];
#define BOARD_FLASH_APP_START   (BOARD_FLASH_ADDR_ZERO+(uint32_t) __flash_boot_size)
#endif

// 双击复位进入 DFU 模式
#define TINYUF2_DBL_TAP_DFU  1

// 启用 Bootloader 写保护
#ifndef TINYUF2_PROTECT_BOOTLOADER
#define TINYUF2_PROTECT_BOOTLOADER    1
#endif

#ifndef NEOPIXEL_BRIGHTNESS
#define NEOPIXEL_BRIGHTNESS   0x10
#endif

#ifdef LED_PIN
#define TINYUF2_LED             1
#endif

#ifdef __cplusplus
 }
#endif

#endif /* BOARDS_H_ */
```

### 4.3 `boards.c` - 板级通用实现

参考 `ports/stm32h5/boards.c`，需要实现以下关键函数：

| 函数 | 功能 | STM32C5 适配要点 |
|------|------|------------------|
| `board_init()` | 初始化时钟、GPIO、LED、按钮 | 替换 HAL 头文件为 `stm32c5xx_hal.h`；GPIO 时钟使能宏名可能不同；`SystemClock_Config()` 在 board.h 中实现 |
| `board_dfu_init()` | 初始化 USB 外设 | USB 引脚为 PA11/PA12；使能 USB 电源（如有 `PWR_USBSCR_USB33DEN` 或等效寄存器）；使能 USB 时钟 |
| `board_app_valid()` | 验证应用程序是否有效 | 读取应用向量表，检查 SP 和 Reset_Handler 地址 |
| `board_app_jump()` | 跳转到应用程序 | 禁用中断、设置 VTOR、设置 MSP、跳转 |
| `board_teardown()` | 反初始化所有外设 | 反初始化 USB、GPIO、时钟 |
| `board_usb_get_serial()` | 获取 USB 序列号 | 使用 96-bit UID（`UID_BASE` 地址） |
| `board_reset()` | 系统复位 | `NVIC_SystemReset()` |
| `board_dfu_complete()` | DFU 完成回调 | `NVIC_SystemReset()` |
| `board_led_write()` | LED 控制 | HAL GPIO 写 |
| `board_timer_start/stop()` | 定时器 | SysTick 配置 |
| `board_uart_write()` | 串口输出（调试用） | HAL UART 发送 |
| USB IRQ Handler | USB 中断处理 | **需确认中断向量名**（可能是 `USB_FS_IRQHandler` 或 `USB_DRD_FS_IRQHandler`，需要从启动文件/参考手册确认） |

**USB 中断处理关键点**：
```c
// 已确认：STM32C5 使用 USB_DRD_FS_IRQHandler（与 STM32H5 相同）
void USB_DRD_FS_IRQHandler(void) {
  tud_int_handler(0);
}
```

**`board_dfu_init()` 适配示例**：
```c
void board_dfu_init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct;

  // 配置 USB DM/DP 引脚 (PA11/PA12)
  GPIO_InitStruct.Pin = (GPIO_PIN_11 | GPIO_PIN_12);
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  // 注意：STM32C5 无需专门的 VddUSB 使能（没有 PWR_USBSCR_USB33DEN）
  // USB 收发器直接由 VDD 供电

  // 使能 USB 时钟（新 API，非 __HAL_RCC_USB_CLK_ENABLE()）
  HAL_RCC_USB_EnableClock();

  // 选择 CK48 时钟源为 HSI/3 (144MHz / 3 = 48MHz)
  HAL_RCC_CK48_SetKernelClkSource(HAL_RCC_CK48_CLK_SRC_HSIDIV3);
}
```

### 4.4 `board_flash.c` - Flash 读写实现

参考 `ports/stm32h5/board_flash.c`，**这是适配最关键的文件之一**。

**STM32C5A3ZGT6 Flash 特性**：
- 8 KB 统一页大小（与 STM32H5 相同）
- 双 Bank 结构（每 Bank 512 KB = 64 页 × 8 KB）
- Quad-word (128-bit = 16 字节) 编程粒度
- 1 MB 总容量

**需要从 STM32H5 修改的部分**：

1. **Flash 保护掩码**：STM32C5A3 的 WRP 每组覆盖 2 页（`FLASH_WRP_GROUP_WIDTH = 2`）。
   ```c
   // 已确认：每组 2 页，通过 WRP1R_PRG / WRP2R_PRG 寄存器控制
   // 对于 24KB Bootloader（Page 0-2，共 3 页）：
   //   WRP bit 0 覆盖 Page 0-1，WRP bit 1 覆盖 Page 2-3
   //   需要设置 bit 0 和 bit 1（覆盖 Page 0-3，共 4 页 = 32KB）
   #define BOARD_FLASH_PROTECT_MASK  0x3u   // 保护 Page 0-3（前 32KB）
   // 注意：如果只想保护 24KB（3 页），无法精确实现（WRP 粒度为 2 页），
   // 最接近的保护范围是 32KB（4 页），或者使用 16KB（2 页）保护
   ```

2. **页数量计算**：
   ```c
   // STM32C5A3: 1 MB Flash, 8 KB pages
   enum {
     SECTOR_COUNT = (1*1024*1024) / FLASH_SECTOR_SIZE  // = 128 pages
   };
   ```

3. **Flash 擦除/写入**：**STM32C5 使用全新 HAL API，与传统 STM32 完全不同**。需要完全重写 `board_flash.c`。关键差异：
   - 解锁/锁定：`HAL_FLASH_ITF_Unlock(HAL_FLASH)` / `HAL_FLASH_ITF_Lock(HAL_FLASH)`（非 `HAL_FLASH_Unlock/Lock`）
   - 擦除：`HAL_FLASH_EraseByAddr(&hflash, addr, size, timeout)`（非 `HAL_FLASHEx_Erase`）
   - 编程：`HAL_FLASH_ProgramByAddr(&hflash, addr, data, size, timeout)`（非 `HAL_FLASH_Program`）
   - 编程模式需预设：`HAL_FLASH_SetProgrammingMode(&hflash, HAL_FLASH_PROGRAM_QUADWORD)`
   - 需要维护 `hal_flash_handle_t` handle 实例
   - 详细 API 对比见第 7.3 节

4. **Flash 写入**：使用 `HAL_FLASH_ProgramByAddr(&hflash, addr, p_data, size, timeout)`，预设 `HAL_FLASH_PROGRAM_QUADWORD` 模式后一次写 16 字节（与 H5 粒度相同，但 API 不同）。

### 4.5 `tusb_config.h` - TinyUSB 配置

可以直接从 STM32H5 复制，基本无需修改：

```c
#ifndef TUSB_CONFIG_H_
#define TUSB_CONFIG_H_

#ifdef __cplusplus
 extern "C" {
#endif

#ifndef CFG_TUSB_MCU
#error CFG_TUSB_MCU must be defined in board.mk
#endif

#define CFG_TUSB_OS                OPT_OS_NONE
#define CFG_TUD_ENABLED            1

#ifndef BOARD_TUD_RHPORT
#define BOARD_TUD_RHPORT           0
#endif

#ifndef CFG_TUSB_DEBUG
#define CFG_TUSB_DEBUG             0
#endif

#ifndef CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_SECTION
#endif

#ifndef CFG_TUSB_MEM_ALIGN
#define CFG_TUSB_MEM_ALIGN         __attribute__ ((aligned(4)))
#endif

// DEVICE CONFIGURATION
#ifndef CFG_TUD_ENDPOINT0_SIZE
#define CFG_TUD_ENDPOINT0_SIZE      64
#endif

#define CFG_TUD_CDC                0
#define CFG_TUD_MSC                1
#define CFG_TUD_HID                0
#define CFG_TUD_MIDI               0
#define CFG_TUD_VENDOR             0

#define CFG_TUD_MSC_BUFSIZE        4096
#define CFG_TUD_HID_BUFSIZE        64
#define CFG_TUD_VENDOR_RX_BUFSIZE  64
#define CFG_TUD_VENDOR_TX_BUFSIZE  64

#ifdef __cplusplus
 }
#endif

#endif
```

### 4.6 `CMakeLists.txt`

参考 `ports/stm32h5/CMakeLists.txt`，基本结构相同。

### 4.7 链接脚本

#### `stm32c5_boot.ld` - Bootloader 链接脚本

参考 `ports/stm32h5/linker/stm32h5_boot.ld`，修改 RAM 大小：

```ld
/* 默认值，可被 board.cmake 覆盖 */
__flash_boot_size = DEFINED(__flash_boot_size) ? __flash_boot_size : 24K;
__ram_size = DEFINED(__ram_size) ? __ram_size : 256K;  /* STM32C5A3: 256KB SRAM */

ENTRY(Reset_Handler)

_estack = ORIGIN(RAM) + LENGTH(RAM);
_board_dfu_dbl_tap = _estack;  /* 双击检测标志存放在栈顶 */

_Min_Heap_Size = 0x200;
_Min_Stack_Size = 0x400;

MEMORY
{
  RAM (xrw)     : ORIGIN = 0x20000000, LENGTH = __ram_size - 4
  FLASH (rx)    : ORIGIN = 0x08000000, LENGTH = __flash_boot_size - 1K
  CONFIG (rx)   : ORIGIN = 0x08000000 + __flash_boot_size - 1K, LENGTH = 1K
}

/* SECTIONS 部分与 STM32H5 完全相同，直接复制即可 */
```

#### `stm32c5_app.ld` - 应用程序链接脚本

```ld
__flash_boot_size = DEFINED(__flash_boot_size) ? __flash_boot_size : 24K;
__ram_size = DEFINED(__ram_size) ? __ram_size : 256K;

ENTRY(Reset_Handler)

_estack = ORIGIN(RAM) + LENGTH(RAM);
_board_dfu_dbl_tap = _estack;

_Min_Heap_Size = 0x200;
_Min_Stack_Size = 0x400;

MEMORY
{
  RAM (xrw)     : ORIGIN = 0x20000000, LENGTH = __ram_size - 4
  CONFIG (rx)   : ORIGIN = 0x08000000 + __flash_boot_size - 1K, LENGTH = 1K
  FLASH (rx)    : ORIGIN = 0x08000000 + __flash_boot_size, LENGTH = 64K
  /* 注意: 应用可用的 Flash 长度按需调整，实际应为 1MB - bootloader 大小 */
}

/* SECTIONS 部分与 STM32H5 完全相同 */
```

### 4.8 板卡配置 - `boards/<board_name>/`

以创建一个通用板卡配置为例，假设板卡名为 `xiao_c5`：

#### `board.h`

```c
#ifndef BOARD_H_
#define BOARD_H_

//--------------------------------------------------------------------+
// LED
//--------------------------------------------------------------------+

// 根据实际板卡原理图修改
#define LED_PORT              GPIOx    // TODO: 确认实际引脚
#define LED_PIN               GPIO_PIN_x
#define LED_STATE_ON          1

//--------------------------------------------------------------------+
// Button
//--------------------------------------------------------------------+
#define BUTTON_PORT           GPIOx    // TODO: 确认实际引脚
#define BUTTON_PIN            GPIO_PIN_x
#define BUTTON_STATE_ACTIVE   0

//--------------------------------------------------------------------+
// UART（调试用，可选）
//--------------------------------------------------------------------+
// #define UART_DEV              USARTx
// #define UART_CLOCK_ENABLE     __HAL_RCC_USARTx_CLK_ENABLE
// #define UART_CLOCK_DISABLE    __HAL_RCC_USARTx_CLK_DISABLE
// #define UART_GPIO_PORT        GPIOx
// #define UART_GPIO_AF          GPIO_AFx_USARTx
// #define UART_TX_PIN           GPIO_PIN_x
// #define UART_RX_PIN           GPIO_PIN_x

//--------------------------------------------------------------------+
// Neopixel（可选）
//--------------------------------------------------------------------+
#define NEOPIXEL_NUMBER       0

//--------------------------------------------------------------------+
// Flash
//--------------------------------------------------------------------+
#define BOARD_FLASH_SIZE      FLASH_SIZE   // 使用 CMSIS 定义的 FLASH_SIZE 宏

//--------------------------------------------------------------------+
// USB UF2
//--------------------------------------------------------------------+
// TODO: 申请正式 VID/PID，当前使用测试值
#define USB_VID           0x239A
#define USB_PID           0xFFFF       // TODO: 分配 PID
#define USB_MANUFACTURER  "Seeed"
#define USB_PRODUCT       "XIAO C5"

#define UF2_PRODUCT_NAME  USB_MANUFACTURER " " USB_PRODUCT
#define UF2_BOARD_ID      "STM32C5A3-XIAO"
#define UF2_VOLUME_LABEL  "XIAOC5BOOT"
#define UF2_INDEX_URL     "https://github.com/seeed"

//--------------------------------------------------------------------+
// RCC Clock
//--------------------------------------------------------------------+
static inline void SystemClock_Config(void)
{
  // STM32C5 时钟配置框架
  // 关键差异：没有专用 HSI48，使用 HSI144/3 产生 USB 所需的 48MHz (CK48)

  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /* 配置主调压器输出电压 */
  // __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);
  // while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /* 配置振荡器 */
  // RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  // RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  // RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  // ... 根据实际晶振频率配置 PLL 参数
  // HAL_RCC_OscConfig(&RCC_OscInitStruct);

  /* 配置总线时钟 */
  // RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
  //                               | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  // RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  // ... 目标: SYSCLK = 144 MHz
  // HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_xx);

  /* 配置 USB 48MHz 时钟（CK48） */
  // 注意：STM32C5 没有专用 HSI48 振荡器
  // 使用 HSI144/3 产生 48MHz，或者使用 HSE/PSI 分频
  HAL_RCC_CK48_SetKernelClkSource(HAL_RCC_CK48_CLK_SRC_HSIDIV3);

  /* 使能 USB 时钟 */
  HAL_RCC_USB_EnableClock();
}

#endif
```

#### `board.cmake`

```cmake
# STM32C5A3ZGT6: 1MB Flash, 256KB RAM
# Bootloader 大小建议: 24KB (3 个 8KB Flash 页)
# 如需 Debug 构建，会自动扩展到 64KB
set(LD_FLASH_BOOT_SIZE 24K)
set(LD_RAM_SIZE 256K)
set(JLINK_DEVICE stm32c5a3zg)   # TODO: 确认 J-Link 设备名

function(update_board TARGET)
  target_sources(${TARGET} PUBLIC
    ${ST_CMSIS}/Source/startup_stm32c5a3xx.c   # 注意：是 .c 文件（非 .s 汇编）
    )
  target_compile_definitions(${TARGET} PUBLIC
    STM32C5A3xx
    HSE_VALUE=8000000                   # TODO: 确认板载晶振频率
    )
endfunction()
```

---

## 5. 构建系统修改

### 5.1 获取依赖库

STM32C5 的 HAL/CMSIS 库目前尚未在 GitHub 上公开发布。需要通过以下方式获取：

1. **STM32CubeMX**：创建 STM32C5A3ZGT6 项目，生成代码后提取 HAL 驱动
2. **ST 官方 GitHub**（可能后续发布）：`github.com/STMicroelectronics`
3. **从 CubeMX 安装目录或本地 STM32CubeC5 包复制**：
   - CMSIS 设备文件：`stm32c5xx.h`，`stm32c5a3xx.h`，`system_stm32c5xx.c`，`startup_stm32c5a3xx.c`（注意是 `.c` 不是 `.s`）
   - HAL 驱动：`stm32c5xx_hal*.c/h`
   - 需要的 HAL 模块：`hal.c`, `hal_cortex.c`, `hal_rcc.c`, `hal_rcc_ex.c`, `hal_gpio.c`, `hal_pwr.c`, `hal_flash.c`, `hal_flash_itf.c`
   - **重要**：STM32C5 HAL 使用了全新的 API 设计（handle-based），与传统 STM32 HAL 不兼容

### 5.2 放置依赖库

```
lib/mcu/st/
├── cmsis_device_c5/
│   ├── Include/
│   │   ├── stm32c5xx.h
│   │   └── stm32c5xx_hal_conf_template.h
│   └── Source/Templates/
│       ├── system_stm32c5xx.c
│       └── gcc/startup_stm32c5a3xx.s
└── stm32c5xx_hal_driver/
    ├── Inc/
    │   ├── stm32c5xx_hal.h
    │   ├── stm32c5xx_hal_cortex.h
    │   ├── stm32c5xx_hal_rcc.h
    │   ├── stm32c5xx_hal_rcc_ex.h
    │   ├── stm32c5xx_hal_gpio.h
    │   ├── stm32c5xx_hal_pwr.h
    │   ├── stm32c5xx_hal_pwr_ex.h
    │   ├── stm32c5xx_hal_flash.h
    │   └── stm32c5xx_hal_flash_ex.h
    └── Src/
        ├── stm32c5xx_hal.c
        ├── stm32c5xx_hal_cortex.c
        ├── stm32c5xx_hal_rcc.c
        ├── stm32c5xx_hal_rcc_ex.c
        ├── stm32c5xx_hal_gpio.c
        ├── stm32c5xx_hal_pwr.c
        ├── stm32c5xx_hal_pwr_ex.c
        ├── stm32c5xx_hal_flash.c
        └── stm32c5xx_hal_flash_ex.c
```

### 5.3 TinyUSB FSDEV 驱动兼容性

STM32C5 的 USB 外设与 TinyUSB `dcd_stm32_fsdev.c` 驱动**寄存器级兼容**，但需要添加条件编译入口：

- 驱动路径：`lib/tinyusb/src/portable/st/stm32_fsdev/dcd_stm32_fsdev.c`
- 头文件路径：`lib/tinyusb/src/portable/st/stm32_fsdev/fsdev_stm32.h`
- STM32C5 使用 `USB_DRD_TypeDef`（与 STM32H5 相同结构），实例名 `USB_DRD_FS`
- 基地址 `USB_DRD_FS_BASE = APB2PERIPH_BASE + 0x6000`
- PMA 大小 2048 字节，PMA 基地址 `USB_DRD_PMAADDR = APB2PERIPH_BASE + 0x6400`

**需要在 TinyUSB 中添加的内容**：

1. `tusb_option.h`：添加 `#define OPT_MCU_STM32C5 319`
2. `fsdev_stm32.h`：添加 STM32C5 条件编译块（基本与 STM32H5 块相同，只是 `#include "stm32c5xx.h"`）
3. `board.mk` / `board.cmake`：添加 `-DCFG_TUSB_MCU=OPT_MCU_STM32C5`

### 5.4 UF2_FAMILY_ID 注册

UF2 Family ID 需要在 Microsoft 的 uf2families 中注册：
- 仓库：`https://github.com/microsoft/uf2/blob/master/uf2families.md`
- 现有 STM32 系列 ID：
  - STM32F4: `0x57755a57`
  - STM32F3: `0x6b846188`
  - STM32H5: `0x4e8f1c5d`
- 需要为 STM32C5 分配新的唯一 ID

---

## 6. 适配步骤总结

### 步骤 1：准备 HAL/CMSIS 库
- 从 STM32CubeMX 获取 STM32C5A3 的 HAL 驱动和 CMSIS 文件
- 放置到 `lib/mcu/st/` 下

### 步骤 2：创建端口目录结构
- 创建 `ports/stm32c5/` 及子目录
- 从 STM32H5 端口复制模板文件

### 步骤 3：修改系列级文件
- `family.cmake`：修改 HAL 路径、UF2_FAMILY_ID
- `boards.h`：修改 include 为 `stm32c5xx.h`
- `tusb_config.h`：基本不用改
- `boards.c`：修改 HAL 头文件 include、USB 中断向量名
- `board_flash.c`：确认 Flash 寄存器/宏名差异

### 步骤 4：修改链接脚本
- `stm32c5_boot.ld`：调整默认 RAM 大小为 256K
- `stm32c5_app.ld`：同上

### 步骤 5：创建板卡配置
- 在 `boards/<board_name>/` 下创建 `board.h` 和 `board.cmake`
- 配置 LED、按钮、USB VID/PID、时钟等

### 步骤 6：确认 USB 兼容性
- 对比 STM32C5 USB 寄存器与 FSDEV 驱动是否兼容
- 确认 USB 中断向量名
- 确认 USB 时钟配置（48MHz 来源）

### 步骤 7：编译调试
- 初始化 TinyUSB 子模块：`git submodule update --init lib/tinyusb`
- 尝试构建：`cmake -DBOARD=<board_name> -DTOOLCHAIN=gcc ..`
- 逐步解决编译错误（主要是 HAL API 差异）

---

## 7. 已确认信息汇总

以下信息已通过阅读 STM32CubeC5 的实际 HAL/CMSIS 代码确认（数据来源：`/mnt/d/workspace/xiao_c5/STM32CubeC5/`）：

| 原待确认项 | 已确认结果 | 来源文件 |
|----------|-----------|---------|
| USB 中断向量名 | **`USB_DRD_FS_IRQHandler`**（IRQn = 62） | `stm32c5xx_dfp/Source/startup_stm32c5a3xx.c` |
| USB 寄存器结构体 | **`USB_DRD_TypeDef`**（实例名 `USB_DRD_FS`，基地址 `APB2PERIPH_BASE + 0x6000`） | `stm32c5xx_dfp/Include/stm32c5a3xx.h` |
| Flash 写保护粒度 | **每 WRP 组 2 页**（`FLASH_WRP_GROUP_WIDTH = 2U`），通过 `WRP1R_PRG`/`WRP2R_PRG` 寄存器控制 | `stm32c5xx_dfp/Include/stm32c5a3xx.h` |
| USB 电源使能宏 | **无需专用 VddUSB 使能**：STM32C5 没有 `PWR_USBSCR_USB33DEN`，USB 收发器直接由 VDD 供电，电源管理通过 `USB_CNTR.PDWN` 位控制 | `stm32c5a3xx.h` PWR_TypeDef 无 USB 相关寄存器 |
| USB 时钟使能宏 | **`HAL_RCC_USB_EnableClock()`**（内部调用 `LL_APB2_GRP1_EnableClock()`），**不是** `__HAL_RCC_USB_CLK_ENABLE()` | `stm32c5xx_drivers/hal/stm32c5xx_hal_rcc.h:2167` |
| CRS 时钟模块 | **有 CRS**（`CRS_TypeDef`，基地址 `APB1PERIPH_BASE + 0x6000`，IRQn = 63），用于同步 HSI144 振荡器 | `stm32c5a3xx.h` |
| GPIO AF 编号 | USB 引脚使用 **AF13**（`HAL_GPIO_AF13_USB`） | `stm32c5xx_drivers/hal/stm32c5xx_hal_gpio.h:612` |
| HAL 宏名 | **`STM32C5A3xx`**（与 CMSIS 头文件名一致） | `stm32c5xx_dfp/Include/stm32c5a3xx.h` |
| 启动文件名 | **`startup_stm32c5a3xx.c`**（注意：是 `.c` 文件，不是 `.s` 汇编文件） | `stm32c5xx_dfp/Source/startup_stm32c5a3xx.c` |
| HSI48 支持 | **无专用 HSI48**：STM32C5 使用 **HSI144**（144 MHz 内部 RC），通过分频产生 48 MHz 时钟（CK48）。时钟源可选 `PSIDIV3`、`HSIDIV3` 或 `HSE` | `stm32c5xx_drivers/hal/stm32c5xx_hal_rcc.h` |
| FLASH_BANK_SIZE 宏 | **有**：`FLASH_BANK_SIZE = (FLASH_SIZE >> 1U)`，动态计算（对于 1MB Flash 为 512KB） | `stm32c5a3xx.h:8051` |

### 7.1 USB 关键寄存器与宏定义

```c
// USB 外设实例与基地址
#define USB_DRD_FS_BASE    (APB2PERIPH_BASE + 0x6000UL)   // USB_DRD_TypeDef 实例基地址
#define USB_DRD_PMAADDR    (APB2PERIPH_BASE + 0x6400UL)   // PMA 包缓冲区基地址
#define USB_DRD_FS         ((USB_DRD_TypeDef *) USB_DRD_FS_BASE)
#define USB_DRD_PMA_BUFF   ((USB_DRD_PMABuffDescTypeDef *) USB_DRD_PMAADDR)
#define USB_DRD_PMA_SIZE   (2048U)     // PMA 大小 2KB
#define USB_DRD_FS_EP_NBR  (8U)        // 8 个端点

// USB 中断
USB_DRD_FS_IRQn = 62                  // 中断号
void USB_DRD_FS_IRQHandler(void);     // 中断处理函数名

// USB 电源控制（通过 USB_CNTR 寄存器）
#define USB_CNTR_PDWN      (0x00000002)  // Power Down 位
#define USB_CNTR_USBRST    (0x00000001)  // USB Reset 位

// USB 时钟使能（新 API，非传统 __HAL_RCC 宏）
HAL_RCC_USB_EnableClock();             // 使能 USB 时钟（APB2, bit 24）
HAL_RCC_USB_DisableClock();            // 禁用 USB 时钟
HAL_RCC_USB_Reset();                   // 复位 USB 外设

// USB 48MHz 时钟源选择（CK48）
HAL_RCC_CK48_CLK_SRC_HSIDIV3   // HSI/3 = 144/3 = 48 MHz（默认，推荐）
HAL_RCC_CK48_CLK_SRC_PSIDIV3   // PSI/3
HAL_RCC_CK48_CLK_SRC_HSE       // 外部晶振

// USB GPIO AF
#define HAL_GPIO_AF13_USB     HAL_GPIO_AF_13   // USB AF 映射

// CRS（Clock Recovery System）
#define CRS_BASE              (APB1PERIPH_BASE + 0x6000UL)
CRS_IRQn = 63                          // CRS 中断号
```

### 7.2 USB 时钟方案（重要差异）

STM32C5 **没有专用 HSI48 振荡器**（这与 STM32H5/G0/G4 不同）。取而代之的是：

- **HSI144**：内部 144 MHz RC 振荡器
- **CK48 时钟**：通过 `RCC_CCIPR2_CK48SEL` 选择 48MHz 来源
  - `HSIDIV3`：HSI144 / 3 = 48 MHz（默认选项，推荐）
  - `PSIDIV3`：PSI / 3（PSI 为精密内部振荡器）
  - `HSE`：外部晶振（需要恰好 48MHz 或 PLL 分频）
- **CRS** 可用于同步 HSI144，以提高 CK48 时钟精度

时钟配置示例（board.h 中 `SystemClock_Config`）：
```c
// 选择 CK48 时钟源为 HSI/3
HAL_RCC_CK48_SetKernelClkSource(HAL_RCC_CK48_CLK_SRC_HSIDIV3);
```

### 7.3 Flash API（重要：全新 API 设计）

**STM32C5 HAL 使用了与传统 STM32 HAL 完全不同的 Flash API**。这是一个重大差异点。

#### 传统 STM32 HAL（STM32H5 等）
```c
// 旧 API（基于结构体参数）
FLASH_EraseInitTypeDef erase_init;
erase_init.TypeErase = FLASH_TYPEERASE_SECTORS;
erase_init.Banks     = FLASH_BANK_1;
erase_init.Page      = page_number;
erase_init.NbPages   = 1;
HAL_FLASHEx_Erase(&erase_init, &page_error);

HAL_FLASH_Program(FLASH_TYPEPROGRAM_QUADWORD, addr, data);
HAL_FLASH_Unlock();
HAL_FLASH_Lock();
```

#### STM32C5 新 HAL API（基于 handle + 直接函数调用）
```c
// 新 API（基于 handle 和直接函数调用）
hal_flash_handle_t hflash;
HAL_FLASH_Init(&hflash, HAL_FLASH);

// 设置编程模式
HAL_FLASH_SetProgrammingMode(&hflash, HAL_FLASH_PROGRAM_QUADWORD);

// 按地址擦除（自动计算 bank 和 page）
HAL_FLASH_EraseByAddr(&hflash, flash_addr, size_byte, timeout_ms);

// 按页擦除
HAL_FLASH_ErasePage(&hflash, HAL_FLASH_BANK_1, page, page_nbr, timeout_ms);

// 按地址编程
HAL_FLASH_ProgramByAddr(&hflash, flash_addr, p_data, size_byte, timeout_ms);

// 锁定/解锁
HAL_FLASH_ITF_Unlock(HAL_FLASH);
HAL_FLASH_ITF_Lock(HAL_FLASH);

// 查询信息
hal_flash_info_t info;
HAL_FLASH_GetInfo(&hflash, &info);
// info.flash_size_byte, info.bank_nbr, info.bank[0].user_flash.page_nbr 等
```

#### 关键类型定义

```c
// Flash 编程模式（枚举值不同，不再是简单的 0/1）
typedef enum {
  HAL_FLASH_PROGRAM_QUADWORD   = 0x5A5A5A5AU,
  HAL_FLASH_PROGRAM_DOUBLEWORD = 0xAAAAAAAAU,
  HAL_FLASH_PROGRAM_WORD       = 0x55555555U,
  HAL_FLASH_PROGRAM_HALFWORD   = 0xF5F5F5F5U,
  HAL_FLASH_PROGRAM_BYTE       = 0xEEEEEEEEU,
} hal_flash_program_mode_t;

// Flash 操作类型
typedef enum {
  HAL_FLASH_NO_OPERATION = 0U,
  HAL_FLASH_PROGRAM      = 1U,
  HAL_FLASH_ADDR_ERASE   = 2U,
  HAL_FLASH_PAGE_ERASE   = 3U,
  HAL_FLASH_BANK_ERASE   = 4U,
  HAL_FLASH_MASS_ERASE   = 5U,
} hal_flash_operation_t;

// Bank 选择
typedef enum {
  HAL_FLASH_BANK_1   = 0U,
  HAL_FLASH_BANK_2   = 1U,
  HAL_FLASH_BANK_ALL = 2U,
} hal_flash_bank_t;
```

### 7.4 Flash 写保护（WRP）详细说明

```c
// 每组覆盖 2 页（FLASH_WRP_GROUP_WIDTH = 2）
// Bank 1: WRP1R_PRG 寄存器，每 bit 控制 2 页的写保护
// Bank 2: WRP2R_PRG 寄存器，每 bit 控制 2 页的写保护

// LL 层 WRP 宏定义
#define LL_FLASH_OB_WRP_PAGE_0_1    0x00000001UL   // 保护 Page 0 & 1
#define LL_FLASH_OB_WRP_PAGE_2_3    0x00000002UL   // 保护 Page 2 & 3
#define LL_FLASH_OB_WRP_PAGE_4_5    0x00000004UL   // 保护 Page 4 & 5
// ... 以此类推，每 bit 代表 2 页
#define LL_FLASH_OB_WRP_PAGE_62_63  0x80000000UL   // 保护 Page 62 & 63
#define LL_FLASH_OB_WRP_PAGE_ALL    0xFFFFFFFFUL   // 保护所有页

// 对于 24KB Bootloader（3 个 8KB 页 = Page 0, 1, 2）:
// 需要保护 Page 0-3（WRP bit 0），对应 BOARD_FLASH_PROTECT_MASK = 0x1
// 对于 64KB Bootloader（8 个 8KB 页 = Page 0-7）:
// 需要保护 Page 0-7（WRP bit 0-3），对应 BOARD_FLASH_PROTECT_MASK = 0xF
```

### 7.5 TinyUSB 驱动兼容性分析

**TinyUSB 目前不支持 STM32C5**。需要在 TinyUSB 中添加以下内容：

1. **`tusb_option.h`**：添加 `OPT_MCU_STM32C5` 定义（建议值 `319`，接在 `OPT_MCU_STM32C0 = 318` 之后）

2. **`fsdev_stm32.h`**：添加 STM32C5 条件编译块
   ```c
   #elif CFG_TUSB_MCU == OPT_MCU_STM32C5
     #include "stm32c5xx.h"
     #define FSDEV_PMA_SIZE (2048u)
     #define USB USB_DRD_FS
     // 寄存器别名（与 STM32H5 相同的 USB_DRD 架构）
     #define USB_EP_CTR_RX  USB_EP_VTRX
     #define USB_EP_CTR_TX  USB_EP_VTTX
     #define USB_EP_T_FIELD USB_CHEP_UTYPE
     // ... 其余别名定义应与 STM32H5 块基本相同
   ```

3. **兼容性评估**：STM32C5 的 `USB_DRD_TypeDef` 与 STM32H5 的完全一致（相同的寄存器布局：CHEP0R-CHEP7R, CNTR, ISTR, FNR, DADDR, LPMCSR, BCDR），因此 TinyUSB FSDEV 驱动在寄存器层面**应该兼容**，只需添加条件编译入口即可。

### 7.6 适配影响评估

由于 STM32C5 HAL API 与 STM32H5 差异巨大，`board_flash.c` 文件**无法直接复制 H5 版本**，需要完全重写。主要差异：

| 对比项 | STM32H5（传统 HAL） | STM32C5（新 HAL） |
|--------|-------------------|------------------|
| Flash 解锁 | `HAL_FLASH_Unlock()` | `HAL_FLASH_ITF_Unlock(HAL_FLASH)` |
| Flash 锁定 | `HAL_FLASH_Lock()` | `HAL_FLASH_ITF_Lock(HAL_FLASH)` |
| 擦除 | `HAL_FLASHEx_Erase(&init, &err)` | `HAL_FLASH_EraseByAddr(&hflash, addr, size, timeout)` |
| 编程 | `HAL_FLASH_Program(TYPE, addr, data)` | `HAL_FLASH_ProgramByAddr(&hflash, addr, data, size, timeout)` |
| 编程模式 | 函数参数 `FLASH_TYPEPROGRAM_QUADWORD` | 预设 `HAL_FLASH_SetProgrammingMode(&hflash, HAL_FLASH_PROGRAM_QUADWORD)` |
| Bank 选择 | 结构体参数 `FLASH_BANK_1` | 枚举参数 `HAL_FLASH_BANK_1` |
| 状态管理 | 全局变量 | handle 实例 `hal_flash_handle_t` |

`boards.c` 中 USB 初始化部分也需要修改：
- 不需要 `HAL_PWREx_EnableVddUSB()`
- 使用 `HAL_RCC_USB_EnableClock()` 代替 `__HAL_RCC_USB_CLK_ENABLE()`
- USB 48MHz 时钟使用 `HAL_RCC_CK48_SetKernelClkSource()` 而非 `RCC_USBCLKSOURCE_HSI48`

---

## 8. 仍需确认的信息

| 待确认项 | 说明 | 优先级 |
|----------|------|--------|
| XIAO C5 板卡引脚 | LED、按钮、UART 的实际 GPIO 引脚定义（参考 `XIAO STM32C5引脚定义表.xlsx`） | **高** |
| HSE 晶振频率 | 板载外部晶振频率（`HSE_VALUE`） | **高** |
| UF2_FAMILY_ID | 需向 Microsoft 申请正式的 Family ID | 中 |
| USB VID/PID | 需申请正式的 USB VID/PID | 中 |
| STM32C5 HAL Option Bytes API | 写保护（`board_flash_protect_bootloader()`）所需的具体 OB 编程 API | 中 |
| GCC 链接脚本适配 | 新的 `.c` 启动文件格式是否需要特殊的 GCC 编译选项 | 低 |

---

## 9. 参考资料

- **数据手册**：STM32C5A3x6/G DS15137 Rev 1
- **参考手册**：RM0522 (STM32C5A3/C5A7/C5B3 reference manual)
- **TinyUF2 仓库**：https://github.com/adafruit/tinyuf2
- **UF2 规范**：https://github.com/microsoft/uf2
- **UF2 Family IDs**：https://github.com/microsoft/uf2/blob/master/uf2families.md
- **TinyUSB FSDEV 驱动**：`lib/tinyusb/src/portable/st/stm32_fsdev/dcd_stm32_fsdev.c`
- **TinyUSB FSDEV 头文件**：`lib/tinyusb/src/portable/st/stm32_fsdev/fsdev_stm32.h`
- **STM32H5 端口**：`ports/stm32h5/` （本适配的参考模板）

### 本地 STM32CubeC5 资源

- **CMSIS 设备头文件**：`/mnt/d/workspace/xiao_c5/STM32CubeC5/stm32c5xx_dfp/Include/stm32c5a3xx.h`
- **CMSIS 启动文件**：`/mnt/d/workspace/xiao_c5/STM32CubeC5/stm32c5xx_dfp/Source/startup_stm32c5a3xx.c`
- **HAL Flash 驱动**：`/mnt/d/workspace/xiao_c5/STM32CubeC5/stm32c5xx_drivers/hal/stm32c5xx_hal_flash.c`
- **HAL Flash 接口层**：`/mnt/d/workspace/xiao_c5/STM32CubeC5/stm32c5xx_drivers/hal/stm32c5xx_hal_flash_itf.c`
- **HAL RCC 驱动**：`/mnt/d/workspace/xiao_c5/STM32CubeC5/stm32c5xx_drivers/hal/stm32c5xx_hal_rcc.h`
- **HAL PCD (USB) 驱动**：`/mnt/d/workspace/xiao_c5/STM32CubeC5/stm32c5xx_drivers/hal/stm32c5xx_hal_pcd.c`
- **USB DRD Core 驱动**：`/mnt/d/workspace/xiao_c5/STM32CubeC5/stm32c5xx_drivers/hal/stm32c5xx_usb_drd_core.c`
- **GCC 链接脚本**：`/mnt/d/workspace/xiao_c5/STM32CubeC5/stm32c5xx_dfp/Source/Templates/gcc/linker/stm32c5a3xg_flash.ld`
- **NUCLEO-C5A3ZG 示例**：`/mnt/d/workspace/xiao_c5/STM32CubeC5/examples/hal/`

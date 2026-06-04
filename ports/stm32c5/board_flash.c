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

#ifndef BUILD_NO_TINYUSB
#include "tusb.h"
#endif

#include "stm32c5xx_hal.h"
#include "stm32c5xx_hal_flash.h"
#include "stm32c5xx_hal_flash_itf.h"

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM
//--------------------------------------------------------------------+

#define FLASH_BASE_ADDR         0x08000000UL

// STM32C5 uses FLASH_PAGE_SIZE (same as sector size in UF2 context)
#define FLASH_SECTOR_SIZE       FLASH_PAGE_SIZE

// STM32C5A3 WRP: each group covers 2 pages (FLASH_WRP_GROUP_WIDTH = 2)
// For 32KB bootloader (4 pages): need WRP bits 0-1 to protect pages 0-3 (32KB)
#define BOARD_FLASH_PROTECT_MASK  0x3u

// STM32C5A3: 1 MB Flash, 8 KB pages = 128 pages total
enum {
  SECTOR_COUNT = (1*1024*1024) / FLASH_SECTOR_SIZE
};
static uint8_t erased_sectors[SECTOR_COUNT];

// Flash handle for STM32C5 new HAL API
static hal_flash_handle_t hflash;

//--------------------------------------------------------------------+
// Internal Helper
//--------------------------------------------------------------------+
static bool is_blank(uint32_t addr, uint32_t size) {
  for (uint32_t i = 0; i < size; i += sizeof(uint32_t)) {
    if (*(uint32_t*)(addr + i) != 0xffffffff) {
      return false;
    }
  }
  return true;
}

static bool flash_erase(uint32_t addr) {
  TUF2_ASSERT(addr < FLASH_BASE_ADDR + BOARD_FLASH_SIZE);

  const uint32_t sector_addr = addr & ~(FLASH_SECTOR_SIZE - 1);
  const uint32_t sector_id = (sector_addr - FLASH_BASE_ADDR) / FLASH_SECTOR_SIZE;

  const uint8_t erased = erased_sectors[sector_id];
  erased_sectors[sector_id] = 1; // mark as erased

#ifndef TINYUF2_SELF_UPDATE
  // skip erasing bootloader
  TUF2_ASSERT(sector_addr >= BOARD_FLASH_APP_START);
#endif

  if ( !erased && !is_blank(sector_addr, FLASH_SECTOR_SIZE) ) {
    TUF2_LOG1("Erase: %08lX size = %u KB ... ", sector_addr, FLASH_SECTOR_SIZE / 1024);

    // Use new STM32C5 HAL API - erase by address
    HAL_FLASH_ITF_Unlock(HAL_FLASH);
    hal_status_t status = HAL_FLASH_EraseByAddr(&hflash, sector_addr, FLASH_SECTOR_SIZE, 1000);
    HAL_FLASH_ITF_Lock(HAL_FLASH);

    if (status != HAL_OK) {
      TUF2_LOG1("FAILED\r\n");
      return false;
    }

    TUF2_LOG1("OK\r\n");
    TUF2_ASSERT( is_blank(sector_addr, FLASH_SECTOR_SIZE) );
  }

  return true;
}

static void flash_write(uint32_t dst, const uint8_t* src, int len) {
  flash_erase(dst);

  TUF2_LOG1("Write flash at address %08lX\r\n", dst);

  HAL_FLASH_ITF_Unlock(HAL_FLASH);

  // Set programming mode to quad-word (16 bytes)
  HAL_FLASH_SetProgrammingMode(&hflash, HAL_FLASH_PROGRAM_QUADWORD);

  // Write using new by-address API
  // HAL_FLASH_ProgramByAddr(hflash, addr, p_data, size_byte, timeout)
  hal_status_t status = HAL_FLASH_ProgramByAddr(&hflash, dst, (const uint32_t *)(uintptr_t) src, len, 1000);
  if (status != HAL_OK) {
    TUF2_LOG1("Failed to write flash at address %08lX\r\n", dst);
  }

  HAL_FLASH_ITF_Lock(HAL_FLASH);

  // verify contents
  if (memcmp((void*)dst, src, len) != 0) {
    TUF2_LOG1("Failed to write\r\n");
  }
}

//--------------------------------------------------------------------+
// Board API
//--------------------------------------------------------------------+
void board_flash_init(void) {
  memset(erased_sectors, 0, sizeof(erased_sectors));
  // Initialize flash handle
  HAL_FLASH_Init(&hflash, HAL_FLASH);
}

uint32_t board_flash_size(void) {
  return BOARD_FLASH_SIZE;
}

void board_flash_read(uint32_t addr, void* buffer, uint32_t len) {
  memcpy(buffer, (void*)addr, len);
}

void board_flash_flush(void) {
}

bool board_flash_write(uint32_t addr, void const* data, uint32_t len) {
  // TODO skip matching contents need to compare a whole sector
  flash_write(addr, data, len);
  return true;
}

void board_flash_erase_app(void) {
  // erase 1st sector of app region is enough to invalid the app
  flash_erase(BOARD_FLASH_APP_START);
}

bool board_flash_protect_bootloader(bool protect) {
  bool ret = true;

  HAL_FLASH_ITF_OB_Unlock(HAL_FLASH);

  // Read current WRP configuration
  uint32_t current_wrp1 = FLASH->WRP1R_CUR;

  // Flash sectors are protected if the bit is cleared (active low)
  bool const already_protected = (current_wrp1 & BOARD_FLASH_PROTECT_MASK) == 0;

  TUF2_LOG1("Protection: current = %u, request = %u\r\n", already_protected, protect);

  // request and current state mismatched --> require ob program
  if (protect != already_protected) {
    if (protect) {
      FLASH->WRP1R_PRG = (~BOARD_FLASH_PROTECT_MASK) & 0xFFFFFFFFUL;
    } else {
      FLASH->WRP1R_PRG = BOARD_FLASH_PROTECT_MASK;
    }

    if (HAL_FLASH_ITF_OB_Program(HAL_FLASH) == HAL_OK) {
      // OB programming triggers system reset
    } else {
      ret = false;
    }
  }

  HAL_FLASH_ITF_OB_Lock(HAL_FLASH);

  return ret;
}

#ifdef TINYUF2_SELF_UPDATE

bool is_new_bootloader_valid(const uint8_t* bootloader_bin, uint32_t bootloader_len) {
  // at least larger than vector table
  if (bootloader_len < 1024) return false;

  // similar to board_app_valid() check
  uint32_t const* app_vector = (uint32_t const*) (uintptr_t) bootloader_bin;
  uint32_t sp = app_vector[0];
  uint32_t boot_entry = app_vector[1];

  // 1st word is stack pointer (must be in SRAM region)
  if ((sp & 0xff000003) != 0x20000000) return false;

  // 2nd word is App entry point (reset), must smaller than app start
  if (boot_entry >= BOARD_FLASH_APP_START) {
    return false;
  }

  return true;
}

void board_self_update(const uint8_t* bootloader_bin, uint32_t bootloader_len) {
  // check if the bootloader payload is valid
  if (is_new_bootloader_valid(bootloader_bin, bootloader_len)) {
    #if TINYUF2_PROTECT_BOOTLOADER
    board_flash_protect_bootloader(false);
    #endif

    // keep writing until flash contents matches new bootloader data
    while (memcmp((const void*)FLASH_BASE_ADDR, bootloader_bin, bootloader_len)) {
      uint32_t sector_addr = FLASH_BASE_ADDR;
      const uint8_t* data = bootloader_bin;
      uint32_t len = bootloader_len;

      for (uint32_t i = 0; i < 4 && len > 0; i++) {
        uint32_t const size = (FLASH_SECTOR_SIZE < len ? FLASH_SECTOR_SIZE : len);
        board_flash_write(sector_addr, data, size);

        sector_addr += size;
        data += size;
        len -= size;
      }
    }
  }

  // self-destruct: write 0 to first entries of vector table
  __disable_irq();
  HAL_FLASH_ITF_Unlock(HAL_FLASH);

  uint32_t null_arr[4] = { 0 };
  HAL_FLASH_SetProgrammingMode(&hflash, HAL_FLASH_PROGRAM_QUADWORD);
  HAL_FLASH_ProgramByAddr(&hflash, BOARD_FLASH_APP_START, (const uint32_t *)(uintptr_t) null_arr, 16, 1000);

  HAL_FLASH_ITF_Lock(HAL_FLASH);

  // reset to run new bootloader
  NVIC_SystemReset();
}
#endif

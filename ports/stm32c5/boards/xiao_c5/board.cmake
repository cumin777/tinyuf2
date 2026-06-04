# STM32C5A3ZGT6: 1MB Flash, 256KB RAM
# Bootloader size: 24KB (3 x 8KB pages)
set(LD_FLASH_BOOT_SIZE 24K)
set(LD_RAM_SIZE 256K)
set(JLINK_DEVICE stm32c5a3zg)

function(update_board TARGET)
  target_sources(${TARGET} PUBLIC
    ${ST_CMSIS}/Source/Templates/startup_stm32c5a3xx.c
    )
  target_compile_definitions(${TARGET} PUBLIC
    STM32C5A3xx
    HSE_VALUE=8000000
    )
endfunction()

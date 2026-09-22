include($ENV{IDF_PATH}/tools/cmake/version.cmake)

# driver/gpio.h and driver/spi_master.h moved to esp_driver_gpio/esp_driver_spi
# in IDF v6.x
set(lcd_reqs boards)
if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "6.0")
    list(APPEND lcd_reqs esp_driver_gpio esp_driver_spi)
endif()

idf_component_register(SRCS "lcd.c"
                       INCLUDE_DIRS "include"
                       REQUIRES ${lcd_reqs})

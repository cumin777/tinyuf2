include($ENV{IDF_PATH}/tools/cmake/version.cmake)

set(component_srcs "ssd1306.c" "ssd1306_i2c.c" "ssd1306_spi.c")

# 'driver' still provides the legacy i2c driver; gpio.h/spi_master.h moved to
# esp_driver_gpio/esp_driver_spi in IDF v6.x
set(ssd1306_priv_reqs driver)
if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "6.0")
    list(APPEND ssd1306_priv_reqs esp_driver_gpio esp_driver_spi)
endif()

idf_component_register(SRCS "${component_srcs}"
                       PRIV_REQUIRES ${ssd1306_priv_reqs}
                       INCLUDE_DIRS ".")

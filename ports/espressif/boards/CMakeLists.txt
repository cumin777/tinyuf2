include($ENV{IDF_PATH}/tools/cmake/version.cmake)

# On IDF v5.x the legacy 'usb' component provides esp_private/usb_phy.h and
# most driver headers; IDF v6.x removed it (usb_phy moved to esp_hw_support,
# driver headers split into per-peripheral esp_driver_* components)
set(boards_priv_reqs driver usb)
if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "6.0")
    list(REMOVE_ITEM boards_priv_reqs usb)
    list(APPEND boards_priv_reqs esp_driver_gpio esp_driver_ledc)
endif()

idf_component_register(SRCS boards.c board_flash.c ${BOARD_SOURCES}
                       INCLUDE_DIRS "." "${BOARD}" ${BOARD_INCLUDES} ${TOP}/src
                       PRIV_REQUIRES ${boards_priv_reqs}
                       REQUIRES esp_timer app_update spi_flash led_strip lcd ssd1306 XPowersLib tinyusb_src)

idf_component_register(SRCS "bootloader_start.c"
                    INCLUDE_DIRS "../../boards/${BOARD}"
                    REQUIRES bootloader bootloader_support hal)

idf_build_get_property(target IDF_TARGET)

# TODO: [ESP32C5] IDF-9197 remove this when beta3 is removed
if(CONFIG_IDF_TARGET_ESP32C5_MP_VERSION)
    set(target_folder "esp32c5/mp")
elseif(CONFIG_IDF_TARGET_ESP32C5_BETA3_VERSION)
    set(target_folder "esp32c5/beta3")
else()
    set(target_folder "${target}")
endif()

include($ENV{IDF_PATH}/tools/cmake/version.cmake)

if(IDF_BUILD_V2)
    # Build system v2 exposes the preprocessed bootloader linker scripts as a
    # build property
    idf_build_get_property(scripts BOOTLOADER_LINKER_SCRIPT)
    target_linker_script(${COMPONENT_LIB} INTERFACE "${scripts}")
elseif("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "6.0")
    # IDF v6.x (v1 build) attaches the default linker scripts to __idf_main
    # itself right after project(bootloader), nothing to do here.
else()
    # Legacy IDF v5.x ships per-target bootloader.ld/bootloader.rom.ld
    set(scripts "${IDF_PATH}/components/bootloader/subproject/main/ld/${target_folder}/bootloader.ld"
                "${IDF_PATH}/components/bootloader/subproject/main/ld/${target_folder}/bootloader.rom.ld")
    target_linker_script(${COMPONENT_LIB} INTERFACE "${scripts}")
endif()

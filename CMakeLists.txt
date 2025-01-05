cmake_minimum_required(VERSION 3.12)

set(ENV{PIMORONI_PICO_PATH} "/Users/andrewcapon/Development2/pico/pimoroni-pico")
message($ENV{PIMORONI_PICO_PATH})
set(PIMORONI_PICO_PATH "/Users/andrewcapon/Development2/pico/pimoroni-pico")

set(PICO_PLATFORM rp2350-arm-s)
set(PICO_BOARD pimoroni_pico_plus2_rp2350)

set(PICO_DEOPTIMIZED_DEBUG ON) 


include(pimoroni_pico_import.cmake)
include(pico_sdk_import.cmake)

# Gooey boilerplate
project(presto-cached-examples C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the SDK
pico_sdk_init()

if (NOT PIMORONI_PRESTO_PATH)
    set(PIMORONI_PRESTO_PATH ../../presto/)
endif()
if(NOT IS_ABSOLUTE ${PIMORONI_PRESTO_PATH})
    get_filename_component(
        PIMORONI_PRESTO_PATH
        "${CMAKE_CURRENT_BINARY_DIR}/${PIMORONI_PRESTO_PATH}"
        ABSOLUTE)
endif()
message("PIMORONI_PRESTO_PATH is ${PIMORONI_PRESTO_PATH}")
set(PIMORONI_PRESTO_PATH ${PIMORONI_PRESTO_PATH} CACHE PATH "Path to the Presto libraries" FORCE)
include_directories(${PIMORONI_PRESTO_PATH})
list(APPEND CMAKE_MODULE_PATH ${PIMORONI_PRESTO_PATH})
include(drivers/st7701/st7701_presto)

include_directories(${CMAKE_CURRENT_LIST_DIR}/uzlib/src)

add_subdirectory(lwmem)


# Include required libraries
# This assumes `pimoroni-pico` is stored alongside your project
include(common/pimoroni_i2c)
include(common/pimoroni_bus)
include(drivers/fatfs/fatfs)
include(drivers/sdcard/sdcard)
include(libraries/pico_graphics/pico_graphics)
include(libraries/pico_vector/pico_vector)


######################################
# Single Psram buffer 480x480
######################################

add_executable(SinglePsramBuffer480x480
    src/SinglePsramBuffer480x480.cpp 
    src/PicoPlusPsram.cpp
)

target_link_libraries(SinglePsramBuffer480x480
    st7701_presto
    pico_stdlib
    pico_multicore
    pimoroni_i2c
    sdcard
    fatfs
    hardware_interp
    pico_graphics
    pico_vector
    lwmem
)

# Configure the SD Card library for Presto
target_compile_definitions(SinglePsramBuffer480x480 PRIVATE
  SDCARD_SPI_BUS=spi0
  SDCARD_PIN_SPI0_CS=39
  SDCARD_PIN_SPI0_SCK=34
  SDCARD_PIN_SPI0_MOSI=35
  SDCARD_PIN_SPI0_MISO=36
  PICO_CLOCK_AJDUST_PERI_CLOCK_WITH_SYS_CLOCK=1
)

# create map/bin/hex file etc.
pico_add_extra_outputs(SinglePsramBuffer480x480)

# Enable USB UART output only
pico_enable_stdio_uart(SinglePsramBuffer480x480 0)
pico_enable_stdio_usb(SinglePsramBuffer480x480 1)



######################################
# Double Psram buffer 480x480
######################################

add_executable(DoublePsramBuffer480x480
    src/DoublePsramBuffer480x480.cpp 
    src/PicoPlusPsram.cpp
)

target_link_libraries(DoublePsramBuffer480x480
    st7701_presto
    pico_stdlib
    pimoroni_i2c
    hardware_interp
    pico_graphics
    lwmem
)

# Configure the SD Card library for Presto
target_compile_definitions(DoublePsramBuffer480x480 PRIVATE
  SDCARD_SPI_BUS=spi0
  SDCARD_PIN_SPI0_CS=39
  SDCARD_PIN_SPI0_SCK=34
  SDCARD_PIN_SPI0_MOSI=35
  SDCARD_PIN_SPI0_MISO=36
  PICO_CLOCK_AJDUST_PERI_CLOCK_WITH_SYS_CLOCK=1
)

# create map/bin/hex file etc.
pico_add_extra_outputs(DoublePsramBuffer480x480)

# Enable USB UART output only
pico_enable_stdio_uart(DoublePsramBuffer480x480 0)
pico_enable_stdio_usb(DoublePsramBuffer480x480 1)

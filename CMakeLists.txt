cmake_minimum_required(VERSION 3.22)

#
# STM32CubeMX makes this file only one time. If you use
# STM32CubeMX more than one time, it does not make this file again.
#
# You can change this file as much as necessary.
#

# Set the configuration of the compiler
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Set the CMake configuration (CMAKE_BUILD_TYPE)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the name of the project
set(CMAKE_PROJECT_NAME Nucleo-H753DisplayTest)

# Let CMake write compile_commands.json. This file helps tools (for example
# clangd) to find the symbols in the code.
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Set the primary configuration of the project
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Let CMake compile ASM and C files
enable_language(C ASM)

# Make the target of the executable
add_executable(${CMAKE_PROJECT_NAME})

# Add the source files from STM32CubeMX
add_subdirectory(cmake/stm32cubemx)

# LCD_LIBRARY selects the library for the display module and its test:
# - st7789 (the default): `Lib/st7789` and the stress test (`stress_demo.c`).
# - st7789_hal: `Lib/st7789_hal` and its test (`hal_demo.c`).
# The port of `Lib/st7789` has interrupt handlers for SPI1 and DMA1 Stream 0.
# Thus, the project links only one of the two libraries.
set(LCD_LIBRARY "st7789" CACHE STRING "The library for the display module: st7789 or st7789_hal")
set_property(CACHE LCD_LIBRARY PROPERTY STRINGS st7789 st7789_hal)

# LCD_BENCH=ON: compile the benchmark (`lcd_bench.c`) and not the test of
# the library that LCD_LIBRARY selects. The benchmark does the same
# operations with the two libraries. Thus, you can compare their speeds.
option(LCD_BENCH "Compile the benchmark (lcd_bench.c) and not the test of the library" OFF)

if(LCD_LIBRARY STREQUAL "st7789")
    # The ST7789 library has a target (st7789) that is different from the
    # target of the executable. Set the configuration macros of the library
    # on the st7789 target (refer to `Lib/st7789/CMakeLists.txt`).
    add_subdirectory(Lib/st7789)
    set(LCD_TEST_SOURCE Core/Src/stress_demo.c)
elseif(LCD_LIBRARY STREQUAL "st7789_hal")
    add_subdirectory(Lib/st7789_hal)
    set(LCD_TEST_SOURCE Core/Src/hal_demo.c)

    # `Core/Inc/st7789_hal_user_config.h` gives the configuration macros of
    # the library. It puts the transmit buffer in AXI SRAM.
    target_compile_definitions(st7789_hal PUBLIC
        ST7789HAL_CONFIG_FILE="st7789_hal_user_config.h")
    target_include_directories(st7789_hal PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc)

    # `main.c` and `stm32h7xx_it.c` use this macro to select the test.
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE USE_ST7789_HAL)

    # HAL_DEMO_USE_DMA=ON: the test prepares DMA2 Stream 0 for SPI1.
    # HAL_DEMO_USE_DMA=OFF: the library uses only HAL_SPI_Transmit.
    # The benchmark also uses HAL_DEMO_USE_DMA.
    option(HAL_DEMO_USE_DMA "The test of st7789_hal uses the DMA" ON)
    if(LCD_BENCH)
        set(HAL_DMA_SOURCE Core/Src/lcd_bench.c)
    else()
        set(HAL_DMA_SOURCE Core/Src/hal_demo.c)
    endif()
    if(HAL_DEMO_USE_DMA)
        set_source_files_properties(${HAL_DMA_SOURCE}
            PROPERTIES COMPILE_DEFINITIONS "HAL_DEMO_USE_DMA=1")
    else()
        set_source_files_properties(${HAL_DMA_SOURCE}
            PROPERTIES COMPILE_DEFINITIONS "HAL_DEMO_USE_DMA=0")
    endif()
else()
    message(FATAL_ERROR "LCD_LIBRARY must be st7789 or st7789_hal, not '${LCD_LIBRARY}'")
endif()

# The benchmark replaces the test of the library. `hal_demo.c` and the
# benchmark have the same interrupt handlers. Thus, CMake must not compile
# the two files together. `main.c` and `stm32h7xx_it.c` use the macro
# LCD_BENCH to select the benchmark.
if(LCD_BENCH)
    set(LCD_TEST_SOURCE Core/Src/lcd_bench.c)
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE LCD_BENCH)
endif()

# Give the linker the folders in which it finds libraries
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add your folders in which the linker finds libraries
)

# Add the source files to the target of the executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add your source files here
    ${LCD_TEST_SOURCE}
)

# Always compile the test with -O2, also in the Debug configuration. Thus,
# the frame rates show the speed of the library, not the speed of code with
# -O0 (the library sets -O2 for its files).
set_source_files_properties(${LCD_TEST_SOURCE}
    PROPERTIES COMPILE_OPTIONS "-O2")

# Add the include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add your include paths
)

# Add the compile definitions (macros) for the target of the executable
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add your compile definitions
)

# If you use .cpp files, the libraries that CMake gives to the linker
# incorrectly include libob.a. Remove libob.a from these libraries.
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add the libraries that the linker uses
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
    ${LCD_LIBRARY}

    # Add your libraries
)

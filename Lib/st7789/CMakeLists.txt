# This file gives the ST7789 library as a CMake target. Put the code that
# follows in the CMakeLists.txt of the project. Put it after the code that
# makes the target for the MCU (for this project, stm32cubemx from
# STM32CubeMX):
#   add_subdirectory(Lib/st7789)
#   target_link_libraries(<executable> st7789)
#
# Set the configuration macros (refer to st7789_config.h) on this target as
# PUBLIC compile definitions. Thus, the library and all the files that include
# st7789.h use the same values:
#   target_compile_definitions(st7789 PUBLIC ST7789_PANEL_HEIGHT=240)
# If you set a compile definition only on the target of the executable, the
# library does not get it.

# ST7789_PORT selects the port. The port is in the file
# port/st7789_port_<name>.c (<name> is the value of ST7789_PORT).
set(ST7789_PORT stm32h7 CACHE STRING "ST7789 port file: port/st7789_port_<name>.c")

# This is an OBJECT library, not a STATIC library, because of the interrupt
# handlers of the port. The linker replaces the .weak interrupt handlers of
# the startup file with the interrupt handlers of the port. But the linker
# does not use an object file from a library (.a file) only to replace a
# .weak symbol.
add_library(st7789 OBJECT
    st7789.c
    st7789_gfx.c
    st7789_font5x7.c
    port/st7789_port_${ST7789_PORT}.c
)

target_include_directories(st7789 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# The inner loops of the library operate one time for each pixel. With -O0,
# the time to draw text is approximately 2 times longer. Thus, the compiler
# always compiles the library with -O2, also in the Debug configuration.
target_compile_options(st7789 PRIVATE -O2)

# These items of the stm32cubemx target are necessary for the port:
# - The header files. For the STM32H7 port, these are the header files of
#   the HAL and of CMSIS, and main.h.
# - The compile definitions, for example STM32H753xx and USE_HAL_DRIVER. In
#   the CMake files from STM32CubeMX, the name of these compile definitions
#   is "symbols (macros)".
if(TARGET stm32cubemx)
    target_link_libraries(st7789 PRIVATE stm32cubemx)
endif()

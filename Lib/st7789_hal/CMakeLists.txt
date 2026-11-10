# This file gives the ST7789 HAL library as a CMake target. Put the code that
# follows in the CMakeLists.txt of the project. Put it after the code that
# makes the target for the MCU (for a project from STM32CubeMX, stm32cubemx):
#   add_subdirectory(Lib/st7789_hal)
#   target_link_libraries(<executable> st7789_hal)
#
# Set the configuration macros (refer to st7789_hal_config.h) on this target
# as PUBLIC compile definitions. Thus, the library and all the files that
# include st7789_hal.h use the same values:
#   target_compile_definitions(st7789_hal PUBLIC ST7789HAL_BUFFER_PIXELS=1024)

# This is a STATIC library. The library has no interrupt handlers. Thus, the
# linker does not have to replace .weak symbols with symbols of the library.
add_library(st7789_hal STATIC
    st7789_hal.c
    st7789_hal_gfx.c
    st7789_hal_font5x7.c
)

target_include_directories(st7789_hal PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# The inner loops of the library operate one time for each pixel. Thus, the
# compiler always compiles the library with -O2, also in the Debug
# configuration.
target_compile_options(st7789_hal PRIVATE -O2)

# The library includes `main.h` and the HAL header files. The stm32cubemx
# target gives the include folders and the compile definitions for them (for
# example STM32H753xx and USE_HAL_DRIVER). For a project without this target,
# give these items to the st7789_hal target.
if(TARGET stm32cubemx)
    target_link_libraries(st7789_hal PRIVATE stm32cubemx)
endif()

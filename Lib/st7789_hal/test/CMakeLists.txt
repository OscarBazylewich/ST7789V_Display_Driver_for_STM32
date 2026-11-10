# The tests of the ST7789 HAL library without hardware. This file is a
# different CMake project, for the compiler of the PC. The firmware does not
# use this file.
#
# To compile and operate the tests, give these commands:
#   `cmake -S Lib/st7789_hal/test -B Lib/st7789_hal/test/build/cmake -G Ninja`
#   `cmake --build Lib/st7789_hal/test/build/cmake`
#   `ctest --test-dir Lib/st7789_hal/test/build/cmake --output-on-failure`
#
# The script `run_tests.sh` does the same tests without CMake.

cmake_minimum_required(VERSION 3.16)
project(st7789_hal_host_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(OLD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../st7789)

set(TEST_SOURCES
    test_main.c
    st7789_model.c
    reference.c
    fake_hal/fake_hal.c
    ${LIB_DIR}/st7789_hal.c
    ${LIB_DIR}/st7789_hal_gfx.c
    ${LIB_DIR}/st7789_hal_font5x7.c
    ${OLD_DIR}/st7789_font5x7.c
)

# `-fsanitize=undefined` finds incorrect C operations. If the compiler does
# not have the library for this function (MinGW), use
# `-fsanitize-undefined-trap-on-error`.
include(CheckCSourceCompiles)
set(CMAKE_REQUIRED_FLAGS "-fsanitize=undefined")
set(CMAKE_REQUIRED_LINK_OPTIONS "-fsanitize=undefined")
check_c_source_compiles("int main(void) { return 0; }" HAVE_UBSAN_LIB)
set(CMAKE_REQUIRED_FLAGS "-fsanitize=undefined -fsanitize-undefined-trap-on-error")
set(CMAKE_REQUIRED_LINK_OPTIONS "-fsanitize=undefined;-fsanitize-undefined-trap-on-error")
check_c_source_compiles("int main(void) { return 0; }" HAVE_UBSAN_TRAP)
unset(CMAKE_REQUIRED_FLAGS)
unset(CMAKE_REQUIRED_LINK_OPTIONS)
if(HAVE_UBSAN_LIB)
    set(SAN_FLAGS -fsanitize=undefined -fno-sanitize-recover=all)
elseif(HAVE_UBSAN_TRAP)
    set(SAN_FLAGS -fsanitize=undefined -fsanitize-undefined-trap-on-error)
else()
    set(SAN_FLAGS)
endif()

enable_testing()

# One executable for each configuration: the name and the compiler flags
function(add_variant name)
    add_executable(st7789_hal_test_${name} ${TEST_SOURCES})
    target_include_directories(st7789_hal_test_${name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/fake_hal ${CMAKE_CURRENT_SOURCE_DIR} ${LIB_DIR} ${OLD_DIR})
    target_compile_options(st7789_hal_test_${name} PRIVATE
        -Wall -Wextra -Wpedantic -Werror -O1 -g ${SAN_FLAGS} ${ARGN})
    target_link_options(st7789_hal_test_${name} PRIVATE ${SAN_FLAGS})
    add_test(NAME ${name} COMMAND st7789_hal_test_${name})
endfunction()

add_variant(buf16 -DST7789HAL_BUFFER_PIXELS=16)
add_variant(buf512)
add_variant(buf4096 -DST7789HAL_BUFFER_PIXELS=4096)
add_variant(buf32752 -DST7789HAL_BUFFER_PIXELS=32752)
add_variant(buf512_unsigned_char_const_hal -funsigned-char -DFAKE_HAL_CONST_PDATA)
add_variant(buf64_dma32 -DST7789HAL_BUFFER_PIXELS=64 -DST7789HAL_DMA_MIN_BYTES=32)

/**
 * Configuration macros of the ST7789 HAL library.
 *
 * Each macro has a default. To set a different value, use one of these
 * methods:
 * - Set the macro as a compile definition, for example with
 *   target_compile_definitions(st7789_hal PUBLIC ST7789HAL_BUFFER_PIXELS=1024).
 * - Put the values in a header file of the application. Then set
 *   ST7789HAL_CONFIG_FILE to the name of this file, for example
 *   ST7789HAL_CONFIG_FILE="my_display_config.h". This file includes that
 *   file first.
 *
 * The library and the application must use the same values. Thus, set the
 * compile definitions on the st7789_hal target as PUBLIC.
 */
#ifndef ST7789_HAL_CONFIG_H
#define ST7789_HAL_CONFIG_H

#ifdef ST7789HAL_CONFIG_FILE
#include ST7789HAL_CONFIG_FILE
#endif

/* The header file with the HAL functions and types. The `main.h` file from
 * STM32CubeMX includes the HAL header file of the family. */
#ifndef ST7789HAL_HAL_HEADER
#define ST7789HAL_HAL_HEADER  "main.h"
#endif

/* The number of pixels in the transmit buffer (2 bytes for each pixel).
 * All the handles use one transmit buffer. A larger transmit buffer gives a
 * smaller number of HAL calls. Thus, the drawing functions are faster.
 *
 * The value must be in the range 16 to 32767. The value divided by 16 must
 * be an integer. Thus, the number of bytes in the transmit buffer is 32
 * times an integer, and the transmit buffer has only full cache lines. */
#ifndef ST7789HAL_BUFFER_PIXELS
#define ST7789HAL_BUFFER_PIXELS  512
#endif

/* The attributes of the transmit buffer. The DMA reads the transmit buffer.
 * Thus, on some MCUs, the transmit buffer must be in a RAM area that the DMA
 * can read. For example, on the STM32H7, the DMA cannot read the DTCM. To
 * put the transmit buffer in a different section, set this macro, for
 * example:
 *   #define ST7789HAL_BUFFER_ATTR \
 *     __attribute__((section(".bss.st7789.dma"), aligned(32)))
 * Keep aligned(32). Then the transmit buffer starts at a cache line, and
 * the data cache operations of the library have an effect on only the
 * transmit buffer. */
#ifndef ST7789HAL_BUFFER_ATTR
#if defined(__GNUC__)
#define ST7789HAL_BUFFER_ATTR  __attribute__((aligned(32)))
#else
#define ST7789HAL_BUFFER_ATTR
#endif
#endif

/* If `use_dma` is 1, the library uses HAL_SPI_Transmit_DMA only for
 * transfers of this number of bytes or more. For a shorter transfer, the
 * time to start the DMA is longer than the time of the transfer. Thus, the
 * library uses HAL_SPI_Transmit. */
#ifndef ST7789HAL_DMA_MIN_BYTES
#define ST7789HAL_DMA_MIN_BYTES  64
#endif

/* The maximum time in milliseconds for one SPI transfer. If a transfer
 * does not complete in this time, the library sets an error bit. */
#ifndef ST7789HAL_SPI_TIMEOUT_MS
#define ST7789HAL_SPI_TIMEOUT_MS  1000
#endif

/* The maximum value of the `size` parameter of the text functions. A
 * larger value gives this value. */
#ifndef ST7789HAL_MAX_TEXT_SIZE
#define ST7789HAL_MAX_TEXT_SIZE  8
#endif

#endif /* ST7789_HAL_CONFIG_H */

/**
 * Configuration macros of the ST7789 HAL library for this project.
 *
 * The top-level `CMakeLists.txt` sets the compile definition
 * ST7789HAL_CONFIG_FILE="st7789_hal_user_config.h" on the st7789_hal
 * target. Thus, st7789_hal_config.h includes this file first. Refer to
 * st7789_hal_config.h for the meaning of each macro.
 */
#ifndef ST7789_HAL_USER_CONFIG_H
#define ST7789_HAL_USER_CONFIG_H

/* The DMA cannot read the DTCM, and the linker script puts `.bss` in the
 * DTCM. Thus, put the transmit buffer in the section `.bss.st7789.dma`. The
 * linker script puts this section in AXI SRAM (RAM, 0x24000000). main()
 * sets this section to 0. */
#define ST7789HAL_BUFFER_ATTR  __attribute__((section(".bss.st7789.dma"), aligned(32)))

/* 4096 pixels (8192 bytes). A larger transmit buffer gives a smaller
 * number of HAL calls. */
#define ST7789HAL_BUFFER_PIXELS  4096

#endif /* ST7789_HAL_USER_CONFIG_H */

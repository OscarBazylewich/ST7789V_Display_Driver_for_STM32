/**
 * ST7789 library configuration.
 *
 * This file gives a default for each configuration macro. Each
 * configuration macro must have the same value in all the files that
 * include st7789.h. This is also necessary for the .c files of the library.
 * Thus, change a configuration macro for all the files that you compile.
 * Do not change it with a `#define` in only one source file. Use one of
 * these methods:
 * - Set it with a compile definition for all the files that you compile.
 *   With CMake, put it on the target of the library:
 *     target_compile_definitions(st7789 PUBLIC ST7789_PANEL_HEIGHT=240)
 *   CMake also gives it to each target that uses the library with
 *   target_link_libraries(). If you put it on only the target of the
 *   executable, the library does not get it.
 * - Put it in your header file. With a compile definition for all the files
 *   that you compile, set ST7789_CONFIG_FILE to the name of your header
 *   file. For example, use ST7789_CONFIG_FILE="st7789_user_config.h".
 * - Change this file.
 *
 * The library uses ST7789_BGR_ORDER, ST7789_PANEL_TUNING and
 * ST7789_GATHER_WIDTH in `#if` conditions. Thus, these must be numbers only
 * (0 or 1 for ST7789_BGR_ORDER and ST7789_PANEL_TUNING). Each of the other
 * configuration macros can be an integer constant expression. When you
 * compile the library, it does a check of the configuration macros.
 */
#ifndef ST7789_CONFIG_H
#define ST7789_CONFIG_H

#ifdef ST7789_CONFIG_FILE
#include ST7789_CONFIG_FILE
#endif

/* These macros give the width and the height of the screen in portrait, in
 * pixels. For the 2" display module from Waveshare, the values are 240x320.
 * There are also panels with 240x240 and 135x240 pixels. */
#ifndef ST7789_PANEL_WIDTH
#define ST7789_PANEL_WIDTH    240
#endif
#ifndef ST7789_PANEL_HEIGHT
#define ST7789_PANEL_HEIGHT   320
#endif

/* These macros give the position of the screen in the controller memory
 * (240x320 pixels), in portrait. For 240x320 panels, the offsets are 0,0.
 * For smaller panels, an offset is frequently necessary, for example 52,40
 * for 135x240. */
#ifndef ST7789_PANEL_X_OFFSET
#define ST7789_PANEL_X_OFFSET 0
#endif
#ifndef ST7789_PANEL_Y_OFFSET
#define ST7789_PANEL_Y_OFFSET 0
#endif

/* IPS panels show the correct colors only when color inversion is on */
#ifndef ST7789_INVERT_COLORS
#define ST7789_INVERT_COLORS  1
#endif

/* If the panel shows red as blue and blue as red, set this macro to 1 */
#ifndef ST7789_BGR_ORDER
#define ST7789_BGR_ORDER      0
#endif

/* This macro gives the largest value of `size` for text. If `size` is
 * larger, the library uses this value. Each text buffer (refer to
 * ST7789_TEXT_BUFFER_PIXELS) must have space for one character at this
 * `size`. */
#ifndef ST7789_MAX_TEXT_SIZE
#define ST7789_MAX_TEXT_SIZE       8
#endif

/* In direct mode, the library draws text into one of the two text buffers.
 * Then it sends a group of adjacent characters in one transfer. Each text
 * buffer has space for this number of pixels. It must have space for one
 * character at the largest `size`,
 * (6 * ST7789_MAX_TEXT_SIZE) x (8 * ST7789_MAX_TEXT_SIZE) pixels. This is
 * 3072 pixels if ST7789_MAX_TEXT_SIZE is 8 (the default), and 192 pixels if
 * it is 2.
 *
 * Together, the two text buffers use 4 bytes for each pixel (15 KB with the
 * default), in RAM that the DMA can read. To use less RAM, decrease
 * ST7789_MAX_TEXT_SIZE and ST7789_TEXT_BUFFER_PIXELS. The linker puts the
 * text buffers in the executable only if the application calls a function
 * that draws text. With larger text buffers, the library sends text in a
 * smaller number of longer transfers. */
#ifndef ST7789_TEXT_BUFFER_PIXELS
#define ST7789_TEXT_BUFFER_PIXELS  (240 * 16)
#endif

/* In direct mode, the DMA sends each filled area or image that has a
 * minimum of this number of pixels. It sends them in background transfers.
 * Thus, the drawing function can return before the pixels are on the
 * screen. The CPU sends smaller areas and images before the function
 * returns.
 *
 * On the STM32H7 port, each DMA transfer adds approximately 0.5 us of work
 * to start and complete it. If the application has other work to do before
 * it calls a drawing function again, the DMA transfer decreases the total
 * time. At 100 MHz SCK, the time for 32 pixels is approximately 5 us. If the
 * value is 0, the DMA sends all the filled areas and images. If the value
 * (maximum 0xFFFFFFFF) is more than the number of pixels of the screen, the
 * DMA sends none of them. */
#ifndef ST7789_DMA_MIN_PIXELS
#define ST7789_DMA_MIN_PIXELS        32
#endif

/* This macro is the same as ST7789_DMA_MIN_PIXELS, but for the rectangles
 * that the library uses to draw a shape:
 * - The horizontal and vertical lines of DrawLine and of the sides of
 *   DrawTriangle
 * - The sides of DrawRect
 * - The horizontal and vertical lines of DrawCircle
 * - The rectangles of FillCircle and FillTriangle
 * - The rectangles of transparent text. For each horizontal line of
 *   character pixels in the font, the library draws one rectangle with
 *   `size` rows.
 *
 * The library draws the next rectangle immediately. Thus, the CPU has only a
 * small quantity of work to do during a DMA transfer. Also, the CPU sends a
 * small rectangle in less time than the DMA. For filled circles, the library
 * is approximately 1.4% faster than with the value 32. For filled triangles
 * and transparent text, the library is approximately 0.5% slower, because it
 * calculates more between the rectangles. */
#ifndef ST7789_SHAPE_DMA_MIN_PIXELS
#define ST7789_SHAPE_DMA_MIN_PIXELS  256
#endif

/* In framebuffer mode, the library keeps a list of the changed areas
 * between two flushes. This value is the maximum number of areas in the
 * list (minimum 1, 16 bytes of RAM for each area). The library includes a
 * new area in a changed area if this adds less than approximately 64
 * pixels. When the list is full, the library calculates the bounding box of
 * all the changed areas and the new area:
 * - If the bounding box has no more pixels than all the areas together, it
 *   replaces all the areas. This occurs when the areas have a large overlap.
 * - If the bounding box has more pixels than all the areas together, the
 *   library includes the new area in one changed area. It selects the
 *   changed area that becomes larger by the smallest number of pixels. This
 *   changed area can be far from the new area.
 *
 * Thus, with many changes at different positions, a flush can send more
 * pixels than the pixels that changed. If many objects on the screen move
 * independently, increase this value. */
#ifndef ST7789_MAX_DIRTY_RECTS
#define ST7789_MAX_DIRTY_RECTS     16
#endif

/* An area is thin if it is not as wide as this number of pixels. In
 * framebuffer mode, the library copies a thin changed area into the gather
 * buffer before it sends the area. Thus, the DMA sends the area in one
 * piece. If the DMA sends the area directly from the framebuffer, each row
 * of the area is a different piece. For each row, the interrupt handler of
 * the DMA stream starts the DMA again.
 *
 * On the STM32H7, rows of 1 pixel are too short to keep the SPI in
 * operation while the interrupt handler starts the DMA again. The SPI sends
 * these rows at approximately 60% of its full speed. With rows of 2 pixels
 * or more, the SPI operates at full speed. But each row causes an interrupt
 * (0.1 to 0.2 us of CPU time).
 *
 * With 2 (the default), the library uses the gather buffer only for areas
 * that are 1 pixel wide. With a larger value, it also uses the gather buffer
 * for wider areas. This decreases the number of interrupts. But the library
 * copies the pixels (approximately 7 ns for each pixel), and this makes the
 * flush longer. In a flush, the library copies these areas in the transfer
 * interrupt. But if the library sends one of these areas first,
 * ST7789_Flush copies that area.
 *
 * The gather buffer is in RAM that the DMA can read. It contains
 * (ST7789_GATHER_WIDTH - 1) pixels for each pixel of the longer side of the
 * panel. For a 240x320 panel and the default value, this is 640 bytes. The
 * gather buffer is always in the executable, also if the application uses
 * only direct mode. The value 0 removes it (for an application that does
 * not use framebuffer mode). */
#ifndef ST7789_GATHER_WIDTH
#define ST7789_GATHER_WIDTH          2
#endif

/* 1: ST7789_Init also sends the voltage and gamma values that the Waveshare
 * driver uses for the 2" display module (240x320 pixels). 0 (default): the
 * controller keeps the voltage and gamma values that it has after a reset.
 * The tests of this library showed that the library operates correctly with
 * these values. For other display modules, different values are necessary. */
#ifndef ST7789_PANEL_TUNING
#define ST7789_PANEL_TUNING        0
#endif

/* This macro gives the timeout of ST7789_WaitIdle in milliseconds. If a
 * background transfer does not complete in this time after ST7789_WaitIdle
 * starts to wait, the function stops the transfer and continues. The
 * timeout is only for a transfer that does not complete. Thus, the timeout
 * must be longer than the longest transfer, the transfer of a full screen.
 * The time of this transfer is PANEL_WIDTH * PANEL_HEIGHT * 16 / SCK
 * (12.3 ms at 100 MHz, 393 ms at 3.125 MHz). If SCK is less than
 * approximately 1.2 MHz, increase the timeout. */
#ifndef ST7789_IDLE_TIMEOUT_MS
#define ST7789_IDLE_TIMEOUT_MS     1000
#endif

/* This macro puts a buffer in memory that the DMA of the port can read. The
 * library also uses it for its text buffers and for the buffer with the
 * color of a filled area. Use it also for a framebuffer, for example:
 *   static uint16_t fb[ST7789_FRAMEBUFFER_PIXELS] ST7789_DMA_BUFFER;
 * With GCC, the macro puts the buffer in the section `.bss.st7789.dma`. The
 * linker script must put this section in RAM that the DMA can read, before
 * `.bss` (refer to the README). No variable name can accidentally give the
 * same section name.
 *
 * All the data in these buffers is 0 at the start, the same as in other
 * `static` variables (after the application sets the section to 0 when it
 * starts, as the README shows). These buffers cannot have an initial
 * value in the C code. Keep constant images `const`. Do not use this macro
 * for them, because the DMA reads them directly from the flash memory.
 *
 * For a different linker script or compiler, set this macro to an
 * applicable value. If the macro is empty, the buffers are in usual RAM.
 * The library also operates correctly with this RAM. But if the DMA cannot
 * read this RAM, the CPU sends the data. */
#ifndef ST7789_DMA_BUFFER
#if defined(__GNUC__) && defined(__ELF__)
#define ST7789_DMA_BUFFER  __attribute__((section(".bss.st7789.dma"), aligned(32)))
#else
#define ST7789_DMA_BUFFER
#endif
#endif

#endif /* ST7789_CONFIG_H */

# ST7789 library

This fast C library is for ST7789 and ST7789V controllers on a 4-wire SPI
bus. It has a port for the STM32H7. We wrote the library for the IPS
display module from Waveshare (2", 240x320 pixels) on a NUCLEO-H753ZI board.

## Properties

- **Two drawing modes**: You can change the drawing mode while the
  application operates.
  - **Direct mode**: The library draws directly into the controller memory,
    without a framebuffer. The library keeps approximately 350 bytes of
    data. It also keeps the 640-byte gather buffer, which framebuffer mode
    uses for areas that are 1 pixel wide. A value of 0 in
    `ST7789_GATHER_WIDTH` removes this buffer. For text, the library adds
    two text buffers that use 15 KB together by default, and you can change
    their length.
  - **Framebuffer mode**: The library draws into a 150 KB copy of the screen
    in RAM. A flush sends only the changed areas, as a background transfer.
    Each drawing function records its clipped bounding box as a changed
    area.
- **DMA for all operations that it makes faster**: With the DMA, the library
  sends large filled rectangles, images and groups of characters as
  background transfers. Each flush is also a background transfer. A drawing
  function returns immediately after its transfer starts.
- **Shapes**: The library draws pixels, lines, rectangles, circles and
  triangles. It draws rectangles, circles and triangles as a contour or as
  a filled shape. A filled shape includes all the pixels of its contour and
  of its inner area, and no other pixels. The library also draws RGB565
  images, and 5x7 text with or without a background color. You can multiply
  the dimensions of the characters by 1 to `ST7789_MAX_TEXT_SIZE` (8 by
  default).
- **Clipping**: A shape, an image or a text can have some or all of its
  pixels out of the screen.
- **Controller commands**: The library has functions for four rotations,
  sleep mode (a different function sets the backlight), color inversion,
  the refresh rate (39–119 Hz) and hardware vertical scrolling. A function
  also sets the panel to show the image or to show no image.
- **Configuration**: In `st7789_config.h`, you can change the configuration
  for other panel dimensions and offsets (240x240, 135x240) and for BGR
  panels. You can also change the lengths of the buffers in this file.
- **Other MCUs**: You can use the library on other MCUs. The library uses
  the hardware only through the small interface in `st7789_port.h`. C++
  code can include the header files. The folder of the library is also a
  CMake target.
- **No wait loops if you do not want them**: While the library waits,
  optional callbacks let the application stop the CPU until the next
  interrupt. As an alternative, they let the RTOS stop the task. The
  callbacks also tell the application about the end of a background
  transfer.
- **Safe if errors occur**: If a transfer is not completed at the end of the
  timeout, the library stops it. The library starts a stopped SPI again.
  Thus, the CPU does not stay in a wait loop. `ST7789_GetErrors()` shows the
  two errors.

## Performance

These results are from the NUCLEO-H753ZI board. SCK is at 100 MHz, and the
data cache is on. 100 MHz is more than the maximum SCK in the datasheet of
the controller (refer to "Speed" in "Necessary conditions for the STM32H7
port"). The stress test draws each scene in the 240x300 area below the
status bar.

The time to send one pixel on the SPI bus is 160 ns. Thus, the time to
draw all 72,000 pixels again is 11.5 ms. A scene that changes the full
area has a maximum frame rate of approximately 87 fps. The stress test
draws the circles, the lines and the random pixels on the previous frame.
In framebuffer mode, a flush sends only the bounding box of the changed
pixels. Thus, in framebuffer mode, the frame rate of the circles scene is
more than 87 fps.

| Scene (in each frame)             | Direct mode | Framebuffer mode |
|-----------------------------------|-------------|------------------|
| One filled rectangle              | 86.7        | 86.6             |
| 1,480 characters of text          | 86.2        | 87.7             |
| 40 filled circles                 | 63.5        | 94.9             |
| 300 random lines                  | 17.2        | 86.7             |
| 72,000 pixels, row by row         | 20.7        | 86.4             |
| 72,000 pixels at random positions | 4.2         | 80.3             |

The table gives the frame rates in fps. In framebuffer mode, the frame rate
of the scenes before the last scene is at the limit of the SPI bus. In the
last scene, the CPU writes to random positions in the full 150 KB
framebuffer. Thus, the frame rate is at the limit of the CPU. Frame rates
at the limit of the CPU can change by approximately 1% from one executable
to the next. This can also occur when the only change is the position of
the code in the flash memory.

With many very small shapes, direct mode is slow, because the library must
set a window for each shape. To set a window, the library sends a maximum
of 7 SPI frames with commands and parameters. At each change between a
command and data, the port waits until the SPI sent the last bit. Pixels
that the application draws row by row, from left to right, continue the
data of one RAMWR command. Thus, they prevent the largest part of this work.
Pixels at many different positions are the worst condition.

## How the library is fast

- **The SPI does not stop while the library draws.** The STM32H7 SPI sends
  16-bit SPI frames in endless mode (no specified length). The library
  starts the SPI again only for some commands and after an error. These
  commands set the configuration or control the panel (for the initial
  configuration, rotation, scrolling, sleep mode, color inversion and the
  refresh rate). The SPI sends them as 8-bit SPI frames.
  - While the library draws, the SPI sends each command as the 16-bit SPI
    frame `0x00, cmd`. `0x00` is the NOP command of the controller. Thus,
    only `cmd` has an effect.
  - The window coordinates and the pixels are 16-bit values.
  - Before the port changes DC, it waits for the TXC flag of the SPI (the
    SPI sent the last bit). Then the port reads DC to make sure that DC has
    the new level before the next byte starts.
- **A copy of the window.** The library records the last window (CASET and
  RASET). It sends each of these two commands again only when its
  parameters change.
- **Pixel write cursor.** In direct mode, the window for a pixel extends to
  the right and bottom edges of the screen. Thus, for the next pixel in the
  row, the library sends no command. This pixel continues the data of the
  same RAMWR command. This also occurs when the write cursor continues on
  the next row.
- **The library draws with horizontal lines, vertical lines and
  rectangles.**
  - In direct mode, the library draws lines and circle contours as
    horizontal lines and vertical lines (in the two drawing modes, the
    library draws the same pixels). For a circle contour, the number of
    pixels is approximately 2.4 times the number of windows that the
    library uses. The library also draws the contour approximately 2.4
    times more quickly than if it uses one window for each pixel.
  - The library draws filled circles and triangles as rectangles of equal
    rows (adjacent rows with the same start and the same end).
  - The CPU sends each rectangle that the library uses to draw a shape (and
    each row of pixels of transparent text), if it has less than
    `ST7789_SHAPE_DMA_MIN_PIXELS` (256) pixels. Because the next rectangle
    follows immediately, a DMA transfer adds almost only the time to start
    it. With 256 (not 32), the library draws filled circles approximately
    1.4% more quickly. It draws filled triangles and transparent text
    approximately 0.5% more slowly, because it calculates more between
    their rectangles. Filled rectangles and images from the application use
    the DMA when they have `ST7789_DMA_MIN_PIXELS` (32) pixels or more.
    Thus, the application can do other work while the DMA sends them.
  - If the DMA cannot read an image, the CPU sends it. When the rows of this
    image are adjacent in memory, the CPU sends the image in one transfer.
  - In direct mode, the library draws each group of characters into one of
    the two text buffers. A group has as many characters as the row of text
    and the text buffer (`ST7789_TEXT_BUFFER_PIXELS` pixels) can contain.
    With the default value, a group can contain a full row of text in
    portrait. This is possible when you multiply the dimensions of the
    characters by 1 or 2. The DMA sends each group in one DMA transfer, not
    with one window for each character. In framebuffer mode, the library
    draws a full row of text directly into the framebuffer.
- **DMA transfers in sequence.**
  - The library sends a rectangle with a stride (part of an image, or an
    area of the framebuffer) row by row. The DMA interrupt starts each row.
  - If a row does not start immediately, the SPI stops SCK until the row
    starts. Thus, the controller receives all the data. Only rows of 1 pixel
    are too short. For these rows, the time to start the next row is
    longer than the time to send the row (the SPI sends these rows at
    approximately 60% of the SCK rate). Thus, the library first copies an
    area of the framebuffer that is 1 pixel wide into the gather buffer.
    Then the DMA sends this buffer in one DMA transfer, which is 1.6 times
    faster.
  - The library collects the areas with a width of less than
    `ST7789_GATHER_WIDTH` (2) pixels into the gather buffer. With a larger
    value, the library also prevents the interrupt for each row of wider
    areas. But the library must copy these areas, and this adds a small
    quantity of time to their flush.
  - During a flush of two or more changed areas, the transfer interrupt
    starts the transfer of the next area. Thus, the application does not do
    this work. For each area, the interrupt does a small quantity of work:
    - It sets the window.
    - If the width of the area is less than `ST7789_GATHER_WIDTH`, it copies
      the area into the gather buffer.
    - It cleans the data cache for the DMA source (the area, or the copy of
      the area in the gather buffer).
- **The library records the changed areas.**
  - In framebuffer mode, the library keeps a maximum of
    `ST7789_MAX_DIRTY_RECTS` (16 by default) changed areas. It includes
    near areas in one larger area.
  - When the list of changed areas is full, the library adds the numbers of
    pixels in all the areas. If their overlap is sufficient, this total is
    not less than the number of pixels in their bounding box. In this
    condition, the library keeps only the bounding box. During a flush, the
    library sends one bounding box if this is faster than one transfer for
    each area.
- **Safe with the data cache.** Before each DMA transfer, the port cleans
  the data cache for the DMA source. This is sufficient for the data that
  the CPU wrote. For data from other bus masters, refer to "Other bus
  masters" in "How to use the library".

## Files

| File                          | Contents                                                                      |
|-------------------------------|-------------------------------------------------------------------------------|
| `st7789.h`                    | The API                                                                       |
| `st7789_config.h`             | The configuration macros (panel dimensions, offsets, buffer lengths)          |
| `st7789.c`                    | The controller configuration, the transfers, the framebuffer and the flushes  |
| `st7789_gfx.c`                | The drawing functions for the shapes and the text                             |
| `st7789_internal.h`           | The internal functions, types and variables that the two .c files use         |
| `st7789_font5x7.c/.h`         | The font                                                                      |
| `st7789_port.h`               | The functions that a port must supply                                         |
| `port/st7789_port_stm32h7.c`  | The STM32H7 port, which uses the registers of SPI1 and DMA1 Stream 0 directly |
| `CMakeLists.txt`              | The CMake target `st7789`                                                     |

Make a copy of the folder and all its contents. In the application, include
only `st7789.h`.

- **With CMake**, add the folder. Then link its target into the executable:

  ```cmake
  add_subdirectory(Lib/st7789)
  target_link_libraries(${CMAKE_PROJECT_NAME} stm32cubemx st7789)
  ```

  The target tells CMake to compile the library with `-O2`, also in the Debug
  configuration. The target also makes the folder an include path. In
  CMake, `ST7789_PORT` selects the port file. The default value of
  `ST7789_PORT` is `stm32h7`.

  Put the configuration macros on the target as `PUBLIC` compile definitions,
  for example `target_compile_definitions(st7789 PUBLIC
  ST7789_PANEL_HEIGHT=240)`. As a result, all the files use the same values.
  For the library, the compiler does not use a compile definition that is
  only on the target of the executable.
- **Without CMake**, compile `st7789.c`, `st7789_gfx.c`, `st7789_font5x7.c`
  and one port file with optimization. When you compile the code to debug it,
  also compile these files with optimization. The inner loops of the library
  operate one time for each pixel. With `-O0`, the time to draw text is
  approximately two times longer. Also make the folder an include path.

## How to use the library

**Direct mode:**

```c
#include "st7789.h"

ST7789_Init();                                     /* after MX_SPI1_Init() */
ST7789_FillScreen(ST7789_BLACK);
ST7789_DrawString(10, 10, "Hello", ST7789_WHITE, ST7789_BLACK, 2);
ST7789_FillCircle(120, 160, 40, ST7789_RED);
```

**Text:** After `ST7789_SetTextWrap(0)`, the library clips the text at the
right edge of the screen. The text does not continue on the next row.

After `ST7789_SetTextTransparent(1)`, the library draws only the character
pixels. The pixels behind the characters do not change. In direct mode, the
library must then set a different window for each horizontal line of
character pixels. The times for this text are:

- In direct mode, with `size` 1, the time is approximately three times the
  time for text with a background.
- In direct mode, with `size` 2 or more, most pixels of the text are
  background pixels. Thus, the time is approximately the same as the time for
  text with a background, or less.
- In framebuffer mode, the time is 1.2 to 2 times the time for text with a
  background.

**Instructions for calls:**

- Call `ST7789_Init()` before the other functions of the library. Call it
  after `MX_SPI1_Init()` and after the GPIO configuration.
- Then call the functions of the library from only one context. Usually, this
  context is the loop in `main()` or one RTOS task. If two or more tasks call
  the functions of the library, use a mutex. This is necessary because a call
  to the library must not start before the end of the previous call. This
  condition also includes the drawing functions in framebuffer mode.
- It must be possible for the interrupts of the port and for the SysTick
  interrupt to preempt this context. The interrupts of the port are SPI1 and
  DMA1_Stream0, with priority `ST7789_PORT_IRQ_PRIORITY` (the default is 5).
  Thus, while the interrupts are disabled, do not call a function of the
  library. Do not call a function of the library from an interrupt handler
  with priority 0–5. If a call must wait in these conditions, one of these
  problems occurs:
  - The call does not return.
  - The library waits for `ST7789_IDLE_TIMEOUT_MS`. Then it stops the
    transfer before its end.
- To draw when an interrupt occurs, set a flag in the interrupt handler. Then
  draw from the loop in `main()`.

**Framebuffer mode** is for frames that change quickly or for many small
shapes:

```c
static uint16_t fb[ST7789_FRAMEBUFFER_PIXELS] ST7789_DMA_BUFFER;

ST7789_Init();
ST7789_AttachFramebuffer(fb, sizeof fb / sizeof fb[0]);   /* returns 0 if too small */
ST7789_FillScreen(ST7789_BLACK);                          /* start from a known picture */
for (;;)
{
  /* draw the next frame with the same functions as above ... */
  ST7789_Flush();          /* waits for the previous flush, then starts this one */
}
```

- **Flushes.** `ST7789_Flush()` first waits for the previous flush (a maximum
  of the time for one frame: 12.3 ms for the full screen at 100 MHz). If the
  loop must not wait, call it only when `ST7789_IsBusy()` returns 0. The
  library continues to record the changed areas, and the next flush sends
  them. If the framebuffer is in a memory that the DMA cannot read, the CPU
  sends the changed areas. Then the call returns at the end of the flush.
- **Pixel sequence.** The framebuffer contains the screen row by row, in the
  rotation that the library uses at this time. Pixel (x, y) is
  `fb[y * ST7789_Width() + x]`. Each pixel is an RGB565 value (the same
  format as the colors and as the images of `ST7789_DrawImage()`). You can
  write directly into the framebuffer. Then call `ST7789_MarkDirty()` for the
  area that you changed.
- **Changed areas.** Each drawing function records the clipped bounding box of
  its shape, its text or its image as a changed area. For a line or a contour
  (`DrawLine`, `DrawRect`, `DrawCircle`, `DrawTriangle`), the changed area is
  the full bounding box. Thus, the next flush also sends the other pixels of
  the bounding box again. If only the contour of a large rectangle changes,
  you can draw its sides with `DrawHLine` and `DrawVLine`. Then the changed
  areas are only the sides.
- **Rotation.** `ST7789_SetRotation()` does not turn the contents of the
  framebuffer. It records the full screen as a changed area, and the next
  flush reads the framebuffer with the new width. After a change between
  portrait and landscape, draw the full screen again before the next flush.
  After you change the rotation by 180°, the contents continue to have the
  correct dimensions.
- **End of framebuffer mode.** `ST7789_DetachFramebuffer()` waits for a flush
  that is in progress. But it discards the changes that you made after the
  last `ST7789_Flush()`. To show these changes, flush the framebuffer before
  you call this function.
- **Other bus masters.** Bus masters that are not the CPU (for example DMA2D,
  MDMA or a different DMA) can write into the framebuffer. If the data cache
  is on and such a bus master writes into the framebuffer, do these steps:
  1. Before you start the bus master, clean and invalidate the data cache
     for each row that it will write (`SCB_CleanInvalidateDCache_by_Addr()`).
     For this call, extend the memory area of each row to full 32-byte cache
     lines.
  2. Do not draw into these rows until the bus master completes its operation.
  3. After the bus master completes its operation, invalidate the data cache
     for the same rows again.
  4. Call `ST7789_MarkDirty()` for these rows.
  5. Flush the framebuffer.

  If you do not do these steps, the data cache can keep data that the CPU
  changed in these rows. When the next flush cleans the data cache, this data
  replaces the output of the bus master.

**How to wait without a wait loop.** All drawing functions in direct mode and
`ST7789_Flush()` in framebuffer mode first wait for the previous transfer. If
you do not use a wait hook, the CPU waits in a wait loop. As an alternative, a
wait hook can stop the CPU until the next interrupt, or let the RTOS stop the
task for a short time. The wait hook must return in less than approximately
1 ms. Thus, with a wait hook, the timeout also occurs if a transfer does not
complete.

After the end of a background transfer (a DMA transfer or a full flush), the
interrupt handler of the transfer interrupt calls the done callback. Thus, the
done callback can let a task continue.

If you use FreeRTOS, `ST7789_PORT_IRQ_PRIORITY` (the interrupt priority of
the port) must be a priority at which FreeRTOS lets interrupt handlers call
`FromISR` functions. The default (5) agrees with the FreeRTOS configuration
of CubeMX. The example that follows is for FreeRTOS:

```c
static TaskHandle_t display_task;            /* the task that draws */

static void display_wait(void)
{
  ulTaskNotifyTake(pdTRUE, 1);               /* sleep until done, at most one tick */
}

static void display_done(void)               /* SPI1 interrupt: no library calls */
{
  BaseType_t woken = pdFALSE;
  vTaskNotifyGiveFromISR(display_task, &woken);
  portYIELD_FROM_ISR(woken);
}

/* in the drawing task, after ST7789_Init(): */
display_task = xTaskGetCurrentTaskHandle();
ST7789_SetWaitHook(display_wait);
ST7789_SetDoneCallback(display_done);
```

Without an RTOS, the wait hook can stop the CPU until the next interrupt. The
wait hook disables the interrupts before it examines if a transfer is in
progress, and enables them again before it returns. Thus, an interrupt that
occurs during this time also lets the CPU continue.

Some bits in `DBGMCU` let you debug the code while the MCU is in the "Sleep",
"Stop" or "Standby" mode. To debug code that uses `WFI` on the STM32H7, set
these bits to 1.

The example that follows is for a system without an RTOS:

```c
static void display_wait(void)
{
  __disable_irq();
  if (ST7789_IsBusy())
  {
    __DSB();
    __WFI();
  }
  __enable_irq();
}
```

On the NUCLEO-H753ZI, this wait hook does not decrease the speed. With this
wait hook, the time to fill the screen 50 times is 614 ms (615 ms with the
wait loop). Also, the wait hook stops the CPU between the interrupts.

Each time that the library waits, it calls the wait hook. If you draw many
very small shapes in direct mode, the library waits more than 100 times for
each frame. In this condition, a short wait loop is faster than a wait hook
that lets the RTOS stop the task.

**LVGL** can operate without the callbacks of this library. The LVGL callback
that sends an area can start the transfer. Then the LVGL wait callback can
wait for the transfer. The example that follows is for LVGL v9 (LVGL v8 uses
`drv.wait_cb` for the wait callback). In this example, the two buffers in
which LVGL writes the pixels for `flush_cb` are variables with
`ST7789_DMA_BUFFER`:

```c
static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px)
{
  ST7789_DrawImage(area->x1, area->y1, lv_area_get_width(area),
                   lv_area_get_height(area), (const uint16_t *)px);
}

static void flush_wait_cb(lv_display_t *disp)
{
  ST7789_WaitIdle();                         /* has the timeout recovery */
  lv_display_flush_ready(disp);
}

lv_display_set_flush_cb(disp, flush_cb);
lv_display_set_flush_wait_cb(disp, flush_wait_cb);
```

In framebuffer mode, you can continue to draw while a flush is in progress.
The next flush sends all the areas that you change during this time. This is
the fastest method. But the flush reads the framebuffer while you change it.
Thus, the panel can show a part of the next frame before the next flush.

Small changes usually show as groups of 16 pixels (one cache line each) at
many different positions. When you draw a large area again (for example, when
you fill the screen), the CPU almost immediately writes the new pixels before
the flush reads them. Thus, the panel can show the new pixels in the largest
part of an area. In a flush of two or more areas, each subsequent area shows
all the pixels that you draw into it before its transfer starts.

The next flush corrects the screen. To get correct frames, call
`ST7789_WaitIdle()` before you draw the next frame.

## Necessary conditions for the STM32H7 port

- **CubeMX configuration:** Set this configuration in CubeMX:
  - SPI1: the master mode ("Transmit Only Master") and SPI mode 0, with
    "Master Keep IO State" enabled. The port sets the "Data Size" and the
    FIFO threshold. Thus, their values in CubeMX are not important.
  - Output pins with the names `LCD_CS`, `LCD_DC`, `LCD_RST` and `LCD_BL`.
- **Only the port uses** SPI1, DMA1 Stream 0, DMAMUX1 channel 0, and the
  `SPI1` and `DMA1_Stream0` interrupts. Do not also enable DMA1 Stream 0,
  DMAMUX1 channel 0 or these interrupts in CubeMX. After you call
  `ST7789_Init()`, do not call a `HAL_SPI_*` function for SPI1. If the
  application calls `MX_SPI1_Init()` again, call `ST7789_Init()` again. If a
  clock of SPI1 or of the CPU changes (for example, after the "Stop" mode),
  also call this function again.

  As a precaution, the port finds a stopped SPI1 in less than 1 ms while the
  CPU sends data. The port then starts SPI1 again and continues. Thus, the
  CPU does not stay in a wait loop. After this, `ST7789_GetErrors()` shows
  `ST7789_ERR_LINK`, because the controller did not receive some pixels (or
  a configuration change) from the time when SPI1 stopped. If SPI1 stops
  during a DMA transfer, the DMA does not complete the transfer. After the
  timeout (`ST7789_IDLE_TIMEOUT_MS`), the library stops the transfer, starts
  SPI1 again and records `ST7789_ERR_TIMEOUT`.
- **DMA stream:** CubeMX does not know that the port uses DMA1 Stream 0. It
  gives this DMA stream to the DMA request that you add first. CubeMX also
  makes a `DMA1_Stream0_IRQHandler` that causes a linker error. Thus, move
  that DMA request to a different DMA stream in its "DMA Settings" tab.
- **Interrupt priority:** The SPI1 interrupt and the DMA1_Stream0 interrupt
  have the priority `ST7789_PORT_IRQ_PRIORITY` (the default is 5). To change
  the priority, set this macro for all the files that you compile. With
  CMake, set it as a compile definition of the library target:
  `target_compile_definitions(st7789 PUBLIC ST7789_PORT_IRQ_PRIORITY=6)`.
  Refer to "Instructions for calls" in "How to use the library".
- **DMA buffers:**
  - DMA1 cannot read the ITCM RAM or the DTCM RAM. CubeMX puts `.data` and
    `.bss` in DTCM. Use `ST7789_DMA_BUFFER` for each buffer that the DMA
    reads (the framebuffer and the images in RAM). The library also uses this
    macro for the text buffers, the gather buffer and the buffer that holds
    the color of filled areas.
  - With GCC, the macro uses the section `.bss.st7789.dma`. The linker
    script must put this section into AXI SRAM before `.bss` (if the linker
    script does not do this, `*(.bss*)` puts it into DTCM). The code in the
    startup file does not set this section to 0. Thus, set it to 0 in
    `main()`. In this project, `STM32H753xx_FLASH.ld` contains this code:

    ```
    .dma_buffers (NOLOAD) : ALIGN(32)
    {
      _sdma_buffers = .;
      *(.bss.st7789.dma)
      . = ALIGN(4);
      _edma_buffers = .;
    } >RAM
    ```

    The code in `main.c` sets this section to 0 immediately after it calls
    `SystemClock_Config()`:

    ```c
    extern uint8_t _sdma_buffers[], _edma_buffers[];
    memset(_sdma_buffers, 0, (size_t)(_edma_buffers - _sdma_buffers));
    ```
  - Examine where the linker put the buffers. In the map file, the
    `.dma_buffers` section (the `.bss.st7789.dma` part of each object file)
    must be at `0x2400xxxx`. `arm-none-eabi-nm -n <elf>` shows each buffer
    with its name: `fill_color`, `text_buf`, `gather_buf` and your buffers.
    The library can also use buffers that the DMA cannot read. But for these
    buffers, the CPU sends the data, and the call waits until the end of the
    transfer.
- **Speed:**
  - SCK is 100 MHz: the prescaler of SPI1 divides PLL2P (200 MHz) by 2. The
    port keeps the prescaler value that CubeMX sets.
  - 100 MHz is the limit of the STM32H753. The kernel clock of SPI1 can be a
    maximum of 200 MHz, but only at voltage scale VOS0 or VOS1. At VOS2, the
    maximum is 150 MHz, and at VOS3, it is 100 MHz. Thus, at VOS2 and VOS3,
    decrease PLL2P. The smallest prescaler value is 2.

    The datasheet of the STM32H753 gives a rating of 100 MHz for SCK if VDD
    is a minimum of 2.7 V (if VDD is less than 2.7 V, the rating is 80 MHz).
    These ratings are applicable when the I/O compensation cell operates.
    `HAL_MspInit()` enables the compensation cell.
  - 100 MHz is more than the maximum SCK in the datasheet of the controller.
    The rating of the ST7789VW is a minimum SCK cycle of 16 ns (62.5 MHz)
    when the MCU writes data to the controller. On this board, with these
    wires, the image was correct at 100 MHz. In a test at 110 MHz, the image
    was also correct, but 110 MHz is more than the specified limit of the
    MCU. At 120 MHz, the image was incorrect. Thus, at 100 MHz, the SPI bus
    operates near its limit.

    To keep SCK in the rating of the controller, set the prescaler of SPI1 to
    4 (50 MHz) in CubeMX. Then, in the scenes in which the speed of the SPI is
    the limit, the frame rate decreases to approximately half.

    After you change a clock, examine the test pattern of the stress test. If
    SCK is too fast for the SPI bus, you can see these errors:

    - Images in an incorrect position
    - Images that are not aligned
    - A screen that shows no image
    - Incorrect colors
  - We recommend that you use the data cache. It lets the library draw into
    the framebuffer as quickly as the SPI sends the data.
  - The default `ST7789_IDLE_TIMEOUT_MS` (1 s) is sufficient if SCK is a
    minimum of approximately 1.2 MHz. If SCK is less than approximately
    1.2 MHz, increase the timeout.

## Limits

- The display module has no pin for the TE (tearing effect) signal of the
  controller. Thus, it is not possible to make the frames synchronized with
  the refresh of the panel. As a result, fast movement on the full screen can
  cause tearing.
- The scrolling of the controller moves the portrait rows of the panel. Thus,
  the scrolling is vertical in portrait (in the opposite direction in
  `ST7789_PORTRAIT_FLIPPED`) and horizontal in landscape.
- Text uses the 5x7 font of the library. This font has only the ASCII
  characters from ' ' to '~' (0x20 to 0x7E). The library draws all other
  bytes as '?'.
- In framebuffer mode, a changed area that is not as wide as the screen
  causes one interrupt for each row (each interrupt uses 0.1 to 0.2 µs of CPU
  time). But if the width of the area is less than `ST7789_GATHER_WIDTH`, the
  library sends the area in one DMA transfer. If an area is a minimum of 2
  pixels wide, the SPI keeps its full speed.
- `ST7789_FillTriangle` calculates the accurate range of pixels in each row.
  Thus, the filled triangle has the same contour as the triangle that
  `ST7789_DrawTriangle` draws.

  In direct mode, the CPU calculates these ranges at the same time as the
  transfers. The time for this work is shorter than the time for the
  transfers. Thus, this work does not increase the time to draw the
  triangle.

  In framebuffer mode, the library draws the triangle into the framebuffer
  without transfers. Thus, the work to calculate the ranges increases the
  time to draw the triangle. The time to draw a triangle with half of the
  area of the screen is approximately 230 µs. For a triangle in an area of
  20 x 20 pixels, the time is approximately 12 µs.

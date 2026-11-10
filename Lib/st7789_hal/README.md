# ST7789 HAL library

This C library is for ST7789 and ST7789V controllers on a 4-wire SPI bus.
It uses only the HAL functions for the SPI and for the pins. Thus, you can
use it on all the STM32 families with the HAL. Examples are STM32F0, F1,
F4, G0, G4, H7, L4 and U5.

The library is for a board with an STM32 MCU and a project from STM32CubeMX
or STM32CubeIDE. Examples are a NUCLEO board with an STM32F4, an STM32G0 or
an STM32L4, or a "Blue Pill" board (STM32F103). The library has no interrupt
handlers and does not contain HAL callbacks. Thus, the interrupt handlers
and the HAL callbacks of the application do not change.

## Properties

- **One handle for each panel**: Each panel has a handle
  (`ST7789HAL_Handle`). Thus, one application can control two or more
  panels.
- **Configuration while the application operates**: You put the SPI handle,
  the pins, the panel dimensions and the offsets in a struct
  (`ST7789HAL_Config`). Then you give it to `ST7789HAL_Init()`.
- **Shapes and text**: The library draws pixels, lines, rectangles, circles
  and triangles. It draws rectangles, circles and triangles as a contour or
  as a filled shape. It also draws RGB565 images and 5x7 text with or
  without a background color.
- **Clipping**: A shape, an image or a text can have some or all of its
  pixels out of the screen.
- **Direct access to the controller memory**: `ST7789HAL_SetWindow()` and
  `ST7789HAL_WritePixels()` send pixels that the application calculates, for
  example from LVGL.
- **Controller commands**: The library has functions for four rotations,
  the backlight, sleep mode and color inversion. A function also sets the
  panel to show the image or to show no image.
- **Optional DMA**: For long transfers, the library can use
  `HAL_SPI_Transmit_DMA()`. The call also waits for the end of the DMA
  transfer.
- **Safe if errors occur**: Each DMA transfer has a timeout. Error bits
  record the errors until you set them to 0.

Each function returns after the SPI sent all the data of the call. The
colors are RGB565 values in the byte sequence of the CPU. The library sends
the high byte first.

## Differences between this library and `Lib/st7789`

This repository has two ST7789 libraries. `Lib/st7789` is the fast library
for the STM32H7. This library (`Lib/st7789_hal`) is for all the STM32
families. The two libraries do not use the same files or names. Thus, one
project can contain the two libraries.

| Property                    | `Lib/st7789`                                                                                      | `Lib/st7789_hal` (this library)                                                     |
|-----------------------------|---------------------------------------------------------------------------------------------------|-------------------------------------------------------------------------------------|
| MCUs                        | STM32H7. The port uses the registers of SPI1 and DMA1 Stream 0. For other MCUs, you must write a port. | All the STM32 families with the HAL                                                 |
| How the library is fast     | 16-bit SPI frames without stops, a copy of the window, DMA transfers in sequence                 | A transmit buffer: the library sends many pixels with one HAL call                  |
| Framebuffer mode            | Yes (150 KB in RAM)                                                                               | No. The library sends each shape immediately.                                       |
| Transfers                   | Background transfers. A drawing function returns after its DMA transfer starts.                   | Each function returns after the end of its transfers, also with the DMA.            |
| Interrupt handlers          | The port has the SPI1 and DMA1_Stream0 interrupt handlers.                                        | None. The HAL and STM32CubeMX supply the interrupt handlers.                        |
| Panel dimensions, offsets   | Configuration macros. You set them before you compile the library.                                | Fields in `ST7789HAL_Config`. You set them while the application operates.          |
| Two or more panels          | No                                                                                                | Yes. Each panel has a handle.                                                       |
| Scrolling, refresh rate     | Yes                                                                                               | No                                                                                  |

## Speed

The table shows the results of the application in `Core/Src/hal_demo.c` on a
Nucleo-H753 board. The SPI operated at an SCK of 100 MHz. The panel was the
2" display module from Waveshare (240x320 pixels). Each value is the number
of operations in one second.

| Scene       | One operation                  | With DMA | Without DMA |
|-------------|--------------------------------|---------:|------------:|
| FILL        | Fill the full screen           |     80.0 |        34.2 |
| RECTS       | Fill one rectangle             |     2816 |        1240 |
| LINES       | Draw one line                  |     1342 |        1354 |
| CIRCLES     | Draw one circle                |     1320 |        1080 |
| TRIANGLES   | Draw one triangle              |      397 |         343 |
| TEXT        | Draw one string                |     1283 |         624 |
| TRANSPARENT | Draw one transparent string    |     1071 |        1061 |
| IMAGE       | Draw one image                 |     67.9 |        31.8 |

`ST7789HAL_Init()` completed in 263 ms with DMA and in 280 ms without DMA.
The library recorded no error bits in the two tests.

The DMA makes the large transfers approximately two times faster. For small
transfers, the DMA does not change the speed.

## Files

| File                    | Contents                                                                            |
|-------------------------|-------------------------------------------------------------------------------------|
| `st7789_hal.h`          | The API                                                                             |
| `st7789_hal_config.h`   | The configuration macros and their defaults                                         |
| `st7789_hal.c`          | The transfers, `ST7789HAL_Init()`, the control functions, the window, fill, images |
| `st7789_hal_gfx.c`      | The lines, the shapes and the text                                                  |
| `st7789_hal_internal.h` | The internal functions that the two .c files use. The application must not use them. |
| `st7789_hal_font5x7.c/.h` | The font                                                                          |
| `CMakeLists.txt`        | The CMake target `st7789_hal`                                                       |
| `test/`                 | The tests for a PC (refer to "Tests")                                               |

In the application, include only `st7789_hal.h`.

## STM32CubeMX configuration

### SPI

Do these steps in STM32CubeMX (or in the `.ioc` file in STM32CubeIDE):

1. Select the SPI that connects to the panel, for example SPI1.
2. Set "Mode" to "Transmit Only Master". "Full-Duplex Master" also
   operates. The library does not use the MISO pin.
3. Set "Hardware NSS Signal" to "Disable". The library sets the CS pin with
   `HAL_GPIO_WritePin()`.
4. In the "Parameter Settings" tab, set these values:
   - "Data Size": "8 Bits"
   - "First Bit": "MSB First"
   - "Clock Polarity (CPOL)": "Low"
   - "Clock Phase (CPHA)": "1 Edge"
5. Set the prescaler ("Prescaler (for Baud Rate)") for an SCK that the
   panel and the wires can operate at.

These values give SPI mode 0. If the data size is not 8 bits,
`ST7789HAL_Init()` returns `ST7789HAL_ERR_PARAM`. The library does not
examine the other values.

**SCK**: STM32CubeMX shows the SCK ("Baud Rate") below the prescaler. The
ST7789VW datasheet gives a minimum SCK cycle of 16 ns (62.5 MHz) when the
MCU writes data to the controller. Also, the datasheet of your MCU gives a
maximum SCK.

Long wires decrease the SCK at which the image is correct. For a first
test, we recommend an SCK of approximately 10 MHz. Then increase it.

### Pins

Set each pin of the panel that the MCU controls as "GPIO_Output". Give it a
label, for example `LCD_DC`. Then STM32CubeMX puts the macros `LCD_DC_Pin`
and `LCD_DC_GPIO_Port` in `main.h`.

| Pin of the panel | Necessary | If the MCU does not control it                                            |
|------------------|-----------|----------------------------------------------------------------------------|
| DC               | Yes       | Not possible                                                               |
| CS               | No        | Connect it to GND. Then the SPI bus must not have other devices.           |
| RST (or RES)     | No        | Connect it to 3.3 V. The library sends the SWRESET command.                |
| BL (or BLK)      | No        | The backlight stays on.                                                    |

`ST7789HAL_Init()` sets the levels of these pins. Thus, the initial output
level in STM32CubeMX is not important. If the backlight is on when the pin
is low, set `bl_active_low` to 1.

### DMA (optional)

The DMA is optional. Without the DMA, the library uses only
`HAL_SPI_Transmit()`. To use the DMA, do these steps:

1. In the "DMA Settings" tab of the SPI, add the DMA request "SPIx_TX" (for
   example "SPI1_TX").
2. Set "Mode" to "Normal".
3. Set "Data Width" to "Byte" for "Peripheral" and for "Memory".
4. Make sure that "Increment Address" has a check mark only for "Memory".
5. In the "NVIC Settings" tab, make sure that STM32CubeMX enabled the TX DMA
   interrupt.
6. If the HAL of your family must have it, also enable the SPI global
   interrupt. Refer to the list that follows.
7. In `ST7789HAL_Config`, set `use_dma` to 1.

The library waits until `HAL_SPI_GetState()` returns `HAL_SPI_STATE_READY`.
An interrupt handler of the HAL sets this condition at the end of the DMA
transfer. The interrupt handler that does this is different for each family:

- **STM32H7** (HAL in `Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_spi.c`
  of this repository): The handler of the TX DMA interrupt only enables the
  EOT interrupt of the SPI. Then `HAL_SPI_IRQHandler()` sets the condition.
  Thus, enable the TX DMA interrupt and the SPI global interrupt.
- **STM32L4** (STM32Cube_FW_L4 V1.18.2) and **STM32F7** (STM32Cube_FW_F7
  V1.17.4): The handler of the TX DMA interrupt waits until the SPI sent the
  last bit. Then it sets the condition. Thus, only the TX DMA interrupt is
  necessary.
- **Other families**: Refer to the HAL of your family. In
  `stm32xxxx_hal_spi.c`, find where `SPI_DMATransmitCplt()` or
  `HAL_SPI_IRQHandler()` sets `hspi->State` to `HAL_SPI_STATE_READY`.

If you do not enable the necessary interrupt, each DMA transfer causes a
timeout. Then `ST7789HAL_Init()` returns `ST7789HAL_ERR_TIMEOUT`.

Make sure that `main()` calls `MX_DMA_Init()` before `MX_SPIx_Init()`.
`HAL_SPI_MspInit()` prepares the DMA stream (or DMA channel), and the clock
of the DMA must operate at that time.

For the RAM that the DMA can read and for the data cache, refer to "DMA".

## Add the library to a project

Make a copy of the folder `st7789_hal` and its contents in your project, for
example in `Lib/st7789_hal`.

### With CMake

STM32CubeMX makes a CMake project if you select "CMake" in "Toolchain /
IDE". In the `CMakeLists.txt` of the project, add the folder after the line
`add_subdirectory(cmake/stm32cubemx)`. Then link the library into the
executable:

```cmake
add_subdirectory(Lib/st7789_hal)
target_link_libraries(${CMAKE_PROJECT_NAME} st7789_hal)
```

- The target `st7789_hal` makes the folder an include path. It also tells
  CMake to compile the library with `-O2`, also in the Debug configuration.
- The library includes the header file that `ST7789HAL_HAL_HEADER` gives.
  The default is `"main.h"`. The `main.h` of STM32CubeMX includes the HAL
  header file of the family.
- If the target `stm32cubemx` is available, the library target links it.
  The target `stm32cubemx` gives the include paths (for example `Core/Inc`)
  and the compile definitions (for example `USE_HAL_DRIVER` and
  `STM32F411xE`). Thus, add the library after the target `stm32cubemx`.
- For a project without the target `stm32cubemx`, give these include paths
  and compile definitions to the target `st7789_hal`:

  ```cmake
  target_include_directories(st7789_hal PRIVATE Core/Inc
      Drivers/STM32F4xx_HAL_Driver/Inc Drivers/CMSIS/Device/ST/STM32F4xx/Include
      Drivers/CMSIS/Include)
  target_compile_definitions(st7789_hal PRIVATE USE_HAL_DRIVER STM32F411xE)
  ```

### With STM32CubeIDE, without CMake

1. Copy the folder into the project, for example into `Drivers/st7789_hal`.
   Do not copy the `test` folder.
2. If the folder is not in a source folder of the project, add it. Use
   "Project > Properties > C/C++ General > Paths and Symbols > Source
   Location".
3. Add the folder to the include paths. Use the "Includes" tab of "Paths and
   Symbols" for "GNU C".
4. Set the configuration macros, if necessary, in the "Symbols" tab.

STM32CubeIDE then compiles `st7789_hal.c`, `st7789_hal_gfx.c` and
`st7789_hal_font5x7.c`. The inner loops of the library operate one time for
each pixel. Thus, we recommend `-O2` for these files, also in the Debug
configuration. The library includes `main.h`, and STM32CubeIDE has
`Core/Inc` in the include paths.

## How to use the library

### Example

This example is for `main.c` from STM32CubeMX. SPI1 connects to a 240x320
panel. The pins have the labels `LCD_DC`, `LCD_CS`, `LCD_RST` and `LCD_BL`.

```c
/* USER CODE BEGIN Includes */
#include "st7789_hal.h"
/* USER CODE END Includes */

/* USER CODE BEGIN PV */
static ST7789HAL_Handle lcd;
/* USER CODE END PV */

int main(void)
{
  /* ... HAL_Init(), SystemClock_Config(), MX_GPIO_Init(), MX_SPI1_Init() ... */

  /* USER CODE BEGIN 2 */
  const ST7789HAL_Config cfg = {
    .hspi     = &hspi1,
    .dc_port  = LCD_DC_GPIO_Port,  .dc_pin  = LCD_DC_Pin,
    .cs_port  = LCD_CS_GPIO_Port,  .cs_pin  = LCD_CS_Pin,
    .rst_port = LCD_RST_GPIO_Port, .rst_pin = LCD_RST_Pin,
    .bl_port  = LCD_BL_GPIO_Port,  .bl_pin  = LCD_BL_Pin,
    .width    = 240, .height = 320,
    .invert_colors = 1,                       /* for most IPS panels */
    .use_dma  = 0,                            /* 1 if SPI1 has a TX DMA stream */
  };
  if (ST7789HAL_Init(&lcd, &cfg) != ST7789HAL_OK)
  {
    Error_Handler();
  }
  ST7789HAL_FillScreen(&lcd, ST7789HAL_BLUE);
  ST7789HAL_DrawString(&lcd, 10, 10, "Hello", ST7789HAL_WHITE, ST7789HAL_BLUE, 2);
  ST7789HAL_FillCircle(&lcd, 120, 160, 40, ST7789HAL_RED);
  ST7789HAL_DrawLine(&lcd, 0, 319, 239, 0, ST7789HAL_YELLOW);
  /* USER CODE END 2 */

  while (1)
  {
  }
}
```

- `ST7789HAL_Init()` copies the configuration into the handle. Thus, `cfg`
  can be a local variable. The handle must stay in memory while you use the
  panel.
- The fields that the example does not set are 0. For a pin, `NULL` (0)
  tells the library that the MCU does not control that pin.
- `ST7789HAL_Init()` does a reset of the controller and fills the screen
  with black. Then it sets the backlight to on. The time for this function
  is approximately 250 ms.

### Panel dimensions and offsets

The controller memory has 240 x 320 pixels. A smaller panel shows only a
part of it. `x_offset` and `y_offset` give the position of the panel in the
controller memory, in portrait. These values are different for each display
module. The table that follows gives usual values:

| Panel                | `width` | `height` | `x_offset` | `y_offset` |
|----------------------|---------|----------|------------|------------|
| 240x320 (2", 2.4")   | 240     | 320      | 0          | 0          |
| 240x240 (1.3", 1.54")| 240     | 240      | 0          | 0          |
| 135x240 (1.14")      | 135     | 240      | 52         | 40         |

Set `width` and `height` for portrait, also if you use the panel in
landscape. The library calculates the offsets for each rotation. For
example, a 240x240 panel with the offsets 0 and 0 uses the offset 80 in
`ST7789HAL_PORTRAIT_FLIPPED` and `ST7789HAL_LANDSCAPE_FLIPPED`.

A 135x240 panel in landscape:

```c
const ST7789HAL_Config cfg = {
  .hspi    = &hspi1,
  .dc_port = LCD_DC_GPIO_Port, .dc_pin = LCD_DC_Pin,
  .cs_port = LCD_CS_GPIO_Port, .cs_pin = LCD_CS_Pin,
  .width   = 135, .height   = 240,
  .x_offset = 52, .y_offset = 40,
  .invert_colors = 1,
};
ST7789HAL_Init(&lcd, &cfg);
ST7789HAL_SetRotation(&lcd, ST7789HAL_LANDSCAPE);   /* the screen is 240 x 135 */
```

A 240x240 panel without CS and RST pins:

```c
const ST7789HAL_Config cfg = {
  .hspi    = &hspi2,
  .dc_port = LCD_DC_GPIO_Port, .dc_pin = LCD_DC_Pin,
  .bl_port = LCD_BL_GPIO_Port, .bl_pin = LCD_BL_Pin,
  .width   = 240, .height = 240,
  .invert_colors = 1,
};
```

If the image is not correct, examine these items:

- The image is in an incorrect position, or one edge shows random pixels:
  the offsets are incorrect.
- Red shows as blue, and blue shows as red: set `bgr_order` to 1.
- Each color shows as its opposite color: change `invert_colors`.
- The screen shows no image: examine the wires, SPI mode 0 and the DC pin.
  Then decrease SCK.

### Text

Each character uses a rectangle of (6 * `size`) x (8 * `size`) pixels.
(`x`, `y`) is the top left corner of the first character. In
`ST7789HAL_DrawString()`, `'\n'` starts a new row of text at `x`. The
function ignores `'\r'`, and it stops at the bottom of the screen.

- After `ST7789HAL_SetTextWrap(&lcd, 0)`, the library clips the text at the
  right edge of the screen. The default is 1: the text continues on the
  next row.
- After `ST7789HAL_SetTextTransparent(&lcd, 1)`, the library draws only the
  pixels of the characters. The other pixels do not change. The default is
  0: the library also fills the rectangle of each character with the
  background color.

### Send pixels directly (for example from LVGL)

`ST7789HAL_SetWindow()` and `ST7789HAL_WritePixels()` send pixels that the
application calculates. The header file gives these instructions for the
window:

- First, set a window. Then send its pixels in one or more calls to
  `ST7789HAL_WritePixels()`, row by row, from left to right.
- The window must be fully on the screen. If it is not,
  `ST7789HAL_SetWindow()` has no effect. Then `ST7789HAL_WritePixels()` has
  no effect until you set a correct window.
- The first `ST7789HAL_WritePixels()` call after `ST7789HAL_SetWindow()`
  sends RAMWR. Each subsequent call sends RAMWRC (memory write continue).
  Thus, CS can go high between the calls.
- A call to a different function that sends a command ends the window. An
  example is a drawing function. After this call, `ST7789HAL_WritePixels()`
  has no effect until you set a new window.

The library does not count the pixels. Send `w * h` pixels for each window.

The example that follows shows the LVGL callback that sends an area to the
controller (`flush_cb`, LVGL v9). LVGL must use RGB565 in the byte sequence
of the CPU (`LV_COLOR_FORMAT_RGB565`). Do not use the format with the bytes
interchanged. Give `lv_display_create()` the width and the height that
`ST7789HAL_Width()` and `ST7789HAL_Height()` return.

```c
static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px)
{
  uint16_t w = (uint16_t)lv_area_get_width(area);
  uint16_t h = (uint16_t)lv_area_get_height(area);

  ST7789HAL_SetWindow(&lcd, (uint16_t)area->x1, (uint16_t)area->y1, w, h);
  ST7789HAL_WritePixels(&lcd, (const uint16_t *)px, (uint32_t)w * h);
  lv_display_flush_ready(disp);      /* the library sent all the pixels */
}

lv_display_set_flush_cb(disp, flush_cb);
```

`ST7789HAL_WritePixels()` returns after the SPI sent all the pixels. Thus,
`flush_cb` can call `lv_display_flush_ready()` immediately. The DMA reads
only the transmit buffer of the library. Thus, the LVGL buffers can be in
each RAM area.

### Two panels

Each panel has a handle and a configuration. Two panels can use one SPI.
Then each panel must have a CS pin that the MCU controls.

```c
static ST7789HAL_Handle left, right;

ST7789HAL_Init(&left,  &left_cfg);     /* .hspi = &hspi1, .cs_pin = LCD1_CS_Pin, ... */
ST7789HAL_Init(&right, &right_cfg);    /* .hspi = &hspi1, .cs_pin = LCD2_CS_Pin, ... */
ST7789HAL_FillScreen(&left,  ST7789HAL_RED);
ST7789HAL_FillScreen(&right, ST7789HAL_GREEN);
```

All the handles use one transmit buffer. Refer to "Limits".

## API

All the functions have the handle as the first parameter. If the handle is
`NULL`, or if `ST7789HAL_Init()` did not complete correctly, the other
functions have no effect. Thus, `ST7789HAL_Width()` and `ST7789HAL_Height()` return 0,
and `ST7789HAL_GetRotation()` returns `ST7789HAL_PORTRAIT`. But
`ST7789HAL_GetErrors()` and `ST7789HAL_ClearErrors()` operate if the handle
is not `NULL`.

### Types

| Type                 | Contents                                                                                 |
|----------------------|------------------------------------------------------------------------------------------|
| `ST7789HAL_Config`   | The configuration of one panel. Refer to the table that follows.                         |
| `ST7789HAL_Handle`   | The data of one panel. The application supplies the memory. Do not change the fields.   |
| `ST7789HAL_Status`   | `ST7789HAL_OK`, `ST7789HAL_ERR_PARAM`, `ST7789HAL_ERR_SPI`, `ST7789HAL_ERR_TIMEOUT`        |
| `ST7789HAL_Rotation` | `ST7789HAL_PORTRAIT`, `ST7789HAL_LANDSCAPE` (90 degrees), `ST7789HAL_PORTRAIT_FLIPPED` (180 degrees), `ST7789HAL_LANDSCAPE_FLIPPED` (270 degrees) |

| Field of `ST7789HAL_Config` | Value                                                                                 |
|-----------------------------|---------------------------------------------------------------------------------------|
| `hspi`                      | The SPI handle from STM32CubeMX, for example `&hspi1`. Necessary.                     |
| `dc_port`, `dc_pin`         | The DC pin. Necessary.                                                                |
| `cs_port`, `cs_pin`         | The CS pin. `NULL`: the CS pin of the panel connects to GND.                          |
| `rst_port`, `rst_pin`       | The RST pin. `NULL`: the library sends SWRESET.                                       |
| `bl_port`, `bl_pin`         | The backlight pin. `NULL`: no backlight pin.                                          |
| `bl_active_low`             | 1: the backlight is on when the pin is low.                                           |
| `width`, `height`           | The panel dimensions in portrait: 1 to 240, 1 to 320.                                 |
| `x_offset`, `y_offset`      | The position of the panel in the controller memory, in portrait. `width + x_offset` must be 240 or less, and `height + y_offset` must be 320 or less. |
| `invert_colors`             | 1 for most IPS panels.                                                                |
| `bgr_order`                 | 1 if the panel interchanges red and blue.                                             |
| `use_dma`                   | 1: the library uses `HAL_SPI_Transmit_DMA()` for long transfers. `hspi->hdmatx` must not be `NULL`. |

### Control functions

| Function                                         | Operation                                                                                              |
|--------------------------------------------------|--------------------------------------------------------------------------------------------------------|
| `ST7789HAL_Init(h, config)`                      | Examines the configuration, does a reset of the controller and prepares it. Fills the screen with black. Sets the backlight to on. Returns a `ST7789HAL_Status`. |
| `ST7789HAL_SetRotation(h, rotation)`             | Sets the rotation. The pixels on the screen do not change.                                             |
| `ST7789HAL_GetRotation(h)`                       | Returns the rotation.                                                                                  |
| `ST7789HAL_Width(h)`, `ST7789HAL_Height(h)`      | Return the width and the height of the screen for the rotation at this time.                           |
| `ST7789HAL_Backlight(h, on)`                     | 1: sets the backlight to on. 0: sets it to off.                                                        |
| `ST7789HAL_DisplayOn(h, on)`                     | 0: the screen shows no image (DISPOFF), and the controller memory keeps the pixels. 1: the screen shows the pixels again (DISPON). |
| `ST7789HAL_Sleep(h, sleep)`                      | 1: starts sleep mode (SLPIN). 0: stops sleep mode (SLPOUT). Each call waits 120 ms.                    |
| `ST7789HAL_InvertColors(h, invert)`              | 1: shows each color as its opposite color. 0: shows the correct colors, with each value of `invert_colors`. |
| `ST7789HAL_GetErrors(h)`                         | Returns the error bits.                                                                                |
| `ST7789HAL_ClearErrors(h)`                       | Sets the error bits to 0.                                                                              |

### Window functions

| Function                                         | Operation                                                                                              |
|--------------------------------------------------|--------------------------------------------------------------------------------------------------------|
| `ST7789HAL_SetWindow(h, x, y, w, h_px)`          | Sets the window for `ST7789HAL_WritePixels()`. Refer to "Send pixels directly".                        |
| `ST7789HAL_WritePixels(h, pixels, count)`        | Sends `count` RGB565 pixels to the window.                                                             |

### Drawing functions

The coordinates are `int16_t` values and can be less than 0. A width or a
height of 0 or less gives no pixels. The colors are RGB565 values.

| Function                                                   | Operation                                                                    |
|------------------------------------------------------------|------------------------------------------------------------------------------|
| `ST7789HAL_FillScreen(h, color)`                           | Fills the screen with one color.                                             |
| `ST7789HAL_FillRect(h, x, y, w, h_px, color)`              | Fills a rectangle.                                                           |
| `ST7789HAL_DrawPixel(h, x, y, color)`                      | Draws one pixel.                                                             |
| `ST7789HAL_DrawHLine(h, x, y, w, color)`                   | Draws a horizontal line of `w` pixels.                                       |
| `ST7789HAL_DrawVLine(h, x, y, h_px, color)`                | Draws a vertical line of `h_px` pixels.                                      |
| `ST7789HAL_DrawLine(h, x0, y0, x1, y1, color)`             | Draws a line from (`x0`, `y0`) to (`x1`, `y1`).                              |
| `ST7789HAL_DrawRect(h, x, y, w, h_px, color)`              | Draws the contour of a rectangle, 1 pixel wide.                              |
| `ST7789HAL_DrawCircle(h, cx, cy, r, color)`                | Draws the contour of a circle with its middle at (`cx`, `cy`). `r` = 0 gives 1 pixel. |
| `ST7789HAL_FillCircle(h, cx, cy, r, color)`                | Draws a filled circle.                                                       |
| `ST7789HAL_DrawTriangle(h, x0, y0, x1, y1, x2, y2, color)` | Draws the contour of a triangle.                                             |
| `ST7789HAL_FillTriangle(h, x0, y0, x1, y1, x2, y2, color)` | Draws a filled triangle.                                                     |
| `ST7789HAL_DrawImage(h, x, y, w, h_px, pixels)`            | Draws an image of `w` x `h_px` RGB565 pixels, row by row, in the byte sequence of the CPU. |

A filled shape includes all the pixels of its contour and of its inner area,
and no other pixels.

### Text functions

| Function                                              | Operation                                                                           |
|-------------------------------------------------------|-------------------------------------------------------------------------------------|
| `ST7789HAL_DrawChar(h, x, y, c, fg, bg, size)`        | Draws one character.                                                                |
| `ST7789HAL_DrawString(h, x, y, str, fg, bg, size)`    | Draws a string. Refer to "Text".                                                    |
| `ST7789HAL_SetTextWrap(h, wrap)`                      | 1 (default): the text continues on the next row. 0: the library clips the text at the right edge. |
| `ST7789HAL_SetTextTransparent(h, transparent)`        | 1: the library draws only the pixels of the characters. 0 (default): it also fills the background. |

A `size` of 0 gives 1. A `size` of more than `ST7789HAL_MAX_TEXT_SIZE` gives
`ST7789HAL_MAX_TEXT_SIZE`. The font has the characters from ' ' to '~'
(0x20 to 0x7E). The library draws other characters as '?'.

### Macros

| Macro                                             | Value                                                                    |
|---------------------------------------------------|--------------------------------------------------------------------------|
| `ST7789HAL_RGB565(r, g, b)`                       | Makes an RGB565 color from 8-bit red, green and blue values.             |
| `ST7789HAL_BLACK`, `ST7789HAL_WHITE`              | 0x0000, 0xFFFF                                                           |
| `ST7789HAL_RED`, `ST7789HAL_GREEN`, `ST7789HAL_BLUE` | 0xF800, 0x07E0, 0x001F                                                |
| `ST7789HAL_YELLOW`, `ST7789HAL_CYAN`, `ST7789HAL_MAGENTA` | 0xFFE0, 0x07FF, 0xF81F                                           |
| `ST7789HAL_CHAR_W`, `ST7789HAL_CHAR_H`            | 6 and 8: the width and the height of the rectangle of one character if `size` is 1. A character is 5 x 7 pixels. |
| `ST7789HAL_ERROR_SPI`                             | 0x01: error bit. A HAL SPI call did not return `HAL_OK`.                 |
| `ST7789HAL_ERROR_TIMEOUT`                         | 0x02: error bit. A DMA transfer did not complete.                        |

## Configuration macros

| Macro                      | Default                        | Value                                                                                   |
|----------------------------|--------------------------------|-----------------------------------------------------------------------------------------|
| `ST7789HAL_CONFIG_FILE`    | (none)                         | A header file of the application that `st7789_hal_config.h` includes first.            |
| `ST7789HAL_HAL_HEADER`     | `"main.h"`                     | The header file with the HAL functions and types.                                       |
| `ST7789HAL_BUFFER_PIXELS`  | 512                            | The number of pixels in the transmit buffer (2 bytes for each pixel). 16 to 32767, and the value divided by 16 must be an integer. |
| `ST7789HAL_BUFFER_ATTR`    | `__attribute__((aligned(32)))` with GCC | The attributes of the transmit buffer. Refer to "DMA".                         |
| `ST7789HAL_DMA_MIN_BYTES`  | 64                             | With `use_dma` = 1, the minimum number of bytes for a DMA transfer.                    |
| `ST7789HAL_SPI_TIMEOUT_MS` | 1000                           | The maximum time in milliseconds for one SPI transfer.                                  |
| `ST7789HAL_MAX_TEXT_SIZE`  | 8                              | The maximum value of the `size` parameter of the text functions.                        |

A larger transmit buffer gives a smaller number of HAL calls. Thus, the
drawing functions are faster, but the library uses more RAM.

The library and the application must use the same values. To set the
macros, use one of these methods:

- With CMake, set them as `PUBLIC` compile definitions on the target
  `st7789_hal`:

  ```cmake
  target_compile_definitions(st7789_hal PUBLIC ST7789HAL_BUFFER_PIXELS=1024)
  ```

- Put the values in a header file of the application. Then set
  `ST7789HAL_CONFIG_FILE` to the name of this file:

  ```cmake
  target_compile_definitions(st7789_hal PUBLIC
      ST7789HAL_CONFIG_FILE="my_display_config.h")
  ```

  ```c
  /* my_display_config.h */
  #define ST7789HAL_BUFFER_PIXELS  1024
  #define ST7789HAL_HAL_HEADER     "stm32f4xx_hal.h"
  ```

  The compiler must find this file in an include path of the target
  `st7789_hal`.

## DMA

- **Speed**: With `HAL_SPI_Transmit()`, the CPU writes each byte to the SPI.
  At a high SCK, the CPU can be slower than the SPI. Then SCK stops between
  the bytes. The DMA can write the bytes more quickly. With the DMA, the
  call also waits until the end of the transfer.
- **Short transfers**: The library uses `HAL_SPI_Transmit_DMA()` only for
  data from the transmit buffer with `ST7789HAL_DMA_MIN_BYTES` (64) bytes or
  more. For a shorter transfer, the time to start the DMA is longer than the
  time of the transfer. Thus, the library sends commands, their parameters
  and short rows of pixels with `HAL_SPI_Transmit()`.
- **Memory of the application**: The library writes all the pixels into its
  transmit buffer. The DMA reads only this transmit buffer. Thus, images,
  strings and LVGL buffers can be in each memory. Examples are the flash
  memory and the stack.
- **RAM that the DMA can read**: The transmit buffer must be in a RAM area
  that the DMA can read. On the STM32H7, DMA1 and DMA2 cannot read the DTCM.
  In this repository, the linker script from STM32CubeMX puts `.bss` in
  DTCM. Thus, put the transmit buffer in a different section. Put this line
  in the file of `ST7789HAL_CONFIG_FILE`:

  ```c
  #define ST7789HAL_BUFFER_ATTR  __attribute__((section(".bss.st7789.dma"), aligned(32)))
  ```

  The linker script must put this section in a RAM area that the DMA can
  read, for example AXI SRAM. In this repository,
  `Core/Inc/st7789_hal_user_config.h` contains this line, and
  `STM32H753xx_FLASH.ld` puts `.bss.st7789.dma` in AXI SRAM. The library
  writes the transmit buffer before each transfer. Thus, the initial values
  in the transmit buffer are not important. On other families, refer to the
  manual of your MCU for the RAM areas that the DMA can read.
- **Data cache**: Some MCUs have a data cache (`__DCACHE_PRESENT` is 1), for
  example the STM32F7 and the STM32H7. If the data cache is on, the library
  cleans the data cache for the transmit buffer before each DMA transfer.
  Keep `aligned(32)` in `ST7789HAL_BUFFER_ATTR`. Then the transmit buffer starts
  at a cache line. The library does not clean the data cache for other data.
- **Timeout**: The library waits for the end of each DMA transfer for a
  maximum of `ST7789HAL_SPI_TIMEOUT_MS` (1000 ms). After this time, the
  library calls `HAL_SPI_Abort()` and sets `ST7789HAL_ERROR_TIMEOUT`. If the
  DMA transfer completes with an error code in `hspi->ErrorCode`, the
  library sets `ST7789HAL_ERROR_SPI`.

## Errors

`ST7789HAL_Init()` returns one of these values:

| Value                   | Cause                                                                                         |
|-------------------------|-----------------------------------------------------------------------------------------------|
| `ST7789HAL_OK`          | The library can operate.                                                                       |
| `ST7789HAL_ERR_PARAM`   | A parameter is not correct. Examples are an SPI without 8-bit data, a `NULL` DC pin, or `use_dma` = 1 without `hspi->hdmatx`. The function does not use the SPI or the pins. |
| `ST7789HAL_ERR_SPI`     | A HAL SPI call did not return `HAL_OK`.                                                       |
| `ST7789HAL_ERR_TIMEOUT` | A DMA transfer did not complete. If the two errors occurred, the function returns this value. |

The other functions do not return an error. They set error bits in the
handle. `ST7789HAL_GetErrors()` returns these bits:

- `ST7789HAL_ERROR_SPI`: A HAL SPI call did not return `HAL_OK`. This bit
  also shows a timeout of `HAL_SPI_Transmit()`.
- `ST7789HAL_ERROR_TIMEOUT`: A DMA transfer did not complete in
  `ST7789HAL_SPI_TIMEOUT_MS`.

An error bit stays set until a call to `ST7789HAL_ClearErrors()` or
`ST7789HAL_Init()`. Thus, you can examine the error bits after many calls.

After an error, the function does not send the remaining data of that
operation, and it sets CS high. The next call starts its transfers again.
The controller possibly did not receive some pixels or commands. To show the
correct image, draw the screen again. If the controller does not show a
correct image, call `ST7789HAL_Init()` again.

If `ST7789HAL_Init()` does not return `ST7789HAL_OK`, the other functions
have no effect until a call to `ST7789HAL_Init()` returns `ST7789HAL_OK`.

## Limits

- **Each call waits.** Each function returns after the SPI sent all the data
  of the call. With the DMA, the CPU waits in a wait loop until the end of
  each DMA transfer.
- **One transmit buffer for all the handles.** Thus, a call must not start
  before the end of the previous call. This condition is also applicable to
  different handles:
  - Call the library from only one context, for example the loop in
    `main()` or one RTOS task.
  - If two or more RTOS tasks call the library, use one mutex for all the
    calls to all the handles.
  - Do not call the library from an interrupt handler.
- **Interrupts**: `HAL_Delay()` and `HAL_GetTick()` use the SysTick
  interrupt. The DMA uses the interrupts of the HAL. Thus, do not disable the
  interrupts while you call the library. It must be possible for these
  interrupts to preempt the context of the calls.
- **No framebuffer mode.** Each drawing function sends its pixels
  immediately. For each rectangle, the library sends CASET, RASET and RAMWR
  in three CS cycles. Thus, many very small shapes are slow.
- **The library only writes.** It does not read pixels or registers from the
  controller. It does not use the MISO pin.
- **Panel dimensions**: A maximum of 240 x 320 pixels (the dimensions of the
  controller memory).
- **SPI**: Only 8-bit SPI frames on a 4-wire SPI bus with a DC pin. The
  library does not operate with a 3-wire SPI bus (9-bit SPI frames).
- **Colors**: Only RGB565 (COLMOD = 0x55).
- **No scrolling and no TE pin.** The library has no functions for the
  scrolling or for the refresh rate. It does not use the TE (tearing
  effect) signal of the controller.
- **Times**: The time for `ST7789HAL_Init()` is approximately 250 ms.
  `ST7789HAL_Sleep()` waits 120 ms after the command.
- **Font**: Only the 5x7 font with the ASCII characters from ' ' to '~'.

## Firmware of this repository

In this repository, `LCD_LIBRARY` tells CMake which library the firmware
uses. With `-DLCD_LIBRARY=st7789_hal`, the firmware uses this library and
the application in `Core/Src/hal_demo.c`. The configuration macros of the
library are in `Core/Inc/st7789_hal_user_config.h`.

```
cmake --preset Debug -DLCD_LIBRARY=st7789_hal
cmake --build build/Debug
```

## Tests

The folder `test/` has tests for a PC. They use code that simulates the
controller. To do the tests, use this command in the folder of the
repository:

```
bash Lib/st7789_hal/test/run_tests.sh
```

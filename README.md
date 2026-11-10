# ST7789V Display Driver for STM32

This repository contains two C libraries for ST7789 and ST7789V display
controllers on a 4-wire SPI bus. It also contains an example project for
the NUCLEO-H753ZI board.

| Library                            | Use it for                                  |
|------------------------------------|---------------------------------------------|
| [`Lib/st7789`](Lib/st7789)         | The maximum speed on an STM32H7             |
| [`Lib/st7789_hal`](Lib/st7789_hal) | All the STM32 families that have the HAL    |

## Contents

- [Properties](#properties)
- [Which library to use](#which-library-to-use)
- [Speed](#speed)
- [Hardware](#hardware)
- [How to compile the example project](#how-to-compile-the-example-project)
- [How to add a library to your project](#how-to-add-a-library-to-your-project)
- [Tests](#tests)
- [Structure of the repository](#structure-of-the-repository)
- [Simplified Technical English](#simplified-technical-english)
- [License](#license)

## Properties

The two libraries have these properties:

- They draw pixels, lines, rectangles, circles, triangles, RGB565 images
  and 5x7 text.
- They clip each shape at the edges of the screen.
- They have four rotations, sleep mode, color inversion and a backlight
  function.
- They are applicable to panels of 240x320, 240x240 and 135x240 pixels.
- They can use the DMA for long transfers.
- Each library folder is a CMake target.
- C++ code can include the header files.

## Which library to use

| Property                | `Lib/st7789`                                    | `Lib/st7789_hal`                              |
|-------------------------|-------------------------------------------------|-----------------------------------------------|
| MCUs                    | STM32H7. For other MCUs, you must write a port. | All the STM32 families that have the HAL      |
| Hardware access         | The registers of SPI1 and DMA1 Stream 0         | Only HAL functions                            |
| Transfers               | Background transfers                            | Each function returns after its transfers end |
| Framebuffer mode        | Yes (150 KB of RAM)                             | No                                            |
| Two or more panels      | No                                              | Yes. Each panel has a handle.                 |
| Scrolling, refresh rate | Yes                                             | No                                            |
| Interrupt handlers      | In the port file                                | None. STM32CubeMX supplies them.              |
| Panel configuration     | Macros, before you compile the library          | A struct, while the application operates      |

Each library has a README file with the full instructions:

- [`Lib/st7789/README.md`](Lib/st7789/README.md)
- [`Lib/st7789_hal/README.md`](Lib/st7789_hal/README.md)

## Speed

One benchmark ([`Core/Src/lcd_bench.c`](Core/Src/lcd_bench.c)) does the same
operations with each library. The table shows the results on the
NUCLEO-H753ZI board at an SCK of 100 MHz. Each value is the number of
operations in one second.

| Scene       | One operation               | `st7789` | `st7789_hal` with DMA | `st7789_hal` without DMA |
|-------------|-----------------------------|---------:|----------------------:|-------------------------:|
| FILL        | Fill the full screen        |     81.4 |                  80.1 |                     34.2 |
| RECTS       | Fill one rectangle          |     3055 |                  2814 |                     1240 |
| LINES       | Draw one line               |     5154 |                  1335 |                     1348 |
| CIRCLES     | Draw one circle             |     3698 |                  1312 |                     1080 |
| TRIANGLES   | Draw one triangle           |     1064 |                   396 |                      343 |
| TEXT        | Draw one string             |     1618 |                  1283 |                      625 |
| TRANSPARENT | Draw one transparent string |     4169 |                  1068 |                     1060 |
| IMAGE       | Draw one full screen image  |     80.4 |                  66.9 |                     31.6 |

For large areas, the two libraries have almost the same speed, because the
SPI is the limit. For small shapes, `Lib/st7789` is approximately three to
four times faster.

100 MHz is more than the maximum SCK in the datasheet of the controller.
Use a lower SCK if the panel does not show a correct image.

## Hardware

The example project uses these parts:

- A NUCLEO-H753ZI board
- The 2" IPS display module from Waveshare (240x320 pixels, ST7789V)

Connect the display module to the board as this table shows:

| Display module | Board pin | Function        |
|----------------|-----------|-----------------|
| VCC            | 3V3       | Supply voltage  |
| GND            | GND       | Ground          |
| DIN            | PB5       | SPI1 MOSI       |
| CLK            | PA5       | SPI1 SCK        |
| CS             | PD14      | CS              |
| DC             | PD15      | DC              |
| RST            | PF3       | Reset           |
| BL             | PG12      | Backlight       |

Keep the wires short. Long wires can cause incorrect data at a high SCK.

## How to compile the example project

These tools are necessary:

- GCC for Arm (`arm-none-eabi-gcc`)
- CMake (3.22 minimum)
- Ninja
- STM32CubeProgrammer, to write the firmware to the board

STM32CubeCLT contains all these tools.

1. Get the repository:

   ```bash
   git clone https://github.com/OscarBazylewich/ST7789V_Display_Driver_for_STM32.git
   ```

2. Go to the folder of the repository:

   ```bash
   cd ST7789V_Display_Driver_for_STM32
   ```

3. Make the CMake configuration:

   ```bash
   cmake --preset Release
   ```

4. Compile the project:

   ```bash
   cmake --build --preset Release
   ```

5. Connect the USB connection of the ST-LINK to the PC.

6. Write the firmware to the board:

   ```bash
   STM32_Programmer_CLI -c port=SWD -w build/Release/Nucleo-H753DisplayTest.elf -v -rst
   ```

The firmware sends its results to the ST-LINK virtual COM port. The speed
is 115200 bits in each second, and the format is 8N1.

### CMake values

You can add these values to the command of step 3, for example
`cmake --preset Release -DLCD_LIBRARY=st7789_hal`.

| Value                      | Default | Effect                                                         |
|----------------------------|---------|----------------------------------------------------------------|
| `-DLCD_LIBRARY=st7789`     | Yes     | The firmware uses `Lib/st7789` and shows the stress test.      |
| `-DLCD_LIBRARY=st7789_hal` | No      | The firmware uses `Lib/st7789_hal` and shows its scenes.       |
| `-DHAL_DEMO_USE_DMA=OFF`   | `ON`    | `Lib/st7789_hal` does not use the DMA.                         |
| `-DLCD_BENCH=ON`           | `OFF`   | The firmware does the benchmark with the library that you set. |

## How to add a library to your project

### `Lib/st7789_hal` (all STM32 families)

1. Copy the folder `Lib/st7789_hal` into your project.
2. Add the `.c` files of the folder to the source files of your project.
   With CMake, use
   `add_subdirectory()` and the target `st7789_hal`.
3. In STM32CubeMX, set the SPI to 8 data bits and SPI mode 0.
4. Call the library from your application:

```c
#include "st7789_hal.h"

extern SPI_HandleTypeDef hspi1;
static ST7789HAL_Handle lcd;

void App_Start(void)
{
  ST7789HAL_Config config = {
    .hspi = &hspi1,
    .dc_port = LCD_DC_GPIO_Port,   .dc_pin = LCD_DC_Pin,
    .cs_port = LCD_CS_GPIO_Port,   .cs_pin = LCD_CS_Pin,
    .rst_port = LCD_RST_GPIO_Port, .rst_pin = LCD_RST_Pin,
    .bl_port = LCD_BL_GPIO_Port,   .bl_pin = LCD_BL_Pin,
    .width = 240, .height = 320,
    .invert_colors = 1,
  };

  if (ST7789HAL_Init(&lcd, &config) == ST7789HAL_OK)
  {
    ST7789HAL_FillScreen(&lcd, ST7789HAL_BLUE);
    ST7789HAL_DrawString(&lcd, 10, 10, "Hello", ST7789HAL_WHITE,
                         ST7789HAL_BLUE, 2);
  }
}
```

For the DMA and the other functions, refer to
[`Lib/st7789_hal/README.md`](Lib/st7789_hal/README.md).

### `Lib/st7789` (STM32H7)

1. Copy the folder `Lib/st7789` into your project.
2. Add the `.c` files and the port file to the source files of your
   project. With CMake, use
   `add_subdirectory()` and the target `st7789`.
3. Make sure that your project agrees with the necessary conditions for the
   port. The README file of the library gives them.
4. Call the library from your application:

```c
#include "st7789.h"

void App_Start(void)
{
  ST7789_Init();
  ST7789_FillScreen(ST7789_BLACK);
  ST7789_DrawString(10, 10, "Hello", ST7789_WHITE, ST7789_BLACK, 2);
}
```

For the framebuffer mode, the linker script and a port for a different MCU,
refer to [`Lib/st7789/README.md`](Lib/st7789/README.md).

## Tests

`Lib/st7789_hal` has a test for a PC. The test uses code that
simulates the controller. Thus, the hardware is not necessary. GCC for the
PC, CMake and Bash are necessary.

```bash
bash Lib/st7789_hal/test/run_tests.sh
```

The script does 17 tests in each of 6 configurations. The time is
approximately 100 seconds.

## Structure of the repository

| Path                         | Contents                                              |
|------------------------------|-------------------------------------------------------|
| `Lib/st7789/`                | The fast library and its STM32H7 port                 |
| `Lib/st7789_hal/`            | The HAL library and its test for a PC                 |
| `Core/`                      | The example application (the scenes and the benchmark) |
| `Drivers/`                   | The HAL and the CMSIS files from ST and Arm           |
| `cmake/`, `CMakeLists.txt`   | The CMake files                                       |
| `Nucleo-H753DisplayTest.ioc` | The STM32CubeMX project                               |
| `Docs/`                      | The glossary and the checker for STE                  |

## Simplified Technical English

The documents and the comments in the code use ASD-STE100 Simplified
Technical English (STE), issue 9. `Docs/ste-glossary.md` gives the
technical nouns and the technical verbs of this repository.

The STE specification is not in this repository, because ASD has the
copyright. You can get it from ASD. The checker
(`Docs/ste_check.py`) reads a text copy of the specification from
`Docs/ASD-STE100_ISSUE9.txt`.

## License

The libraries and the example application have the MIT license. Refer to
[`LICENSE`](LICENSE).

The files in `Drivers/` and the files that STM32CubeMX made have the
licenses of ST and Arm. Refer to the `LICENSE.txt` files in `Drivers/`.

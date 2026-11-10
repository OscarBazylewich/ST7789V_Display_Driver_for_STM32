# ST7789V Display Driver for STM32

This repository contains two C libraries that draw on a display with an
ST7789 or ST7789V controller. The display connects to an STM32 MCU through
a 4-wire SPI bus.

With each library, you can do these tasks:

- Draw pixels, lines, rectangles, circles and triangles.
- Draw text with a 5x7 font, in different dimensions.
- Draw RGB565 images.
- Turn the image in four rotations.
- Control the backlight, sleep mode and color inversion.

The libraries are applicable to panels of 240x320, 240x240 and 135x240
pixels.

## Contents

- [Select a library](#select-a-library)
- [How to start with the HAL library](#how-to-start-with-the-hal-library)
- [How to start with the fast library](#how-to-start-with-the-fast-library)
- [Example project for the NUCLEO-H753ZI](#example-project-for-the-nucleo-h753zi)
- [Speed](#speed)
- [Tests](#tests)
- [Folders](#folders)
- [License](#license)

## Select a library

If you are not sure, use the HAL library. It operates on all the STM32
families, and it is easy to add to a project from STM32CubeMX.

| Your project                                             | Library to use                                   |
|----------------------------------------------------------|--------------------------------------------------|
| An STM32F0, F1, F4, G0, G4, L4, U5 or a different family | HAL library ([`Lib/st7789_hal`](Lib/st7789_hal)) |
| An STM32H7, and the HAL library is sufficiently fast     | HAL library ([`Lib/st7789_hal`](Lib/st7789_hal)) |
| An STM32H7, and you must have the maximum speed          | Fast library ([`Lib/st7789`](Lib/st7789))        |
| Two or more panels on one MCU                            | HAL library ([`Lib/st7789_hal`](Lib/st7789_hal)) |
| A framebuffer, scrolling or a change of the refresh rate | Fast library ([`Lib/st7789`](Lib/st7789))        |

This table shows the differences between the two libraries:

| Property                             | HAL library (`Lib/st7789_hal`)                               | Fast library (`Lib/st7789`)                                            |
|--------------------------------------|--------------------------------------------------------------|------------------------------------------------------------------------|
| MCUs                                 | All the STM32 families that have the HAL                     | STM32H7 only (SPI1 and DMA1 Stream 0)                                  |
| Necessary changes in your project    | None. You give your SPI handle and your pins to the library. | The linker script and the interrupt handlers of SPI1 and DMA1 Stream 0 |
| Time when a drawing function returns | After the panel received all the pixels                      | Immediately. The DMA sends the pixels in the background.               |
| Number of panels for one MCU         | One or more                                                  | One                                                                    |
| Location of the panel dimensions     | A struct in your code                                        | Macros, before you compile                                             |
| DMA                                  | Optional                                                     | Necessary                                                              |
| Framebuffer mode                     | No                                                           | Yes (150 KB of RAM)                                                    |
| Scrolling and refresh rate functions | No                                                           | Yes                                                                    |
| Speed                                | Refer to [Speed](#speed)                                     | Refer to [Speed](#speed)                                               |

## How to start with the HAL library

### 1. Connect the display

Connect the pins of the display module to your board:

| Pin on the display module | Connect it to            | Is it necessary?                            |
|---------------------------|--------------------------|---------------------------------------------|
| VCC                       | 3.3 V                    | Yes                                         |
| GND                       | GND                      | Yes                                         |
| DIN (also MOSI or SDA)    | The MOSI pin of your SPI | Yes                                         |
| CLK (also SCK or SCL)     | The SCK pin of your SPI  | Yes                                         |
| DC                        | An output pin            | Yes                                         |
| CS                        | An output pin            | No. As an alternative, connect it to GND.   |
| RST                       | An output pin            | No. The library can send a reset command.   |
| BL                        | An output pin            | No. As an alternative, connect it to 3.3 V. |

### 2. Set the SPI in STM32CubeMX

| Parameter in STM32CubeMX | Value                                    |
|--------------------------|------------------------------------------|
| Mode                     | Transmit Only Master                     |
| Hardware NSS Signal      | Disable                                  |
| Data Size                | 8 Bits                                   |
| First Bit                | MSB First                                |
| Clock Polarity (CPOL)    | Low                                      |
| Clock Phase (CPHA)       | 1 Edge                                   |
| Prescaler                | Start with a low SCK, for example 10 MHz |

Set the DC, CS, RST and BL pins to "GPIO_Output".

### 3. Add the files

1. Copy the folder `Lib/st7789_hal` into your project.
2. Add these files to the source files of your project: `st7789_hal.c`,
   `st7789_hal_gfx.c` and `st7789_hal_font5x7.c`.
3. Add the folder to the include paths of your project.

With CMake, as an alternative to steps 2 and 3, add these lines to your
`CMakeLists.txt`:

```cmake
add_subdirectory(Lib/st7789_hal)
target_link_libraries(${CMAKE_PROJECT_NAME} st7789_hal)
```

### 4. Draw on the screen

Call this function from `main()`, after `MX_SPI1_Init()`:

```c
#include "main.h"
#include "st7789_hal.h"

extern SPI_HandleTypeDef hspi1;      /* the SPI from STM32CubeMX */
static ST7789HAL_Handle lcd;

void App_Start(void)
{
  ST7789HAL_Config config = {
    .hspi = &hspi1,
    .dc_port = GPIOD,  .dc_pin = GPIO_PIN_15,
    .cs_port = GPIOD,  .cs_pin = GPIO_PIN_14,
    .rst_port = GPIOF, .rst_pin = GPIO_PIN_3,
    .bl_port = GPIOG,  .bl_pin = GPIO_PIN_12,
    .width = 240, .height = 320,
    .invert_colors = 1,
  };

  if (ST7789HAL_Init(&lcd, &config) != ST7789HAL_OK)
  {
    return;                          /* the configuration is not correct */
  }

  ST7789HAL_FillScreen(&lcd, ST7789HAL_BLUE);
  ST7789HAL_DrawString(&lcd, 10, 10, "Hello", ST7789HAL_WHITE,
                       ST7789HAL_BLUE, 2);
  ST7789HAL_FillCircle(&lcd, 120, 160, 40, ST7789HAL_YELLOW);
}
```

Change these fields of `config` for your hardware:

| Field                  | Value to write                                                                                     |
|------------------------|----------------------------------------------------------------------------------------------------|
| `hspi`                 | The SPI handle that connects to the display                                                        |
| `dc_port`, `dc_pin`    | The pin that connects to DC                                                                        |
| `cs_port`, `cs_pin`    | The pin that connects to CS. Write `NULL` and 0 if CS connects to GND.                             |
| `rst_port`, `rst_pin`  | The pin that connects to RST. Write `NULL` and 0 if you have no reset pin.                         |
| `bl_port`, `bl_pin`    | The pin that connects to BL. Write `NULL` and 0 if you have no backlight pin.                      |
| `width`, `height`      | The dimensions of your panel in portrait, for example 240 and 320                                  |
| `x_offset`, `y_offset` | 0 for 240x320 panels. Smaller panels frequently have an offset, for example 52 and 40 for 135x240. |
| `invert_colors`        | 1 for most IPS panels. Change it if white shows as black.                                          |
| `bgr_order`            | Set it to 1 if red shows as blue.                                                                  |
| `use_dma`              | Set it to 1 to use the DMA (refer to the README of the library).                                   |

### 5. If the screen shows no image

| Problem                                           | Correction                                                                                  |
|---------------------------------------------------|---------------------------------------------------------------------------------------------|
| The backlight is off                              | Examine the BL pin and the VCC pin.                                                         |
| The backlight is on, but there is no image        | Examine the DC, DIN and CLK wires. Decrease the SCK.                                        |
| White shows as black                              | Change `invert_colors`.                                                                     |
| Red shows as blue                                 | Change `bgr_order`.                                                                         |
| The image has an offset or noise at an edge       | Set `width`, `height`, `x_offset` and `y_offset` for your panel.                            |
| `ST7789HAL_Init()` does not return `ST7789HAL_OK` | Make sure that the SPI has 8 data bits. Make sure that `hspi` and `dc_port` are not `NULL`. |

For all the functions, the DMA and the errors, refer to
[`Lib/st7789_hal/README.md`](Lib/st7789_hal/README.md).

## How to start with the fast library

The fast library is for the STM32H7. It uses the registers of SPI1 and
DMA1 Stream 0 directly. Thus, more steps are necessary than for the HAL
library.

1. Copy the folder `Lib/st7789` into your project.
2. Add the `.c` files of the folder and `port/st7789_port_stm32h7.c` to the
   source files of your project.
3. Do the steps in "Necessary conditions for the STM32H7 port" in
   [`Lib/st7789/README.md`](Lib/st7789/README.md).
4. Call the library from your application:

```c
#include "st7789.h"

void App_Start(void)                 /* call it after MX_SPI1_Init() */
{
  ST7789_Init();
  ST7789_FillScreen(ST7789_BLACK);
  ST7789_DrawString(10, 10, "Hello", ST7789_WHITE, ST7789_BLACK, 2);
}
```

The example project in this repository is a complete project with this
library. You can use it as the start of your project.

## Example project for the NUCLEO-H753ZI

The repository is also a complete project for a NUCLEO-H753ZI board with
the 2" IPS display module from Waveshare (240x320 pixels).

### Wires

| Pin on the display module | Pin on the NUCLEO-H753ZI | Signal       |
|---------------------------|--------------------------|--------------|
| VCC                       | 3V3                      | 3.3 V supply |
| GND                       | GND                      | Ground       |
| DIN                       | PB5                      | SPI1 MOSI    |
| CLK                       | PA5                      | SPI1 SCK     |
| CS                        | PD14                     | CS           |
| DC                        | PD15                     | DC           |
| RST                       | PF3                      | Reset        |
| BL                        | PG12                     | Backlight    |

Keep the wires short, because the SCK of this project is 100 MHz.

### Tools

Install STM32CubeCLT. It contains all the necessary tools: GCC for Arm,
CMake, Ninja and STM32CubeProgrammer.

### Compile and write the firmware

1. Get the repository:

   ```bash
   git clone https://github.com/OscarBazylewich/ST7789V_Display_Driver_for_STM32.git
   ```

2. Go to the folder of the repository:

   ```bash
   cd ST7789V_Display_Driver_for_STM32
   ```

3. Make the CMake configuration. Use one of the commands from the table
   below.
4. Compile the project:

   ```bash
   cmake --build --preset Release
   ```

5. Connect the USB connection of the ST-LINK to the PC.
6. Write the firmware to the board:

   ```bash
   STM32_Programmer_CLI -c port=SWD -w build/Release/Nucleo-H753DisplayTest.elf -v -rst
   ```

Select the command for step 3 from this table:

| Firmware                                        | Command for step 3                                                                      |
|-------------------------------------------------|-----------------------------------------------------------------------------------------|
| The stress test of the fast library             | `cmake --preset Release`                                                                |
| The scenes of the HAL library, with DMA         | `cmake --preset Release -DLCD_LIBRARY=st7789_hal`                                       |
| The scenes of the HAL library, without DMA      | `cmake --preset Release -DLCD_LIBRARY=st7789_hal -DHAL_DEMO_USE_DMA=OFF`                |
| The benchmark with the fast library             | `cmake --preset Release -DLCD_BENCH=ON`                                                 |
| The benchmark with the HAL library, with DMA    | `cmake --preset Release -DLCD_BENCH=ON -DLCD_LIBRARY=st7789_hal`                        |
| The benchmark with the HAL library, without DMA | `cmake --preset Release -DLCD_BENCH=ON -DLCD_LIBRARY=st7789_hal -DHAL_DEMO_USE_DMA=OFF` |

The firmware also sends its results as text to the ST-LINK virtual COM
port. To read them, open the COM port on the PC at 115200 bits in each
second (8N1 format).

## Speed

The table shows how many times in one second each library does an
operation. A larger number is better. The results are from the benchmark
on the NUCLEO-H753ZI board at an SCK of 100 MHz.

| Operation                                | Fast library | HAL library with DMA | HAL library without DMA |
|------------------------------------------|-------------:|---------------------:|------------------------:|
| Fill the full screen (240x320 pixels)    |           81 |                   80 |                      34 |
| Draw a full screen image                 |           80 |                   67 |                      32 |
| Fill a rectangle of random dimensions    |        3,055 |                2,814 |                   1,240 |
| Draw a string with a background color    |        1,618 |                1,283 |                     625 |
| Draw a string without a background color |        4,169 |                1,068 |                   1,060 |
| Draw a line                              |        5,154 |                1,335 |                   1,348 |
| Draw a circle                            |        3,698 |                1,312 |                   1,080 |
| Draw a triangle                          |        1,064 |                  396 |                     343 |

The results show these points:

- For full screens and large rectangles, the two libraries have almost the
  same speed with DMA. The SPI bus is the limit.
- For small shapes, the fast library is approximately three to four times
  faster than the HAL library.
- The DMA makes the HAL library approximately two times faster for large
  areas. For small shapes, the DMA does not change the speed.

100 MHz is more than the maximum SCK in the datasheet of the controller.
If the panel does not show a correct image, use a lower SCK.

## Tests

The HAL library has tests that you can do on a PC, without the hardware.
The tests use code that simulates the controller. GCC for the PC, CMake
and Bash are necessary.

```bash
bash Lib/st7789_hal/test/run_tests.sh
```

The time for the tests is approximately 100 seconds. The last line shows
`RESULT: PASS` if all the tests are correct.

## Folders

| Folder or file               | Contents                                          | Is it necessary for your project? |
|------------------------------|---------------------------------------------------|-----------------------------------|
| `Lib/st7789_hal/`            | The HAL library, its README and its tests         | Yes, if you use the HAL library   |
| `Lib/st7789/`                | The fast library, its README and its STM32H7 port | Yes, if you use the fast library  |
| `Core/`                      | The example application for the NUCLEO-H753ZI     | No. It is an example.             |
| `Drivers/`                   | The HAL and the CMSIS files from ST and Arm       | No. Your project has its HAL.     |
| `cmake/`, `CMakeLists.txt`   | The CMake files of the example project            | No                                |
| `Nucleo-H753DisplayTest.ioc` | The STM32CubeMX file of the example project       | No                                |
| `Docs/`                      | The glossary of the terms that the documents use  | No                                |

## License

The libraries and the example application have the MIT license. Refer to
[`LICENSE`](LICENSE).

The files in `Drivers/` and the files that STM32CubeMX made have the
licenses of ST and Arm. Refer to the `LICENSE.txt` files in `Drivers/`.

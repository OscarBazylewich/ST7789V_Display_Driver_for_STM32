# STE glossary

This file gives the technical nouns (TN) and the technical verbs (TV) of this project (rules 1.5, 1.8 and 1.12 of ASD-STE100 Issue 9).

Obey these instructions:

- Use each term only with the meaning that this file gives.
- Use one term for one item (rule 1.11).
- Before you use a new term, add it to this file.

The checker (`Docs/ste_check.py`) reads each row with the type TN or TV in the three tables that follow. Thus, keep one term in each row. Do not change the structure of the tables.

## Technical nouns

Use a technical noun only as a noun. Do not use it as a verb (rule 1.7). Do not change code names, file names, and the names from a datasheet or from a tool.

| Term | Type | Other forms | Meaning |
|---|---|---|---|
| address | TN | addresses | The number that identifies one location in memory. |
| algorithm | TN | algorithms | A specified sequence of steps that calculates a result. |
| API | TN |  | The functions, types and macros in `st7789.h` that the application can use. |
| application | TN | applications | The code that uses the library and calls its functions, for example `main.c` and `stress_demo.c`. |
| ASCII | TN |  | The standard code that gives a number from 0 to 127 to each character, for example 0x41 to the character "A". The font has the ASCII characters from 0x20 (' ') to 0x7E ('~'). |
| atomic operation | TN | atomic operations | A memory access that an interrupt cannot divide. It occurs fully, or it does not occur. |
| attribute | TN | attributes | Data for the compiler in GCC code, for example `__attribute__((aligned(32)))`. |
| average | TN | averages | The total of a group of values, divided by the number of values. |
| axis | TN | axes | A straight line in the x direction or in the y direction, for example the x axis of the screen. A circle has 2 axes through its middle. |
| background color | TN | background colors, background | The color of the pixels behind the characters of a text (the `bg` parameter). |
| background transfer | TN | background transfers | A transfer that continues after the drawing function returns: one DMA transfer or a full flush. During this time, the CPU can do other work. |
| backlight | TN |  | The LEDs behind the panel. Without them, you cannot see the screen. `ST7789_Backlight()` sets them to ON or OFF. |
| barrier | TN | barriers | A CPU instruction, for example DSB, that completes all memory accesses before the CPU continues. |
| base address | TN | base addresses | The address of the first byte of a memory area or of a group of registers. |
| benchmark | TN | benchmarks | The test in `Core/Src/lcd_bench.c`. It does the same operations with `Lib/st7789` and with `Lib/st7789_hal`, and it records the number of operations in each second. Thus, you can compare the speeds of the two libraries. |
| Bash | TN |  | The shell that does the script `Lib/st7789_hal/test/run_tests.sh` on the PC. |
| copyright | TN |  | The legal right of the owner of a text or of code to control its copies. |
| license | TN | licenses | The text that gives the conditions for the use of code, for example the MIT license in the file `LICENSE`. |
| Ninja | TN |  | The tool that CMake uses to start the compiler and the linker for each file. |
| specification | TN | specifications | A document that gives rules, for example the ASD-STE100 document for STE. |
| BGR panel | TN | BGR panels | A panel that has the colors of each pixel in the sequence blue, green, red. If `ST7789_BGR_ORDER` is 0, a BGR panel shows red as blue and blue as red. |
| bit | TN | bits | The smallest unit of data. Its value is 0 or 1. |
| black | TN |  | The color of a pixel with the value 0x0000 (`ST7789_BLACK`). |
| blue | TN |  | The color blue. In an RGB565 pixel, blue is in bits 4 to 0. |
| board | TN | boards | A hardware unit that holds components and their connections, for example the NUCLEO-H753ZI with the MCU. |
| bounce buffer | TN | bounce buffers | A temporary buffer with a changed copy of pixel data for the DMA. For example, the 2 bytes of each pixel have the opposite sequence. |
| bounding box | TN | bounding boxes | The smallest rectangle that contains all the pixels of a shape or of a group of areas. |
| Bresenham's algorithm | TN | Bresenham algorithm | The algorithm that finds the pixels of a straight line between two points. It uses only integers. |
| buffer | TN | buffers | An area of RAM that holds data for a short time, for example pixels before the DMA sends them. |
| bus | TN | buses | The connections that move data between the parts of a system, for example the AHB bus in the MCU. |
| bus master | TN | bus masters | A unit of the MCU that can read and write memory, for example the CPU, DMA1 or MDMA. |
| button | TN | buttons | A part that you push to give an input, for example the blue USER button on the board. |
| byte | TN | bytes | A unit of data that has 8 bits. |
| C linkage | TN |  | With `extern "C"`, a C++ compiler uses C names for the functions. Thus, C code and C++ code can call them. |
| cache line | TN | cache lines | A unit of 32 bytes in the data cache. The data cache reads, writes and cleans only full cache lines. |
| call | TN | calls | One start of a function. For example, each `ST7789_FillRect()` in the code is one call. |
| callback | TN | callbacks | A function of the application that the library calls when a specified condition occurs. The wait hook and the done callback are callbacks. |
| cast | TN | casts | C code that changes a value to a different type, for example `(uint16_t)x`. |
| changed area | TN | changed areas | An area of the framebuffer with pixels that changed after the last flush. The library records it, and the next flush sends it. |
| channel | TN | channels | A part of the DMAMUX that connects the DMA request of a unit to one DMA stream. For example, DMAMUX1 channel 0 connects the DMA requests of SPI1 to DMA1 Stream 0. |
| chapter | TN | chapters | A large part of a manual, for example the SPI chapter of RM0433. |
| character | TN | characters | One symbol of text, for example A, 7 or ?. The font gives 5 x 7 pixels for each character. |
| checkerboard | TN |  | A test pattern of rectangles in 2 colors. Adjacent rectangles have different colors. |
| circle | TN | circles | A shape that has all its points at the same distance from one point. |
| clangd | TN |  | A tool that reads C and C++ code and gives data about its symbols to other tools. It reads `compile_commands.json`. |
| clipping | TN |  | The library operation that removes the parts of a shape that are not on the screen. |
| clock | TN | clocks | A signal that goes high and low at a constant rate, for example SCK or the kernel clock of SPI1. It controls the speed of a unit. |
| CMake | TN |  | The tool that reads the `CMakeLists.txt` files and controls the compiler and the linker. |
| CMSIS | TN |  | The header files from Arm and ST for the Cortex-M7 and for the registers of the STM32H7, in the folder `Drivers/CMSIS`. |
| color inversion | TN |  | A mode of the controller in which each color shows as its opposite color (INVON and INVOFF commands). |
| column | TN | columns | All the pixels that have the same x coordinate, on the screen, in an area or in a character. |
| command | TN | commands | A byte that the MCU sends with DC low. It tells the controller which operation to do, for example RAMWR. |
| compensation cell | TN | I/O compensation cell | A part of the STM32H7 that adjusts the output of the I/O pins. It is necessary at high pin speeds. |
| compile definition | TN | compile definitions | A macro that CMake gives to the compiler with -D, for example with `target_compile_definitions()`. |
| compiler | TN | compilers | The tool (GCC) that changes C source files into object files for the MCU. |
| configuration | TN | configurations | The values that control the operation of a unit or of a tool. Examples are the registers of SPI1 and the Debug configuration. |
| configuration command | TN | configuration commands | A command that the library sends with `st7789_port_command()`, for example MADCTL or COLMOD. The port sends each configuration command and its parameters in a different CS cycle (on the STM32H7 port, as 8-bit SPI frames). CASET, RASET and RAMWR are not configuration commands. The library sends them with `st7789_port_cmd_fast()`. |
| configuration macro | TN | configuration macros | A macro in `st7789_config.h` that sets a value of the library before you compile it, for example `ST7789_TEXT_BUFFER_PIXELS`. |
| constant expression | TN | constant expressions | C code with a value that the compiler can calculate, for example 240 or (2 * 120). |
| contact bounce | TN |  | The fast changes between the open and the closed condition of a button, immediately after you push or release it. |
| context | TN | contexts | The location where code operates: in thread mode (the loop in `main()` or one task), or in one interrupt handler. |
| controller | TN | controllers | The ST7789 component in the display module. It receives commands and pixel data through the SPI bus. It keeps the pixels in the controller memory and controls the panel. |
| controller memory | TN |  | The memory in the controller that keeps the pixel data for the panel (240 x 320 pixels). |
| coordinate | TN | coordinates | One of the 2 values (x and y) that give the position of a pixel. x gives the column and y gives the row. |
| counter | TN | counters | A register or a variable that counts, for example the DMA counter NDTR or the millisecond counter. |
| CPU | TN |  | The part of the MCU that does the instructions of the code (Cortex-M7). |
| data cache | TN | cache | The data cache (level 1) of the Cortex-M7: a small, fast memory that keeps copies of data from the RAM. |
| datasheet | TN | datasheets | The data and the limits of one component from its manufacturer, for example the ST7789V datasheet. |
| Debug configuration | TN |  | The CMake configuration that `CMAKE_BUILD_TYPE=Debug` selects. Use it to debug the code. |
| default | TN | defaults | The value that the library uses if you do not set a different value. |
| degree | TN | degrees | A unit of a turn (symbol °). One full turn is 360 degrees. |
| direct mode | TN |  | The drawing mode in which the library sends the pixels of each shape to the controller immediately, without a framebuffer. RM0433 uses the same name for a different item: the mode of a DMA stream without its FIFO (DMDIS = 0). In the port file, the words "in RM0433" follow the name of that item. |
| display module | TN | display modules | The Waveshare board with the panel, the controller and the backlight (240 x 320 pixels). |
| DMA | TN |  | The unit of the MCU that moves data between memory and other units without the CPU. On the STM32H7, the port uses DMA1. |
| DMA buffer | TN | DMA buffers | A buffer in a RAM area that the DMA can read (AXI SRAM, not DTCM), for example a buffer with `ST7789_DMA_BUFFER`. |
| DMA channel | TN | DMA channels | On some families, for example the STM32F1 and the STM32L4, one part of a DMA unit that does one transfer at a time. Other families have DMA streams. |
| DMA interrupt | TN | DMA interrupts | The interrupt of the DMA stream of the port. It occurs when the DMA moved all the data of a piece to the SPI ("transfer complete"), or when an error occurs ("transfer error"). If the DMA transfer has more pieces, the interrupt handler starts the next piece. On the STM32H7 port, it is the DMA1_Stream0 interrupt (`DMA1_Stream0_IRQHandler()`). |
| DMA request | TN | DMA requests | A signal from SPI1 or from a different unit that tells the DMA to move the next data. |
| DMA stream | TN | DMA streams, stream, streams | One of the 8 parts of DMA1 or DMA2, for example DMA1 Stream 0. Each part can do one transfer at a time. |
| DMA transfer | TN | DMA transfers | One transfer that the DMA does for one call of `st7789_port_dma16()`. The DMA moves the pixels from memory to the SPI. |
| done callback | TN |  | The callback that the library calls after the end of each background transfer (`ST7789_SetDoneCallback()`). The interrupt handler of the transfer interrupt calls it. |
| drawing function | TN | drawing functions | An API function that draws pixels, for example `ST7789_FillRect()` or `ST7789_DrawString()`. |
| drawing mode | TN | drawing modes | One of the 2 methods that the library uses to draw: direct mode or framebuffer mode. |
| driver | TN | drivers | Code that controls a hardware unit. Examples are the HAL SPI driver and the code from the manufacturer of the display module. |
| endless mode | TN |  | The STM32H7 SPI mode with TSIZE = 0. In this mode, the SPI sends data until the code stops it. |
| enum | TN | enums | A C type that gives a name to each of its integer values, for example `ST7789_Rotation`. |
| error bit | TN | error bits | A bit in the handle of the ST7789 HAL library that records an error (`ST7789HAL_ERROR_SPI` or `ST7789HAL_ERROR_TIMEOUT`). It stays set until a call to `ST7789HAL_ClearErrors()` or `ST7789HAL_Init()`. |
| executable | TN | executables | The file that the linker makes from the object files and the libraries. You write it into the flash memory of the MCU. |
| family | TN | families, MCU family | A group of STM32 MCUs with the same HAL, for example STM32F4 or STM32H7. |
| field | TN | fields | One value in a struct, with a name and a type. |
| FIFO | TN |  | A small memory in a unit, for example in SPI1. It sends its data in the same sequence in which it received the data. |
| FIFO threshold | TN |  | The quantity of data in the SPI FIFO at which the SPI sets a flag (FTHLV). |
| file | TN | files | Data with a name in a file system, for example `st7789.c` or `CMakeLists.txt`. |
| first | TN |  | The number 1 in a sequence, for example in "the first frame". FIRST (adv) is also an approved word. |
| fixed area | TN | fixed areas, top fixed area, bottom fixed area | A group of rows at the top or at the bottom of the screen. It does not move when the controller scrolls (TFA and BFA of VSCRDEF). |
| flag | TN | flags | A bit in a register or in memory that shows a condition, for example the TXC flag of SPI1. |
| flash memory | TN | flash | The memory of the MCU that keeps the code and the constant data, also without a supply voltage. |
| flush | TN | flushes | One operation in which the library sends all the changed areas of the framebuffer to the controller (`ST7789_Flush()`). |
| folder | TN | folders | A location in a file system that contains files and other folders, for example `Lib/st7789`. |
| font | TN | fonts | The pixel data of each ASCII character (5 x 7 pixels) that the library uses to draw text. |
| format | TN | formats | The structure of data, for example the bits of an RGB565 pixel or the byte sequence on the SPI bus. |
| four | TN |  | The number 4. |
| frame | TN | frames | One full image that the panel shows. |
| frame rate | TN | frame rates | The number of frames that the stress test draws in one second (fps). |
| framebuffer | TN | framebuffers | A buffer in RAM with one 16-bit value for each pixel of the screen. In framebuffer mode, the library draws into it. |
| framebuffer mode | TN |  | The drawing mode in which the library draws into a framebuffer. A flush sends the changed areas to the controller. |
| function | TN | functions | A part of C code with a name that other code can call, for example `ST7789_FillRect()`. |
| gamma | TN |  | The curve that the controller uses to change pixel values into voltages for the panel (PVGAMCTRL and NVGAMCTRL commands). |
| gather buffer | TN | gather buffers | The buffer (`gather_buf`) in which the library collects the rows of a thin changed area. Then the DMA sends all of them in one DMA transfer. |
| GCC | TN |  | The GNU C compiler for Arm (`arm-none-eabi-gcc`) that compiles this project. |
| glossary | TN |  | This file (`Docs/ste-glossary.md`). It gives the TN and the TV of this project. |
| GPIO port | TN | GPIO ports | One group of 16 pins of the STM32H7, for example GPIOD. |
| gradient | TN | gradients | An image area in which the color changes by a small value from each pixel to the next pixel. |
| green | TN |  | The color green. In an RGB565 pixel, green is in bits 10 to 5. |
| HAL | TN |  | The driver code from ST for the units of the STM32H7, for example `HAL_GetTick()`. |
| half | TN | halves | One of 2 equal parts of a quantity or of a shape. |
| handle | TN | handles | A struct with the data of one unit or of one panel, for example `SPI_HandleTypeDef` of the HAL or `ST7789HAL_Handle` of the ST7789 HAL library. |
| hardware | TN |  | All the parts of the system that are not code: the MCU, its units, the wires and the display module. |
| header file | TN | header files | A C file (.h), for example `st7789.h`, that other C files include with `#include`. |
| idle | TN |  | The condition of the SPI or of the library when no transfer occurs. |
| image | TN | images | A rectangle of RGB565 pixels, for example the pixel data that `ST7789_DrawImage()` sends. |
| integer | TN | integers | One of the numbers ..., -2, -1, 0, 1, 2, ..., for example 240. The C types `int` and `uint16_t` hold integers. |
| interrupt | TN | interrupts | A signal from a unit of the MCU. It makes the CPU stop its code, do the interrupt handler, and then continue its code. |
| interrupt handler | TN | interrupt handlers, handler, handlers | The function that the CPU starts when an interrupt occurs, for example `SPI1_IRQHandler()`. |
| IPS panel | TN | IPS panels | A panel of the IPS LCD type. Most IPS panels with an ST7789 controller show the correct colors only with color inversion. |
| issue | TN | issues | The ASD-STE100 text with all its changes until a date. This project uses Issue 9 (2025-01-15). |
| kernel clock | TN | kernel clocks | The clock that the RCC of the STM32H7 supplies to SPI1. SPI1 divides it to make SCK. |
| label | TN | labels | A short text on the screen that identifies an item, for example the scene name in the status bar. |
| landscape | TN |  | The rotation in which the width of the screen is more than its height (320 x 240 pixels). |
| library | TN | libraries | A set of functions and data that other code can use, for example this ST7789 library or the C library. |
| line | TN | lines | The straight shape that connects two points. `ST7789_DrawLine()` draws it. |
| linker | TN | linkers | The tool that makes the executable from the object files and the libraries. |
| linker script | TN | linker scripts | The file that tells the linker where to put each section in memory, for example `STM32H753xx_FLASH.ld`. |
| list | TN | lists | A group of items in a sequence, for example the list of changed areas that the library keeps in RAM. |
| loop | TN | loops, inner loop, inner loops | A part of the code that the CPU does again and again, for example `for (;;)` or the loop in `main()`. An inner loop is a loop in a different loop. |
| LVGL | TN |  | A library that makes the images for a screen. It writes the pixels of an area into a buffer. Then it calls a callback of the application, and this callback sends the area to the controller. |
| macro | TN | macros | A name that the compiler replaces with a value or with code before it compiles the file, for example `ST7789_DMA_BUFFER`. |
| magenta | TN |  | The color with full red, full blue and no green (`ST7789_MAGENTA`). |
| major axis | TN |  | Of a line: the axis (x or y) on which the line has the larger difference between its two ends. |
| manufacturer | TN | manufacturers | The group of persons that makes an item, for example Waveshare for the display module. |
| map file | TN | map files | The file (.map) that the linker makes. It shows the address and the length of each section and of each symbol. |
| master mode | TN |  | The SPI mode in which the SPI of the MCU supplies SCK and starts each transfer. In STM32CubeMX, the name of this mode of SPI1 is "Transmit Only Master". |
| max | TN |  | The value max(x, y) is the larger of the two values x and y, for example max(3, 5) = 5. |
| MCU | TN | MCUs | The component that contains the CPU, the memory and the other units: for this project, the STM32H753ZI on the NUCLEO-H753ZI board. |
| meaning | TN | meanings | The item, the operation or the condition that a word or a term identifies. |
| memory | TN | memories | A part of the MCU or of the controller that keeps data. Examples are RAM, flash memory and the controller memory. |
| midpoint circle algorithm | TN |  | The algorithm that finds the pixels of a circle with integers only. It calculates the pixels of one octant. Then it finds the pixels of the 7 other octants at the symmetrical positions. |
| millisecond | TN | milliseconds | A unit of time: 0.001 second (symbol ms). |
| millisecond counter | TN |  | The counter that increases by 1 at each millisecond. `st7789_port_millis()` returns its value. On the STM32H7 port, it is the counter that `HAL_GetTick()` reads. The interrupt handler of SysTick increases it. |
| minor axis | TN |  | Of a line: the axis (x or y) on which the line has the smaller difference between its two ends. |
| modulo | TN |  | The remaining value when you divide one integer by a different integer, for example 250 modulo 240 = 10. |
| mutex | TN | mutexes | An item of the RTOS. It lets only one task at a time use a part of the code, for example the library. |
| newlib-nano | TN | newlib, nano | The small C library for MCUs that GCC for Arm uses with `--specs=nano.specs`. |
| newline | TN | newlines | The character `'\n'`. It starts a new row of text. |
| NVIC | TN |  | The part of the Cortex-M7 that enables and disables each interrupt and gives it a priority. |
| object file | TN | object files | The file (.o) that the compiler makes from one source file. The linker uses it. |
| octant | TN | octants | One of the 8 equal parts of a circle (45 degrees each). |
| odd number | TN | odd numbers | An integer, for example 1, 3 or 5. If you divide it by 2, the result is not an integer. |
| offset | TN | offsets | A distance in pixels from a start position, for example the position of the screen in the controller memory (`ST7789_PANEL_X_OFFSET`). |
| one | TN |  | The number 1. |
| optimization | TN |  | The work of the compiler that makes the code faster or smaller, for example with -O2. |
| panel | TN | panels | The part of the display module that shows the pixels (an IPS LCD). |
| parameter | TN | parameters | A value that you give to a function or to a command. For a command, the MCU sends each parameter as a data byte after the command byte. |
| part of speech | TN | n, v, adj, adv, prep, conj, pron, art | The type of a word in the dictionary: n (noun), v (verb), adj, adv, prep, conj, pron or art. |
| path | TN | paths | The location of a file or of a folder, for example `Lib/st7789`. An include path is a folder in which the compiler finds header files. |
| PC | TN | PCs | The system on which you compile the firmware, for example with Windows or Linux. The tests in `Lib/st7789_hal/test` also operate on it. |
| pending bit | TN | pending bits | The NVIC bit that is 1 when an interrupt occurred, but the CPU did not start its interrupt handler. |
| percent | TN |  | One part in 100 (symbol %). |
| piece | TN | pieces | One part of a DMA transfer that the DMA stream sends with one start (one NDTR value): a maximum of 65535 pixels, or one row of a rectangle with a stride. The DMA interrupt starts the next piece. |
| pin | TN | pins | One connection of the MCU or of the display module for one signal, for example LCD_CS. |
| pixel | TN | pixels | The smallest part of an image. It has one color. In RGB565, a pixel has 16 bits. |
| porch | TN | porches | A time at the start or at the end of each frame. During this time, the controller does not send rows to the panel (PORCTRL). |
| port | TN | ports | The part of the library that controls the hardware of one MCU type: the SPI, the DMA, the pins, the interrupts and the millisecond counter. For the STM32H7, it is `port/st7789_port_stm32h7.c`. |
| portrait | TN |  | The rotation in which the height of the screen is more than its width (240 x 320 pixels, `ST7789_PORTRAIT`). |
| prescaler | TN | prescalers | A part of a unit that divides a clock by a number to make a slower clock. For example, the prescaler of SPI1 makes SCK from the kernel clock. |
| priority | TN | priorities | The level that tells which interrupt or task operates first, or which DMA stream moves its data first. On the NVIC, 0 is the highest priority. For a DMA stream, the PL bits in its CR register set the priority. |
| project | TN | projects | The CMake project: the source files and the CMake files that make the executable for the NUCLEO-H753ZI. |
| RAM | TN |  | The memory of the MCU for variables, for example DTCM and AXI SRAM. It does not keep its data without a supply voltage. |
| rating | TN | ratings | A limit that the manufacturer gives for a component, for example the maximum SCK of the controller. |
| rectangle | TN | rectangles | A shape with 4 straight sides and 4 corners of 90 degrees. |
| red | TN |  | The color red. In an RGB565 pixel, red is in bits 15 to 11. |
| refresh | TN | refreshes, refresh rate | One cycle in which the controller sends the contents of the controller memory to the panel. The refresh rate is 39 to 119 Hz. |
| register | TN | registers | A memory location that controls a unit or shows its condition, for example the CR1 register of SPI1. |
| repository | TN | repositories | The folder with all the files of the project that Git records (`Nucleo-H753DisplayTest`). |
| reset | TN | resets | The change of a unit to its initial condition. For example, the RST pin or the SWRESET command causes a reset of the controller. |
| revision | TN | revisions | A datasheet with all its changes until a date, for example revision 1.6 of the ST7789V datasheet. |
| RGB565 | TN |  | The format of a pixel: 5 bits for red, 6 bits for green and 5 bits for blue in one 16-bit value. |
| rotation | TN | rotations | One of the 4 directions in which the screen shows its contents: portrait, landscape, and these two turned 180 degrees (`ST7789_SetRotation()`). |
| RTOS | TN |  | Code that lets 2 or more tasks use one CPU, for example FreeRTOS. |
| rule | TN | rules | One instruction of ASD-STE100 Issue 9 for STE text, for example rule 1.5. |
| scene | TN | scenes | One part of the stress test or of the application in `Core/Src/hal_demo.c`, for example FILL or LINES. The stress test shows each scene for 6 seconds and records its frame rate. |
| SCK | TN |  | The clock signal of the SPI bus. SPI1 makes it from its kernel clock. |
| screen | TN |  | All the pixels that the panel shows. Its width and its height change with the rotation. |
| scroll area | TN |  | The group of rows between the top fixed area and the bottom fixed area. Only these rows move when the controller scrolls (`ST7789_SetScrollArea()`). |
| scrolling | TN |  | The operation of the controller that moves the image on the screen up or down without new pixel data (VSCRDEF, VSCSAD). |
| second | TN | seconds | A unit of time (symbol s). 1 s is 1000 ms. |
| section | TN | sections | A part of a datasheet or of an object file with a number or a name. Examples are section 9 of the ST7789V datasheet and `.bss.st7789.dma`. |
| serial interface | TN |  | The part of the controller that receives the data from the SPI bus. |
| serial interface pause | TN |  | The name in the ST7789V datasheet for a time with CS high between two bytes. During this time, the controller keeps its write cursor. |
| signal | TN | signals | A voltage on a pin or on a wire that changes to give data, for example SCK or CS. |
| sleep mode | TN |  | A condition of the controller in which the panel shows no image, but the controller memory keeps its data. The Sleep In command starts it, and the Sleep Out command stops it. |
| SPI | TN |  | The unit of the MCU that sends data on the SPI bus, for example SPI1. |
| SPI bus | TN |  | The connection between SPI1 of the MCU and the serial interface of the controller: the wires SCK, MOSI, CS and DC. |
| SPI frame | TN | SPI frames | One unit of 8 or 16 data bits on the SPI bus. |
| SPI global interrupt | TN | SPIx global interrupt | The interrupt of an SPI unit. STM32CubeMX shows it in the "NVIC Settings" tab of the SPI as "SPIx global interrupt". Its interrupt handler calls `HAL_SPI_IRQHandler()`. |
| SPI mode 0 | TN | mode 0 | The SPI mode with CPOL = 0 and CPHA = 0. SCK is low while the SPI sends no data. The controller reads each bit when SCK goes from low to high. |
| stack | TN | stacks | The area of RAM where the CPU keeps local variables and the addresses where it continues after each function. |
| stack frame | TN | stack frames | The part of the stack for one call of a function. |
| startup file | TN |  | The STM32CubeMX file `startup_stm32h753xx.s`. It contains the vector table and the code that operates before `main()`. |
| status bar | TN |  | The area at the top of the screen where the stress test shows the scene name and the frame rate. |
| STM32CubeIDE | TN | CubeIDE | The ST tool that contains STM32CubeMX and GCC. It can compile and debug a project without CMake. |
| STM32CubeMX | TN | CubeMX | The ST tool that makes the start code and the CMake files for the configuration that you select. |
| Stop mode | TN |  | A mode of the STM32H7 in which the clocks of the CPU and of most units stop. The name is from RM0433. |
| stress test | TN |  | The test in `Core/Src/stress_demo.c`. It draws scenes at the maximum speed and shows the frame rate of each scene. |
| stride | TN | strides | The distance in memory, in pixels, from the start of one row to the start of the next row. |
| string | TN | strings | A sequence of characters in memory with the byte 0 at its end, as in C. |
| struct | TN | structs | A C type that contains a group of fields in one memory area. |
| tab | TN | tabs | One group of values in STM32CubeMX that you open with its name, for example the `DMA Settings` tab. |
| table | TN | tables | Data in rows, with the same items in each row, for example the table of results in the README. |
| tail call | TN | tail calls | A call that is the last operation of a function. |
| target | TN | targets | A CMake item that tells CMake how to make one library or one executable from its source files. |
| task | TN | tasks | A part of the application that the RTOS operates independently. Each task has a stack and a priority. |
| tearing | TN | tearing effect | An image error: one part of the screen shows the new frame, and the other part shows the previous frame. |
| technical noun | TN | technical nouns, TN | A noun with one specified meaning in this project (rule 1.5). Use it only as a noun. |
| technical verb | TN | technical verbs, TV | A verb with one specified meaning in this project (rule 1.12). Use it only as a verb. |
| term | TN | terms | A word or a group of words with one specified meaning in this glossary. |
| test pattern | TN | test patterns | An image with known pixel values. Bit errors on the SPI bus show as incorrect pixels in it. |
| text | TN |  | Words and characters that a person can read. Examples are a comment, a document, and the characters that the library draws with its 5 x 7 font (for example with `ST7789_DrawString()`). |
| text buffer | TN | text buffers | One of the 2 buffers (`text_buf`) for text in direct mode. The library draws characters into it. Then the DMA sends it. |
| thread mode | TN |  | The Cortex-M7 mode of all code that is not in an interrupt handler. Examples are the loop in `main()` and a task. |
| three | TN |  | The number 3. |
| tick | TN | ticks | One step of the counter of SysTick (1 ms), or of the RTOS (one RTOS tick). |
| timeout | TN | timeouts | The maximum time that the library waits for a background transfer (`ST7789_IDLE_TIMEOUT_MS`). After this time, the library stops the transfer. |
| transfer | TN | transfers | One operation in which the SPI sends data to the controller. The DMA or the CPU supplies the data. |
| transfer interrupt | TN |  | The interrupt of the port that occurs when the SPI sent the last bit of a DMA transfer. On the STM32H7 port, it is the SPI1 interrupt for the TXC flag. |
| transmit buffer | TN | transmit buffers | The buffer (`st7789hal_buf`) of the ST7789 HAL library. The library writes pixels into it. Then the SPI sends it. |
| transparent text | TN |  | Text that the library draws without its background color (after `ST7789_SetTextTransparent(1)`). The library changes only the pixels of the characters. |
| triangle | TN | triangles | A shape with 3 straight sides and 3 corners. |
| two | TN |  | The number 2. |
| TX DMA interrupt | TN | TX DMA interrupts | In the ST7789 HAL library: the interrupt of the DMA stream or DMA channel of the DMA request SPIx_TX. Its interrupt handler calls `HAL_DMA_IRQHandler()`. |
| variable | TN | variables | A location in memory with a name that holds a value in C code. |
| vector table | TN |  | The table in memory that gives the address of each interrupt handler. |
| virtual COM port | TN |  | The USB connection of the ST-LINK on the NUCLEO-H753ZI. On the PC, it shows as a COM device, for example COM3. USART3 of the MCU sends text to it. |
| voltage | TN | voltages | A quantity that you measure with the unit V, for example 3.3 V on the pins. |
| voltage scale | TN | voltage scales | The internal voltage of the STM32H7, from VOS0 (highest) to VOS3. It sets the maximum clock speed of the CPU. |
| wait callback | TN |  | The LVGL callback that waits for the end of the transfer of an area (`flush_wait_cb` in the README). In LVGL v8, it is `drv.wait_cb`. It is not the wait hook of this library. |
| wait hook | TN |  | The callback that the library calls again and again while it waits for a background transfer (`ST7789_SetWaitHook()`). |
| wait loop | TN | wait loops | A loop in which the CPU reads a flag or a register again and again until its value changes. The CPU does no other work during this time. |
| Waveshare | TN |  | The manufacturer of the display module. |
| weak symbol | TN | weak symbols | A symbol in an object file that the linker uses only if no other object file contains a symbol with the same name. In the startup file, each interrupt handler is a weak symbol (`.weak`). If the port has an interrupt handler with the same name, the linker replaces the weak symbol with that interrupt handler. |
| window | TN | windows | The rectangle in the controller memory that the next pixel data fills (CASET and RASET commands). |
| wire | TN | wires | A connection between two pins, for example one of the 4 wires of the SPI bus. |
| word | TN | words | A unit of text, for example "draw" or "panel". |
| write cursor | TN |  | The position in the controller memory where the controller writes the next pixel. |

## Technical verbs

Use a technical verb only as a verb (rule 1.13), and only as in these examples (rule 3.2):

- "Draw the line."
- "To draw the line, the library sends one command."
- "The library draws the line."
- "The library drew the line."
- "The library will draw the line."
- "The drawn line is 1 pixel wide."

Do not write "is drawing", "has drawn" or "was drawing" (rule 3.2). Do not use "drawing" as a verb (rule 3.5).

| Term | Type | Other forms | Meaning |
|---|---|---|---|
| call | TV | calls, called | To start a function from a different part of the code. When the function returns, the code continues after the call. |
| clean | TV | cleans, cleaned | Of the data cache: to write the changed data of a cache line to RAM (`SCB_CleanDCache_by_Addr()`). Then the DMA can read the correct data. |
| clip | TV | clips, clipped | To remove the parts of a shape that are not on the screen before the library draws the shape. |
| compile | TV | compiles, compiled | To change source files into object files with the compiler. |
| copy | TV | copies, copied | To read data from one memory area and write the same data to a different memory area. |
| debug | TV | debugs, debugged | To find and correct errors in code, usually with the ST-LINK. |
| disable | TV | disables, disabled | To stop the operation of a unit, an interrupt or a DMA stream with its control bit, for example EN. |
| draw | TV | draws, drew, drawn | To write the pixels of a shape, a character or an image to the controller memory or to the framebuffer. |
| enable | TV | enables, enabled | To let a unit, an interrupt or a DMA stream operate with its control bit, for example EN. |
| flush | TV | flushes, flushed | To send the changed areas of the framebuffer to the controller. |
| invalidate | TV | invalidates, invalidated | Of the data cache: to discard the copy of some data in the data cache. Then the CPU reads that data from RAM. |
| link | TV | links, linked | Of the linker: to make one executable from the object files and the libraries. |
| preempt | TV | preempts, preempted | Of an interrupt: to stop the code that operates, do its interrupt handler, and then let that code continue. |
| return | TV | returns, returned | Of a function: to stop and let the code that called it continue, with or without a value. |
| scroll | TV | scrolls, scrolled | To move the image on the screen up or down with the scrolling of the controller (`ST7789_ScrollTo()`). |

## Terms about texts

`CLAUDE.md` uses these technical nouns. Some of them are the names that ASD-STE100 uses for the parts of a text. The other terms are about this project, for example Git and the chat.

| Term | Type | Other forms | Meaning |
|---|---|---|---|
| active voice | TN |  | A sentence in which the item that does the work comes before the verb, for example "The DMA sends the data." |
| adjective | TN | adjectives | A word that gives a property of a noun, for example "fast" in "a fast flush". |
| agent | TN | agents | A copy of Claude that does one part of a larger work, for example the changes to one file. The dictionary noun AGENT has a different meaning. |
| American English | TN |  | The English that STE uses for words, for example "color" and not "colour". |
| article | TN | articles | One of the words "the", "a" and "an". |
| branch | TN | branches | A sequence of commits in Git with a name, for example `main`. |
| chat | TN |  | The part of Claude Code that shows the text that the user writes and the replies of Claude. |
| checker | TN |  | The Python file `Docs/ste_check.py`. It shows possible STE problems in a file. |
| Claude | TN |  | The AI that changes this project for the user. |
| comma | TN | commas | The character ",". |
| comment | TN | comments | Text in a source file that the compiler ignores: between /* and */, after //, or after # in a CMake file. |
| commit | TN | commits | A set of changes that Git records together, with a commit message. |
| commit message | TN | commit messages | The text that Git records with a commit. |
| contraction | TN | contractions | One short word that replaces two words, for example "don't" for "do not". |
| demonstrative adjective | TN | demonstrative adjectives | One of the words "this", "these", "that" and "those" before a noun. |
| description | TN | descriptions | A text that gives information, not instructions. |
| dictionary | TN |  | Part 2 of ASD-STE100. It gives the approved words and the words that are not approved. |
| document | TN | documents | A file with text for persons, for example a README file. |
| firmware | TN |  | The code that the MCU does. The compiler and the linker make it from the source files of the project. |
| Git | TN |  | The tool that records the changes to the files of the project in commits. |
| identifier | TN | identifiers | The name of a function, a variable, a type or a macro in the code. |
| imperative | TN |  | The verb form of an instruction, for example "Connect" in "Connect the cable." |
| infinitive | TN |  | The basic verb form, with or without "to", for example "send" or "to send". |
| paragraph | TN | paragraphs | A group of sentences about one topic. |
| passive voice | TN |  | A sentence in which the item that receives the work comes before the verb, for example "The data is sent." |
| past participle | TN | past participles | The verb form that you can use as an adjective, for example "connected" in "the connected cable". |
| phrasal verb | TN | phrasal verbs | A verb and a different word that together have a different meaning, for example "set up". |
| Python | TN |  | The tool that operates the checker. The checker is a file of Python code. |
| reader | TN | readers | A person who reads the text. |
| reply | TN | replies | The text that Claude writes in the chat after the user writes in it. |
| semicolon | TN | semicolons | The character ";". Do not use it (rule 8.1). |
| sentence | TN | sentences | A group of words with a verb. In a text, the character ".", ":" or "?" shows the end of a sentence. |
| Simplified Technical English | TN | STE | The rules and the dictionary of ASD-STE100. |
| simple future tense | TN |  | The verb form with "will", for example "The DMA will send the data." |
| simple past tense | TN |  | The verb form for a time before this time, for example "sent" in "The DMA sent the data." |
| simple present tense | TN |  | The verb form for a fact, for example "sends" in "The DMA sends the data." |
| string literal | TN | string literals | A text in the C code, for example "D CHECK". |
| topic | TN | topics | The item that a paragraph gives information about. |
| user | TN | users | The person who uses Claude for this project. The user writes to Claude in the chat. |
| verb form | TN | verb forms | One of the different words of one verb, for example "send", "sends" and "sent". |
| vertical list | TN | vertical lists | A list with each item in a different row, for example the list of rules in `CLAUDE.md`. |
| workflow | TN | workflows | A script that starts many agents and controls the sequence of their work. |

## One term for one item

For each item, use only the term in the first part of its row. Do not use the words in the other part of the row for that item (rule 1.11).

| Use | Do not use |
|---|---|
| application | program, main program, user code, caller |
| bounding box | box |
| callback (for the wait hook and the done callback together) | hook, hooks |
| changed area | dirty area, dirty rectangle, dirty rect, kept area, marked area, region |
| character | letter |
| color | colour |
| compile definition | compiler definition, PUBLIC definition, definition |
| configuration | setup |
| configuration macro | setting, option (for a value in `st7789_config.h`) |
| controller | panel, TFT controller, chip (for the ST7789) |
| data cache | L1 data cache, D-cache |
| Debug configuration | Debug build, debug build |
| difference | delta |
| display module | module, Waveshare module, LCD module |
| DMA transfer | DMA job, job, DMA block, block |
| draw | render, paint, repaint |
| drawing function | drawing call, draw call, primitive |
| end | endpoint |
| folder | directory |
| framebuffer | frame buffer, the buffer (for the framebuffer) |
| gather buffer | small buffer |
| header file | header |
| horizontal line, vertical line, row of pixels | run, pixel run, span |
| image | picture |
| library (for this ST7789 code) | driver |
| millisecond counter | millisecond tick, tick counter |
| octant | eighth, one eighth |
| panel | display, TFT (for the part that shows the pixels) |
| port | hardware port, port layer, version |
| portrait | native orientation |
| rotation | orientation |
| row (of text) | line (of text) |
| SCK | SPI clock, SPI rate |
| screen | display (for all the pixels that the panel shows) |
| sleep mode | sleep, controller sleep |
| SPI bus | SPI link, link, SPI connection, wire, line (for the connection to the controller) |
| stopped SPI | stalled SPI, stall, stuck SPI |
| stress test | stress demo, demo |
| stride | pitch, row pitch |
| tearing | tear, tear line |
| text buffer | scratch buffer |
| transfer interrupt | completion interrupt, flush interrupt, end-of-transfer interrupt |
| wait hook | hook (for the wait hook) |
| wait loop | busy-wait, busy waiting, spin, spinning, waiting loop |

## Words to replace

These words are not approved, or the project uses them with a meaning that is not approved. Use the approved alternatives that this table gives. Do not change the code names.

| Word | Approved alternative |
|---|---|
| abandon (v) | STOP (v) |
| abort (n, v) | STOP (v). The function `st7789_port_dma_abort()` is code. |
| accumulate (v) | COLLECT (v) |
| acknowledge (v) | SET (v) THE FLAG (TN) TO 0, DISARM (v) |
| actual (adj) | CORRECT (adj), or tell which value |
| advance (v) | INCREASE (v), MOVE (v), CONTINUE (v) |
| afterwards (adv) | THEN (adv), AFTER (prep) |
| ahead (adv) | BEFORE (prep) |
| allow (v) | LET (v), GIVE (v), PERMITTED (adj) |
| alone (adj, adv) | ONLY (adv) |
| already (adv) | Usually, no word is necessary. |
| animation (n) | IMAGES (TN) THAT MOVE (v), FRAMES (TN) THAT CHANGE (v) QUICKLY (adv) |
| another (adj) | DIFFERENT (adj), ONE MORE, AGAIN (adv) |
| any (adj) | EACH (adj), ALL (adj), A (art), or no word |
| anything (pron) | ALL (adj), OTHER (adj) |
| anyway (adv) | ALSO (adv), or no word |
| appear (v) | SHOW (v), SEE (v) |
| archive (n) | LIBRARY (TN) (.a FILE) |
| arithmetic (n) | CALCULATE (v) |
| arrive (v) | OCCUR (v), COME (v) |
| ask (v) | TELL (v) |
| assert (n) | CHECK (n). `_Static_assert` is code. |
| assume (v) | IF (conj) |
| at least | MINIMUM (adj), A MINIMUM (n) OF |
| at once | IMMEDIATELY (adv), AT THE SAME TIME |
| avoid (v) | PREVENT (v) |
| away (adv) | AWAY FROM (prep), FAR (adj) |
| band (n) | RECTANGLE (TN) |
| bar (n) | RECTANGLE (TN). Use "bar" only in STATUS BAR (TN). |
| bare metal | WITHOUT AN RTOS (TN) |
| based (adj) | WITH (prep) |
| beat (v) | FASTER (adj) THAN, MORE THAN |
| been | WAS (v), WERE (v), or an active verb |
| begin (v), began | START (v) |
| belong (v) | BE (v) PART (n) OF |
| big (adj), bigger | LARGE (adj), LARGER |
| bit mask | A VALUE (n) WITH ONE BIT (TN) FOR EACH FLAG (TN) |
| blank (adj) | EMPTY (adj), SHOW (v) NO IMAGE (TN) |
| block (n) | DMA TRANSFER (TN), AREA (n), RECTANGLE (TN) |
| block (v) | WAIT (v), STOP (v). For a task: THE RTOS STOPS THE TASK UNTIL ... |
| bookkeeping (n) | SET (v) THE VARIABLES (TN) |
| both (adj, pron) | THE TWO (TN) |
| bounded (adj) | HAVE (v) A TIME LIMIT (n) |
| bring (v) | CONNECT (v), MOVE (v), PUT (v) |
| build (n) | DEBUG CONFIGURATION (TN), ALL THE FILES THAT YOU COMPILE (TV) |
| build (v), built | MAKE (v), COMPILE (TV) |
| busy (adj) | IN OPERATION (n), OPERATE (v). For a CPU that waits: WAIT LOOP (TN). |
| cache (v), cached | KEEP (v), RECORD (v), COPY (n). Use CACHE only for the DATA CACHE (TN). |
| caller (n) | THE CODE THAT CALLS (TV) THE FUNCTION (TN), APPLICATION (TN), YOU (pron) |
| carry on (v) | CONTINUE (v) |
| case (n) | CONDITION (n) |
| chain (n, v) | SEQUENCE (n), IN SEQUENCE, START (v) THE NEXT TRANSFER (TN) |
| cheap (adj) | FAST (adj), FASTER, IN LESS TIME |
| check (v) | DO A CHECK (n), EXAMINE (v), COMPARE (v), MAKE SURE (v) |
| clamp (v) | CHANGE (v) ... TO THE LIMIT (n), KEEP (v) ... IN THE RANGE (n) |
| clash (v) | CAUSE (v) AN ERROR (n) |
| classic (adj) | USUAL (adj) |
| clean (adj) (for an image or a frame) | CORRECT (adj), WITHOUT TEARING (TN). Use CLEAN (TV) only for the data cache. |
| cleanly (adv) | FULLY (adv) |
| clear (v) | SET (v) ... TO 0 for a bit or a flag, FILL (v) for the screen, REMOVE (v) |
| clock (v) | Use CLOCK (TN) as a noun: SCK (TN) OPERATES. |
| cluster (n) | GROUP (n) |
| coalesce (v) | INCLUDE (v), BECOME (v) ONE AREA (n) |
| colour (n) | COLOR (n) |
| common (adj) | FREQUENT (adj), USUAL (adj), SAME (adj) |
| completely (adv) | FULLY (adv) |
| completion (n) | END (n), COMPLETE (v). For the interrupt: TRANSFER INTERRUPT (TN). |
| compute (v) | CALCULATE (v) |
| configurable (adj) | YOU CAN CHANGE (v), YOU CAN SET (v) |
| configure (v) | SET (v), PREPARE (v), CHANGE (v) THE CONFIGURATION (TN) |
| content (n) | CONTENTS (n) |
| converter (n) | STM32CubeMX (TN) |
| core (adj, n) | PRIMARY (adj), BASIC (adj) |
| corrupted (adj) | INCORRECT (adj), DAMAGED (adj) |
| cost (n, v) | TIME (n), CAUSE (v), ADD (v), DECREASE (v) |
| couple (n) | TWO (TN), A SMALL (adj) NUMBER (n) OF |
| cover (v) | INCLUDE (v), FILL (v), CHANGE (v) |
| create (v) | MAKE (v) |
| current (adj) | AT THIS TIME, LAST (adj), THAT YOU SET (v) |
| current (n) (electric) | Give the value in mA. |
| decide (v) | SELECT (v), FIND (v) |
| declaration (n), declare (v) | FUNCTION (TN), VARIABLE (TN), PUT (v), WITH (prep) |
| defer (v) | DO (v) ... SUBSEQUENTLY (adv), LET (v) A DIFFERENT CONTEXT (TN) DO IT |
| define (v) | SET (v), GIVE (v), SPECIFIED (adj). `#define` is code. |
| delay (n) | INTERVAL (n), TIME (n), WAIT (v) |
| deliberately (adv) | Use BECAUSE (conj) and give the cause. |
| delta (n) | DIFFERENCE (n) |
| depend (v), dependency (n) | USE (v), NECESSARY (adj), IF (conj) |
| destination (n) | THE AREA (n) THAT ... WRITES TO |
| detach (v) | DISCONNECT (v), REMOVE (v) |
| directory (n) | FOLDER (TN) |
| dirty (adj) | CHANGED (adj) |
| distant (adj) | FAR (adj) |
| double (v) | TWO (TN) TIMES MORE, 2 TIMES LONGER |
| early (adj, adv) | BEFORE (conj), SHORT (adj) |
| ease (v) | HELP (v), EASY (adj) |
| edit (v) | CHANGE (v) |
| eighth (n) | OCTANT (TN), 1/8 |
| either (adj, conj, pron) | THE TWO (TN), ONE (TN) OR THE OTHER, OR (conj) |
| else (adv) | OTHER (adj) |
| elsewhere (adv) | IN OTHER (adj) ..., WITHOUT (prep) CMAKE (TN) |
| end (v) | COMPLETE (v), STOP (v). You can use END (n). |
| endpoint (n) | END (n) |
| enough (adj) | SUFFICIENT (adj) |
| even (adv) | ALSO (adv), or no word |
| ever (adv) | No word. |
| every (adj) | EACH (adj), ALL (adj) |
| everything (pron) | ALL (adj) and a noun |
| everywhere (adv) | FOR ALL (adj) OPERATIONS (n), IN ALL (adj) ... |
| exact (adj) | ACCURATE (adj), CORRECT (adj) |
| exactly (adv) | ACCURATELY (adv), FULLY (adv), ONLY (adv), THE SAME (adj) AS |
| exceed (v) | BE (v) MORE THAN |
| exception (n) | BUT NOT |
| exclusive (adj), inclusive (adj) | INCLUDE (v), IS NOT IN THE AREA (n) |
| exercise (v) | USE (v) |
| exist (v) | BE (v): THERE IS, THERE ARE. AVAILABLE (adj) |
| fast path | THE FAST (adj) FUNCTIONS (TN), THE USUAL (adj) CONDITION (n) |
| feature (n) | PROPERTY (n) |
| feed (v), fed | SUPPLY (v), SEND (v), PUT (v) |
| few (adj), fewer | SOME (adj), A SMALL (adj) NUMBER (n) OF, LESS THAN, or the number |
| figure (n) | RESULT (n), VALUE (n) |
| finish (v) | COMPLETE (v), COMPLETED (adj) |
| flip (n, v) | TURN (v) ... 180 DEGREES (TN), OPPOSITE (adj), CHANGE (n) |
| form (n) | SHAPE (n), METHOD (n) |
| fraction (n) | PART (n), LESS THAN |
| gap (n) | SPACE (n), DISTANCE (n), CLEARANCE (n) |
| gather (v) | COLLECT (v). Use "gather" only in GATHER BUFFER (TN). |
| generate (v) | MAKE (v), ADD (v), SUPPLY (v), GIVE (v) |
| generator (n) | A FUNCTION (TN) THAT MAKES RANDOM (adj) NUMBERS (n) |
| geometry (n) | DIMENSIONS (n), POSITION (n), VALUES (n) |
| gone | WENT (v), SENT (v) |
| grow (v), grew, grown | INCREASE (v), BECOME (v) LARGER |
| guard (v) | PREVENT (v) |
| hand (v) | GIVE (v), SEND (v) |
| hang (v) | STOP (v) |
| harmless (adj) | SAFE (adj) |
| helper (n) | INTERNAL (adj) FUNCTION (TN), LOCAL (adj) FUNCTION (TN) |
| hide (v) | AT THE SAME TIME AS, SHORTER (adj) THAN |
| hollow (adj) | EMPTY (adj) |
| however (adv) | BUT (conj) |
| hundreds (n) | MORE THAN 100 |
| identical (adj) | SAME (adj) |
| implement (v) | DO (v), CONTAIN (v), SUPPLY (v) |
| in case | IF (conj) |
| including (prep) | WITH (prep), ALSO (adv), INCLUDE (v) |
| incomplete (adj) | NOT COMPLETED, NOT FULL, MISSING (adj) |
| index (n) | NUMBER (n) |
| init (n) | Write the function name as code: `ST7789_Init()`. |
| initializer (n) | INITIAL (adj) VALUE (n) |
| inline (adj, adv) | DIRECTLY (adv), WITHOUT A FUNCTION (TN) CALL (TN). The C word `inline` is code. |
| inside (prep, n) | IN (prep), INTO (prep), INNER (adj) |
| instead (adv), instead of | ALTERNATIVE (n), NOT (adv) |
| interrupt (v) | STOP (v). Use INTERRUPT (TN) only as a noun. |
| involved (adj) | DO (v) THIS WORK (n) |
| itself, themselves, yourself (pron) | YOU (pron), ALSO (adv), or no word |
| job (n) | DMA TRANSFER (TN) |
| just (adv) | ONLY (adv), IMMEDIATELY (adv), or no word |
| lag (v) | OCCUR (v) ... AFTER |
| land (v) | OCCUR (v), GO (v), PUT (v), BE (v) COMPLETED |
| language (n) | No word, for example LET CMAKE (TN) COMPILE (TV) ASM AND C FILES (TN). |
| late (adj) | NOT IMMEDIATELY (adv), AFTER (prep) A SHORT TIME |
| later (adj, adv) | SUBSEQUENT (adj), SUBSEQUENTLY (adv), THEN (adv), AFTER (prep) |
| lay (v), laid | PUT (v), POSITION (n) |
| layout (n) | POSITION (n), SEQUENCE (n) |
| least (adv) | MINIMUM (adj), SMALLEST (adj) QUANTITY (n) |
| leave (v) | KEEP (v), LET (v) ... STAY (v), GO (v), MAKE (v) |
| leftmost (adj) | AT THE LEFT (adj) END (n) |
| legal (adj) | PERMITTED (adj), CORRECT (adj) |
| letter (n) | CHARACTER (TN) |
| lie (v) | BE (v) |
| like (prep) | AS (prep, conj), THE SAME AS, EQUIVALENT (adj) TO |
| likeliest (adj) | USUALLY (adv), BEST (adj) |
| limited (adj) | SMALL (adj), WITH A LIMIT (n) |
| list (v) | SHOW (v), RECORD (v). Use LIST (TN) only as a noun. |
| little (adj) | SMALL (adj), SHORT (adj), A SMALL QUANTITY (n) |
| little-endian (adj) | THE LOW (adj) BYTE (TN) IS AT THE LOWER (adj) ADDRESS (TN) |
| load (n, v) | READ (v) |
| lose (v), lost | DECREASE (v), NOT RECEIVE (v), NOT FIND (v) |
| low-power mode | SLEEP MODE (TN), or give the name of the mode |
| lower (v) | SET (v) ... LOW (adj). Use LOWER (v) only for a movement down. |
| main (adj) | PRIMARY (adj) |
| main loop | THE LOOP (TN) IN `main()` |
| main program | APPLICATION (TN) |
| major (adj) | PRIMARY (adj), LARGE (adj). Use "major" only in MAJOR AXIS (TN). |
| map (v) | PUT (v) |
| margin (n) | LIMIT (n), DIFFERENCE (n), VALUE (n) |
| marginal (adj) | NEAR (prep) ITS LIMIT (n) |
| mark (v), unmarked (adj) | RECORD (v), IDENTIFY (v). `ST7789_MarkDirty()` is code. |
| mask (v), unmask (v) | DISABLE (TV), ENABLE (TV) AGAIN |
| match (v) | AGREE (v), ALIGN (v), BE (v) THE SAME (adj) AS |
| matter (n, v) | IMPORTANT (adj) |
| may (v) | CAN (v). For a test condition: MUST (v), MUST NOT. |
| mean (v), meant | CAUSE (v), GIVE (v), SHOW (v), BE (v), FOR (prep) |
| meanwhile (adv) | DURING THIS TIME, AT THE SAME TIME, WHILE (conj) |
| member (n) | OBJECT FILE (TN) in a library, FIELD (TN) in a struct |
| merely (adv) | ONLY (adv) |
| merge (v) | INCLUDE (v), MAKE (v) ... LARGER, ADD (v) ... TO |
| minor (adj) | SMALL (adj), OTHER (adj). Use "minor" only in MINOR AXIS (TN). |
| mirror (n, v) | SYMMETRICAL (adj) |
| misplaced (adj) | IN AN INCORRECT (adj) POSITION (n) |
| mistaken (adj) | INCORRECTLY (adv) |
| modify (v) | CHANGE (v) |
| moment (n) | SHORT (adj) TIME (n), THAT TIME |
| mostly (adv) | MOST (adv), ALMOST (adv), MORE ... THAN |
| motion (n) | MOVEMENT (n) |
| multi- (prefix), multiple (adj) | TWO (TN) OR MORE, MORE THAN ONE, MANY (adj) |
| named (adj) | WITH THE NAME (n) |
| narrow (adj) | THIN (adj), NOT AS WIDE (adj) AS |
| native (adj) | PORTRAIT (TN), or THE BYTE SEQUENCE (n) OF THE CPU (TN) |
| nearby (adj) | NEAR (adj, prep) |
| need (v) | NECESSARY (adj), MUST (v) |
| negative (adj) | LESS THAN 0 |
| neighbour (n) | ADJACENT (adj) |
| nested (adj) | Tell which function calls which function. |
| net (n) | PRECAUTION (n) |
| never (adv) | DO NOT, NOT (adv) |
| non- (prefix) | NOT (adv) |
| normally (adv) | USUALLY (adv) |
| nothing (pron) | NO (adj) and a noun, NONE (pron), or a verb with NOT |
| notice (v) | FIND (v), SENSE (v) |
| now (adv) | AT THIS TIME, IMMEDIATELY (adv) |
| numerically (adv) | NUMBER (n), VALUE (n) |
| often (adv) | FREQUENTLY (adv) |
| old (adj) | PREVIOUS (adj), REMAINING (adj) |
| once (adv) | ONE (TN) TIME |
| once (conj) | WHEN (conj), AFTER (conj) |
| optimise (v), optimisation (n) | COMPILE (TV) WITH -O2. For the noun: OPTIMIZATION (TN) (rule 1.14). |
| or so | APPROXIMATELY (adv) |
| order (n, v) | SEQUENCE (n) |
| ordinary (adj) | USUAL (adj) |
| orientation (n) | ROTATION (TN) |
| otherwise (adv) | IF ... NOT, DIFFERENTLY (adv) |
| outline (n, v) | CONTOUR (n) |
| outside (prep, adj, adv, n) | OUT OF (prep), OUTER (adj), EXTERNAL (adj) |
| over (prep) | ON (prep), ABOVE (prep), ALONG (prep), THROUGH (prep), MORE THAN |
| overclock (n, v) | MORE THAN THE MAXIMUM (adj) SCK (TN) IN THE DATASHEET (TN) |
| overlap (v) | HAVE (v) AN OVERLAP (n) |
| overrun (v) | WRITE (v) AFTER THE END (n) OF THE BUFFER (TN) |
| overtake (v) | GO (v) FASTER THAN, OCCUR (v) BEFORE |
| overwrite (v) | REPLACE (v) |
| own (adj) | DIFFERENT (adj), ONLY (adv), or no word |
| own (v) | CONTROL (v), ONLY ... USE (v) |
| particular (adj) | ONLY APPLICABLE, ONE (TN) |
| partly (adv) | NOT FULLY, PART (n) OF |
| pass (v) | GIVE (v), GO (v), PUT (v) |
| past (prep) | MORE THAN, AFTER (prep) |
| pause (n, v) | STOP (v) ... FOR A SHORT TIME. Use "pause" only in SERIAL INTERFACE PAUSE (TN). |
| pay off (v) | DECREASE (v) THE TIME, FASTER (adj) THAN |
| per (prep) | FOR EACH, or "/" in a unit: frames/s |
| phase (n) | STEP (n) |
| pick (v) | SELECT (v) |
| picture (n) | IMAGE (TN) |
| pitch (n) | STRIDE (TN) |
| place (n, v) | POSITION (n), LOCATION (n), PUT (v) |
| plain (adj) | ONLY (adv), or no word |
| platform (n) | MCU (TN), HAL (TN), BOARD (TN) |
| polled (adj) | WITHOUT DMA (TN): THE CPU SENDS THE DATA |
| portable (adj) | YOU CAN USE (v) THE LIBRARY (TN) ON OTHER MCUS (TN) |
| preemptible (adj) | IT MUST BE POSSIBLE FOR ... TO PREEMPT (TV) ... |
| press (n, v) | PUSH (v): WHEN YOU PUSH THE BUTTON |
| primitive (n) | INTERNAL (adj) FUNCTION (TN) THAT DRAWS (TV) PIXELS AND RECTANGLES |
| print (v) | SEND (v), WRITE (v) |
| printable (adj) | Give the range: ' ' TO '~' (0x20 TO 0x7E). |
| private (adj) | INTERNAL (adj). The CMake word `PRIVATE` is code. |
| produce (v) | MAKE (v), CAUSE (v), GIVE (v), SUPPLY (v) |
| progress (n) | IN PROGRESS (adv), CONTINUE (v) |
| provide (v) | SUPPLY (v), GIVE (v) |
| public (adj) | API (TN): API FUNCTION (TN). `PUBLIC` in CMake is code. |
| pulse (n, v) | SET (v) ... LOW, THEN ... HIGH |
| punning (n) | ACCESS (n) WITH A DIFFERENT C TYPE (n) |
| quiet (adj) | STOP (v), SEND (v) THE LAST BIT (TN), DISABLE (TV) |
| raise (v) | INCREASE (v) for a value, SET (v) ... HIGH for a pin |
| raster (n) | ROW (n) BY ROW, FROM LEFT TO RIGHT |
| rate (v), rated | SPECIFIED (adj), RATING (TN) |
| rather than | NOT (adv), AS AN ALTERNATIVE (n) TO |
| re- (prefix): redraw, repaint, reselect, resend, restart, reuse | the verb with AGAIN (adv): DRAW (TV) ... AGAIN, SELECT (v) ... AGAIN, SEND (v) ... AGAIN, START (v) ... AGAIN, USE (v) ... AGAIN |
| reach (v) | GET (v), GO (v), EXTEND (v), BE (v) |
| read-modify-write (n) | READ (v), CHANGE (v) AND WRITE (v) ... AGAIN |
| reading (n) | INDICATION (n), or READ (v) |
| real (adj) | CORRECT (adj): IN CORRECT OPERATION |
| recently (adv) | LAST (adj) |
| reconfigure (v) | CHANGE (v) THE CONFIGURATION (TN), SET (v) ... AGAIN |
| recovery (n) | PROCEDURE (n): THE PROCEDURE THAT STARTS THE SPI AGAIN |
| rect (n) | RECTANGLE (TN) |
| recurse (v) | CALL (TV) THE SAME FUNCTION (TN) AGAIN |
| reentrant (adj) | A CALL (TN) MUST NOT START BEFORE THE END OF THE PREVIOUS CALL |
| refuse (v) | REJECT (v) |
| region (n) | AREA (n) |
| remain (v) | STAY (v), REMAINING (adj) |
| remember (v) | RECORD (v), KEEP (v) |
| render (v) | DRAW (TV), WRITE (v) |
| repeat (v) | DO (v) ... AGAIN, AGAIN (adv), or give the number |
| report (v) | SHOW (v), RECORD (v), TELL (v), RETURN (TV). You can use REPORT (n). |
| request (n, v) | TELL (v). Use "request" only in DMA REQUEST (TN). |
| requirement (n) | NECESSARY (adj), CONDITION (n) |
| reset (v) | SET (v), DO (v) A RESET (TN). Use RESET (TN) only as a noun. |
| response (n) | WHEN ... OCCURS (v), AFTER (prep) |
| rest (n) | REMAINING (adj), OTHER (adj) |
| reversed (adj) | OPPOSITE (adj) |
| rightmost (adj) | AT THE RIGHT (adj) END (n) |
| ring (n) | CONTOUR (n) OF THE CIRCLE (TN) |
| rise (v) | INCREASE (v), GO (v) TO 1 |
| robust (adj) | SAFE (adj) IF ERRORS (n) OCCUR |
| room (n) | SPACE (n) |
| rotate (v) | TURN (v) |
| roughly (adv) | APPROXIMATELY (adv) |
| rule (n) (in the code text) | INSTRUCTION (n), CONDITION (n). Use RULE (TN) only for a rule of ASD-STE100. |
| run (n) | HORIZONTAL (adj) LINE (TN), VERTICAL (adj) LINE (TN), ROW (n) OF PIXELS (TN) |
| run (v), ran, running | OPERATE (v), OCCUR (v), GO (v), EXTEND (v). For code: CALL (TV). For a transfer: IN PROGRESS (adv). |
| save (v) | USE (v) LESS (adj), DECREASE (v), PREVENT (v), KEEP (v) |
| scale (v) | MULTIPLY (v). Use "scale" only in VOLTAGE SCALE (TN). |
| scattered (adj) | AT MANY DIFFERENT (adj) POSITIONS (n), NOT ADJACENT (adj) |
| scratch buffer | TEXT BUFFER (TN) |
| search (v) | FIND (v), EXAMINE (v) |
| separate (adj, v) | DIFFERENT (adj), ISOLATED (adj), NOT CONNECTED, DIVIDE (v), DISCONNECT (v) |
| separately (adv) | INDEPENDENTLY (adv) |
| setting (n) | VALUE (n), ADJUSTMENT (n), CONFIGURATION MACRO (TN) |
| settle (v) | COMPLETE (v), STABLE (adj), WAIT (v) |
| setup (n) | CONFIGURATION (TN). For the verb: SET (v), PREPARE (v). |
| seven, ten (numbers) | 7, 10 |
| several (adj) | TWO (TN) OR MORE, SOME (adj), or the number |
| share (v) | HAVE (v) THE SAME, USE (v), BE (v) ON THE SAME |
| sheared (adj) | NOT ALIGNED (adj) |
| shift (v) | MOVE (v), SEND (v), TRANSMIT (v) |
| should (v) | MUST (v), RECOMMEND (v), CAN (v) |
| sideways (adv) | AT THE LEFT OR RIGHT SIDE (n) |
| simply (adv) | ONLY (adv), or no word |
| single (adj) | ONE (TN) |
| sit (v) | BE (v), LOCATION (n) |
| size (n), sized | DIMENSION (n), LENGTH (n), NUMBER (n) OF PIXELS (TN), NUMBER OF BYTES (TN). For text: the parameter `size`. |
| skip (v) | IGNORE (v), GO (v) TO, PREVENT (v) |
| sleep (v) | BE (v) IN SLEEP MODE (TN), STOP (v) THE CPU (TN) UNTIL THE NEXT INTERRUPT (TN) |
| so (adv, conj) | THUS (adv), AS A RESULT, BECAUSE (conj) |
| so far | UNTIL THIS TIME |
| so that (conj) | TO and a verb, UNTIL (prep), PREVENT (v) |
| soon (adv), sooner | A SHORT TIME AFTER, WHEN (conj), IN LESS TIME, MORE QUICKLY (adv) |
| spacing (n) | DISTANCE (n), EMPTY (adj) |
| span (n) | RANGE (n), ROW (n) OF PIXELS (TN) |
| spec (n) | SPECIFIED (adj) LIMIT (n) |
| spend (v), spent | No word: THE TIME IN THIS FUNCTION (TN) |
| spin (v) | WAIT LOOP (TN) |
| split (v) | DIVIDE (v) |
| spurious (adj) | UNWANTED (adj) |
| stale (adj) | REMAINING (adj), PREVIOUS (adj), UNWANTED (adj) |
| stall (n, v) | STOP (v), STOPPED (adj), WAIT (v) |
| stand still (v), stood | STOP (v) |
| state (n) | CONDITION (n), DATA (n) |
| state (v) | TELL (v) |
| statics (n) | `static` VARIABLES (TN) |
| step (v) | MOVE (v) ONE STEP (n) |
| sticky (adj) | STAY (v) SET UNTIL ... |
| still (adv) | ALSO (adv), CONTINUE (v), STAY (v), or no word |
| store (n, v) | WRITE (v), KEEP (v), CONTAIN (v) |
| straightforward (adj) | USUAL (adj), EASY (adj) |
| stray (adj) | UNWANTED (adj) |
| stuck (adj) | THAT DOES NOT COMPLETE (v), THAT STOPPED (v) |
| suit (v) | FOR (prep), APPLICABLE (adj) |
| summary (n) | RESULTS (n). `SUMMARY_MS` is code. |
| support (n, v) | CAN (v), LET (v) |
| survive (v) | STAY (v), KEEP (v) |
| swap (v) | INTERCHANGE (v) |
| switch (v) | SET (v), CHANGE (v), SELECT (v), ENERGIZE (v), DE-ENERGIZE (v) |
| switchable (adj) | YOU CAN CHANGE (v) |
| take (v), taken | USE (v), GET (v), READ (v), PUT (v). For a time: THE TIME FOR ... IS ... |
| tall (adj) | LONG (adj), HIGH (adj), VERTICAL (adj) |
| tenths (n) | x 10, IN UNITS OF 0.1 |
| test (v) | DO A TEST (n), EXAMINE (v), USE (v) |
| text size | The parameter `size`. |
| ticker (n) | TEXT (TN) THAT MOVES ACROSS THE SCREEN (TN) |
| tie (n, v) | SAME (adj) DISTANCE (n), EQUALLY (adv) NEAR, SELECT (v) |
| time out (v) | A TIMEOUT (TN) OCCURS |
| timing (n) | TIME (n) |
| tiny (adj) | VERY SMALL (adj) |
| toggle (v) | CHANGE (v) |
| toward (prep) | TO (prep), PART (n) OF |
| track (v) | RECORD (v), MONITOR (v) |
| transfer (v) | SEND (v), MOVE (v). Use TRANSFER (TN) only as a noun. |
| twice (adv) | TWO (TN) TIMES |
| unaligned (adj) | NOT ALIGNED (adj) |
| unchanged (adj) | THAT DOES NOT CHANGE (v) |
| under (prep) | LESS THAN, BELOW (prep), IN (prep) |
| unlike (prep) | DIFFERENT (adj) FROM |
| unused (adj) | DOES NOT USE (v) |
| update (v) | SHOW (v), CHANGE (v) |
| user (n) | YOU (pron), YOUR (adj), APPLICATION (TN). Use USER (TN) only for the person who uses Claude. The label USER on the button stays. |
| using | USE (v), WITH (prep) |
| valid (adj) | CORRECT (adj), APPLICABLE (adj) |
| vendor (n) | MANUFACTURER (TN) |
| verify (v) | MAKE SURE (v), TEST (n) |
| version (n) | PORT (TN) |
| visible (adj) | ON THE SCREEN (TN), THAT YOU CAN SEE (v) |
| wake (v) | LET (v) ... CONTINUE (v), START (v) ... AGAIN |
| way (n) | METHOD (n), PROCEDURE (n), DIRECTION (n), MODE (n) |
| well (adv) | MUCH (adv), FAR (adj), CORRECTLY (adv) |
| what (pron) | WHICH (pron), THE ... THAT |
| whatever (pron) | THE ... THAT, ALL (adj) |
| whenever (conj) | WHEN (conj), IF (conj) |
| whether (conj) | IF (conj) |
| whichever (pron) | THAT (pron), WHICH (pron) |
| whole (adj) | ALL (adj), FULL (adj), FULLY (adv) |
| whose (pron) | OF (prep), WITH (prep), THAT HAS |
| widen (v) | EXTEND (v) |
| wiring (n) | WIRES (TN), CONNECTIONS (n) |
| within (prep) | IN (prep), IN LESS THAN |
| work out (v) | CALCULATE (v) |
| working set | THE VALUES (n) THAT THE PORT WRITES |
| would (v) | CAN (v), WILL (v), or a verb without WOULD |
| wrap (v) | CONTINUE (v) ON THE NEXT ROW (n), GO (v) BACK TO 0. `ST7789_SetTextWrap()` is code. |
| wrong (adj) | INCORRECT (adj), INCORRECTLY (adv) |
| zero (v), zeroed | SET (v) ... TO 0 |
| calling, debugging, disabling, drawing, enabling, flushing, returning (as verbs) | the same technical verb, as in these examples: BEFORE YOU CALL (TV) IT, BEFORE IT RETURNS (TV) (rule 3.5) |
| can't, doesn't, don't (contractions) | CANNOT (v), DOES NOT, DO NOT (rule 4.2) |
| arming, attaching, being, changing, doing, going, holding, moving, sending, showing, starting, stopping, trying, waiting, writing | the same verb, as in these examples: BEFORE THE PORT ARMS TXC, WHILE THE CPU SENDS DATA (rule 3.5) |

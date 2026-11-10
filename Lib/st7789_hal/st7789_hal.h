/**
 * ST7789 HAL library: a library for ST7789 panels on all STM32 MCUs with
 * the ST HAL.
 *
 * The library uses only the HAL functions for the SPI and for the pins.
 * Thus, the library operates on all the STM32 families with the HAL.
 * Examples are F0, F1, F4, G0, G4, H7, L4 and U5. The library has no
 * interrupt handlers and no HAL callbacks.
 *
 * Instructions:
 * - In STM32CubeMX, set the SPI to master, 8-bit data, CPOL = 0, CPHA = 0
 *   (SPI mode 0) and MSB first.
 * - Set the DC pin, the CS pin, the RST pin and the backlight pin as GPIO
 *   outputs. Only the DC pin is necessary.
 * - To use the DMA, add a TX DMA stream (or channel) to the SPI in
 *   STM32CubeMX.
 * - Enable the DMA interrupt and the SPI interrupt. Then set `use_dma` to 1.
 * - Put the values in an ST7789HAL_Config. Then call ST7789HAL_Init.
 *
 * The library does not use the RX line of the SPI. If `use_dma` is 1, the
 * library waits for the end of each DMA transfer.
 *
 * Each function returns after the SPI sent all the data of the call. The
 * colors are RGB565 values in the byte sequence of the CPU. The library
 * sends the high byte first.
 *
 * The drawing functions clip the shapes at the edges of the screen. The
 * coordinates can be less than 0 or out of the screen. A width or a height
 * of 0 or less gives no pixels.
 *
 * All the handles use one transmit buffer. Thus, a call must not start
 * before the end of the previous call, also for different handles. Obey
 * these instructions:
 * - Call the library from only one context, for example the loop in main()
 *   or one RTOS task.
 * - Do not call the library from an interrupt handler. The HAL uses
 *   interrupts for the DMA transfers and HAL_GetTick for the timeouts.
 */
#ifndef ST7789_HAL_H
#define ST7789_HAL_H

#include <stdint.h>
#include "st7789_hal_config.h"
#include ST7789HAL_HAL_HEADER

#ifdef __cplusplus
extern "C" {
#endif

/* ---- status and errors -------------------------------------------------- */

typedef enum
{
  ST7789HAL_OK = 0,
  ST7789HAL_ERR_PARAM,       /* a parameter of ST7789HAL_Init is not correct */
  ST7789HAL_ERR_SPI,         /* a HAL SPI call did not return HAL_OK */
  ST7789HAL_ERR_TIMEOUT      /* a DMA transfer did not complete */
} ST7789HAL_Status;

/* The error bits for ST7789HAL_GetErrors. An error bit stays set until a
 * call to ST7789HAL_ClearErrors or ST7789HAL_Init. */
#define ST7789HAL_ERROR_SPI      0x01u   /* a HAL SPI call did not return HAL_OK */
#define ST7789HAL_ERROR_TIMEOUT  0x02u   /* a DMA transfer did not complete */

/* ---- rotation ----------------------------------------------------------- */

typedef enum
{
  ST7789HAL_PORTRAIT = 0,
  ST7789HAL_LANDSCAPE,           /* turned 90 degrees from portrait */
  ST7789HAL_PORTRAIT_FLIPPED,    /* turned 180 degrees from portrait */
  ST7789HAL_LANDSCAPE_FLIPPED    /* turned 270 degrees from portrait */
} ST7789HAL_Rotation;

/* ---- configuration and handle ------------------------------------------- */

/* The configuration of one panel. ST7789HAL_Init copies it into the handle. */
typedef struct
{
  SPI_HandleTypeDef *hspi;                    /* necessary */
  GPIO_TypeDef *dc_port;  uint16_t dc_pin;    /* necessary */
  GPIO_TypeDef *cs_port;  uint16_t cs_pin;    /* NULL: the CS pin of the panel connects to GND */
  GPIO_TypeDef *rst_port; uint16_t rst_pin;   /* NULL: the library sends SWRESET */
  GPIO_TypeDef *bl_port;  uint16_t bl_pin;    /* NULL: no backlight pin */
  uint8_t  bl_active_low;                     /* 1: the backlight is on when the pin is low */
  uint16_t width, height;                     /* the panel dimensions in portrait, max 240 x 320 */
  uint16_t x_offset, y_offset;                /* panel position in the 240x320 controller memory, portrait */
  uint8_t  invert_colors;                     /* 1 for most IPS panels */
  uint8_t  bgr_order;                         /* 1 if the panel interchanges red and blue */
  uint8_t  use_dma;                           /* 1: HAL_SPI_Transmit_DMA for long transfers (hspi->hdmatx must be set) */
} ST7789HAL_Config;

/* The data of one panel. The application supplies the memory for the
 * handle, and ST7789HAL_Init sets all its fields. Do not change the fields. */
typedef struct
{
  ST7789HAL_Config cfg;
  uint16_t width, height;       /* for the rotation at this time */
  uint16_t x_start, y_start;    /* the offsets for the rotation at this time */
  uint8_t  rotation, errors, text_wrap, text_transparent, initialized;
  uint8_t  window_state;        /* for the library only: the condition of the
                                   window of ST7789HAL_SetWindow */
} ST7789HAL_Handle;

/* ---- colors (RGB565) ---------------------------------------------------- */

/* This macro makes an RGB565 color from 8-bit red, green and blue values */
#define ST7789HAL_RGB565(r, g, b) \
  ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | (((b) & 0xFF) >> 3)))

#define ST7789HAL_BLACK    0x0000u
#define ST7789HAL_WHITE    0xFFFFu
#define ST7789HAL_RED      0xF800u
#define ST7789HAL_GREEN    0x07E0u
#define ST7789HAL_BLUE     0x001Fu
#define ST7789HAL_YELLOW   0xFFE0u
#define ST7789HAL_CYAN     0x07FFu
#define ST7789HAL_MAGENTA  0xF81Fu

/* The width and the height in pixels of the rectangle of one character,
 * if `size` is 1. A character is 5 x 7 pixels. The rectangle also has 1
 * empty column and 1 empty row. */
#define ST7789HAL_CHAR_W 6
#define ST7789HAL_CHAR_H 8

/* ---- control ------------------------------------------------------------ */

/* This function examines the configuration, does a reset of the
 * controller and prepares it. Then it fills the screen with black and sets
 * the backlight to on. The time for the function is approximately 250 ms.
 *
 * If a configuration value is not correct, the function returns
 * ST7789HAL_ERR_PARAM. An example is an SPI without 8-bit data. In this
 * condition, the function does not use the SPI or the pins. After an error,
 * you can call the function again. */
ST7789HAL_Status ST7789HAL_Init(ST7789HAL_Handle *h, const ST7789HAL_Config *config);

/* After this call, the coordinates, the width and the height are for the
 * new rotation. The pixels on the screen do not change. */
void     ST7789HAL_SetRotation(ST7789HAL_Handle *h, ST7789HAL_Rotation rotation);
ST7789HAL_Rotation ST7789HAL_GetRotation(const ST7789HAL_Handle *h);

/* The width and the height of the screen for the rotation at this time */
uint16_t ST7789HAL_Width(const ST7789HAL_Handle *h);
uint16_t ST7789HAL_Height(const ST7789HAL_Handle *h);

/* `on` = 1 sets the backlight to on. `on` = 0 sets it to off. */
void     ST7789HAL_Backlight(ST7789HAL_Handle *h, uint8_t on);

/* `on` = 0: the screen shows no image (DISPOFF), and the controller memory
 * keeps the pixels. `on` = 1: the screen shows the pixels again (DISPON). */
void     ST7789HAL_DisplayOn(ST7789HAL_Handle *h, uint8_t on);

/* `sleep` = 1 starts sleep mode (SLPIN). `sleep` = 0 stops sleep mode
 * (SLPOUT). Each call waits 120 ms after the command. */
void     ST7789HAL_Sleep(ST7789HAL_Handle *h, uint8_t sleep);

/* `invert` = 1 shows each color as its opposite color. `invert` = 0 shows
 * the correct colors. The function uses the `invert_colors` value of the
 * configuration. Thus, `invert` = 0 gives the correct colors on all
 * panels. */
void     ST7789HAL_InvertColors(ST7789HAL_Handle *h, uint8_t invert);

/* This function returns the error bits (ST7789HAL_ERROR_SPI and
 * ST7789HAL_ERROR_TIMEOUT). After an error, a function stops the remaining
 * transfers of that call. It returns the error bits also when
 * ST7789HAL_Init did not complete correctly. */
uint8_t  ST7789HAL_GetErrors(const ST7789HAL_Handle *h);
void     ST7789HAL_ClearErrors(ST7789HAL_Handle *h);

/* ---- direct access to the controller memory ------------------------------ */

/* Use these functions to send pixels that the application calculates, for
 * example from LVGL. First, set a window. Then send its pixels in one or
 * more calls to ST7789HAL_WritePixels (row by row, from left to right).
 *
 * The window must be fully on the screen. If it is not, ST7789HAL_SetWindow
 * has no effect. Then ST7789HAL_WritePixels has no effect until you set a
 * correct window.
 *
 * The first ST7789HAL_WritePixels call after ST7789HAL_SetWindow sends
 * RAMWR. Each subsequent call sends RAMWRC (memory write continue). Thus, CS
 * can go high between the calls.
 *
 * A call to a different function that sends a command ends the window. An
 * example is a drawing function. After this call, ST7789HAL_WritePixels has
 * no effect until you set a new window. */
void ST7789HAL_SetWindow(ST7789HAL_Handle *h, uint16_t x, uint16_t y, uint16_t w, uint16_t h_px);
void ST7789HAL_WritePixels(ST7789HAL_Handle *h, const uint16_t *pixels, uint32_t count);

/* ---- drawing functions --------------------------------------------------- */

void ST7789HAL_FillScreen(ST7789HAL_Handle *h, uint16_t color);
void ST7789HAL_FillRect(ST7789HAL_Handle *h, int16_t x, int16_t y, int16_t w, int16_t h_px, uint16_t color);
void ST7789HAL_DrawPixel(ST7789HAL_Handle *h, int16_t x, int16_t y, uint16_t color);
void ST7789HAL_DrawHLine(ST7789HAL_Handle *h, int16_t x, int16_t y, int16_t w, uint16_t color);
void ST7789HAL_DrawVLine(ST7789HAL_Handle *h, int16_t x, int16_t y, int16_t h_px, uint16_t color);
void ST7789HAL_DrawLine(ST7789HAL_Handle *h, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);

/* The contour of a rectangle, 1 pixel wide */
void ST7789HAL_DrawRect(ST7789HAL_Handle *h, int16_t x, int16_t y, int16_t w, int16_t h_px, uint16_t color);

/* A circle with its middle at (`cx`, `cy`). `r` = 0 gives 1 pixel. */
void ST7789HAL_DrawCircle(ST7789HAL_Handle *h, int16_t cx, int16_t cy, int16_t r, uint16_t color);
void ST7789HAL_FillCircle(ST7789HAL_Handle *h, int16_t cx, int16_t cy, int16_t r, uint16_t color);

void ST7789HAL_DrawTriangle(ST7789HAL_Handle *h, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color);
void ST7789HAL_FillTriangle(ST7789HAL_Handle *h, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color);

/* The image has `w` x `h_px` RGB565 pixels, row by row, in the byte
 * sequence of the CPU. The function clips the image at the edges of the
 * screen. */
void ST7789HAL_DrawImage(ST7789HAL_Handle *h, int16_t x, int16_t y, int16_t w, int16_t h_px, const uint16_t *pixels);

/* ---- text ---------------------------------------------------------------- */

/* Each character uses a rectangle of (6 * `size`) x (8 * `size`) pixels.
 * (`x`, `y`) is the top left corner of the first rectangle. A `size` of 0
 * gives 1, and a `size` more than ST7789HAL_MAX_TEXT_SIZE gives
 * ST7789HAL_MAX_TEXT_SIZE. The font has the characters from ' ' to '~'.
 * The functions show other characters as '?'.
 *
 * In ST7789HAL_DrawString:
 * - `'\n'` moves the next character to the start of the next row (at `x`).
 * - The function ignores `'\r'`.
 * - The function stops at the bottom of the screen.
 * - Refer to ST7789HAL_SetTextWrap for the right edge of the screen. */
void ST7789HAL_DrawChar(ST7789HAL_Handle *h, int16_t x, int16_t y, char c, uint16_t fg, uint16_t bg, uint8_t size);
void ST7789HAL_DrawString(ST7789HAL_Handle *h, int16_t x, int16_t y, const char *str, uint16_t fg, uint16_t bg, uint8_t size);

/* - `wrap` = 1 (the default): if a row has no space for the next character,
 *   the text continues on the next row.
 * - `wrap` = 0: the function clips the text at the right edge of the
 *   screen. */
void ST7789HAL_SetTextWrap(ST7789HAL_Handle *h, uint8_t wrap);

/* - `transparent` = 1: the text functions draw only the pixels of the
 *   characters. The other pixels do not change.
 * - `transparent` = 0 (the default): the text functions also fill the
 *   rectangle of each character with the background color. */
void ST7789HAL_SetTextTransparent(ST7789HAL_Handle *h, uint8_t transparent);

#ifdef __cplusplus
}
#endif

#endif /* ST7789_HAL_H */

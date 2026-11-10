/**
 * ST7789 HAL library: the internal functions that st7789_hal.c gives to
 * st7789_hal_gfx.c. The application must not use them.
 */
#ifndef ST7789_HAL_INTERNAL_H
#define ST7789_HAL_INTERNAL_H

#include "st7789_hal.h"

/* The transmit buffer. The library writes the pixels into it with the high
 * byte first (refer to st7789hal_swap). The DMA reads only this buffer. */
extern uint16_t st7789hal_buf[ST7789HAL_BUFFER_PIXELS];

/* This function returns the RGB565 value with its two bytes interchanged.
 * On all STM32 MCUs, the low byte of a 16-bit value is at the lower
 * address. Thus, in the transmit buffer, the high byte of the color is
 * first, as the controller must receive it. */
static inline uint16_t st7789hal_swap(uint16_t v)
{
  return (uint16_t)((uint16_t)(v >> 8) | (uint16_t)(v << 8));
}

/* This function clips the rectangle (`x`, `y`, `w`, `hh`) at the edges of
 * the screen. If a part of the rectangle is on the screen, the function
 * returns 1 and the area from (`*x0`, `*y0`) to (`*x1`, `*y1`). Column
 * `*x1` and row `*y1` are not in the area. If no part of the rectangle is
 * on the screen, the function returns 0. */
int st7789hal_clip(const ST7789HAL_Handle *h, int32_t x, int32_t y, int32_t w, int32_t hh,
                   int32_t *x0, int32_t *y0, int32_t *x1, int32_t *y1);

/* This function clips the rectangle and fills the part on the screen with
 * one color. */
void st7789hal_rect(ST7789HAL_Handle *h, int32_t x, int32_t y, int32_t w, int32_t hh,
                    uint16_t color);

/* These functions send pixels to a window that is fully on the screen:
 * - st7789hal_ram_begin sets the window and sends RAMWR. CS stays low.
 * - st7789hal_send_buf sends the first `n` pixels of the transmit buffer
 *   (n <= ST7789HAL_BUFFER_PIXELS). It returns 0 if an error occurred in
 *   this operation. After an error, it does not send more data.
 * - st7789hal_ram_end sets CS high. */
void st7789hal_ram_begin(ST7789HAL_Handle *h, uint32_t x, uint32_t y, uint32_t w, uint32_t hh);
int  st7789hal_send_buf(ST7789HAL_Handle *h, uint32_t n);
void st7789hal_ram_end(ST7789HAL_Handle *h);

#endif /* ST7789_HAL_INTERNAL_H */

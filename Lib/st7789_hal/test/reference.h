/**
 * The drawing functions of the tests. They draw into a framebuffer of the
 * panel (in portrait) one pixel at a time, with simple algorithms. The
 * tests compare the memory of the simulated controller with this
 * framebuffer.
 *
 * The coordinates are for the rotation at this time, as in the library:
 * - ST7789HAL_PORTRAIT: no change.
 * - ST7789HAL_LANDSCAPE: the image turns 90 degrees. The first column of
 *   the screen is the first row of the panel, from right to left.
 * - ST7789HAL_PORTRAIT_FLIPPED and ST7789HAL_LANDSCAPE_FLIPPED: the image
 *   turns 180 degrees more.
 * The library in `Lib/st7789` gives the direction of the 90 degree turn.
 */
#ifndef REFERENCE_H
#define REFERENCE_H

#include <stdint.h>

#define REF_MAX_TEXT_SIZE  8

typedef struct
{
  int32_t  pw, ph;              /* the panel dimensions in portrait */
  int32_t  rotation;
  int32_t  w, h;                /* the screen dimensions for the rotation */
  uint16_t fb[320][240];        /* [panel row][panel column] */
} ref_t;

extern ref_t ref;

void ref_begin(int32_t panel_w, int32_t panel_h, uint16_t fill);
void ref_set_rotation(int32_t rotation);

/* This function calculates the panel coordinates of a point on the
 * screen. It returns 0 if the point is not on the screen. */
int  ref_to_panel(int32_t x, int32_t y, int32_t *px, int32_t *py);

void ref_pixel(int32_t x, int32_t y, uint16_t c);
void ref_fill_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c);
void ref_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c);
void ref_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c);
void ref_circle(int32_t cx, int32_t cy, int32_t r, uint16_t c);
void ref_fill_circle(int32_t cx, int32_t cy, int32_t r, uint16_t c);
void ref_triangle(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint16_t c);
void ref_fill_triangle(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2,
                       uint16_t c);
void ref_image(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t *pixels);
void ref_char(int32_t x, int32_t y, char ch, uint16_t fg, uint16_t bg, int32_t size,
              int transparent);
void ref_string(int32_t x, int32_t y, const char *s, uint16_t fg, uint16_t bg, int32_t size,
                int wrap, int transparent);

#endif /* REFERENCE_H */

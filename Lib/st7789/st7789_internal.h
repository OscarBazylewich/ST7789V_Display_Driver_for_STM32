/**
 * st7789.c (controller, transfers, framebuffer) and st7789_gfx.c (shapes and
 * text) use this header file. It is not part of the API.
 */
#ifndef ST7789_INTERNAL_H
#define ST7789_INTERNAL_H

#include <stdint.h>

#if defined(__GNUC__)
/* This type is for 32-bit accesses to pixel memory. `may_alias` makes these
 * accesses with a different C type correct. */
typedef uint32_t __attribute__((may_alias)) st7789_u32_alias;

/* This function copies `n` pixels, from the first to the last. For as many
 * pixels as possible, it copies two pixels at a time. The parameters `d` and
 * `s` do not use `restrict`: with `restrict`, GCC replaces the loops with a
 * call to memmove() of the C library. In newlib-nano, memmove() copies one
 * byte at a time. An overlap is safe only if `d` is before `s`. */
static inline void st7789_copy16(uint16_t *d, const uint16_t *s, uint32_t n)
{
  if (n >= 2u && ((uintptr_t)d & 2u) != 0u)        /* align `d` */
  {
    *d++ = *s++;
    n--;
  }
  st7789_u32_alias *q = (st7789_u32_alias *)d;
  if (((uintptr_t)s & 2u) == 0u)                   /* `s` is also aligned: copy 8 pixels in each step */
  {
    const st7789_u32_alias *p = (const st7789_u32_alias *)s;
    for (; n >= 8u; n -= 8u)
    {
      uint32_t a = p[0], b = p[1], c = p[2], e = p[3];
      q[0] = a; q[1] = b; q[2] = c; q[3] = e;
      p += 4;
      q += 4;
    }
    for (; n >= 2u; n -= 2u)
    {
      *q++ = *p++;
    }
    s = (const uint16_t *)p;
  }
  else                                             /* `s` is only 2-byte aligned: read 32 bits
                                                      from addresses that are not aligned */
  {
    for (; n >= 4u; n -= 4u)
    {
      uint32_t a, b;
      __builtin_memcpy(&a, s, 4);                  /* GCC uses one LDR if the CPU can read
                                                      32 bits at an address that is not aligned */
      __builtin_memcpy(&b, s + 2, 4);
      q[0] = a;
      q[1] = b;
      s += 4;
      q += 2;
    }
    for (; n >= 2u; n -= 2u)
    {
      uint32_t a;
      __builtin_memcpy(&a, s, 4);
      *q++ = a;
      s += 2;
    }
  }
  if (n != 0u)
  {
    *(uint16_t *)q = *s;
  }
}
#else
static inline void st7789_copy16(uint16_t *d, const uint16_t *s, uint32_t n)
{
  while (n--)
  {
    *d++ = *s++;
  }
}
#endif

/* The width and the height of the screen for the rotation at this time, and
 * the framebuffer (NULL in direct mode) */
extern uint16_t  st7789_width;
extern uint16_t  st7789_height;
extern uint16_t *st7789_fb;

/* These internal functions draw pixels and rectangles. The code that calls
 * them clips the coordinates to the screen before the call. The rectangle is
 * not empty, and it does not include the column `x1` and the row `y1`. None
 * of these functions records a changed area.
 *
 * st7789_fill and st7789_blit draw into the framebuffer or directly into the
 * controller memory. st7789_pixel is only for direct mode: in framebuffer
 * mode, the code that calls these functions writes the pixels directly into
 * the framebuffer. This is much faster than a call. In direct mode,
 * st7789_fill uses the DMA to fill a rectangle of `dma_min` pixels or more
 * (ST7789_DMA_MIN_PIXELS or ST7789_SHAPE_DMA_MIN_PIXELS). */
void st7789_fill(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t color,
                 uint32_t dma_min);
void st7789_blit(int32_t x0, int32_t y0, uint32_t w, uint32_t h,
                 const uint16_t *src, uint32_t stride);
void st7789_pixel(int32_t x, int32_t y, uint16_t color);

/* Framebuffer mode: record that this clipped area changed. The area does not
 * include the column `x1` and the row `y1`. */
void st7789_mark(int32_t x0, int32_t y0, int32_t x1, int32_t y1);

/* A changed area that the library recorded (empty if there is no changed
 * area). st7789_mark_fast first examines it without a function call. Thus,
 * for most small shapes and one-pixel areas, it does not call st7789_mark. */
typedef struct { int16_t x0, y0, x1, y1; } st7789_rect_t;
extern st7789_rect_t st7789_dirty_hint;

static inline void st7789_mark_fast(int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
  const st7789_rect_t *h = &st7789_dirty_hint;
  if (x0 >= h->x0 && y0 >= h->y0 && x1 <= h->x1 && y1 <= h->y1)
  {
    return;
  }
  st7789_mark(x0, y0, x1, y1);
}

/* Clip (x, y, w, h) to the screen. If the clipped area is empty, return 0. */
int  st7789_clip(int32_t x, int32_t y, int32_t w, int32_t h,
                 int32_t *x0, int32_t *y0, int32_t *x1, int32_t *y1);

#endif /* ST7789_INTERNAL_H */

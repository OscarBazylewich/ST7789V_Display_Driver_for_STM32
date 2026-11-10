/**
 * ST7789 library: shapes and text.
 *
 * Each drawing function clips its shape and records its bounding box as a
 * changed area (only in framebuffer mode). Then it draws the shape with the
 * internal functions of st7789.c. In direct mode, each call first waits for
 * the previous transfer. A call that draws no pixels also waits.
 *
 * The library makes each shape from the smallest possible number of
 * rectangles, and each rectangle is as large as possible:
 * - A line: from its horizontal lines or its vertical lines.
 * - DrawCircle: from the horizontal lines and the vertical lines of its
 *   contour.
 * - FillCircle and FillTriangle: from rectangles of equal rows.
 * The library does this because in direct mode, it must set a window for
 * each rectangle. In framebuffer mode, DrawCircle writes its pixels one at
 * a time (not as rectangles), because this is faster.
 *
 * The library draws text one group of adjacent characters at a time:
 * - In direct mode, the library draws the group into a text buffer. Then it
 *   sets one window for the group and sends the group in one transfer. It
 *   does not set one window for each character. The DMA supplies the data
 *   of the transfer. If the group is small, the CPU supplies the data.
 * - In framebuffer mode, the library draws the group directly into the
 *   framebuffer.
 * Transparent text is different: the library draws each horizontal line of
 * character pixels as one rectangle.
 */
#include "st7789.h"
#include "st7789_internal.h"
#include "st7789_font5x7.h"
#include <string.h>

_Static_assert(ST7789_TEXT_BUFFER_PIXELS >=
               (ST7789_CHAR_W * ST7789_MAX_TEXT_SIZE) * (ST7789_CHAR_H * ST7789_MAX_TEXT_SIZE),
               "ST7789_TEXT_BUFFER_PIXELS must hold one character at the largest size");

/* Two text buffers: the library can fill one during the transfer of the other */
static uint16_t text_buf[2][ST7789_TEXT_BUFFER_PIXELS] ST7789_DMA_BUFFER;
static uint8_t  text_next;
static uint8_t  text_wrap = 1;      /* refer to ST7789_SetTextWrap */
static uint8_t  text_transparent;   /* refer to ST7789_SetTextTransparent */

/* ---- internal functions ------------------------------------------------- */

static inline void mark_box(int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
  /* Input: two corners (x0, y0) and (x1, y1) that the area includes.
   * Output: the clipped area from (cx0, cy0) to (cx1, cy1). Column cx1 and
   * row cy1 are not in the clipped area. */
  int32_t cx0, cy0, cx1, cy1;
  if (st7789_fb && st7789_clip(x0, y0, x1 - x0 + 1, y1 - y0 + 1, &cx0, &cy0, &cx1, &cy1))
  {
    st7789_mark_fast(cx0, cy0, cx1, cy1);
  }
}

/* Direct mode: each call waits for the previous transfer. A call that draws
 * no pixels also waits. Thus, after the call, the application can use a
 * buffer again.
 *
 * A call that draws also waits when it starts its first transfer, but it
 * calculates its values before it waits. Thus, the CPU calculates while the
 * previous transfer continues. Because of this, the functions that draw
 * shapes record if they drew (call_begin/call_end). They wait at the end
 * only if they did not draw.
 *
 * In framebuffer mode, the drawing functions do not wait. Thus, they can
 * draw during a flush. */
static uint8_t drew;                /* rect_raw, pixel_raw and draw_run set it
                                       to 1 when they draw */

static inline void direct_sync(void)
{
  if (st7789_fb == NULL)
  {
    ST7789_WaitIdle();
  }
}

static inline void call_begin(void)
{
  drew = 0;
}

static inline void call_end(void)
{
  if (!drew)
  {
    direct_sync();
  }
}

/* The internal functions that follow clip a shape and draw it, but they do
 * not record a changed area. The drawing functions use them. The library
 * makes shapes and transparent text from rectangles that rect_raw draws.
 * Thus, rect_raw uses the limit ST7789_SHAPE_DMA_MIN_PIXELS. */
static void rect_raw(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t color)
{
  int32_t x0, y0, x1, y1;
  if (st7789_clip(x, y, w, h, &x0, &y0, &x1, &y1))
  {
    drew = 1;
    st7789_fill(x0, y0, x1, y1, color, (uint32_t)ST7789_SHAPE_DMA_MIN_PIXELS);
  }
}

static inline void pixel_raw(int32_t x, int32_t y, uint16_t color)
{
  if ((uint32_t)x < st7789_width && (uint32_t)y < st7789_height)
  {
    drew = 1;
    if (st7789_fb)
    {
      st7789_fb[(uint32_t)y * st7789_width + (uint32_t)x] = color;
    }
    else
    {
      st7789_pixel(x, y, color);
    }
  }
}

#define SWAP(a, b) do { int32_t t_ = (a); (a) = (b); (b) = t_; } while (0)

/* abs() has an `int` parameter, and on some MCUs, an `int` has 16 bits. In
 * this file, a difference between two coordinates can be as large as
 * 65535. */
static inline int32_t abs32(int32_t v)
{
  return (v < 0) ? -v : v;
}

/* ---- rectangles and lines ----------------------------------------------- */

void ST7789_FillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
  int32_t x0, y0, x1, y1;
  if (st7789_clip(x, y, w, h, &x0, &y0, &x1, &y1))
  {
    if (st7789_fb)
    {
      st7789_mark_fast(x0, y0, x1, y1);
    }
    st7789_fill(x0, y0, x1, y1, color, (uint32_t)ST7789_DMA_MIN_PIXELS);
  }
  else
  {
    direct_sync();
  }
}

void ST7789_FillScreen(uint16_t color)
{
  ST7789_FillRect(0, 0, (int16_t)st7789_width, (int16_t)st7789_height, color);
}

void ST7789_DrawPixel(int16_t x, int16_t y, uint16_t color)
{
  if ((uint32_t)x < st7789_width && (uint32_t)y < st7789_height)
  {
    if (st7789_fb)
    {
      st7789_mark_fast(x, y, x + 1, y + 1);
      st7789_fb[(uint32_t)y * st7789_width + (uint32_t)x] = color;
    }
    else
    {
      st7789_pixel(x, y, color);
    }
  }
  else
  {
    direct_sync();
  }
}

void ST7789_DrawHLine(int16_t x, int16_t y, int16_t w, uint16_t color)
{
  ST7789_FillRect(x, y, w, 1, color);
}

void ST7789_DrawVLine(int16_t x, int16_t y, int16_t h, uint16_t color)
{
  ST7789_FillRect(x, y, 1, h, color);
}

/* In framebuffer mode, this function draws lines with the same steps as
 * the direct-mode code in line_raw:
 * - The code puts the two ends in a sequence in which the coordinate on
 *   the major axis increases.
 * - `err` starts at half of the difference on the major axis.
 * Thus, the two modes draw the same pixels. These pixels do not change if
 * you interchange the two ends of the line. This function writes one pixel
 * in each step.
 *
 * side_span (FillTriangle) calculates the result of the same steps
 * directly, without a loop. Keep the steps of the three functions the same. */
static void line_fb(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t color)
{
  uint32_t w = st7789_width, h = st7789_height;
  uint16_t *fb = st7789_fb;

  if (abs32(x1 - x0) >= abs32(y1 - y0))
  {
    if (x0 > x1) { SWAP(x0, x1); SWAP(y0, y1); }
    int32_t dx = x1 - x0, dy = abs32(y1 - y0), sy = (y0 < y1) ? 1 : -1;
    int32_t err = dx / 2, y = y0;
    for (int32_t x = x0; x <= x1; x++)
    {
      if ((uint32_t)x < w && (uint32_t)y < h)
      {
        fb[(uint32_t)y * w + (uint32_t)x] = color;
      }
      err -= dy;
      if (err < 0) { y += sy; err += dx; }
    }
  }
  else
  {
    if (y0 > y1) { SWAP(x0, x1); SWAP(y0, y1); }
    int32_t dy = y1 - y0, dx = abs32(x1 - x0), sx = (x0 < x1) ? 1 : -1;
    int32_t err = dy / 2, x = x0;
    for (int32_t y = y0; y <= y1; y++)
    {
      if ((uint32_t)x < w && (uint32_t)y < h)
      {
        fb[(uint32_t)y * w + (uint32_t)x] = color;
      }
      err -= dx;
      if (err < 0) { x += sx; err += dy; }
    }
  }
}

/* In direct mode, this function uses Bresenham's algorithm. It divides the
 * line into horizontal lines or vertical lines. It draws each of them as
 * one thin rectangle, because the library must set a window for each
 * rectangle. */
static void line_raw(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t color)
{
  if (y0 == y1)
  {
    if (x0 > x1) SWAP(x0, x1);
    rect_raw(x0, y0, x1 - x0 + 1, 1, color);
    return;
  }
  if (x0 == x1)
  {
    if (y0 > y1) SWAP(y0, y1);
    rect_raw(x0, y0, 1, y1 - y0 + 1, color);
    return;
  }
  if (st7789_fb)
  {
    line_fb(x0, y0, x1, y1, color);
    return;
  }

  if (abs32(x1 - x0) >= abs32(y1 - y0))
  {
    /* The major axis is x: the parts are horizontal lines */
    if (x0 > x1) { SWAP(x0, x1); SWAP(y0, y1); }
    int32_t dx = x1 - x0, dy = abs32(y1 - y0), sy = (y0 < y1) ? 1 : -1;
    int32_t err = dx / 2, y = y0, run = x0;
    for (int32_t x = x0; x <= x1; x++)
    {
      err -= dy;
      if (err < 0)
      {
        rect_raw(run, y, x - run + 1, 1, color);
        y   += sy;
        err += dx;
        run  = x + 1;
      }
    }
    if (run <= x1)
    {
      rect_raw(run, y, x1 - run + 1, 1, color);
    }
  }
  else
  {
    /* The major axis is y: the parts are vertical lines */
    if (y0 > y1) { SWAP(x0, x1); SWAP(y0, y1); }
    int32_t dy = y1 - y0, dx = abs32(x1 - x0), sx = (x0 < x1) ? 1 : -1;
    int32_t err = dy / 2, x = x0, run = y0;
    for (int32_t y = y0; y <= y1; y++)
    {
      err -= dx;
      if (err < 0)
      {
        rect_raw(x, run, 1, y - run + 1, color);
        x   += sx;
        err += dy;
        run  = y + 1;
      }
    }
    if (run <= y1)
    {
      rect_raw(x, run, 1, y1 - run + 1, color);
    }
  }
}

void ST7789_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color)
{
  call_begin();
  mark_box((x0 < x1) ? x0 : x1, (y0 < y1) ? y0 : y1,
           (x0 > x1) ? x0 : x1, (y0 > y1) ? y0 : y1);
  line_raw(x0, y0, x1, y1, color);
  call_end();
}

void ST7789_DrawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
  if (w <= 0 || h <= 0)
  {
    direct_sync();
    return;
  }
  if (w <= 2 || h <= 2)          /* the rectangle has no empty inner area:
                                    the function draws one filled rectangle */
  {
    ST7789_FillRect(x, y, w, h, color);
    return;
  }
  call_begin();
  int32_t right = (int32_t)x + w - 1, bottom = (int32_t)y + h - 1;
  mark_box(x, y, right, bottom);
  rect_raw(x, y, w, 1, color);
  rect_raw(x, bottom, w, 1, color);
  rect_raw(x, (int32_t)y + 1, 1, h - 2, color);
  rect_raw(right, (int32_t)y + 1, 1, h - 2, color);
  call_end();
}

/* ---- circles ------------------------------------------------------------ */

/* Direct mode: this function draws the row of pixels x = a..b at height y
 * of the octant (a <= b <= y) at its 8 symmetrical positions. In the
 * sequence of the rectangles, each window has the same rows or the same
 * columns as the previous window. Thus, the library sends only one of the
 * two window commands (CASET or RASET) again, because it records the last
 * window. */
static void circle_runs(int32_t cx, int32_t cy, int32_t a, int32_t b, int32_t y, uint16_t color)
{
  if (a == 0)                          /* the row starts on the axis: each
                                          rectangle contains two symmetrical
                                          positions of the row, one on each
                                          side of the axis */
  {
    int32_t n = 2 * b + 1;
    rect_raw(cx - b, cy + y, n, 1, color);
    rect_raw(cx - b, cy - y, n, 1, color);
    rect_raw(cx + y, cy - b, 1, n, color);
    rect_raw(cx - y, cy - b, 1, n, color);
    return;
  }
  int32_t n = b - a + 1;
  int32_t m = (b == y) ? n - 1 : n;    /* if b = y, the horizontal lines include
                                          the pixel (y, y). Thus, the vertical
                                          lines are 1 pixel shorter. */
  rect_raw(cx + a, cy + y, n, 1, color);
  rect_raw(cx - b, cy + y, n, 1, color);
  rect_raw(cx - b, cy - y, n, 1, color);
  rect_raw(cx + a, cy - y, n, 1, color);
  if (m > 0)
  {
    rect_raw(cx + y, cy + a,         1, m, color);
    rect_raw(cx + y, cy - a - m + 1, 1, m, color);
    rect_raw(cx - y, cy - a - m + 1, 1, m, color);
    rect_raw(cx - y, cy + a,         1, m, color);
  }
}

void ST7789_DrawCircle(int16_t cx_, int16_t cy_, int16_t r_, uint16_t color)
{
  if (r_ < 0)
  {
    direct_sync();
    return;
  }
  call_begin();
  int32_t cx = cx_, cy = cy_, r = r_;
  mark_box(cx - r, cy - r, cx + r, cy + r);

  /* Midpoint circle algorithm: the code calculates one octant and draws it
   * at its 8 symmetrical positions */
  int32_t f = 1 - r, ddx = 1, ddy = -2 * r;
  int32_t x = 0, y = r;

  if (r == 0)
  {
    pixel_raw(cx, cy, color);
  }
  else if (st7789_fb)
  {
    /* Framebuffer mode: the code writes the pixels one at a time */
    pixel_raw(cx, cy + r, color);
    pixel_raw(cx, cy - r, color);
    pixel_raw(cx + r, cy, color);
    pixel_raw(cx - r, cy, color);

    while (x < y)
    {
      if (f >= 0)
      {
        y--;
        ddy += 2;
        f   += ddy;
      }
      x++;
      ddx += 2;
      f   += ddx;

      pixel_raw(cx + x, cy + y, color);
      pixel_raw(cx - x, cy + y, color);
      pixel_raw(cx + x, cy - y, color);
      pixel_raw(cx - x, cy - y, color);
      pixel_raw(cx + y, cy + x, color);
      pixel_raw(cx - y, cy + x, color);
      pixel_raw(cx + y, cy - x, color);
      pixel_raw(cx - y, cy - x, color);
    }
  }
  else
  {
    /* Direct mode: the code does the same steps, but it sends one row of
       pixels at a time. A row contains the pixels at one height. The row
       stops when y changes, or at the end of the octant (45 degrees). */
    int32_t run = 0;                   /* the first x of the row at height y */
    for (;;)
    {
      int32_t nx = x + 1, ny = y;
      if (f >= 0)
      {
        ny--;
        ddy += 2;
        f   += ddy;
      }
      ddx += 2;
      f   += ddx;
      if (ny != y || nx > ny)          /* the row at height y stops at x */
      {
        circle_runs(cx, cy, run, x, y, color);
        run = nx;
      }
      if (nx > ny)                     /* after 45 degrees, each remaining pixel
                                          is symmetrical to a drawn pixel */
      {
        break;
      }
      x = nx;
      y = ny;
    }
  }
  call_end();
}

void ST7789_FillCircle(int16_t cx, int16_t cy, int16_t r, uint16_t color)
{
  if (r < 0)
  {
    direct_sync();
    return;
  }
  call_begin();
  mark_box((int32_t)cx - r, (int32_t)cy - r, (int32_t)cx + r, (int32_t)cy + r);

  /* The function calculates the width of each row, from the outer rows to
   * the middle row. Rows with the same width become one rectangle (the flat
   * top, the flat bottom, and the middle rectangle, which has many rows).
   * Thus, the library uses a smaller number of windows. A point (x, y) is
   * on the ST7789_DrawCircle contour or in its inner area if, and only if,
   * x*x + y*y - max(x, y) < r*r. This function uses the same condition.
   * Thus, each row has the same ends as the contour on that row. */
  const int32_t rr = (int32_t)r * r;
  int32_t x = 0;
  int32_t band_start = r;               /* the value of `dy` where this width started */
  int32_t band_x = -1;

  for (int32_t dy = r; dy >= 0; dy--)
  {
    for (;;)
    {
      int32_t nx = x + 1, m = (nx > dy) ? nx : dy;
      if (nx * nx + dy * dy - m >= rr)
      {
        break;
      }
      x = nx;
    }
    if (x != band_x && band_x >= 0)
    {
      /* All the rows from `band_start` to `dy+1` had the same width: `2*band_x + 1` pixels */
      int32_t rows = band_start - dy;
      rect_raw(cx - band_x, cy - band_start, 2 * band_x + 1, rows, color);
      rect_raw(cx - band_x, cy + dy + 1,     2 * band_x + 1, rows, color);
      band_start = dy;
    }
    band_x = x;
  }
  /* The last rectangle includes the middle row (`dy` = 0) */
  rect_raw(cx - band_x, cy - band_start, 2 * band_x + 1, 2 * band_start + 1, color);
  call_end();
}

/* ---- triangles ---------------------------------------------------------- */

/* One side of a triangle, prepared for the same steps that line_raw and
 * line_fb do along it:
 * - The steps start at the end with the smaller coordinate on the major
 *   axis.
 * - `err` starts at half of the difference on the major axis.
 * - Each time `err` becomes less than 0, the coordinate on the minor axis
 *   moves 1 pixel. */
typedef struct
{
  int32_t  x0, y0;         /* the point where the steps start */
  uint32_t major, minor;   /* the differences on the major and minor axes */
  int32_t  step;           /* the direction on the minor axis, +1 or -1 */
  uint8_t  steep;          /* 1: the steps go down, 1 pixel in each row */
  int32_t  top, bottom;    /* the top row and the bottom row of the side */
} tri_side;

static void side_init(tri_side *e, int32_t xa, int32_t ya, int32_t xb, int32_t yb)
{
  if (abs32(xb - xa) >= abs32(yb - ya))     /* the major axis is x, as in line_raw */
  {
    if (xa > xb) { SWAP(xa, xb); SWAP(ya, yb); }
    e->steep = 0;
    e->major = (uint32_t)(xb - xa);
    e->minor = (uint32_t)abs32(yb - ya);
    e->step  = (ya < yb) ? 1 : -1;
  }
  else
  {
    if (ya > yb) { SWAP(xa, xb); SWAP(ya, yb); }
    e->steep = 1;
    e->major = (uint32_t)(yb - ya);
    e->minor = (uint32_t)abs32(xb - xa);
    e->step  = (xa < xb) ? 1 : -1;
  }
  e->x0     = xa;
  e->y0     = ya;
  e->top    = (ya < yb) ? ya : yb;
  e->bottom = (ya < yb) ? yb : ya;
}

/* On row y, DrawTriangle draws the pixels of the side from the left end
 * (column `*lo`) to the right end (column `*hi`). This function calculates
 * `*lo` and `*hi`. If `steep` is 1, `*lo` and `*hi` are equal. The row
 * numbers increase from the top of the screen to the bottom. Row y must be
 * in the range from the top row to the bottom row of the side.
 *
 * At step k, the coordinate on the minor axis is
 * `floor((k*minor - major/2 + major - 1) / major)` pixels from the start.
 * For corners with `int16_t` coordinates, the sums stay less than 2^32. */
static void side_span(const tri_side *e, int32_t y, int32_t *lo, int32_t *hi)
{
  uint32_t half = e->major / 2u;
  if (e->steep)
  {
    uint32_t k = (uint32_t)(y - e->y0);
    uint32_t q = (k * e->minor + e->major - 1u - half) / e->major;
    *lo = *hi = e->x0 + e->step * (int32_t)q;
  }
  else
  {
    /* `m` is the number of rows from the start. `k0` and `k1` are the first
     * step and the last step in row `m`. */
    uint32_t m  = (uint32_t)((y - e->y0) * e->step);
    uint32_t k0 = (m == 0u) ? 0u : ((m - 1u) * e->major + half + e->minor) / e->minor;
    uint32_t k1 = (m == e->minor) ? e->major : (m * e->major + half + e->minor) / e->minor - 1u;
    *lo = e->x0 + (int32_t)k0;
    *hi = e->x0 + (int32_t)k1;
  }
}

void ST7789_DrawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                         int16_t x2, int16_t y2, uint16_t color)
{
  call_begin();
  int32_t lx = x0, hx = x0, ly = y0, hy = y0;
  if (x1 < lx) lx = x1;
  if (x2 < lx) lx = x2;
  if (x1 > hx) hx = x1;
  if (x2 > hx) hx = x2;
  if (y1 < ly) ly = y1;
  if (y2 < ly) ly = y2;
  if (y1 > hy) hy = y1;
  if (y2 > hy) hy = y2;
  mark_box(lx, ly, hx, hy);
  line_raw(x0, y0, x1, y1, color);
  line_raw(x1, y1, x2, y2, color);
  line_raw(x2, y2, x0, y0, color);
  call_end();
}

void ST7789_FillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                         int16_t x2, int16_t y2, uint16_t color)
{
  call_begin();
  int32_t lx = x0, hx = x0, ly = y0, hy = y0;
  if (x1 < lx) lx = x1;
  if (x2 < lx) lx = x2;
  if (x1 > hx) hx = x1;
  if (x2 > hx) hx = x2;
  if (y1 < ly) ly = y1;
  if (y2 < ly) ly = y2;
  if (y1 > hy) hy = y1;
  if (y2 > hy) hy = y2;
  mark_box(lx, ly, hx, hy);

  /* Each row starts at the left end and stops at the right end of the
   * pixels that DrawTriangle draws on that row. Thus, the function fills all
   * the pixels of the DrawTriangle contour and of its inner area, and no
   * other pixels. It calculates only the rows on the screen. When adjacent
   * rows have the same range on the screen, the function draws them as one
   * rectangle. */
  tri_side side[3];
  side_init(&side[0], x0, y0, x1, y1);
  side_init(&side[1], x1, y1, x2, y2);
  side_init(&side[2], x2, y2, x0, y0);
  int32_t ystart = (ly < 0) ? 0 : ly;
  int32_t yend   = (hy >= (int32_t)st7789_height) ? (int32_t)st7789_height - 1 : hy;
  int32_t xmax   = (int32_t)st7789_width - 1;
  int32_t band_y = ystart, band_lo = 1, band_hi = 0;          /* empty */

  for (int32_t y = ystart; y <= yend; y++)
  {
    int32_t lo = INT32_MAX, hi = INT32_MIN;
    for (uint32_t i = 0; i < 3u; i++)
    {
      if (y >= side[i].top && y <= side[i].bottom)
      {
        int32_t a, b;
        side_span(&side[i], y, &a, &b);
        if (a < lo) lo = a;
        if (b > hi) hi = b;
      }
    }
    if (lo < 0) lo = 0;
    if (hi > xmax) hi = xmax;
    if (lo > hi)                                                /* all the pixels of this row are
                                                                   out of the screen, at the left
                                                                   or right side */
    {
      lo = 1;
      hi = 0;
    }
    if (lo != band_lo || hi != band_hi)
    {
      if (band_lo <= band_hi)
      {
        rect_raw(band_lo, band_y, band_hi - band_lo + 1, y - band_y, color);
      }
      band_lo = lo;
      band_hi = hi;
      band_y  = y;
    }
  }
  if (band_lo <= band_hi)
  {
    rect_raw(band_lo, band_y, band_hi - band_lo + 1, yend + 1 - band_y, color);
  }
  call_end();
}

/* ---- images ------------------------------------------------------------- */

void ST7789_DrawImage(int16_t x, int16_t y, int16_t w, int16_t h,
                      const uint16_t *pixels)
{
  int32_t x0, y0, x1, y1;
  if (pixels == NULL || !st7789_clip(x, y, w, h, &x0, &y0, &x1, &y1))
  {
    direct_sync();
    return;
  }
  if (st7789_fb)
  {
    st7789_mark_fast(x0, y0, x1, y1);
  }
  uint32_t stride = (uint32_t)w;                  /* `w` is also the stride */
  const uint16_t *first = pixels + (uint32_t)(y0 - y) * stride + (uint32_t)(x0 - x);
  st7789_blit(x0, y0, (uint32_t)(x1 - x0), (uint32_t)(y1 - y0), first, stride);
}

/* ---- text --------------------------------------------------------------- */

static const uint8_t *glyph_for(char c)
{
  if (c < FONT5X7_FIRST_CHAR || c > FONT5X7_LAST_CHAR)
  {
    c = '?';
  }
  return st7789_font5x7[c - FONT5X7_FIRST_CHAR];
}

/* This function draws `n` characters in one row into `dst` (with a stride of `stride` pixels) */
static void render_run(uint16_t *dst, uint32_t stride, const char *s, uint32_t n,
                       uint16_t fg, uint16_t bg, uint32_t size)
{
  uint32_t run_w = n * ST7789_CHAR_W * size;

  for (uint32_t font_row = 0; font_row < ST7789_CHAR_H; font_row++)
  {
    uint16_t *line = dst + font_row * size * stride;
    uint16_t *p = line;
    for (uint32_t k = 0; k < n; k++)
    {
      const uint8_t *glyph = glyph_for(s[k]);
      for (uint32_t col = 0; col < ST7789_CHAR_W; col++)
      {
        uint8_t bits = (col < FONT5X7_GLYPH_W) ? glyph[col] : 0;   /* the last column is the space
                                                                      between two characters */
        uint16_t c = ((bits >> font_row) & 1u) ? fg : bg;          /* row 7 is the space between
                                                                      two rows of text */
        for (uint32_t i = 0; i < size; i++)
        {
          *p++ = c;
        }
      }
    }
    /* If `size` > 1, all the rows of pixels in a font row are the same */
    for (uint32_t i = 1; i < size; i++)
    {
      st7789_copy16(line + i * stride, line, run_w);
    }
  }
}

/* Transparent text: each horizontal line of character pixels in a font row
 * is one rectangle. The function draws no other pixels. */
static void draw_run_transparent(int32_t x, int32_t y, const char *s, uint32_t n,
                                 uint16_t fg, uint32_t size)
{
  int32_t x0, y0, x1, y1;
  if (!st7789_clip(x, y, (int32_t)(n * ST7789_CHAR_W * size), (int32_t)(ST7789_CHAR_H * size),
                   &x0, &y0, &x1, &y1))
  {
    return;
  }
  if (st7789_fb)
  {
    st7789_mark_fast(x0, y0, x1, y1);
  }
  for (uint32_t k = 0; k < n; k++)
  {
    const uint8_t *glyph = glyph_for(s[k]);
    int32_t gx = x + (int32_t)(k * ST7789_CHAR_W * size);
    for (uint32_t row = 0; row < FONT5X7_GLYPH_H; row++)
    {
      uint32_t col = 0;
      while (col < FONT5X7_GLYPH_W)
      {
        if (((glyph[col] >> row) & 1u) == 0u)
        {
          col++;
          continue;
        }
        uint32_t first = col;
        while (col < FONT5X7_GLYPH_W && ((glyph[col] >> row) & 1u) != 0u)
        {
          col++;
        }
        rect_raw(gx + (int32_t)(first * size), y + (int32_t)(row * size),
                 (int32_t)((col - first) * size), (int32_t)size, fg);
      }
    }
  }
}

/* This function draws `n` characters (no newlines). The first character is at (x, y). */
static void draw_run(int32_t x, int32_t y, const char *s, uint32_t n,
                     uint16_t fg, uint16_t bg, uint32_t size)
{
  if (text_transparent)
  {
    draw_run_transparent(x, y, s, n, fg, size);
    return;
  }
  uint32_t run_w = n * ST7789_CHAR_W * size;
  uint32_t run_h = ST7789_CHAR_H * size;
  int32_t x0, y0, x1, y1;
  if (!st7789_clip(x, y, (int32_t)run_w, (int32_t)run_h, &x0, &y0, &x1, &y1))
  {
    return;
  }
  drew = 1;

  if (st7789_fb)
  {
    st7789_mark_fast(x0, y0, x1, y1);
    if (x0 == x && y0 == y && x1 - x0 == (int32_t)run_w && y1 - y0 == (int32_t)run_h)
    {
      /* If the characters are fully on the screen, the function draws them
         directly into the framebuffer */
      render_run(st7789_fb + (uint32_t)y * st7789_width + (uint32_t)x, st7789_width,
                 s, n, fg, bg, size);
      return;
    }
    if (n > 1)
    {
      /* The text buffer possibly cannot contain all `n` characters, but it
         can always contain one character. Thus, if the characters are not
         fully on the screen, the function draws one character at a time. */
      for (uint32_t k = 0; k < n; k++)
      {
        draw_run(x + (int32_t)(k * ST7789_CHAR_W * size), y, s + k, 1, fg, bg, size);
      }
      return;
    }
  }

  /* The function draws the characters into the text buffer that is not in a
   * transfer at this time. Then it sends the part of the text buffer that is
   * on the screen. */
  uint16_t *buf = text_buf[text_next];
  text_next ^= 1u;
  render_run(buf, run_w, s, n, fg, bg, size);
  st7789_blit(x0, y0, (uint32_t)(x1 - x0), (uint32_t)(y1 - y0),
              buf + (uint32_t)(y0 - y) * run_w + (uint32_t)(x0 - x), run_w);
}

static uint32_t clamp_size(uint8_t size)
{
  if (size < 1) return 1;
  if (size > ST7789_MAX_TEXT_SIZE) return ST7789_MAX_TEXT_SIZE;
  return size;
}

void ST7789_DrawChar(int16_t x, int16_t y, char c,
                     uint16_t fg, uint16_t bg, uint8_t size)
{
  call_begin();
  draw_run(x, y, &c, 1, fg, bg, clamp_size(size));
  call_end();
}

void ST7789_SetTextWrap(uint8_t wrap)
{
  text_wrap = wrap ? 1u : 0u;
}

void ST7789_SetTextTransparent(uint8_t transparent)
{
  text_transparent = transparent ? 1u : 0u;
}

void ST7789_DrawString(int16_t x, int16_t y, const char *str,
                       uint16_t fg, uint16_t bg, uint8_t size)
{
  call_begin();
  if (str == NULL)
  {
    call_end();
    return;
  }
  uint32_t s = clamp_size(size);
  int32_t cell_w = (int32_t)(ST7789_CHAR_W * s);
  int32_t cell_h = (int32_t)(ST7789_CHAR_H * s);
  uint32_t max_run = ST7789_TEXT_BUFFER_PIXELS / (uint32_t)(cell_w * cell_h);
  int32_t cx = x, cy = y;

  while (*str != '\0')
  {
    if (*str == '\n')
    {
      cx = x;
      cy += cell_h;
      str++;
      continue;
    }
    if (*str == '\r')                           /* the function ignores `'\r'`: thus `"\r\n"`
                                                   has the same result as `"\n"` */
    {
      str++;
      continue;
    }
    if (cx + cell_w > st7789_width && cx > x)   /* this row has no space for the next
                                                   character */
    {
      if (text_wrap && x + cell_w <= st7789_width)
      {
        cx = x;                                 /* a new row has space for the character:
                                                   the text continues on the next row at x */
        cy += cell_h;
      }
      else if (cx >= st7789_width)              /* all the remaining characters of this row
                                                   are out of the screen */
      {
        while (*str != '\0' && *str != '\n')
        {
          str++;
        }
        continue;
      }
    }
    if (cy >= st7789_height)                    /* the screen has no space for more rows */
    {
      break;
    }

    /* The maximum number of characters that this row (and the text buffer)
     * can contain */
    uint32_t n = 1;
    while (str[n] != '\0' && str[n] != '\n' && str[n] != '\r' &&
           cx + (int32_t)(n + 1) * cell_w <= st7789_width &&
           (st7789_fb != NULL || n < max_run))
    {
      n++;
    }
    draw_run(cx, cy, str, n, fg, bg, s);
    cx  += (int32_t)n * cell_w;
    str += n;
  }
  call_end();
}

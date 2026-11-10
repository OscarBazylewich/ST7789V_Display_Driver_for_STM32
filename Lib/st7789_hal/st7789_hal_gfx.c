/**
 * ST7789 HAL library: lines, shapes and text.
 *
 * The library must set a window for each rectangle that it sends. Thus, it
 * makes each shape from the smallest possible number of rectangles, and
 * each rectangle is as large as possible:
 * - A line: from its horizontal lines or its vertical lines.
 * - DrawCircle: from the horizontal lines and the vertical lines of its
 *   contour.
 * - FillCircle and FillTriangle: from rectangles of equal rows.
 *
 * Text with a background color: the library sets one window for each group
 * of adjacent characters on one row of text. It writes the pixels of the
 * characters into the transmit buffer. It sends the transmit buffer each
 * time it is full. Thus, the characters can be larger than the transmit
 * buffer.
 *
 * Transparent text: the library draws each horizontal line of character
 * pixels as one rectangle.
 *
 * The functions in this file give the same pixels as the functions with the
 * same names in `Lib/st7789`.
 */
#include "st7789_hal.h"
#include "st7789_hal_internal.h"
#include "st7789_hal_font5x7.h"
#include <stddef.h>

#define SWAP(a, b) do { int32_t t_ = (a); (a) = (b); (b) = t_; } while (0)

/* abs() has an `int` parameter, and on some MCUs, an `int` has 16 bits. In
 * this file, a difference between two coordinates can be as large as
 * 65535. */
static inline int32_t abs32(int32_t v)
{
  return (v < 0) ? -v : v;
}

static inline int ready(const ST7789HAL_Handle *h)
{
  return h != NULL && h->initialized != 0u;
}

/* ---- pixels and lines ---------------------------------------------------- */

void ST7789HAL_DrawPixel(ST7789HAL_Handle *h, int16_t x, int16_t y, uint16_t color)
{
  if (ready(h))
  {
    st7789hal_rect(h, x, y, 1, 1, color);    /* one window of 1 pixel */
  }
}

void ST7789HAL_DrawHLine(ST7789HAL_Handle *h, int16_t x, int16_t y, int16_t w, uint16_t color)
{
  if (ready(h))
  {
    st7789hal_rect(h, x, y, w, 1, color);
  }
}

void ST7789HAL_DrawVLine(ST7789HAL_Handle *h, int16_t x, int16_t y, int16_t h_px, uint16_t color)
{
  if (ready(h))
  {
    st7789hal_rect(h, x, y, 1, h_px, color);
  }
}

/* This function uses Bresenham's algorithm. It divides the line into
 * horizontal lines or vertical lines, and draws each of them as one thin
 * rectangle. The steps start at the end with the smaller coordinate on the
 * major axis. `err` starts at half of the difference on the major axis.
 * Thus, the pixels do not change if you interchange the two ends of the
 * line.
 *
 * side_span (FillTriangle) calculates the result of the same steps
 * directly. Keep the steps of the two functions the same. */
static void line(ST7789HAL_Handle *h, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t color)
{
  if (y0 == y1)
  {
    if (x0 > x1) SWAP(x0, x1);
    st7789hal_rect(h, x0, y0, x1 - x0 + 1, 1, color);
    return;
  }
  if (x0 == x1)
  {
    if (y0 > y1) SWAP(y0, y1);
    st7789hal_rect(h, x0, y0, 1, y1 - y0 + 1, color);
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
        st7789hal_rect(h, run, y, x - run + 1, 1, color);
        y   += sy;
        err += dx;
        run  = x + 1;
      }
    }
    if (run <= x1)
    {
      st7789hal_rect(h, run, y, x1 - run + 1, 1, color);
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
        st7789hal_rect(h, x, run, 1, y - run + 1, color);
        x   += sx;
        err += dy;
        run  = y + 1;
      }
    }
    if (run <= y1)
    {
      st7789hal_rect(h, x, run, 1, y1 - run + 1, color);
    }
  }
}

void ST7789HAL_DrawLine(ST7789HAL_Handle *h, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color)
{
  if (ready(h))
  {
    line(h, x0, y0, x1, y1, color);
  }
}

void ST7789HAL_DrawRect(ST7789HAL_Handle *h, int16_t x, int16_t y, int16_t w, int16_t h_px, uint16_t color)
{
  if (!ready(h) || w <= 0 || h_px <= 0)
  {
    return;
  }
  if (w <= 2 || h_px <= 2)        /* the rectangle has no empty inner area:
                                     the function draws one filled rectangle */
  {
    st7789hal_rect(h, x, y, w, h_px, color);
    return;
  }
  int32_t right = (int32_t)x + w - 1, bottom = (int32_t)y + h_px - 1;
  st7789hal_rect(h, x, y, w, 1, color);
  st7789hal_rect(h, x, bottom, w, 1, color);
  st7789hal_rect(h, x, (int32_t)y + 1, 1, h_px - 2, color);
  st7789hal_rect(h, right, (int32_t)y + 1, 1, h_px - 2, color);
}

/* ---- circles ------------------------------------------------------------- */

/* This function draws the row of pixels x = a..b at height y of the octant
 * (a <= b <= y) at its 8 symmetrical positions. */
static void circle_runs(ST7789HAL_Handle *h, int32_t cx, int32_t cy, int32_t a, int32_t b,
                        int32_t y, uint16_t color)
{
  if (a == 0)                          /* the row starts on the axis: each
                                          rectangle contains two symmetrical
                                          positions of the row, one on each
                                          side of the axis */
  {
    int32_t n = 2 * b + 1;
    st7789hal_rect(h, cx - b, cy + y, n, 1, color);
    st7789hal_rect(h, cx - b, cy - y, n, 1, color);
    st7789hal_rect(h, cx + y, cy - b, 1, n, color);
    st7789hal_rect(h, cx - y, cy - b, 1, n, color);
    return;
  }
  int32_t n = b - a + 1;
  int32_t m = (b == y) ? n - 1 : n;    /* if b = y, the horizontal lines include
                                          the pixel (y, y). Thus, the vertical
                                          lines are 1 pixel shorter. */
  st7789hal_rect(h, cx + a, cy + y, n, 1, color);
  st7789hal_rect(h, cx - b, cy + y, n, 1, color);
  st7789hal_rect(h, cx - b, cy - y, n, 1, color);
  st7789hal_rect(h, cx + a, cy - y, n, 1, color);
  if (m > 0)
  {
    st7789hal_rect(h, cx + y, cy + a,         1, m, color);
    st7789hal_rect(h, cx + y, cy - a - m + 1, 1, m, color);
    st7789hal_rect(h, cx - y, cy - a - m + 1, 1, m, color);
    st7789hal_rect(h, cx - y, cy + a,         1, m, color);
  }
}

void ST7789HAL_DrawCircle(ST7789HAL_Handle *h, int16_t cx_, int16_t cy_, int16_t r_, uint16_t color)
{
  if (!ready(h) || r_ < 0)
  {
    return;
  }
  int32_t cx = cx_, cy = cy_, r = r_;
  if (r == 0)
  {
    st7789hal_rect(h, cx, cy, 1, 1, color);
    return;
  }

  /* Midpoint circle algorithm: the code calculates one octant and draws it
   * at its 8 symmetrical positions. It sends one row of pixels at a time. A
   * row contains the pixels at one height. The row stops when y changes, or
   * at the end of the octant (45 degrees). */
  int32_t f = 1 - r, ddx = 1, ddy = -2 * r;
  int32_t x = 0, y = r;
  int32_t run = 0;                     /* the first x of the row at height y */
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
    if (ny != y || nx > ny)            /* the row at height y stops at x */
    {
      circle_runs(h, cx, cy, run, x, y, color);
      run = nx;
    }
    if (nx > ny)                       /* after 45 degrees, each remaining pixel
                                          is symmetrical to a drawn pixel */
    {
      break;
    }
    x = nx;
    y = ny;
  }
}

void ST7789HAL_FillCircle(ST7789HAL_Handle *h, int16_t cx, int16_t cy, int16_t r, uint16_t color)
{
  if (!ready(h) || r < 0)
  {
    return;
  }

  /* The function calculates the width of each row, from the outer rows to
   * the middle row. Rows with the same width become one rectangle. A point
   * (x, y) is on the DrawCircle contour or in its inner area if, and only
   * if, x*x + y*y - max(x, y) < r*r. This function uses the same condition.
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
      st7789hal_rect(h, cx - band_x, cy - band_start, 2 * band_x + 1, rows, color);
      st7789hal_rect(h, cx - band_x, cy + dy + 1,     2 * band_x + 1, rows, color);
      band_start = dy;
    }
    band_x = x;
  }
  /* The last rectangle includes the middle row (`dy` = 0) */
  st7789hal_rect(h, cx - band_x, cy - band_start, 2 * band_x + 1, 2 * band_start + 1, color);
}

/* ---- triangles ----------------------------------------------------------- */

/* One side of a triangle, prepared for the same steps that `line` does
 * along it:
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
  if (abs32(xb - xa) >= abs32(yb - ya))     /* the major axis is x, as in `line` */
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

void ST7789HAL_DrawTriangle(ST7789HAL_Handle *h, int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                            int16_t x2, int16_t y2, uint16_t color)
{
  if (ready(h))
  {
    line(h, x0, y0, x1, y1, color);
    line(h, x1, y1, x2, y2, color);
    line(h, x2, y2, x0, y0, color);
  }
}

void ST7789HAL_FillTriangle(ST7789HAL_Handle *h, int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                            int16_t x2, int16_t y2, uint16_t color)
{
  if (!ready(h))
  {
    return;
  }
  int32_t ly = y0, hy = y0;
  if (y1 < ly) ly = y1;
  if (y2 < ly) ly = y2;
  if (y1 > hy) hy = y1;
  if (y2 > hy) hy = y2;

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
  int32_t yend   = (hy >= (int32_t)h->height) ? (int32_t)h->height - 1 : hy;
  int32_t xmax   = (int32_t)h->width - 1;
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
    if (lo > hi)                       /* all the pixels of this row are out of
                                          the screen, at the left or right side */
    {
      lo = 1;
      hi = 0;
    }
    if (lo != band_lo || hi != band_hi)
    {
      if (band_lo <= band_hi)
      {
        st7789hal_rect(h, band_lo, band_y, band_hi - band_lo + 1, y - band_y, color);
      }
      band_lo = lo;
      band_hi = hi;
      band_y  = y;
    }
  }
  if (band_lo <= band_hi)
  {
    st7789hal_rect(h, band_lo, band_y, band_hi - band_lo + 1, yend + 1 - band_y, color);
  }
}

/* ---- text ---------------------------------------------------------------- */

static const uint8_t *glyph_for(char c)
{
  if (c < ST7789HAL_FONT_FIRST_CHAR || c > ST7789HAL_FONT_LAST_CHAR)
  {
    c = '?';
  }
  return st7789hal_font5x7[c - ST7789HAL_FONT_FIRST_CHAR];
}

/* Text with a background color: this function draws `n` characters (no
 * newlines) in one window. The first character is at (`x`, `y`). The
 * function writes the pixels of the window into the transmit buffer, row by
 * row. It sends the transmit buffer each time it is full. */
static void draw_run_opaque(ST7789HAL_Handle *h, int32_t x, int32_t y, const char *s, uint32_t n,
                            uint16_t fg, uint16_t bg, uint32_t size)
{
  uint32_t cell_w = ST7789HAL_CHAR_W * size;
  int32_t x0, y0, x1, y1;
  if (!st7789hal_clip(h, x, y, (int32_t)(n * cell_w), (int32_t)(ST7789HAL_CHAR_H * size),
                      &x0, &y0, &x1, &y1))
  {
    return;
  }
  uint16_t fgs = st7789hal_swap(fg), bgs = st7789hal_swap(bg);
  uint32_t used = 0;
  int ok = 1;

  st7789hal_ram_begin(h, (uint32_t)x0, (uint32_t)y0, (uint32_t)(x1 - x0), (uint32_t)(y1 - y0));
  for (int32_t py = y0; py < y1 && ok; py++)
  {
    uint32_t font_row = (uint32_t)(py - y) / size;   /* row 7 is the empty row
                                                        between two rows of text */
    /* The position of the first pixel of the window row in the characters */
    uint32_t px   = (uint32_t)(x0 - x);
    uint32_t k    = px / cell_w;                     /* the character */
    uint32_t rem  = px % cell_w;
    uint32_t col  = rem / size;                      /* the font column */
    uint32_t sub  = rem % size;                      /* the pixel in the font column */
    const uint8_t *glyph = glyph_for(s[k]);

    for (int32_t i = x0; i < x1; i++)
    {
      uint8_t bits = (col < ST7789HAL_FONT_GLYPH_W) ? glyph[col] : 0u;  /* column 5 is the
                                                                          empty column between
                                                                          two characters */
      st7789hal_buf[used++] = ((bits >> font_row) & 1u) ? fgs : bgs;
      if (used == ST7789HAL_BUFFER_PIXELS)
      {
        if (!st7789hal_send_buf(h, used))
        {
          ok = 0;
          break;
        }
        used = 0;
      }
      if (++sub == size)
      {
        sub = 0;
        if (++col == ST7789HAL_CHAR_W)
        {
          col = 0;
          if (++k < n)
          {
            glyph = glyph_for(s[k]);
          }
        }
      }
    }
  }
  if (ok && used > 0u)
  {
    (void)st7789hal_send_buf(h, used);
  }
  st7789hal_ram_end(h);
}

/* Transparent text: each horizontal line of character pixels in a font row
 * is one rectangle. The function draws no other pixels. */
static void draw_run_transparent(ST7789HAL_Handle *h, int32_t x, int32_t y, const char *s,
                                 uint32_t n, uint16_t fg, uint32_t size)
{
  int32_t x0, y0, x1, y1;
  if (!st7789hal_clip(h, x, y, (int32_t)(n * ST7789HAL_CHAR_W * size),
                      (int32_t)(ST7789HAL_CHAR_H * size), &x0, &y0, &x1, &y1))
  {
    return;
  }
  for (uint32_t k = 0; k < n; k++)
  {
    const uint8_t *glyph = glyph_for(s[k]);
    int32_t gx = x + (int32_t)(k * ST7789HAL_CHAR_W * size);
    for (uint32_t row = 0; row < ST7789HAL_FONT_GLYPH_H; row++)
    {
      uint32_t col = 0;
      while (col < ST7789HAL_FONT_GLYPH_W)
      {
        if (((glyph[col] >> row) & 1u) == 0u)
        {
          col++;
          continue;
        }
        uint32_t first = col;
        while (col < ST7789HAL_FONT_GLYPH_W && ((glyph[col] >> row) & 1u) != 0u)
        {
          col++;
        }
        st7789hal_rect(h, gx + (int32_t)(first * size), y + (int32_t)(row * size),
                       (int32_t)((col - first) * size), (int32_t)size, fg);
      }
    }
  }
}

static void draw_run(ST7789HAL_Handle *h, int32_t x, int32_t y, const char *s, uint32_t n,
                     uint16_t fg, uint16_t bg, uint32_t size)
{
  if (h->text_transparent)
  {
    draw_run_transparent(h, x, y, s, n, fg, size);
  }
  else
  {
    draw_run_opaque(h, x, y, s, n, fg, bg, size);
  }
}

static uint32_t clamp_size(uint8_t size)
{
  if (size < 1u) return 1u;
  if (size > ST7789HAL_MAX_TEXT_SIZE) return ST7789HAL_MAX_TEXT_SIZE;
  return size;
}

void ST7789HAL_DrawChar(ST7789HAL_Handle *h, int16_t x, int16_t y, char c,
                        uint16_t fg, uint16_t bg, uint8_t size)
{
  if (ready(h))
  {
    draw_run(h, x, y, &c, 1u, fg, bg, clamp_size(size));
  }
}

void ST7789HAL_SetTextWrap(ST7789HAL_Handle *h, uint8_t wrap)
{
  if (ready(h))
  {
    h->text_wrap = wrap ? 1u : 0u;
  }
}

void ST7789HAL_SetTextTransparent(ST7789HAL_Handle *h, uint8_t transparent)
{
  if (ready(h))
  {
    h->text_transparent = transparent ? 1u : 0u;
  }
}

void ST7789HAL_DrawString(ST7789HAL_Handle *h, int16_t x, int16_t y, const char *str,
                          uint16_t fg, uint16_t bg, uint8_t size)
{
  if (!ready(h) || str == NULL)
  {
    return;
  }
  uint32_t s = clamp_size(size);
  int32_t cell_w = (int32_t)(ST7789HAL_CHAR_W * s);
  int32_t cell_h = (int32_t)(ST7789HAL_CHAR_H * s);
  int32_t width  = (int32_t)h->width;
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
    if (*str == '\r')                           /* the function ignores `'\r'`: thus
                                                   `"\r\n"` has the same result
                                                   as `"\n"` */
    {
      str++;
      continue;
    }
    if (cx + cell_w > width && cx > x)          /* this row has no space for the next
                                                   character */
    {
      if (h->text_wrap && x + cell_w <= width)
      {
        cx = x;                                 /* a new row has space for the character:
                                                   the text continues on the next row at x */
        cy += cell_h;
      }
      else if (cx >= width)                     /* all the remaining characters of this row
                                                   are out of the screen */
      {
        while (*str != '\0' && *str != '\n')
        {
          str++;
        }
        continue;
      }
    }
    if (cy >= (int32_t)h->height)               /* the screen has no space for more rows */
    {
      break;
    }

    /* The maximum number of characters that this row can contain */
    uint32_t n = 1;
    while (str[n] != '\0' && str[n] != '\n' && str[n] != '\r' &&
           cx + (int32_t)(n + 1u) * cell_w <= width)
    {
      n++;
    }
    draw_run(h, cx, cy, str, n, fg, bg, s);
    cx  += (int32_t)n * cell_w;
    str += n;
  }
}

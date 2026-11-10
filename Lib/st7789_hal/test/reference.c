/**
 * The drawing functions of the tests. Refer to `reference.h`.
 *
 * For lines and circles, more than one correct set of pixels is possible.
 * These functions use the algorithms of `Lib/st7789` (framebuffer mode),
 * one pixel at a time:
 * - A line: Bresenham's algorithm. The steps start at the end with the
 *   smaller coordinate on the major axis. `err` starts at half of the
 *   difference on the major axis.
 * - A circle: the midpoint circle algorithm.
 * - A filled circle or a filled triangle: on each row, all the pixels from
 *   the left end to the right end of the contour on that row.
 */
#include "reference.h"
#include "st7789_font5x7.h"
#include <stdlib.h>

ref_t ref;

typedef void (*plot_fn)(void *ctx, int32_t x, int32_t y);

static int32_t abs32(int32_t v)
{
  return (v < 0) ? -v : v;
}

void ref_begin(int32_t panel_w, int32_t panel_h, uint16_t fill)
{
  ref.pw = panel_w;
  ref.ph = panel_h;
  for (int32_t r = 0; r < 320; r++)
  {
    for (int32_t c = 0; c < 240; c++)
    {
      ref.fb[r][c] = fill;
    }
  }
  ref_set_rotation(0);
}

void ref_set_rotation(int32_t rotation)
{
  ref.rotation = rotation & 3;
  if (ref.rotation & 1)
  {
    ref.w = ref.ph;
    ref.h = ref.pw;
  }
  else
  {
    ref.w = ref.pw;
    ref.h = ref.ph;
  }
}

int ref_to_panel(int32_t x, int32_t y, int32_t *px, int32_t *py)
{
  if (x < 0 || y < 0 || x >= ref.w || y >= ref.h)
  {
    return 0;
  }
  switch (ref.rotation)
  {
    case 0:
      *px = x;
      *py = y;
      break;
    case 1:
      *px = ref.pw - 1 - y;
      *py = x;
      break;
    case 2:
      *px = ref.pw - 1 - x;
      *py = ref.ph - 1 - y;
      break;
    default:
      *px = y;
      *py = ref.ph - 1 - x;
      break;
  }
  return 1;
}

void ref_pixel(int32_t x, int32_t y, uint16_t c)
{
  int32_t px, py;
  if (ref_to_panel(x, y, &px, &py))
  {
    ref.fb[py][px] = c;
  }
}

static void plot_pixel(void *ctx, int32_t x, int32_t y)
{
  ref_pixel(x, y, *(const uint16_t *)ctx);
}

void ref_fill_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)
{
  if (w <= 0 || h <= 0)
  {
    return;
  }
  int32_t x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
  int32_t x1 = x + w > ref.w ? ref.w : x + w;
  int32_t y1 = y + h > ref.h ? ref.h : y + h;
  for (int32_t j = y0; j < y1; j++)
  {
    for (int32_t i = x0; i < x1; i++)
    {
      ref_pixel(i, j, c);
    }
  }
}

static void walk_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, plot_fn plot, void *ctx)
{
  int32_t adx = abs32(x1 - x0), ady = abs32(y1 - y0);
  if (adx >= ady)
  {
    if (x0 > x1)
    {
      int32_t t = x0; x0 = x1; x1 = t;
      t = y0; y0 = y1; y1 = t;
    }
    int32_t dx = x1 - x0, dy = abs32(y1 - y0), sy = (y0 < y1) ? 1 : -1;
    int32_t err = dx / 2, y = y0;
    for (int32_t x = x0; x <= x1; x++)
    {
      plot(ctx, x, y);
      err -= dy;
      if (err < 0)
      {
        y += sy;
        err += dx;
      }
    }
  }
  else
  {
    if (y0 > y1)
    {
      int32_t t = x0; x0 = x1; x1 = t;
      t = y0; y0 = y1; y1 = t;
    }
    int32_t dy = y1 - y0, dx = abs32(x1 - x0), sx = (x0 < x1) ? 1 : -1;
    int32_t err = dy / 2, x = x0;
    for (int32_t y = y0; y <= y1; y++)
    {
      plot(ctx, x, y);
      err -= dx;
      if (err < 0)
      {
        x += sx;
        err += dy;
      }
    }
  }
}

void ref_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c)
{
  walk_line(x0, y0, x1, y1, plot_pixel, &c);
}

void ref_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)
{
  if (w <= 0 || h <= 0)
  {
    return;
  }
  ref_fill_rect(x, y, w, 1, c);
  ref_fill_rect(x, y + h - 1, w, 1, c);
  ref_fill_rect(x, y, 1, h, c);
  ref_fill_rect(x + w - 1, y, 1, h, c);
}

static void walk_circle(int32_t cx, int32_t cy, int32_t r, plot_fn plot, void *ctx)
{
  if (r < 0)
  {
    return;
  }
  if (r == 0)
  {
    plot(ctx, cx, cy);
    return;
  }
  int32_t f = 1 - r, ddx = 1, ddy = -2 * r, x = 0, y = r;
  plot(ctx, cx, cy + r);
  plot(ctx, cx, cy - r);
  plot(ctx, cx + r, cy);
  plot(ctx, cx - r, cy);
  while (x < y)
  {
    if (f >= 0)
    {
      y--;
      ddy += 2;
      f += ddy;
    }
    x++;
    ddx += 2;
    f += ddx;
    plot(ctx, cx + x, cy + y);
    plot(ctx, cx - x, cy + y);
    plot(ctx, cx + x, cy - y);
    plot(ctx, cx - x, cy - y);
    plot(ctx, cx + y, cy + x);
    plot(ctx, cx - y, cy + x);
    plot(ctx, cx + y, cy - x);
    plot(ctx, cx - y, cy - x);
  }
}

void ref_circle(int32_t cx, int32_t cy, int32_t r, uint16_t c)
{
  walk_circle(cx, cy, r, plot_pixel, &c);
}

/* The left end and the right end of a contour on each row */
typedef struct
{
  int32_t top, rows;
  int32_t *lo, *hi;
} row_ends;

static void ends_init(row_ends *e, int32_t top, int32_t bottom)
{
  e->top = top;
  e->rows = bottom - top + 1;
  e->lo = malloc(sizeof(int32_t) * (size_t)e->rows);
  e->hi = malloc(sizeof(int32_t) * (size_t)e->rows);
  if (e->lo == NULL || e->hi == NULL)
  {
    abort();
  }
  for (int32_t i = 0; i < e->rows; i++)
  {
    e->lo[i] = INT32_MAX;
    e->hi[i] = INT32_MIN;
  }
}

static void plot_ends(void *ctx, int32_t x, int32_t y)
{
  row_ends *e = ctx;
  int32_t i = y - e->top;
  if (i < 0 || i >= e->rows)
  {
    abort();                           /* not possible */
  }
  if (x < e->lo[i]) e->lo[i] = x;
  if (x > e->hi[i]) e->hi[i] = x;
}

static void ends_fill(row_ends *e, uint16_t c)
{
  for (int32_t i = 0; i < e->rows; i++)
  {
    if (e->lo[i] <= e->hi[i])
    {
      ref_fill_rect(e->lo[i], e->top + i, e->hi[i] - e->lo[i] + 1, 1, c);
    }
  }
  free(e->lo);
  free(e->hi);
}

void ref_fill_circle(int32_t cx, int32_t cy, int32_t r, uint16_t c)
{
  if (r < 0)
  {
    return;
  }
  row_ends e;
  ends_init(&e, cy - r, cy + r);
  walk_circle(cx, cy, r, plot_ends, &e);
  ends_fill(&e, c);
}

void ref_triangle(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint16_t c)
{
  ref_line(x0, y0, x1, y1, c);
  ref_line(x1, y1, x2, y2, c);
  ref_line(x2, y2, x0, y0, c);
}

void ref_fill_triangle(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2,
                       uint16_t c)
{
  int32_t top = y0, bottom = y0;
  if (y1 < top) top = y1;
  if (y2 < top) top = y2;
  if (y1 > bottom) bottom = y1;
  if (y2 > bottom) bottom = y2;
  row_ends e;
  ends_init(&e, top, bottom);
  walk_line(x0, y0, x1, y1, plot_ends, &e);
  walk_line(x1, y1, x2, y2, plot_ends, &e);
  walk_line(x2, y2, x0, y0, plot_ends, &e);
  ends_fill(&e, c);
}

void ref_image(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t *pixels)
{
  if (w <= 0 || h <= 0)
  {
    return;
  }
  for (int32_t j = (y < 0 ? -y : 0); j < h && y + j < ref.h; j++)
  {
    for (int32_t i = (x < 0 ? -x : 0); i < w && x + i < ref.w; i++)
    {
      ref_pixel(x + i, y + j, pixels[(size_t)j * (size_t)w + (size_t)i]);
    }
  }
}

static int32_t clamp_size(int32_t size)
{
  if (size < 1) return 1;
  if (size > REF_MAX_TEXT_SIZE) return REF_MAX_TEXT_SIZE;
  return size;
}

void ref_char(int32_t x, int32_t y, char ch, uint16_t fg, uint16_t bg, int32_t size,
              int transparent)
{
  unsigned char u = (unsigned char)ch;
  if (u < 0x20u || u > 0x7Eu)
  {
    u = '?';
  }
  const uint8_t *g = st7789_font5x7[u - 0x20u];
  int32_t s = clamp_size(size);
  for (int32_t j = 0; j < 8 * s; j++)
  {
    for (int32_t i = 0; i < 6 * s; i++)
    {
      int32_t col = i / s, row = j / s;
      int on = col < 5 && row < 7 && ((g[col] >> row) & 1u) != 0u;
      if (on)
      {
        ref_pixel(x + i, y + j, fg);
      }
      else if (!transparent)
      {
        ref_pixel(x + i, y + j, bg);
      }
    }
  }
}

void ref_string(int32_t x, int32_t y, const char *str, uint16_t fg, uint16_t bg, int32_t size,
                int wrap, int transparent)
{
  if (str == NULL)
  {
    return;
  }
  int32_t s = clamp_size(size), cw = 6 * s, chh = 8 * s;
  int32_t cx = x, cy = y;
  for (; *str != '\0'; str++)
  {
    if (*str == '\n')
    {
      cx = x;
      cy += chh;
      continue;
    }
    if (*str == '\r')
    {
      continue;
    }
    if (wrap && cx + cw > ref.w && cx > x && x + cw <= ref.w)
    {
      cx = x;
      cy += chh;
    }
    if (cy >= ref.h)
    {
      break;
    }
    ref_char(cx, cy, *str, fg, bg, s, transparent);
    cx += cw;
  }
}

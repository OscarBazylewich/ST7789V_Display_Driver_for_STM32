/**
 * The ST7789 library: the configuration of the controller, the transfers and
 * framebuffer mode. The functions that draw shapes and text are in
 * st7789_gfx.c. The functions that control the hardware are in a port file.
 *
 * These properties make the library faster than a usual driver:
 * - The library records the last window (CASET and RASET) that it sent. It
 *   sends each of these two commands again only when its parameters change.
 *   Thus, if the next window is in the same rows, the library sends only
 *   CASET again.
 * - In direct mode, the window of a pixel extends to the right and bottom
 *   edges of the screen. Thus, the library can send the next pixel in the
 *   row (or, after the end of the row, the first pixel of the window in the
 *   next row) without a command. The controller continues to write with the
 *   same RAMWR command.
 * - If a filled rectangle or an image has ST7789_DMA_MIN_PIXELS or more
 *   pixels, the DMA sends it as a background transfer. For the rectangles of
 *   a shape, the limit is ST7789_SHAPE_DMA_MIN_PIXELS. The drawing function
 *   returns immediately after the transfer starts.
 * - In framebuffer mode, the library draws into the framebuffer in RAM. It
 *   records a maximum of ST7789_MAX_DIRTY_RECTS (default 16) changed areas.
 *   If a new area is near a recorded area, the library adds the new area to
 *   the recorded area. It first examines the last area that became larger.
 * - A flush sends each area as one DMA transfer. When the transfer interrupt
 *   of the port occurs, its interrupt handler calls st7789_transfer_done().
 *   This function starts the DMA transfer of the next area. Thus, the
 *   application does not do this work. For each area, st7789_transfer_done()
 *   does a small quantity of work:
 *   - It sets the window.
 *   - If the area is less than ST7789_GATHER_WIDTH pixels wide, it copies
 *     the area into gather_buf.
 *   - It starts the DMA. Before the DMA starts, the STM32H7 port cleans the
 *     data cache.
 *   The port starts the DMA again for each row of an area that is not as
 *   wide as the screen. It does this in the interrupt handler of the DMA.
 *   But it does not do this for an area in gather_buf, because the rows in
 *   gather_buf are adjacent in memory.
 */
#include "st7789.h"
#include "st7789_internal.h"
#include "st7789_port.h"
#include <string.h>

/* The ST7789 commands (section 9 of the datasheet) */
#define CMD_SWRESET  0x01
#define CMD_SLPIN    0x10
#define CMD_SLPOUT   0x11
#define CMD_NORON    0x13
#define CMD_INVOFF   0x20
#define CMD_INVON    0x21
#define CMD_DISPOFF  0x28
#define CMD_DISPON   0x29
#define CMD_CASET    0x2A
#define CMD_RASET    0x2B
#define CMD_RAMWR    0x2C
#define CMD_VSCRDEF  0x33
#define CMD_MADCTL   0x36
#define CMD_VSCSAD   0x37
#define CMD_COLMOD   0x3A
#define CMD_FRCTRL2  0xC6

/* The MADCTL bits */
#define MADCTL_MY    0x80   /* sets the opposite row sequence */
#define MADCTL_MX    0x40   /* sets the opposite column sequence */
#define MADCTL_MV    0x20   /* interchanges the rows and the columns */
#define MADCTL_BGR   0x08

/* The controller memory always has 240 x 320 pixels */
#define GRAM_WIDTH   240
#define GRAM_HEIGHT  320

/* These checks examine the configuration macros. They use _Static_assert and
 * not #if. Thus, a configuration macro can also contain a cast or an enum
 * value. */
_Static_assert(ST7789_PANEL_WIDTH >= 1 && ST7789_PANEL_HEIGHT >= 1 &&
               (long)ST7789_PANEL_X_OFFSET >= 0 && (long)ST7789_PANEL_Y_OFFSET >= 0 &&
               ST7789_PANEL_X_OFFSET + ST7789_PANEL_WIDTH <= GRAM_WIDTH &&
               ST7789_PANEL_Y_OFFSET + ST7789_PANEL_HEIGHT <= GRAM_HEIGHT,
               "ST7789 panel size + offset must fit the controller's 240x320 memory; "
               "give the native portrait size (e.g. 135x240, not 240x135)");
_Static_assert(ST7789_MAX_DIRTY_RECTS >= 1,
               "ST7789_MAX_DIRTY_RECTS must be at least 1 (1 = one bounding box per flush)");
_Static_assert(ST7789_MAX_TEXT_SIZE >= 1, "ST7789_MAX_TEXT_SIZE must be at least 1");
_Static_assert(ST7789_IDLE_TIMEOUT_MS >= 1,
               "ST7789_IDLE_TIMEOUT_MS must be at least 1 and longer than the longest transfer");
_Static_assert((long long)ST7789_DMA_MIN_PIXELS >= 0 &&
               (long long)ST7789_DMA_MIN_PIXELS <= 0xFFFFFFFFLL &&
               (long long)ST7789_SHAPE_DMA_MIN_PIXELS >= 0 &&
               (long long)ST7789_SHAPE_DMA_MIN_PIXELS <= 0xFFFFFFFFLL,
               "ST7789_DMA_MIN_PIXELS and ST7789_SHAPE_DMA_MIN_PIXELS must be 0 to 0xFFFFFFFF");
_Static_assert((long long)ST7789_GATHER_WIDTH >= 0, "ST7789_GATHER_WIDTH must not be negative");

#if defined(__GNUC__)
#define ST7789_NOINLINE  __attribute__((noinline))
#else
#define ST7789_NOINLINE
#endif

/* The library can add a new changed area to a changed area in the list. It
 * does this if their bounding box has a maximum of this number of pixels
 * more than the two areas together. Each area in the list adds this work to
 * a flush:
 * - The window: a maximum of 7 SPI frames of commands and parameters. At
 *   each change of DC, the port waits until the SPI sent the last bit.
 * - A new start of the DMA.
 *
 * On the STM32H7 port at 100 MHz SCK, the SPI can send approximately 15 to
 * 22 pixels in this time. MERGE_SLACK_PIXELS is much larger. Thus, groups of
 * small shapes become one area before the list of changed areas is full
 * (ST7789_MAX_DIRTY_RECTS areas). When the list is full, the library adds a
 * new area to the area that becomes larger by the smallest number of pixels.
 * This area can be far from the new area. ST7789_Flush uses the same value
 * for each area to find if it is faster to send one bounding box than all
 * the areas. */
#define MERGE_SLACK_PIXELS  64u

typedef st7789_rect_t Rect;                        /* the rectangle does not include
                                                      the column x1 and the row y1 */

/* During a flush, the interrupt handler of the transfer interrupt starts the
 * DMA transfer of each subsequent area. To do this, it reads st7789_width,
 * st7789_fb and the window data (`win`, after these variables). When a
 * transfer is in progress, do not change these values. */
uint16_t  st7789_width  = ST7789_PANEL_WIDTH;
uint16_t  st7789_height = ST7789_PANEL_HEIGHT;
uint16_t *st7789_fb;

static uint8_t  colors_inverted;
static uint8_t  current_rotation;           /* an ST7789_Rotation for ST7789_GetRotation */
static uint32_t slpout_at;                  /* st7789_port_millis() at the last Sleep Out */
static uint8_t  slpout_sent;                /* slpout_at is correct */

#if ST7789_PANEL_TUNING
/* The driver from Waveshare for its 2" display module (240 x 320 pixels)
 * sends these voltage and gamma values. Each row contains the command, the
 * number of parameters and the parameters. */
static const uint8_t panel_tuning[] = {
  0xBB, 1,  0x1F,                                                  /* VCOMS: VCOM 0.875 V */
  0xC3, 1,  0x12,                                                  /* VRHS */
  0xD0, 2,  0xA4, 0xA1,                                            /* PWCTRL1 */
  0xE0, 14, 0xD0, 0x08, 0x11, 0x08, 0x0C, 0x15, 0x39, 0x33, 0x50, 0x36, 0x13, 0x14, 0x29, 0x2D,
  0xE1, 14, 0xD0, 0x08, 0x10, 0x08, 0x06, 0x06, 0x39, 0x44, 0x51, 0x0B, 0x16, 0x14, 0x2F, 0x31,
};
#endif
/* The scroll area in rows of the controller memory (TFA and VSA of VSCRDEF) */
static uint16_t scroll_top = ST7789_PANEL_Y_OFFSET, scroll_height = ST7789_PANEL_HEIGHT;

/* The window data:
 * - The last window that the library sent to the controller
 * - The position of the screen in the controller memory, for the rotation at
 *   this time
 * - In direct mode, the write cursor after the last pixel. It is correct only
 *   until the next command or the next window.
 * All this data is in one struct. Thus, the code can read and write this
 * data with one base address. */
static struct
{
  uint16_t x0, x1, y0, y1;           /* the window includes x1 and y1 */
  uint16_t col_offset, row_offset;   /* the screen offset in the controller memory */
  uint16_t cur_x, cur_y;
  uint8_t  valid;                    /* the controller has this window */
  uint8_t  cur_valid;
  uint8_t  resets;                   /* counts the st7789_link_reset() calls */
} win;

static volatile uint8_t busy;
static uint16_t fill_color ST7789_DMA_BUFFER;

/* Framebuffer mode */
static Rect     dirty[ST7789_MAX_DIRTY_RECTS];
static uint32_t dirty_count;
static uint32_t dirty_last;                       /* the last area that became larger */
st7789_rect_t   st7789_dirty_hint;                /* a copy of `dirty[dirty_last]`, or empty */
static Rect     flush_list[ST7789_MAX_DIRTY_RECTS];
static volatile uint32_t flush_count, flush_index;
static volatile uint8_t  flushing;
static volatile uint8_t  abandoning;    /* 1 while the library stops the transfer
                                           (after a timeout or in ST7789_Init) */

/* The error flags (refer to ST7789_GetErrors). Each flag stays set until
 * ST7789_ClearErrors or ST7789_Init sets it to 0. Each flag is a different
 * byte. Thus, the interrupt handler and the code in thread mode do not read,
 * change and write the same byte. */
static volatile uint8_t err_dma, err_timeout, err_link;
static volatile uint8_t resend;          /* a flush error: the next flush sends all the pixels */

/* The callbacks (ST7789_SetWaitHook, ST7789_SetDoneCallback) */
static void (*wait_hook)(void);
static void (*volatile done_callback)(void);   /* the interrupt handler of the
                                                  transfer interrupt reads it */

static void notify_done(void)
{
  void (*callback)(void) = done_callback;
  if (callback != NULL)
  {
    callback();
  }
}

/* ---- basic operations --------------------------------------------------- */

/* The wait loop is not in wait_idle() but in a different function
 * (ST7789_NOINLINE), because each drawing function first examines `busy`.
 * This check must stay a small number of instructions, without a function
 * call. */
static void ST7789_NOINLINE wait_idle_slow(void)
{
  uint32_t start = st7789_port_millis();
  while (busy)
  {
    if (st7789_port_millis() - start > ST7789_IDLE_TIMEOUT_MS)
    {
      if (!busy)
      {
        break;                       /* it completed during the time check */
      }
      /* The transfer does not complete, or the SPI bus is too slow for
         ST7789_IDLE_TIMEOUT_MS. Stop the transfer, because the application
         must not wait without a time limit. */
      uint8_t was_flushing = flushing;
      abandoning = 1;                /* if the transfer completes during this time,
                                        the library does not call the done callback */
      flushing = 0;                  /* before the DMA stops: if the transfer completes
                                        during this time, the library must not start
                                        the next DMA transfer */
      uint32_t first = flush_index;  /* stable at this time: because `flushing` is 0,
                                        st7789_transfer_done does not increase it */
      uint32_t count = flush_count;
      st7789_port_dma_abort();
      abandoning = 0;
      win.valid = 0;
      st7789_port_select(0);
      busy = 0;
      err_timeout = 1;
      if (was_flushing)              /* record again the areas that the DMA did not fully send */
      {
        for (uint32_t i = first; i < count; i++)
        {
          st7789_mark(flush_list[i].x0, flush_list[i].y0, flush_list[i].x1, flush_list[i].y1);
        }
      }
      break;
    }
    void (*hook)(void) = wait_hook;
    if (hook != NULL && busy)
    {
      hook();                        /* it stops the CPU or the task until
                                        approximately the next interrupt */
    }
  }
}

static inline void wait_idle(void)
{
  if (busy)
  {
    wait_idle_slow();
  }
}

void ST7789_WaitIdle(void)
{
  wait_idle();
}

uint8_t ST7789_IsBusy(void)
{
  return busy;
}

uint8_t ST7789_GetErrors(void)
{
  return (uint8_t)((err_timeout ? ST7789_ERR_TIMEOUT : 0u) | (err_dma ? ST7789_ERR_DMA : 0u) |
                    (err_link ? ST7789_ERR_LINK : 0u));
}

void ST7789_ClearErrors(void)
{
  err_timeout = 0;
  err_dma     = 0;
  err_link    = 0;
}

void ST7789_SetWaitHook(void (*hook)(void))
{
  wait_hook = hook;
}

void ST7789_SetDoneCallback(void (*callback)(void))
{
  done_callback = callback;
}

/* A configuration command: it waits until no transfer is in progress */
static void command(uint8_t cmd, const uint8_t *params, uint32_t len)
{
  wait_idle();
  win.cur_valid = 0;                   /* a command stops the RAMWR sequence */
  st7789_port_command(cmd, params, len);
}

/* set_window() sets the window and starts RAMWR. The window includes the
 * columns x0 to x1 and the rows y0 to y1. CS stays low. The library calls
 * set_window() only when no transfer is in progress (during a flush, the
 * interrupt handler of the transfer interrupt also calls it). */
static void set_window_again(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

static void set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
  uint8_t resets = win.resets;
  win.cur_valid = 0;
  st7789_port_select(1);
  if (!win.valid || x0 != win.x0 || x1 != win.x1)
  {
    uint16_t cols[2] = { (uint16_t)(x0 + win.col_offset), (uint16_t)(x1 + win.col_offset) };
    st7789_port_cmd_fast(CMD_CASET);
    st7789_port_data16(cols, 2);
    win.x0 = x0;
    win.x1 = x1;
  }
  if (!win.valid || y0 != win.y0 || y1 != win.y1)
  {
    uint16_t rows[2] = { (uint16_t)(y0 + win.row_offset), (uint16_t)(y1 + win.row_offset) };
    st7789_port_cmd_fast(CMD_RASET);
    st7789_port_data16(rows, 2);
    win.y0 = y0;
    win.y1 = y1;
  }
  win.valid = 1;
  st7789_port_cmd_fast(CMD_RAMWR);
  if (win.resets != resets)
  {
    set_window_again(x0, y0, x1, y1);
  }
}

/* The port started its SPI again during set_window(). Thus, the controller
 * possibly did not receive a part of the window. Send the full window
 * again. */
static void ST7789_NOINLINE set_window_again(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
  uint8_t resets = win.resets;
  uint16_t cols[2] = { (uint16_t)(x0 + win.col_offset), (uint16_t)(x1 + win.col_offset) };
  uint16_t rows[2] = { (uint16_t)(y0 + win.row_offset), (uint16_t)(y1 + win.row_offset) };
  st7789_port_cmd_fast(CMD_CASET);
  st7789_port_data16(cols, 2);
  st7789_port_cmd_fast(CMD_RASET);
  st7789_port_data16(rows, 2);
  st7789_port_cmd_fast(CMD_RAMWR);
  win.x0 = x0;
  win.x1 = x1;
  win.y0 = y0;
  win.y1 = y1;
  win.valid = (win.resets == resets) ? 1u : 0u;
}

/* The port calls this function after it started a stopped SPI again. The
 * controller possibly did not receive the data that the port sent during
 * this time. Thus, the next call of set_window() sends the full window
 * again. */
void st7789_link_reset(void)
{
  win.valid = 0;
  win.cur_valid = 0;
  win.resets++;
  err_link = 1;
}

#if ST7789_GATHER_WIDTH > 1
/* If the width of a changed area is less than ST7789_GATHER_WIDTH, the
 * library copies the area into the gather buffer. Thus, the DMA does not stop
 * between the rows (refer to st7789_config.h). The height of the area is a
 * maximum of the longer side of the panel. Thus, the gather buffer always
 * has sufficient space for the area. */
#define GATHER_ROWS  ((ST7789_PANEL_WIDTH > ST7789_PANEL_HEIGHT) ? ST7789_PANEL_WIDTH : ST7789_PANEL_HEIGHT)
static uint16_t gather_buf[(ST7789_GATHER_WIDTH - 1) * GATHER_ROWS] ST7789_DMA_BUFFER;
#endif

static void flush_send(uint32_t i)
{
  const Rect *r = &flush_list[i];
  uint32_t w = (uint32_t)(r->x1 - r->x0), h = (uint32_t)(r->y1 - r->y0);
  const uint16_t *src = st7789_fb + (uint32_t)r->y0 * st7789_width + (uint32_t)r->x0;
  set_window((uint16_t)r->x0, (uint16_t)r->y0, (uint16_t)(r->x1 - 1), (uint16_t)(r->y1 - 1));
#if ST7789_GATHER_WIDTH > 1
  if (w < ST7789_GATHER_WIDTH && h > 1u && st7789_port_dma_can_read(gather_buf))
  {
    /* The SPI sent the last bit of the previous area. Thus, the gather buffer
       is available. The interrupt handler of the transfer interrupt also
       calls this function, for the next area. */
    uint16_t *d = gather_buf;
    for (uint32_t y = 0; y < h; y++, src += st7789_width)
    {
      for (uint32_t x = 0; x < w; x++)
      {
        *d++ = src[x];
      }
    }
    /* The rows in gather_buf are adjacent in memory (the stride is w). Thus,
       the DMA does not stop between the rows. */
    st7789_port_dma16(gather_buf, w, h, w, 1u);
    return;
  }
#endif
  st7789_port_dma16(src, w, h, st7789_width, 1u);
}

/* The port calls this function from the interrupt handler of its transfer
 * interrupt, when the SPI sent the last bit of a background transfer */
void st7789_transfer_done(void)
{
  if (flushing && ++flush_index < flush_count)
  {
    flush_send(flush_index);
    return;
  }
  flushing = 0;
  st7789_port_select(0);
  busy = 0;
  if (!abandoning)
  {
    notify_done();
  }
}

/* The port calls this function from an interrupt handler when a bus error
 * occurred in a background transfer. Then the port calls
 * st7789_transfer_done(). This function does not change the list of changed
 * areas, because the code in thread mode can change it at the same time. The
 * next flush sends the full screen again. */
void st7789_transfer_error(void)
{
  err_dma = 1;
  if (flushing)
  {
    resend = 1;
  }
}

/* ---- internal functions that draw pixels -------------------------------- */

/* These functions use a clipped area. This area does not include the column
 * x1 and the row y1. */

#if defined(__GNUC__)
/* The CPU writes two pixels at a time, as one 32-bit value */
static void fb_fill(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t color)
{
  uint32_t pair = ((uint32_t)color << 16) | color;
  for (int32_t y = y0; y < y1; y++)
  {
    uint16_t *p = st7789_fb + (uint32_t)y * st7789_width + (uint32_t)x0;
    uint32_t  n = (uint32_t)(x1 - x0);
    if (((uintptr_t)p & 2u) != 0)                     /* align p to 4 bytes */
    {
      *p++ = color;
      n--;
    }
    st7789_u32_alias *q = (st7789_u32_alias *)p;
    for (; n >= 8; n -= 8)
    {
      q[0] = pair; q[1] = pair; q[2] = pair; q[3] = pair;
      q += 4;
    }
    for (; n >= 2; n -= 2)
    {
      *q++ = pair;
    }
    if (n)
    {
      *(uint16_t *)q = color;
    }
  }
}
#else
static void fb_fill(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t color)
{
  for (int32_t y = y0; y < y1; y++)
  {
    uint16_t *p = st7789_fb + (uint32_t)y * st7789_width + (uint32_t)x0;
    uint32_t  n = (uint32_t)(x1 - x0);
    while (n--)
    {
      *p++ = color;
    }
  }
}
#endif

void st7789_fill(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t color,
                 uint32_t dma_min)
{
  if (st7789_fb)
  {
    fb_fill(x0, y0, x1, y1, color);
    return;
  }

  uint32_t w = (uint32_t)(x1 - x0), h = (uint32_t)(y1 - y0);
  wait_idle();
  set_window((uint16_t)x0, (uint16_t)y0, (uint16_t)(x1 - 1), (uint16_t)(y1 - 1));
  if (w * h >= dma_min && st7789_port_dma_can_read(&fill_color))
  {
    if (fill_color != color)     /* the same color: its cache line stays cleaned */
    {
      fill_color = color;        /* safe: the previous transfer completed */
    }
    busy = 1;
    st7789_port_dma16(&fill_color, w, h, w, 0);
  }
  else
  {
    st7789_port_repeat16(color, w * h);
    st7789_port_select(0);
  }
}

/* The CPU sends h rows of w pixels, with a stride of `stride` pixels. If the
 * rows are adjacent in memory (stride == w), one call sends all the rows.
 * Thus, the SPI does not stop between the rows. */
static void send_rows(const uint16_t *src, uint32_t w, uint32_t h, uint32_t stride)
{
  if (w == stride)
  {
    st7789_port_data16(src, w * h);
    return;
  }
  for (uint32_t r = 0; r < h; r++)
  {
    st7789_port_data16(src + r * stride, w);
  }
}

void st7789_blit(int32_t x0, int32_t y0, uint32_t w, uint32_t h,
                 const uint16_t *src, uint32_t stride)
{
  if (st7789_fb)
  {
    /* The source can also be the framebuffer, to move rows (for example, to
       move the image on the screen). If the rows move down or to the right
       side, the code starts with the last row. It uses memmove() to copy
       each row from its end to its start. Thus, the code does not replace a
       pixel before it reads it. In all other conditions, the code copies from
       the start to the end. The code compares the addresses as integers,
       because this operation gives a specified result for all pairs of
       addresses. */
    uint16_t *dst = st7789_fb + (uint32_t)y0 * st7789_width + (uint32_t)x0;
    uintptr_t fb_end = (uintptr_t)(st7789_fb + (uint32_t)st7789_width * st7789_height);
    if ((uintptr_t)src >= (uintptr_t)st7789_fb && (uintptr_t)src < fb_end &&
        (uintptr_t)dst > (uintptr_t)src)
    {
      for (uint32_t r = h; r-- > 0; )
      {
        memmove(dst + r * st7789_width, src + r * stride, w * sizeof(uint16_t));
      }
    }
    else
    {
      for (uint32_t r = 0; r < h; r++)
      {
        st7789_copy16(dst + r * st7789_width, src + r * stride, w);
      }
    }
    return;
  }

  wait_idle();
  set_window((uint16_t)x0, (uint16_t)y0, (uint16_t)(x0 + (int32_t)w - 1), (uint16_t)(y0 + (int32_t)h - 1));
  if (w * h >= (uint32_t)ST7789_DMA_MIN_PIXELS && st7789_port_dma_can_read(src))
  {
    busy = 1;
    st7789_port_dma16(src, w, h, stride, 1);
  }
  else
  {
    send_rows(src, w, h, stride);
    st7789_port_select(0);
  }
}

/* When CS is high between two full bytes, the controller keeps its write
 * cursor ("serial interface pause" in the ST7789V datasheet, section 8.6 of
 * revision 1.6). Only a command stops the RAMWR sequence. Thus, for a pixel
 * at the write cursor after the last pixel, no command is necessary. The
 * library only sets CS low again and sends the pixel. The window extends to
 * the right and bottom edges of the screen. After the right edge of the
 * window, the controller continues at the left column of the window in the
 * next row. */
void st7789_pixel(int32_t x, int32_t y, uint16_t color)
{
  wait_idle();
  if (win.cur_valid && (uint16_t)x == win.cur_x && (uint16_t)y == win.cur_y)
  {
    st7789_port_select(1);
  }
  else
  {
    set_window((uint16_t)x, (uint16_t)y, (uint16_t)(st7789_width - 1u), (uint16_t)(st7789_height - 1u));
    win.cur_valid = 1;
    win.cur_x = (uint16_t)x;
    win.cur_y = (uint16_t)y;
  }
  st7789_port_repeat16(color, 1);
  st7789_port_select(0);
  if (++win.cur_x > win.x1)
  {
    win.cur_x = win.x0;
    if (++win.cur_y > win.y1)
    {
      win.cur_valid = 0;             /* after this pixel, the write cursor goes
                                        back to the start of the window */
    }
  }
}

int st7789_clip(int32_t x, int32_t y, int32_t w, int32_t h,
                int32_t *x0, int32_t *y0, int32_t *x1, int32_t *y1)
{
  if (w <= 0 || h <= 0)
  {
    return 0;
  }
  *x0 = (x < 0) ? 0 : x;
  *y0 = (y < 0) ? 0 : y;
  *x1 = (x + w > st7789_width)  ? st7789_width  : x + w;
  *y1 = (y + h > st7789_height) ? st7789_height : y + h;
  return (*x0 < *x1) && (*y0 < *y1);
}

/* ---- framebuffer mode --------------------------------------------------- */

static uint32_t rect_area(const Rect *r)
{
  return (uint32_t)(r->x1 - r->x0) * (uint32_t)(r->y1 - r->y0);
}

static Rect rect_union(const Rect *a, const Rect *b)
{
  Rect u;
  u.x0 = (a->x0 < b->x0) ? a->x0 : b->x0;
  u.y0 = (a->y0 < b->y0) ? a->y0 : b->y0;
  u.x1 = (a->x1 > b->x1) ? a->x1 : b->x1;
  u.y1 = (a->y1 > b->y1) ? a->y1 : b->y1;
  return u;
}

/* The bounding box of all the changed areas, and the sum of these areas */
static Rect dirty_bounds(uint32_t *sum)
{
  Rect all = dirty[0];
  uint32_t s = rect_area(&dirty[0]);
  for (uint32_t i = 1; i < dirty_count; i++)
  {
    all = rect_union(&all, &dirty[i]);
    s += rect_area(&dirty[i]);
  }
  *sum = s;
  return all;
}

static void mark_all(void)
{
  dirty[0]    = (Rect){ 0, 0, (int16_t)st7789_width, (int16_t)st7789_height };
  dirty_count = 1;
  dirty_last  = 0;
  st7789_dirty_hint = dirty[0];
}

static void clear_dirty(void)
{
  dirty_count = 0;
  st7789_dirty_hint = (Rect){ 0, 0, 0, 0 };
}

void st7789_mark(int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
  Rect r = { (int16_t)x0, (int16_t)y0, (int16_t)x1, (int16_t)y1 };

  /* Fast check: `r` is very frequently in the last area that became larger */
  if (dirty_count > 0)
  {
    const Rect *d = &dirty[dirty_last];
    if (r.x0 >= d->x0 && r.y0 >= d->y0 && r.x1 <= d->x1 && r.y1 <= d->y1)
    {
      return;
    }
  }

  /* The library adds `r` to an area if this adds a small number of pixels.
     Usually, the next shape or row is adjacent to the previous shape or row.
     Thus, the last area that became larger is usually the best area. Examine
     it first. */
  uint32_t r_area = rect_area(&r);
  if (dirty_count > 0)
  {
    Rect u = rect_union(&dirty[dirty_last], &r);
    if (rect_area(&u) <= rect_area(&dirty[dirty_last]) + r_area + MERGE_SLACK_PIXELS)
    {
      dirty[dirty_last] = u;
      st7789_dirty_hint = u;
      return;
    }
  }
  for (uint32_t i = 0; i < dirty_count; i++)
  {
    Rect u = rect_union(&dirty[i], &r);
    if (rect_area(&u) <= rect_area(&dirty[i]) + r_area + MERGE_SLACK_PIXELS)
    {
      dirty[i]   = u;
      dirty_last = i;
      st7789_dirty_hint = u;
      return;
    }
  }

  if (dirty_count < ST7789_MAX_DIRTY_RECTS)
  {
    dirty_last = dirty_count;
    dirty[dirty_count++] = r;
    st7789_dirty_hint = r;
    return;
  }

  /* The list is full. Find the area that `r` makes larger by the smallest
     number of pixels. Also find the bounding box of all the areas and `r`. */
  uint32_t best = 0, best_growth = UINT32_MAX, sum = r_area;
  Rect all = r;
  for (uint32_t i = 0; i < dirty_count; i++)
  {
    uint32_t area = rect_area(&dirty[i]);
    Rect u = rect_union(&dirty[i], &r);
    uint32_t growth = rect_area(&u) - area;
    if (growth < best_growth)
    {
      best_growth = growth;
      best = i;
    }
    all = rect_union(&all, &dirty[i]);
    sum += area;
  }
  /* If the sum of the areas is not less than the bounding box (because the
     areas have much overlap), keep only the bounding box. A flush of the
     bounding box sends no more pixels than a flush of the areas. Also, the
     library records each subsequent shape or pixel in the bounding box
     immediately (with random pixels on the full screen, the bounding box
     quickly becomes as large as the screen). If the sum of the areas is less
     than the bounding box, keep all the areas. Then add `r` to the area that
     `r` makes larger by the smallest number of pixels. */
  if (rect_area(&all) <= sum)
  {
    dirty[0]    = all;
    dirty_count = 1;
    dirty_last  = 0;
    st7789_dirty_hint = all;
    return;
  }
  dirty[best] = rect_union(&dirty[best], &r);
  dirty_last  = best;
  st7789_dirty_hint = dirty[best];
}

uint8_t ST7789_AttachFramebuffer(uint16_t *framebuffer, uint32_t pixels)
{
  /* This check uses ST7789_PANEL_WIDTH and ST7789_PANEL_HEIGHT of this file.
     Thus, if the application calculated the length of the framebuffer with
     different values of the configuration macros, the function rejects it.
     The library does not write after the end of the framebuffer. */
  if (framebuffer == NULL || pixels < (uint32_t)ST7789_PANEL_WIDTH * ST7789_PANEL_HEIGHT)
  {
    return 0;
  }
  wait_idle();
  st7789_fb = framebuffer;
  resend = 0;
  mark_all();
  return 1;
}

void ST7789_DetachFramebuffer(void)
{
  wait_idle();
  st7789_fb   = NULL;
  resend      = 0;
  clear_dirty();
}

uint16_t *ST7789_GetFramebuffer(void)
{
  return st7789_fb;
}

void ST7789_MarkDirty(int16_t x, int16_t y, int16_t w, int16_t h)
{
  int32_t x0, y0, x1, y1;
  if (st7789_fb && st7789_clip(x, y, w, h, &x0, &y0, &x1, &y1))
  {
    st7789_mark(x0, y0, x1, y1);
  }
}

void ST7789_Flush(void)
{
  if (!st7789_fb)
  {
    return;
  }
  wait_idle();               /* first, the previous flush must complete */
  if (resend)                      /* it had a DMA error: send the full screen again */
  {
    resend = 0;
    mark_all();
  }
  if (dirty_count == 0)
  {
    return;
  }

  /* The sum of many areas with overlaps can be more than their bounding box.
     In this condition, it is faster to send only the bounding box. */
  if (ST7789_MAX_DIRTY_RECTS > 1 && dirty_count > 1)
  {
    uint32_t sum;
    Rect all = dirty_bounds(&sum);
    if (rect_area(&all) <= sum + MERGE_SLACK_PIXELS * (dirty_count - 1))
    {
      dirty[0]    = all;
      dirty_count = 1;
    }
  }

  memcpy(flush_list, dirty, dirty_count * sizeof(Rect));
  flush_count = dirty_count;
  clear_dirty();

  if (!st7789_port_dma_can_read(st7789_fb))
  {
    /* The DMA cannot read the framebuffer. Use the slow method: the CPU
       sends it. */
    for (uint32_t i = 0; i < flush_count; i++)
    {
      const Rect *r = &flush_list[i];
      set_window((uint16_t)r->x0, (uint16_t)r->y0, (uint16_t)(r->x1 - 1), (uint16_t)(r->y1 - 1));
      send_rows(st7789_fb + (uint32_t)r->y0 * st7789_width + (uint32_t)r->x0,
                (uint32_t)(r->x1 - r->x0), (uint32_t)(r->y1 - r->y0), st7789_width);
      st7789_port_select(0);
    }
    return;
  }

  flush_index = 0;
  flushing    = 1;
  busy        = 1;
  flush_send(0);
}

/* ---- the controller and the backlight ----------------------------------- */

void ST7789_Backlight(uint8_t on)
{
  st7789_port_backlight(on);
}

void ST7789_SetRotation(ST7789_Rotation rotation)
{
  wait_idle();               /* the interrupt handler of the transfer interrupt reads
                                the dimensions and the offsets that this function changes */

  static const uint8_t madctl_for[4] = {
    0,                            /* portrait */
    MADCTL_MX | MADCTL_MV,        /* landscape */
    MADCTL_MX | MADCTL_MY,        /* portrait, turned 180 degrees */
    MADCTL_MY | MADCTL_MV,        /* landscape, turned 180 degrees */
  };
  uint8_t madctl = madctl_for[(unsigned)rotation & 3u];
  current_rotation = (uint8_t)((unsigned)rotation & 3u);

  /* The position of the screen in the controller memory, for the MX and MY
     bits of this rotation */
  uint16_t x_off = (madctl & MADCTL_MX)
                 ? (uint16_t)(GRAM_WIDTH - ST7789_PANEL_X_OFFSET - ST7789_PANEL_WIDTH)
                 : (uint16_t)ST7789_PANEL_X_OFFSET;
  uint16_t y_off = (madctl & MADCTL_MY)
                 ? (uint16_t)(GRAM_HEIGHT - ST7789_PANEL_Y_OFFSET - ST7789_PANEL_HEIGHT)
                 : (uint16_t)ST7789_PANEL_Y_OFFSET;

  if (madctl & MADCTL_MV)
  {
    st7789_width  = ST7789_PANEL_HEIGHT;
    st7789_height = ST7789_PANEL_WIDTH;
    win.col_offset = y_off;
    win.row_offset = x_off;
  }
  else
  {
    st7789_width  = ST7789_PANEL_WIDTH;
    st7789_height = ST7789_PANEL_HEIGHT;
    win.col_offset = x_off;
    win.row_offset = y_off;
  }

#if ST7789_BGR_ORDER
  madctl |= MADCTL_BGR;
#endif
  command(CMD_MADCTL, &madctl, 1);
  win.valid = 0;
  if (st7789_fb)
  {
    mark_all();
  }
}

uint16_t ST7789_Width(void)  { return st7789_width; }
uint16_t ST7789_Height(void) { return st7789_height; }
ST7789_Rotation ST7789_GetRotation(void) { return (ST7789_Rotation)current_rotation; }

void ST7789_DisplayOn(uint8_t on)
{
  command(on ? CMD_DISPON : CMD_DISPOFF, NULL, 0);
}

void ST7789_Sleep(uint8_t sleep)
{
  wait_idle();                         /* this time is part of the 120 ms interval */
  if (sleep)
  {
    /* The library must send Sleep In a minimum of 120 ms after Sleep Out.
       The millisecond counter can increase immediately after Sleep Out.
       Thus, the code waits until the counter increases by 121, not by 120.
       As a result, the time between the two commands is a minimum of
       120 ms. */
    uint32_t since = st7789_port_millis() - slpout_at;
    if (since < 121u)
    {
      st7789_port_delay_ms(121u - since);
    }
  }
  command(sleep ? CMD_SLPIN : CMD_SLPOUT, NULL, 0);
  if (!sleep)
  {
    slpout_at   = st7789_port_millis();
    slpout_sent = 1;
  }
  st7789_port_delay_ms(5);                   /* 5 ms before the next command */
}

void ST7789_InvertColors(uint8_t invert)
{
  colors_inverted = invert ? 1 : 0;
  command((colors_inverted ^ (ST7789_INVERT_COLORS ? 1u : 0u)) ? CMD_INVON : CMD_INVOFF, NULL, 0);
}

void ST7789_SetRefreshRate(uint8_t hz)
{
  /* The refresh rate for each RTNA value of FRCTRL2, with the default porch
     values (datasheet, FRCTRL2): 0 gives 119 Hz, 15 gives 60 Hz and 31 gives
     39 Hz */
  static const uint8_t rate[32] = { 119, 111, 105, 99, 94, 90, 86, 82, 78, 75, 72,
                                    69, 67, 64, 62, 60, 58, 57, 55, 53, 52, 50,
                                    49, 48, 46, 45, 44, 43, 42, 41, 40, 39 };
  uint8_t code = 0;
  while (code < 31u && rate[code] > hz)    /* the first rate at or below hz */
  {
    code++;
  }
  if (code > 0u && (int32_t)rate[code - 1u] - hz < (int32_t)hz - rate[code])
  {
    code--;                                /* the next faster rate is nearer */
  }
  command(CMD_FRCTRL2, &code, 1);
}

void ST7789_SetScrollArea(uint16_t top_fixed, uint16_t bottom_fixed)
{
  if ((uint32_t)top_fixed + bottom_fixed >= ST7789_PANEL_HEIGHT)
  {
    return;
  }
  /* The fixed areas and the scroll area (VSCRDEF) include all 320 rows of the
     controller memory. The rows that a smaller panel does not show are part
     of the fixed areas. Thus, the scrolling does not move them onto the
     screen. TFA + VSA + BFA is always 320. */
  uint16_t tfa = (uint16_t)(ST7789_PANEL_Y_OFFSET + top_fixed);
  uint16_t vsa = (uint16_t)(ST7789_PANEL_HEIGHT - top_fixed - bottom_fixed);
  uint16_t bfa = (uint16_t)(GRAM_HEIGHT - tfa - vsa);
  uint8_t p[6] = { (uint8_t)(tfa >> 8), (uint8_t)tfa,
                   (uint8_t)(vsa >> 8), (uint8_t)vsa,
                   (uint8_t)(bfa >> 8), (uint8_t)bfa };
  command(CMD_VSCRDEF, p, 6);
  scroll_top    = tfa;
  scroll_height = vsa;
  ST7789_ScrollTo(0);              /* the start row must be in the new area */
}

void ST7789_ScrollTo(uint16_t line)
{
  uint16_t start = (uint16_t)(scroll_top + line % scroll_height);
  uint8_t p[2] = { (uint8_t)(start >> 8), (uint8_t)start };
  command(CMD_VSCSAD, p, 2);
}

void ST7789_Init(void)
{
  if (busy)                        /* a new call during a transfer: stop it first */
  {
    abandoning = 1;                /* if the transfer completes during this time,
                                      the library does not call the done callback */
    flushing = 0;                  /* thus, if the transfer completes during this time,
                                      the library does not start the next DMA transfer */
    st7789_port_dma_abort();
    abandoning = 0;
    st7789_port_select(0);         /* set CS high after the stopped transfer */
  }
  busy          = 0;
  flushing      = 0;
  win.valid     = 0;
  win.cur_valid = 0;
  st7789_fb     = NULL;
  resend        = 0;
  err_dma       = 0;
  err_timeout   = 0;
  err_link      = 0;
  clear_dirty();

  st7789_port_init();
  st7789_port_hw_reset();

  /* The library must not send SWRESET during the 120 ms Sleep Out sequence.
     This is important only for a new call of ST7789_Init a short time after
     the last call, on a port without a reset pin. A reset with the pin stops
     the sequence. */
  if (slpout_sent)
  {
    uint32_t since = st7789_port_millis() - slpout_at;
    if (since < 121u)
    {
      st7789_port_delay_ms(121u - since);
    }
  }

  /* After a reset, the controller is in sleep mode (Sleep In). In this mode,
     the controller receives configuration commands and writes pixel data to
     its memory. Thus, the library sends them during the 120 ms interval
     before Sleep Out (refer to the datasheet), not after this interval. */
  command(CMD_SWRESET, NULL, 0);
  uint32_t t_reset = st7789_port_millis();
  st7789_port_delay_ms(5);              /* 5 ms after SWRESET before the next command */

  uint8_t colmod = 0x55;                /* 16 bits for each pixel (RGB565) */
  command(CMD_COLMOD, &colmod, 1);
  ST7789_SetRotation(ST7789_PORTRAIT);
  ST7789_InvertColors(0);
#if ST7789_PANEL_TUNING
  for (uint32_t i = 0; i < sizeof panel_tuning; i += 2u + panel_tuning[i + 1])
  {
    command(panel_tuning[i], &panel_tuning[i + 2], panel_tuning[i + 1]);
  }
#endif
  ST7789_SetScrollArea(0, 0);           /* no scrolling, and the recorded values agree
                                           with the scroll area in the controller */
  ST7789_FillScreen(ST7789_BLACK);      /* the DMA replaces the random pixels on the
                                           screen with black pixels during the interval */

  /* The library must send Sleep Out a minimum of 120 ms after the reset.
     The millisecond counter can increase immediately after the reset. Thus,
     the code waits until the counter increases by 121, not by 120. As a
     result, the time between the reset and Sleep Out is a minimum of 120 ms. */
  uint32_t elapsed = st7789_port_millis() - t_reset;
  if (elapsed < 121u)
  {
    st7789_port_delay_ms(121u - elapsed);
  }
  command(CMD_SLPOUT, NULL, 0);         /* it waits until the DMA transfer completes */
  slpout_at   = st7789_port_millis();
  slpout_sent = 1;
  st7789_port_delay_ms(5);              /* 5 ms after Sleep Out before the next command */
  command(CMD_DISPON, NULL, 0);

  /* The panel shows no image in the first two frames after Sleep Out. Set
     the backlight to ON after these two frames. */
  elapsed = st7789_port_millis() - slpout_at;
  if (elapsed < 40u)
  {
    st7789_port_delay_ms(40u - elapsed);
  }
  st7789_port_backlight(1);
}

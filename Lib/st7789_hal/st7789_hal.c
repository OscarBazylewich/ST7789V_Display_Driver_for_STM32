/**
 * ST7789 HAL library: the transfers, ST7789HAL_Init, the control
 * functions, the window, and the functions that fill areas and draw images.
 *
 * Each command is one CS cycle: CS low, DC low and the command byte, DC
 * high and the parameters or the pixels, CS high.
 *
 * The library sends all the pixels from one transmit buffer. It
 * writes the pixels into the transmit buffer with the high byte first. Then
 * it sends the transmit buffer. The DMA reads only this transmit buffer,
 * not the memory of the application and not the stack. Thus, the data cache
 * operation and the RAM area for the DMA have an effect on only one buffer.
 */
#include "st7789_hal.h"
#include "st7789_hal_internal.h"
#include <stddef.h>

_Static_assert(ST7789HAL_BUFFER_PIXELS >= 16 && ST7789HAL_BUFFER_PIXELS <= 32767,
               "ST7789HAL_BUFFER_PIXELS must be in the range 16 to 32767");
_Static_assert((ST7789HAL_BUFFER_PIXELS % 16) == 0,
               "ST7789HAL_BUFFER_PIXELS must be a multiple of 16");

/* The ST7789 commands */
#define CMD_SWRESET  0x01u
#define CMD_SLPIN    0x10u
#define CMD_SLPOUT   0x11u
#define CMD_NORON    0x13u
#define CMD_INVOFF   0x20u
#define CMD_INVON    0x21u
#define CMD_DISPOFF  0x28u
#define CMD_DISPON   0x29u
#define CMD_CASET    0x2Au
#define CMD_RASET    0x2Bu
#define CMD_RAMWR    0x2Cu
#define CMD_MADCTL   0x36u
#define CMD_COLMOD   0x3Au
#define CMD_RAMWRC   0x3Cu

/* The MADCTL bits */
#define MADCTL_MY    0x80u   /* sets the opposite row sequence */
#define MADCTL_MX    0x40u   /* sets the opposite column sequence */
#define MADCTL_MV    0x20u   /* interchanges the rows and the columns */
#define MADCTL_BGR   0x08u

/* The dimensions of the controller memory in portrait */
#define GRAM_WIDTH   240u
#define GRAM_HEIGHT  320u

/* The values of window_state in the handle */
#define WINDOW_NONE     0u   /* no window: ST7789HAL_WritePixels has no effect */
#define WINDOW_SET      1u   /* the next ST7789HAL_WritePixels sends RAMWR */
#define WINDOW_STARTED  2u   /* the next ST7789HAL_WritePixels sends RAMWRC */

uint16_t st7789hal_buf[ST7789HAL_BUFFER_PIXELS] ST7789HAL_BUFFER_ATTR;

/* 1 after an error in the operation at this time. Then the library does
 * not send the remaining data of the operation. */
static uint8_t st7789hal_failed;

/* ---- transfers ----------------------------------------------------------- */

static inline int ready(const ST7789HAL_Handle *h)
{
  return h != NULL && h->initialized != 0u;
}

static inline void pin(GPIO_TypeDef *port, uint16_t pin_mask, uint8_t high)
{
  if (port != NULL)
  {
    HAL_GPIO_WritePin(port, pin_mask, high ? GPIO_PIN_SET : GPIO_PIN_RESET);
  }
}

static void set_error(ST7789HAL_Handle *h, uint8_t bit)
{
  h->errors |= bit;
  st7789hal_failed = 1u;
}

/* This function sends `len` bytes and returns after the SPI sent them. It
 * uses the DMA only for data in the transmit buffer. */
static void send(ST7789HAL_Handle *h, const uint8_t *data, uint16_t len)
{
  if (st7789hal_failed || len == 0u)
  {
    return;
  }
  SPI_HandleTypeDef *hspi = h->cfg.hspi;
  uintptr_t first = (uintptr_t)data;
  uintptr_t buf   = (uintptr_t)st7789hal_buf;

  if (h->cfg.use_dma && len >= (uint32_t)ST7789HAL_DMA_MIN_BYTES &&
      first >= buf && first + len <= buf + sizeof st7789hal_buf)
  {
#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
    /* If the data cache operates, write the data from the data cache to
       the RAM. The DMA reads the RAM, not the data cache. The transmit
       buffer starts at a cache line and has full cache lines. Thus, this
       operation has an effect on only the transmit buffer. */
    if ((SCB->CCR & SCB_CCR_DC_Msk) != 0u)
    {
      uintptr_t start = first & ~(uintptr_t)31u;
      uintptr_t end   = (first + len + 31u) & ~(uintptr_t)31u;
      SCB_CleanDCache_by_Addr((uint32_t *)start, (int32_t)(end - start));
    }
#endif
    /* The cast removes `const`. In some previous HAL drivers, the data
       parameter does not have `const`. */
    if (HAL_SPI_Transmit_DMA(hspi, (uint8_t *)data, len) != HAL_OK)
    {
      set_error(h, ST7789HAL_ERROR_SPI);
      return;
    }
    uint32_t start_tick = HAL_GetTick();
    while (HAL_SPI_GetState(hspi) != HAL_SPI_STATE_READY)
    {
      if ((uint32_t)(HAL_GetTick() - start_tick) > (uint32_t)ST7789HAL_SPI_TIMEOUT_MS)
      {
        (void)HAL_SPI_Abort(hspi);
        set_error(h, ST7789HAL_ERROR_TIMEOUT);
        return;
      }
    }
    if (hspi->ErrorCode != HAL_SPI_ERROR_NONE)
    {
      set_error(h, ST7789HAL_ERROR_SPI);
    }
  }
  else if (HAL_SPI_Transmit(hspi, (uint8_t *)data, len, (uint32_t)ST7789HAL_SPI_TIMEOUT_MS) != HAL_OK)
  {
    set_error(h, ST7789HAL_ERROR_SPI);
  }
}

/* The start of an operation: a command, or a window and its pixels */
static inline void op_begin(void)
{
  st7789hal_failed = 0u;
}

static inline void cs_low(ST7789HAL_Handle *h)
{
  pin(h->cfg.cs_port, h->cfg.cs_pin, 0u);
}

static inline void cs_high(ST7789HAL_Handle *h)
{
  pin(h->cfg.cs_port, h->cfg.cs_pin, 1u);
}

/* This function sends a command byte with DC low. Then it sets DC high for
 * the parameters or the pixels. CS must be low. */
static void write_cmd(ST7789HAL_Handle *h, uint8_t cmd)
{
  pin(h->cfg.dc_port, h->cfg.dc_pin, 0u);
  send(h, &cmd, 1u);
  pin(h->cfg.dc_port, h->cfg.dc_pin, 1u);
}

/* One CS cycle with a command and its parameters. This function does not
 * start a new operation. */
static void cmd_cycle(ST7789HAL_Handle *h, uint8_t cmd, const uint8_t *params, uint16_t n)
{
  cs_low(h);
  write_cmd(h, cmd);
  send(h, params, n);
  cs_high(h);
}

/* One command as one operation. It ends the window of ST7789HAL_SetWindow. */
static void command(ST7789HAL_Handle *h, uint8_t cmd, const uint8_t *params, uint16_t n)
{
  h->window_state = WINDOW_NONE;
  op_begin();
  cmd_cycle(h, cmd, params, n);
}

/* This function sends CASET and RASET for a window that is fully on the
 * screen. It adds the offsets of the rotation at this time. */
static void set_window(ST7789HAL_Handle *h, uint32_t x, uint32_t y, uint32_t w, uint32_t hh)
{
  uint32_t c0 = x + h->x_start, c1 = c0 + w - 1u;
  uint32_t r0 = y + h->y_start, r1 = r0 + hh - 1u;
  uint8_t cols[4] = { (uint8_t)(c0 >> 8), (uint8_t)c0, (uint8_t)(c1 >> 8), (uint8_t)c1 };
  uint8_t rows[4] = { (uint8_t)(r0 >> 8), (uint8_t)r0, (uint8_t)(r1 >> 8), (uint8_t)r1 };
  cmd_cycle(h, CMD_CASET, cols, 4u);
  cmd_cycle(h, CMD_RASET, rows, 4u);
}

void st7789hal_ram_begin(ST7789HAL_Handle *h, uint32_t x, uint32_t y, uint32_t w, uint32_t hh)
{
  h->window_state = WINDOW_NONE;
  op_begin();
  set_window(h, x, y, w, hh);
  cs_low(h);
  write_cmd(h, CMD_RAMWR);
}

int st7789hal_send_buf(ST7789HAL_Handle *h, uint32_t n)
{
  send(h, (const uint8_t *)st7789hal_buf, (uint16_t)(n * 2u));
  return st7789hal_failed ? 0 : 1;
}

void st7789hal_ram_end(ST7789HAL_Handle *h)
{
  cs_high(h);
}

/* This function sends `w` x `hh` pixels from `src` (with a distance of
 * `stride` pixels between the rows) after RAMWR or RAMWRC. It interchanges
 * the bytes of the pixels in the transmit buffer. Each transfer fills the
 * full transmit buffer if possible, also with pixels from more than one row. */
static void send_pixels(ST7789HAL_Handle *h, const uint16_t *src, uint32_t w, uint32_t hh,
                        uint32_t stride)
{
  uint32_t used = 0, col = 0, row = 0;
  while (row < hh)
  {
    uint32_t take = w - col;
    if (take > ST7789HAL_BUFFER_PIXELS - used)
    {
      take = ST7789HAL_BUFFER_PIXELS - used;
    }
    const uint16_t *s = src + col;
    uint16_t *d = st7789hal_buf + used;
    for (uint32_t i = 0; i < take; i++)
    {
      d[i] = st7789hal_swap(s[i]);
    }
    used += take;
    col  += take;
    if (col == w)
    {
      col = 0;
      if (++row < hh)
      {
        src += stride;               /* only to a row of the image: in C, an
                                        address more than 1 pixel after the
                                        end of the image is not correct */
      }
    }
    if (used == ST7789HAL_BUFFER_PIXELS || row == hh)
    {
      if (!st7789hal_send_buf(h, used))
      {
        return;
      }
      used = 0;
    }
  }
}

/* This function fills an area that is fully on the screen with one color.
 * It writes the color into the transmit buffer one time. Then it sends the
 * transmit buffer as many times as necessary. */
static void fill(ST7789HAL_Handle *h, uint32_t x, uint32_t y, uint32_t w, uint32_t hh, uint16_t color)
{
  uint32_t total = w * hh;
  uint32_t n = (total < ST7789HAL_BUFFER_PIXELS) ? total : ST7789HAL_BUFFER_PIXELS;
  uint16_t v = st7789hal_swap(color);
  for (uint32_t i = 0; i < n; i++)
  {
    st7789hal_buf[i] = v;
  }
  st7789hal_ram_begin(h, x, y, w, hh);
  while (total > 0u)
  {
    uint32_t chunk = (total < n) ? total : n;
    if (!st7789hal_send_buf(h, chunk))
    {
      break;
    }
    total -= chunk;
  }
  st7789hal_ram_end(h);
}

int st7789hal_clip(const ST7789HAL_Handle *h, int32_t x, int32_t y, int32_t w, int32_t hh,
                   int32_t *x0, int32_t *y0, int32_t *x1, int32_t *y1)
{
  if (w <= 0 || hh <= 0)
  {
    return 0;
  }
  int32_t ax = (x < 0) ? 0 : x;
  int32_t ay = (y < 0) ? 0 : y;
  int32_t bx = x + w, by = y + hh;
  if (bx > (int32_t)h->width)
  {
    bx = (int32_t)h->width;
  }
  if (by > (int32_t)h->height)
  {
    by = (int32_t)h->height;
  }
  if (ax >= bx || ay >= by)
  {
    return 0;
  }
  *x0 = ax;
  *y0 = ay;
  *x1 = bx;
  *y1 = by;
  return 1;
}

void st7789hal_rect(ST7789HAL_Handle *h, int32_t x, int32_t y, int32_t w, int32_t hh,
                    uint16_t color)
{
  int32_t x0, y0, x1, y1;
  if (st7789hal_clip(h, x, y, w, hh, &x0, &y0, &x1, &y1))
  {
    fill(h, (uint32_t)x0, (uint32_t)y0, (uint32_t)(x1 - x0), (uint32_t)(y1 - y0), color);
  }
}

/* ---- control ------------------------------------------------------------- */

/* This function sets MADCTL, the dimensions and the offsets for a rotation.
 * The panel has the offsets (x_offset, y_offset) in the 240 x 320
 * controller memory in portrait. If MX is 1, the columns of the controller
 * memory have the opposite sequence. Thus, the offset of the panel from the
 * new first column is 240 - width - x_offset. MY has the same effect on
 * the rows. If MV is 1, the controller interchanges the columns and the
 * rows. */
static void apply_rotation(ST7789HAL_Handle *h, uint8_t rotation)
{
  static const uint8_t madctl_for[4] = {
    0u,                           /* portrait */
    MADCTL_MX | MADCTL_MV,        /* landscape */
    MADCTL_MX | MADCTL_MY,        /* portrait, turned 180 degrees */
    MADCTL_MY | MADCTL_MV,        /* landscape, turned 180 degrees */
  };
  const ST7789HAL_Config *c = &h->cfg;
  uint8_t madctl = madctl_for[rotation & 3u];
  h->rotation = (uint8_t)(rotation & 3u);

  uint16_t x_off = (madctl & MADCTL_MX)
                 ? (uint16_t)(GRAM_WIDTH - c->x_offset - c->width)
                 : c->x_offset;
  uint16_t y_off = (madctl & MADCTL_MY)
                 ? (uint16_t)(GRAM_HEIGHT - c->y_offset - c->height)
                 : c->y_offset;

  if (madctl & MADCTL_MV)
  {
    h->width   = c->height;
    h->height  = c->width;
    h->x_start = y_off;
    h->y_start = x_off;
  }
  else
  {
    h->width   = c->width;
    h->height  = c->height;
    h->x_start = x_off;
    h->y_start = y_off;
  }

  if (c->bgr_order)
  {
    madctl |= MADCTL_BGR;
  }
  command(h, CMD_MADCTL, &madctl, 1u);
}

static void backlight(ST7789HAL_Handle *h, uint8_t on)
{
  uint8_t level = (uint8_t)((on ? 1u : 0u) ^ (h->cfg.bl_active_low ? 1u : 0u));
  pin(h->cfg.bl_port, h->cfg.bl_pin, level);
}

ST7789HAL_Status ST7789HAL_Init(ST7789HAL_Handle *h, const ST7789HAL_Config *config)
{
  if (h == NULL || config == NULL)
  {
    return ST7789HAL_ERR_PARAM;
  }
  h->initialized = 0u;
  if (config->hspi == NULL || config->dc_port == NULL ||
      config->hspi->Init.DataSize != SPI_DATASIZE_8BIT ||
      config->width == 0u || config->width > GRAM_WIDTH ||
      config->height == 0u || config->height > GRAM_HEIGHT ||
      (uint32_t)config->width + config->x_offset > GRAM_WIDTH ||
      (uint32_t)config->height + config->y_offset > GRAM_HEIGHT ||
      (config->use_dma && config->hspi->hdmatx == NULL))
  {
    return ST7789HAL_ERR_PARAM;
  }

  h->cfg              = *config;
  h->errors           = 0u;
  h->text_wrap        = 1u;
  h->text_transparent = 0u;
  h->window_state     = WINDOW_NONE;
  h->rotation         = (uint8_t)ST7789HAL_PORTRAIT;
  h->width            = config->width;
  h->height           = config->height;
  h->x_start          = config->x_offset;
  h->y_start          = config->y_offset;

  backlight(h, 0u);                      /* the screen shows random pixels until
                                            the end of ST7789HAL_Init */
  pin(h->cfg.dc_port, h->cfg.dc_pin, 1u);
  cs_high(h);

  if (h->cfg.rst_port != NULL)
  {
    /* The RST pin must be low for a minimum of 10 us. In some previous HAL
       drivers, HAL_Delay(1) can wait less than 1 ms. Thus, the code waits
       2 ms. */
    pin(h->cfg.rst_port, h->cfg.rst_pin, 0u);
    HAL_Delay(2);
    pin(h->cfg.rst_port, h->cfg.rst_pin, 1u);
    HAL_Delay(5);
  }
  else
  {
    command(h, CMD_SWRESET, NULL, 0u);
  }
  HAL_Delay(120);                        /* 120 ms after the reset before SLPOUT */
  command(h, CMD_SLPOUT, NULL, 0u);
  HAL_Delay(120);                        /* 120 ms after SLPOUT before SLPIN, and
                                            5 ms before the other commands */

  uint8_t colmod = 0x55u;                /* 16 bits for each pixel (RGB565) */
  command(h, CMD_COLMOD, &colmod, 1u);
  apply_rotation(h, (uint8_t)ST7789HAL_PORTRAIT);
  command(h, h->cfg.invert_colors ? CMD_INVON : CMD_INVOFF, NULL, 0u);
  command(h, CMD_NORON, NULL, 0u);
  fill(h, 0u, 0u, h->width, h->height, ST7789HAL_BLACK);
  command(h, CMD_DISPON, NULL, 0u);
  backlight(h, 1u);

  if (h->errors & ST7789HAL_ERROR_TIMEOUT)
  {
    return ST7789HAL_ERR_TIMEOUT;
  }
  if (h->errors & ST7789HAL_ERROR_SPI)
  {
    return ST7789HAL_ERR_SPI;
  }
  h->initialized = 1u;
  return ST7789HAL_OK;
}

void ST7789HAL_SetRotation(ST7789HAL_Handle *h, ST7789HAL_Rotation rotation)
{
  if (ready(h))
  {
    apply_rotation(h, (uint8_t)((unsigned)rotation & 3u));
  }
}

ST7789HAL_Rotation ST7789HAL_GetRotation(const ST7789HAL_Handle *h)
{
  return ready(h) ? (ST7789HAL_Rotation)h->rotation : ST7789HAL_PORTRAIT;
}

uint16_t ST7789HAL_Width(const ST7789HAL_Handle *h)
{
  return ready(h) ? h->width : 0u;
}

uint16_t ST7789HAL_Height(const ST7789HAL_Handle *h)
{
  return ready(h) ? h->height : 0u;
}

void ST7789HAL_Backlight(ST7789HAL_Handle *h, uint8_t on)
{
  if (ready(h))
  {
    backlight(h, on);
  }
}

void ST7789HAL_DisplayOn(ST7789HAL_Handle *h, uint8_t on)
{
  if (ready(h))
  {
    command(h, on ? CMD_DISPON : CMD_DISPOFF, NULL, 0u);
  }
}

void ST7789HAL_Sleep(ST7789HAL_Handle *h, uint8_t sleep)
{
  if (ready(h))
  {
    command(h, sleep ? CMD_SLPIN : CMD_SLPOUT, NULL, 0u);
    HAL_Delay(120);                      /* the controller must not get SLPIN or
                                            SLPOUT again in this time */
  }
}

void ST7789HAL_InvertColors(ST7789HAL_Handle *h, uint8_t invert)
{
  if (ready(h))
  {
    uint8_t on = (uint8_t)((invert ? 1u : 0u) ^ (h->cfg.invert_colors ? 1u : 0u));
    command(h, on ? CMD_INVON : CMD_INVOFF, NULL, 0u);
  }
}

uint8_t ST7789HAL_GetErrors(const ST7789HAL_Handle *h)
{
  return (h != NULL) ? h->errors : 0u;
}

void ST7789HAL_ClearErrors(ST7789HAL_Handle *h)
{
  if (h != NULL)
  {
    h->errors = 0u;
  }
}

/* ---- direct access to the controller memory ------------------------------ */

void ST7789HAL_SetWindow(ST7789HAL_Handle *h, uint16_t x, uint16_t y, uint16_t w, uint16_t h_px)
{
  if (!ready(h))
  {
    return;
  }
  h->window_state = WINDOW_NONE;
  if (w == 0u || h_px == 0u ||
      (uint32_t)x + w > h->width || (uint32_t)y + h_px > h->height)
  {
    return;
  }
  op_begin();
  set_window(h, x, y, w, h_px);
  h->window_state = WINDOW_SET;
}

void ST7789HAL_WritePixels(ST7789HAL_Handle *h, const uint16_t *pixels, uint32_t count)
{
  if (!ready(h) || h->window_state == WINDOW_NONE || pixels == NULL || count == 0u)
  {
    return;
  }
  op_begin();
  cs_low(h);
  write_cmd(h, (h->window_state == WINDOW_SET) ? CMD_RAMWR : CMD_RAMWRC);
  send_pixels(h, pixels, count, 1u, count);
  cs_high(h);
  h->window_state = WINDOW_STARTED;
}

/* ---- fill and images ------------------------------------------------------- */

void ST7789HAL_FillScreen(ST7789HAL_Handle *h, uint16_t color)
{
  if (ready(h))
  {
    fill(h, 0u, 0u, h->width, h->height, color);
  }
}

void ST7789HAL_FillRect(ST7789HAL_Handle *h, int16_t x, int16_t y, int16_t w, int16_t h_px, uint16_t color)
{
  if (ready(h))
  {
    st7789hal_rect(h, x, y, w, h_px, color);
  }
}

void ST7789HAL_DrawImage(ST7789HAL_Handle *h, int16_t x, int16_t y, int16_t w, int16_t h_px,
                         const uint16_t *pixels)
{
  int32_t x0, y0, x1, y1;
  if (!ready(h) || pixels == NULL || !st7789hal_clip(h, x, y, w, h_px, &x0, &y0, &x1, &y1))
  {
    return;
  }
  uint32_t stride = (uint32_t)w;                 /* `w` is also the stride */
  const uint16_t *first = pixels + (uint32_t)(y0 - y) * stride + (uint32_t)(x0 - x);
  st7789hal_ram_begin(h, (uint32_t)x0, (uint32_t)y0, (uint32_t)(x1 - x0), (uint32_t)(y1 - y0));
  send_pixels(h, first, (uint32_t)(x1 - x0), (uint32_t)(y1 - y0), stride);
  st7789hal_ram_end(h);
}

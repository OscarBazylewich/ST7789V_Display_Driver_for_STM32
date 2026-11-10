/**
 * The simulated ST7789 controller. Refer to st7789_model.h.
 *
 * The times come from the ST7789V datasheet:
 * - After a reset (RST pin or SWRESET): 5 ms before the next command, and
 *   120 ms before SLPOUT.
 * - After SLPOUT: 5 ms before the next command, and 120 ms before SLPIN.
 * - After SLPIN: 5 ms before the next command, and 120 ms before SLPOUT.
 * - The RST pin must be low for a minimum of 10 us. The simulated
 *   controller can measure only ticks. Thus, it must see 1 tick or more.
 *
 * MADCTL:
 * - If MV is 1, the simulated controller first interchanges the column
 *   address and the row address.
 * - Then MX sets the opposite sequence of the 240 columns of the memory.
 * - MY sets the opposite sequence of the 320 rows of the memory.
 */
#include "st7789_model.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define MADCTL_MY  0x80u
#define MADCTL_MX  0x40u
#define MADCTL_MV  0x20u

model_t model;

static model_pins pins;
static int cur_cmd;                 /* -1: no command */
static uint8_t nparam;
static uint8_t params[4];
static int half;                    /* -1, or the first byte of a pixel */
static int in_reset;
static uint32_t rst_low_tick;

static int have_reset, have_slpout, have_slpin;
static uint32_t t_reset, t_slpout, t_slpin;

static int cursor_ok;               /* RAMWRC can continue */
static int pix_ok;                  /* the window of this RAMWR is correct */
static uint32_t cc, cr;             /* the write cursor */
static int run_active;              /* a RAMWR sequence (RAMWR and RAMWRC) */
static uint32_t run_pixels, run_size;
static model_cmd *cur_log;

static void model_error(const char *fmt, ...)
{
  if (model.errors++ == 0u)
  {
    va_list ap;
    va_start(ap, fmt);
    (void)vsnprintf(model.first_error, sizeof model.first_error, fmt, ap);
    va_end(ap);
  }
}

static int pin_high(GPIO_TypeDef *port, uint16_t pin)
{
  return (port->ODR & pin) != 0u;
}

static int cs_low(void)
{
  return pins.cs_port == NULL || !pin_high(pins.cs_port, pins.cs_pin);
}

int model_bl_level(void)
{
  return pins.bl_port == NULL ? -1 : pin_high(pins.bl_port, pins.bl_pin);
}

static uint8_t expected_params(int cmd)
{
  switch (cmd)
  {
    case MODEL_CASET:
    case MODEL_RASET:
      return 4u;
    case MODEL_MADCTL:
    case MODEL_COLMOD:
      return 1u;
    default:
      return 0u;
  }
}

/* The end of the command at this time (a new command, CS high or a reset) */
static void end_command(void)
{
  if (cur_cmd < 0)
  {
    return;
  }
  if (cur_cmd == MODEL_RAMWR || cur_cmd == MODEL_RAMWRC)
  {
    if (half >= 0)
    {
      model_error("command 0x%02X: odd number of bytes in the pixel data", cur_cmd);
    }
  }
  else if (nparam != expected_params(cur_cmd))
  {
    model_error("command 0x%02X: %u parameters, not %u", cur_cmd, (unsigned)nparam,
                (unsigned)expected_params(cur_cmd));
  }
  half = -1;
  cur_cmd = -1;
  cur_log = NULL;
}

static void end_run(void)
{
  if (run_active && run_pixels < run_size)
  {
    model.ramwr_short++;
  }
  run_active = 0;
}

static void do_reset(void)
{
  end_command();
  run_active = 0;
  model.madctl = 0u;
  model.colmod = 0x66u;
  model.inverted = 0u;
  model.display_on = 0u;
  model.sleeping = 1u;
  model.normal_mode = 1u;
  model.xs = 0u;
  model.xe = MODEL_COLS - 1u;
  model.ys = 0u;
  model.ye = MODEL_ROWS - 1u;
  cursor_ok = 0;
  have_reset = 1;
  have_slpout = 0;
  have_slpin = 0;
  t_reset = fake.tick;
}

void model_clear_counts(void)
{
  model.errors = 0u;
  model.first_error[0] = '\0';
  model.data_without_cmd = 0u;
  model.ramwr_short = 0u;
  model.overflow = 0u;
  model.pixels_total = 0u;
  model.hw_resets = 0u;
  model.sw_resets = 0u;
  model.log_len = 0u;
  cur_log = NULL;
}

void model_power_on(const model_pins *p, uint16_t fill)
{
  pins = *p;
  for (uint32_t r = 0; r < MODEL_ROWS; r++)
  {
    for (uint32_t c = 0; c < MODEL_COLS; c++)
    {
      model.mem[r][c] = fill;
      model.written[r][c] = 0u;
    }
  }
  cur_cmd = -1;
  half = -1;
  in_reset = 0;
  run_active = 0;
  pix_ok = 0;
  do_reset();
  have_reset = 0;                   /* no reset before the start */
  model_clear_counts();
}

int model_window_open(void)
{
  return run_active && run_pixels < run_size;
}

void model_drop_window(void)
{
  run_active = 0;
}

/* The time checks for a new command */
static void check_times(uint8_t cmd)
{
  uint32_t now = fake.tick;
  if (have_reset)
  {
    uint32_t d = now - t_reset;
    if (d < 5u)
    {
      model_error("command 0x%02X %u ms after a reset (min. 5 ms)", cmd, (unsigned)d);
    }
    if (cmd == MODEL_SLPOUT && d < 120u)
    {
      model_error("SLPOUT %u ms after a reset (min. 120 ms)", (unsigned)d);
    }
  }
  if (have_slpout)
  {
    uint32_t d = now - t_slpout;
    if (d < 5u)
    {
      model_error("command 0x%02X %u ms after SLPOUT (min. 5 ms)", cmd, (unsigned)d);
    }
    if (cmd == MODEL_SLPIN && d < 120u)
    {
      model_error("SLPIN %u ms after SLPOUT (min. 120 ms)", (unsigned)d);
    }
  }
  if (have_slpin)
  {
    uint32_t d = now - t_slpin;
    if (d < 5u)
    {
      model_error("command 0x%02X %u ms after SLPIN (min. 5 ms)", cmd, (unsigned)d);
    }
    if (cmd == MODEL_SLPOUT && d < 120u)
    {
      model_error("SLPOUT %u ms after SLPIN (min. 120 ms)", (unsigned)d);
    }
  }
}

static void start_ram(int cont)
{
  uint32_t max_c = (model.madctl & MADCTL_MV) ? MODEL_ROWS : MODEL_COLS;
  uint32_t max_r = (model.madctl & MADCTL_MV) ? MODEL_COLS : MODEL_ROWS;
  if ((model.colmod & 0x07u) != 0x05u)
  {
    model_error("pixel data with COLMOD 0x%02X (not 16 bits for each pixel)", model.colmod);
  }
  if (cont)
  {
    if (!cursor_ok)
    {
      model_error("RAMWRC without a previous RAMWR for this window");
      pix_ok = 0;
    }
    return;
  }
  end_run();
  if (model.xs > model.xe || model.ys > model.ye || model.xe >= max_c || model.ye >= max_r)
  {
    model_error("window columns %u..%u rows %u..%u is not in the memory (MADCTL 0x%02X)",
                (unsigned)model.xs, (unsigned)model.xe, (unsigned)model.ys,
                (unsigned)model.ye, model.madctl);
    pix_ok = 0;
    cursor_ok = 0;
    return;
  }
  pix_ok = 1;
  cursor_ok = 1;
  cc = model.xs;
  cr = model.ys;
  run_active = 1;
  run_pixels = 0u;
  run_size = (uint32_t)(model.xe - model.xs + 1u) * (uint32_t)(model.ye - model.ys + 1u);
}

static void command(uint8_t b)
{
  end_command();
  check_times(b);
  if (b != MODEL_RAMWRC)
  {
    end_run();
  }
  cur_cmd = b;
  nparam = 0u;
  half = -1;
  cur_log = NULL;
  if (model.log_len < MODEL_LOG_MAX)
  {
    cur_log = &model.log[model.log_len];
    memset(cur_log, 0, sizeof *cur_log);
    cur_log->cmd = b;
    cur_log->tick = fake.tick;
    cur_log->bl_level = (uint8_t)(model_bl_level() == 1);
  }
  model.log_len++;

  switch (b)
  {
    case MODEL_SWRESET:
      do_reset();
      model.sw_resets++;
      break;
    case MODEL_SLPIN:
      model.sleeping = 1u;
      have_slpin = 1;
      t_slpin = fake.tick;
      break;
    case MODEL_SLPOUT:
      model.sleeping = 0u;
      have_slpout = 1;
      t_slpout = fake.tick;
      break;
    case MODEL_NORON:
      model.normal_mode = 1u;
      break;
    case MODEL_INVOFF:
      model.inverted = 0u;
      break;
    case MODEL_INVON:
      model.inverted = 1u;
      break;
    case MODEL_DISPOFF:
      model.display_on = 0u;
      break;
    case MODEL_DISPON:
      model.display_on = 1u;
      break;
    case MODEL_RAMWR:
      start_ram(0);
      break;
    case MODEL_RAMWRC:
      start_ram(1);
      break;
    case MODEL_CASET:
    case MODEL_RASET:
    case MODEL_MADCTL:
    case MODEL_COLMOD:
      break;
    default:
      model_error("unknown command 0x%02X", b);
      break;
  }
}

static void write_pixel(uint16_t v)
{
  if (!pix_ok)
  {
    return;
  }
  if (cr > model.ye)
  {
    model_error("pixel after the end of the window (window %u..%u x %u..%u)",
                (unsigned)model.xs, (unsigned)model.xe, (unsigned)model.ys, (unsigned)model.ye);
    model.overflow++;
    cc = model.xs;
    cr = model.ys;
  }
  uint32_t x, y;
  if (model.madctl & MADCTL_MV)
  {
    x = cr;
    y = cc;
  }
  else
  {
    x = cc;
    y = cr;
  }
  if (model.madctl & MADCTL_MX)
  {
    x = MODEL_COLS - 1u - x;
  }
  if (model.madctl & MADCTL_MY)
  {
    y = MODEL_ROWS - 1u - y;
  }
  model.mem[y][x] = v;
  model.written[y][x] = 1u;
  model.pixels_total++;
  run_pixels++;
  if (cur_log != NULL)
  {
    cur_log->pixels++;
  }
  if (++cc > model.xe)
  {
    cc = model.xs;
    cr++;
  }
}

static void data(uint8_t b)
{
  if (cur_cmd < 0)
  {
    model_error("data byte 0x%02X without a command", b);
    model.data_without_cmd++;
    return;
  }
  if (cur_cmd == MODEL_RAMWR || cur_cmd == MODEL_RAMWRC)
  {
    if (half < 0)
    {
      half = b;
    }
    else
    {
      write_pixel((uint16_t)(((unsigned)half << 8) | b));
      half = -1;
    }
    return;
  }
  uint8_t n = expected_params(cur_cmd);
  if (nparam >= n)
  {
    model_error("command 0x%02X: too many parameters", cur_cmd);
    return;
  }
  params[nparam++] = b;
  if (cur_log != NULL)
  {
    cur_log->param[cur_log->nparam++] = b;
  }
  if (nparam < n)
  {
    return;
  }
  switch (cur_cmd)
  {
    case MODEL_CASET:
      model.xs = (uint16_t)((params[0] << 8) | params[1]);
      model.xe = (uint16_t)((params[2] << 8) | params[3]);
      cursor_ok = 0;
      break;
    case MODEL_RASET:
      model.ys = (uint16_t)((params[0] << 8) | params[1]);
      model.ye = (uint16_t)((params[2] << 8) | params[3]);
      cursor_ok = 0;
      break;
    case MODEL_MADCTL:
      model.madctl = params[0];
      cursor_ok = 0;
      break;
    case MODEL_COLMOD:
      model.colmod = params[0];
      break;
    default:
      break;
  }
}

void model_bytes(const uint8_t *d, uint32_t n)
{
  if (!cs_low())
  {
    model_error("%u bytes with CS high", (unsigned)n);
    return;
  }
  if (in_reset)
  {
    model_error("%u bytes with RST low", (unsigned)n);
    return;
  }
  if (!pin_high(pins.dc_port, pins.dc_pin))
  {
    if (n != 1u)
    {
      model_error("one transfer with DC low has %u bytes (not 1)", (unsigned)n);
    }
    for (uint32_t i = 0; i < n; i++)
    {
      command(d[i]);
    }
  }
  else
  {
    for (uint32_t i = 0; i < n; i++)
    {
      data(d[i]);
    }
  }
}

void model_pin(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState state)
{
  if (port == pins.cs_port && (pin & pins.cs_pin) != 0u && state == GPIO_PIN_SET)
  {
    end_command();                      /* CS high stops the command */
  }
  if (port == pins.rst_port && (pin & pins.rst_pin) != 0u)
  {
    if (state == GPIO_PIN_RESET && !in_reset)
    {
      in_reset = 1;
      rst_low_tick = fake.tick;
      end_command();
    }
    else if (state == GPIO_PIN_SET && in_reset)
    {
      in_reset = 0;
      if (fake.tick - rst_low_tick < 1u)
      {
        model_error("RST low for less than 1 tick");
      }
      do_reset();
      model.hw_resets++;
    }
  }
}

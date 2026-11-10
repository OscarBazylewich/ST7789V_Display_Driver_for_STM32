/**
 * The tests of the ST7789 HAL library without hardware.
 *
 * The library operates with the HAL functions in `fake_hal` and with the
 * simulated controller in `st7789_model.c`. After each call, the tests do
 * these checks:
 * - They compare the controller memory with the framebuffer of
 *   `reference.c`.
 * - They examine the SPI calls and the commands.
 *
 * The executable returns 0 if all the tests are correct. If not, it
 * returns 1.
 */
#include "st7789_hal.h"
#include "st7789_hal_internal.h"
#include "st7789_hal_font5x7.h"
#include "st7789_font5x7.h"
#include "st7789_model.h"
#include "reference.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

_Static_assert(ST7789HAL_MAX_TEXT_SIZE == REF_MAX_TEXT_SIZE, "text size limits must agree");

/* ---- test functions ------------------------------------------------------ */

static unsigned g_checks, g_fails, g_tests, g_failed_tests, g_test_fails, g_notes;
static const char *g_test;

static void report(const char *file, int line, const char *fmt, ...)
{
  g_fails++;
  if (g_test_fails++ < 15u)
  {
    va_list ap;
    va_start(ap, fmt);
    printf("    FAIL %s:%d: ", file, line);
    vprintf(fmt, ap);
    printf("\n");
    va_end(ap);
  }
  else if (g_test_fails == 16u)
  {
    printf("    (more failures in this test are not shown)\n");
  }
}

#define CHECK(cond, ...)                                     \
  do                                                         \
  {                                                          \
    g_checks++;                                              \
    if (!(cond))                                             \
    {                                                        \
      report(__FILE__, __LINE__, __VA_ARGS__);               \
    }                                                        \
  } while (0)

static void note(const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  printf("    NOTE: ");
  vprintf(fmt, ap);
  printf("\n");
  va_end(ap);
  g_notes++;
}

static void run_test(const char *name, void (*fn)(void))
{
  unsigned before = g_checks;
  g_test = name;
  g_test_fails = 0u;
  g_tests++;
  printf("[ RUN  ] %s\n", name);
  fflush(stdout);
  fn();
  if (g_test_fails != 0u)
  {
    g_failed_tests++;
    printf("[ FAIL ] %s (%u checks, %u failed)\n", name, g_checks - before, g_test_fails);
  }
  else
  {
    printf("[  OK  ] %s (%u checks)\n", name, g_checks - before);
  }
  fflush(stdout);
}

/* Random numbers (xorshift32). The sequence is always the same. */
static uint32_t rng_state = 0x12345678u;

static uint32_t rnd(void)
{
  uint32_t x = rng_state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  rng_state = x;
  return x;
}

static int32_t rnd_range(int32_t lo, int32_t hi)     /* from `lo` to `hi`, with `lo` and `hi` */
{
  return lo + (int32_t)(rnd() % (uint32_t)(hi - lo + 1));
}

static uint16_t color_counter;

static uint16_t next_color(void)
{
  color_counter = (uint16_t)(color_counter * 31421u + 6927u);
  return (color_counter == 0u) ? 0x1234u : color_counter;
}

/* ---- memory with an area that the CPU cannot read --------------------------- */

typedef struct
{
  void  *base;
  size_t total;
} guard_mem;

static size_t page_size(void)
{
#ifdef _WIN32
  SYSTEM_INFO si;
  GetSystemInfo(&si);
  return (size_t)si.dwPageSize;
#else
  return (size_t)sysconf(_SC_PAGESIZE);
#endif
}

/* This function returns `size` bytes of memory. If `at_end` is 1, the byte
 * after the last byte is in a memory area that the CPU cannot read. If
 * `at_end` is 0, the byte before the first byte is in that area. Thus, a
 * read out of the data stops the executable. */
static void *guard_alloc(size_t size, int at_end, guard_mem *g)
{
  size_t page = page_size();
  size_t data_pages = (size + page - 1u) / page;
  if (data_pages == 0u)
  {
    data_pages = 1u;
  }
  g->total = (data_pages + 1u) * page;
  uint8_t *base;
#ifdef _WIN32
  base = VirtualAlloc(NULL, g->total, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  if (base == NULL)
  {
    abort();
  }
  DWORD old;
  if (at_end)
  {
    (void)VirtualProtect(base + data_pages * page, page, PAGE_NOACCESS, &old);
  }
  else
  {
    (void)VirtualProtect(base, page, PAGE_NOACCESS, &old);
  }
#else
  base = mmap(NULL, g->total, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (base == MAP_FAILED)
  {
    abort();
  }
  if (at_end)
  {
    (void)mprotect(base + data_pages * page, page, PROT_NONE);
  }
  else
  {
    (void)mprotect(base, page, PROT_NONE);
  }
#endif
  g->base = base;
  return at_end ? (void *)(base + data_pages * page - size) : (void *)(base + page);
}

static void guard_free(guard_mem *g)
{
#ifdef _WIN32
  (void)VirtualFree(g->base, 0, MEM_RELEASE);
#else
  (void)munmap(g->base, g->total);
#endif
}

/* A copy of a string. The CPU cannot read the memory after the 0 byte. */
static const char *guard_string(const char *s, guard_mem *g)
{
  size_t n = strlen(s) + 1u;
  char *p = guard_alloc(n, 1, g);
  memcpy(p, s, n);
  return p;
}

/* ---- panels and the test configuration ----------------------------------- */

typedef struct
{
  uint16_t w, h, xo, yo;
} panel_t;

static const panel_t panels[] = {
  { 240, 320, 0, 0 },
  { 240, 240, 0, 0 },
  { 135, 240, 52, 40 },
  { 170, 320, 35, 0 },
};
#define N_PANELS  (sizeof panels / sizeof panels[0])

/* The configuration values for make_cfg() */
#define OPT_DMA     0x01u
#define OPT_NO_CS   0x02u
#define OPT_NO_RST  0x04u
#define OPT_NO_BL   0x08u
#define OPT_BL_LOW  0x10u
#define OPT_BGR     0x20u
#define OPT_INVERT  0x40u

#define SENTINEL    0x5A5Au   /* the controller memory at the start */

static GPIO_TypeDef port_a = { 0u, "GPIOA" };   /* DC and CS */
static GPIO_TypeDef port_b = { 0u, "GPIOB" };   /* RST */
static GPIO_TypeDef port_c = { 0u, "GPIOC" };   /* backlight */
#define DC_PIN   GPIO_PIN_1
#define CS_PIN   GPIO_PIN_2
#define RST_PIN  GPIO_PIN_3
#define BL_PIN   GPIO_PIN_15

static DMA_HandleTypeDef dma_tx;
static SPI_HandleTypeDef spi;
static ST7789HAL_Handle H;
static ST7789HAL_Config cfg;
static panel_t cur;
static unsigned cur_opts;

static void clear_counts(void)
{
  fake_hal_reset();
  fake.dma_area = st7789hal_buf;
  fake.dma_area_size = sizeof st7789hal_buf;
  fake.long_size = ST7789HAL_DMA_MIN_BYTES;
  model_clear_counts();
}

static ST7789HAL_Config make_cfg(const panel_t *p, unsigned opts)
{
  ST7789HAL_Config c;
  memset(&c, 0, sizeof c);
  c.hspi = &spi;
  c.dc_port = &port_a;
  c.dc_pin = DC_PIN;
  if (!(opts & OPT_NO_CS))
  {
    c.cs_port = &port_a;
    c.cs_pin = CS_PIN;
  }
  if (!(opts & OPT_NO_RST))
  {
    c.rst_port = &port_b;
    c.rst_pin = RST_PIN;
  }
  if (!(opts & OPT_NO_BL))
  {
    c.bl_port = &port_c;
    c.bl_pin = BL_PIN;
  }
  c.bl_active_low = (opts & OPT_BL_LOW) ? 1u : 0u;
  c.width = p->w;
  c.height = p->h;
  c.x_offset = p->xo;
  c.y_offset = p->yo;
  c.invert_colors = (opts & OPT_INVERT) ? 1u : 0u;
  c.bgr_order = (opts & OPT_BGR) ? 1u : 0u;
  c.use_dma = (opts & OPT_DMA) ? 1u : 0u;
  return c;
}

/* The start: the pins, the SPI, the simulated controller and the counters.
 * This function does not call ST7789HAL_Init(). */
static void prepare(const panel_t *p, unsigned opts)
{
  port_a.ODR = 0xFFFFu;
  port_b.ODR = 0xFFFFu;
  port_c.ODR = 0xFFFFu;
  memset(&spi, 0, sizeof spi);
  spi.Init.DataSize = SPI_DATASIZE_8BIT;
  spi.hdmatx = &dma_tx;
  spi.State = HAL_SPI_STATE_READY;
  cfg = make_cfg(p, opts);
  cur = *p;
  cur_opts = opts;
  model_pins mp;
  mp.dc_port = cfg.dc_port;
  mp.dc_pin = cfg.dc_pin;
  mp.cs_port = cfg.cs_port;
  mp.cs_pin = cfg.cs_pin;
  mp.rst_port = cfg.rst_port;
  mp.rst_pin = cfg.rst_pin;
  mp.bl_port = cfg.bl_port;
  mp.bl_pin = cfg.bl_pin;
  model_power_on(&mp, SENTINEL);
  fake_hal_set_sink(model_bytes, model_pin);
  clear_counts();
  memset(&H, 0xA5, sizeof H);          /* random data before ST7789HAL_Init() */
}

static int cs_is_high(void)
{
  return cfg.cs_port == NULL || (cfg.cs_port->ODR & cfg.cs_pin) != 0u;
}

static void panel_to_screen(int32_t px, int32_t py, int32_t *x, int32_t *y)
{
  switch (ref.rotation)
  {
    case 0:  *x = px;              *y = py;              break;
    case 1:  *x = py;              *y = ref.pw - 1 - px; break;
    case 2:  *x = ref.pw - 1 - px; *y = ref.ph - 1 - py; break;
    default: *x = ref.ph - 1 - py; *y = px;              break;
  }
}

/* This function compares all the controller memory with the framebuffer
 * of `reference.c`.
 * In the panel area, the pixels must be equal. Out of the panel area, the
 * controller must not write pixels. After a difference, the framebuffer
 * gets the pixels of the controller, to prevent more reports. */
static int compare_screen(const char *what)
{
  unsigned bad = 0;
  g_checks++;
  for (int32_t r = 0; r < MODEL_ROWS; r++)
  {
    for (int32_t c = 0; c < MODEL_COLS; c++)
    {
      int inside = c >= cur.xo && c < cur.xo + cur.w && r >= cur.yo && r < cur.yo + cur.h;
      if (inside)
      {
        int32_t px = c - cur.xo, py = r - cur.yo;
        uint16_t e = ref.fb[py][px];
        if (model.mem[r][c] != e)
        {
          if (bad == 0u)
          {
            int32_t x, y;
            panel_to_screen(px, py, &x, &y);
            report(__FILE__, __LINE__,
                   "%s: pixel (%d, %d) on the screen (rotation %d, panel %ux%u): 0x%04X, "
                   "not 0x%04X", what, (int)x, (int)y, (int)ref.rotation, cur.w, cur.h,
                   model.mem[r][c], e);
          }
          bad++;
          ref.fb[py][px] = model.mem[r][c];
        }
      }
      else if (model.written[r][c])
      {
        if (bad == 0u)
        {
          report(__FILE__, __LINE__,
                 "%s: the controller wrote the memory column %d row %d, out of the panel",
                 what, (int)c, (int)r);
        }
        bad++;
        model.written[r][c] = 0u;
      }
    }
  }
  if (bad > 1u)
  {
    printf("      (%u different pixels)\n", bad);
  }
  return bad == 0u;
}

/* The checks after each call. `window_open` = 1: the call can stop before
 * the end of the window (ST7789HAL_WritePixels). */
static void after_call_ex(const char *what, int window_open)
{
  CHECK(model.errors == 0u, "%s: controller: %s (%u errors)", what, model.first_error,
        model.errors);
  CHECK(fake.errors == 0u, "%s: HAL: %s (%u errors)", what, fake.first_error, fake.errors);
  if (!window_open)
  {
    CHECK(!model_window_open() && model.ramwr_short == 0u,
          "%s: a RAMWR did not fill its window", what);
  }
  CHECK(model.overflow == 0u, "%s: %u pixels after the end of the window", what,
        model.overflow);
  CHECK(cs_is_high(), "%s: CS is low after the call", what);
  CHECK(!fake_hal_dma_busy(), "%s: a DMA transfer did not complete", what);
  CHECK(fake.max_size <= sizeof st7789hal_buf, "%s: one SPI call with %u bytes", what,
        fake.max_size);
  if (cfg.use_dma)
  {
    CHECK(fake.n_long_tx == 0u, "%s: %u HAL_SPI_Transmit calls with ST7789HAL_DMA_MIN_BYTES "
          "bytes or more (DMA is on)",
          what, fake.n_long_tx);
  }
  else
  {
    CHECK(fake.n_dma == 0u, "%s: DMA transfers with use_dma = 0", what);
  }
  CHECK(H.errors == 0u, "%s: error bits 0x%02X", what, H.errors);
  clear_counts();
}

static void after_call(const char *what)
{
  after_call_ex(what, 0);
}

static void check_draw(const char *what)
{
  after_call(what);
  (void)compare_screen(what);
}

static char msgbuf[512];

static const char *msg(const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  (void)vsnprintf(msgbuf, sizeof msgbuf, fmt, ap);
  va_end(ap);
  return msgbuf;
}

/* The start, ST7789HAL_Init() and the checks of ST7789HAL_Init(). After
 * this function, the screen is black. */
static int setup(const panel_t *p, unsigned opts)
{
  prepare(p, opts);
  ST7789HAL_Status st = ST7789HAL_Init(&H, &cfg);
  CHECK(st == ST7789HAL_OK, "Init returned %d (panel %ux%u, options 0x%02X)", (int)st, p->w,
        p->h, opts);
  ref_begin(p->w, p->h, ST7789HAL_BLACK);
  check_draw(msg("Init (panel %ux%u, options 0x%02X)", p->w, p->h, opts));
  return st == ST7789HAL_OK;
}

static void set_rotation(int r)
{
  ST7789HAL_SetRotation(&H, (ST7789HAL_Rotation)r);
  ref_set_rotation(r);
  after_call("SetRotation");
}

/* ---- tests --------------------------------------------------------------- */

static void test_font(void)
{
  CHECK(sizeof st7789hal_font5x7 == sizeof st7789_font5x7, "the fonts have different sizes");
  CHECK(memcmp(st7789hal_font5x7, st7789_font5x7, sizeof st7789_font5x7) == 0,
        "the font is not the same as the font of Lib/st7789");
  for (unsigned i = 0; i < sizeof st7789hal_font5x7 / sizeof st7789hal_font5x7[0]; i++)
  {
    for (unsigned c = 0; c < ST7789HAL_FONT_GLYPH_W; c++)
    {
      CHECK((st7789hal_font5x7[i][c] & 0x80u) == 0u, "character 0x%02X uses bit 7", i + 0x20u);
    }
  }
  CHECK(ST7789HAL_CHAR_W == 6 && ST7789HAL_CHAR_H == 8, "character cell is not 6 x 8");
  CHECK(ST7789HAL_RGB565(255, 255, 255) == 0xFFFFu && ST7789HAL_RGB565(255, 0, 0) == 0xF800u &&
        ST7789HAL_RGB565(0, 255, 0) == 0x07E0u && ST7789HAL_RGB565(0, 0, 255) == 0x001Fu &&
        ST7789HAL_RGB565(0x12, 0x34, 0x56) == 0x11AAu, "ST7789HAL_RGB565 gives incorrect values");
}

/* All the API functions with the handle `h`. They must not use the HAL. */
static void call_all(ST7789HAL_Handle *h)
{
  static const uint16_t px[4] = { 1, 2, 3, 4 };
  ST7789HAL_SetRotation(h, ST7789HAL_LANDSCAPE);
  ST7789HAL_Backlight(h, 1);
  ST7789HAL_Backlight(h, 0);
  ST7789HAL_DisplayOn(h, 1);
  ST7789HAL_DisplayOn(h, 0);
  ST7789HAL_Sleep(h, 1);
  ST7789HAL_Sleep(h, 0);
  ST7789HAL_InvertColors(h, 1);
  ST7789HAL_ClearErrors(h);
  ST7789HAL_SetWindow(h, 0, 0, 2, 2);
  ST7789HAL_WritePixels(h, px, 4);
  ST7789HAL_FillScreen(h, 0x1234);
  ST7789HAL_FillRect(h, 0, 0, 10, 10, 0x1234);
  ST7789HAL_DrawPixel(h, 1, 1, 0x1234);
  ST7789HAL_DrawHLine(h, 0, 0, 10, 0x1234);
  ST7789HAL_DrawVLine(h, 0, 0, 10, 0x1234);
  ST7789HAL_DrawLine(h, 0, 0, 10, 7, 0x1234);
  ST7789HAL_DrawRect(h, 0, 0, 10, 10, 0x1234);
  ST7789HAL_DrawCircle(h, 20, 20, 5, 0x1234);
  ST7789HAL_FillCircle(h, 20, 20, 5, 0x1234);
  ST7789HAL_DrawTriangle(h, 0, 0, 10, 0, 5, 9, 0x1234);
  ST7789HAL_FillTriangle(h, 0, 0, 10, 0, 5, 9, 0x1234);
  ST7789HAL_DrawImage(h, 0, 0, 2, 2, px);
  ST7789HAL_DrawChar(h, 0, 0, 'A', 0x1234, 0, 1);
  ST7789HAL_DrawString(h, 0, 0, "AB", 0x1234, 0, 1);
  ST7789HAL_SetTextWrap(h, 0);
  ST7789HAL_SetTextTransparent(h, 1);
}

static void check_no_hal_use(const char *what)
{
  CHECK(fake.n_spi == 0u && fake.n_gpio == 0u && fake.n_delay == 0u && fake.n_getstate == 0u &&
        fake.n_abort == 0u, "%s: HAL calls: %u SPI, %u pin, %u delay", what, fake.n_spi,
        fake.n_gpio, fake.n_delay);
  CHECK(model.log_len == 0u, "%s: the controller received %u commands", what, model.log_len);
}

static void test_null_and_uninitialized(void)
{
  prepare(&panels[0], 0);
  call_all(NULL);
  CHECK(ST7789HAL_Width(NULL) == 0u && ST7789HAL_Height(NULL) == 0u, "size of NULL handle");
  CHECK(ST7789HAL_GetRotation(NULL) == ST7789HAL_PORTRAIT, "rotation of NULL handle");
  CHECK(ST7789HAL_GetErrors(NULL) == 0u, "errors of NULL handle");
  check_no_hal_use("NULL handle");

  ST7789HAL_Handle z;
  memset(&z, 0, sizeof z);
  z.cfg = cfg;                         /* a correct configuration, but no ST7789HAL_Init() */
  z.width = 240;
  z.height = 320;
  call_all(&z);
  CHECK(ST7789HAL_Width(&z) == 0u && ST7789HAL_Height(&z) == 0u, "size of a handle without Init");
  CHECK(ST7789HAL_GetRotation(&z) == ST7789HAL_PORTRAIT, "rotation of a handle without Init");
  CHECK(z.text_wrap == 0u && z.text_transparent == 0u, "the text settings changed without Init");
  check_no_hal_use("handle without Init");
}

static void test_init_parameters(void)
{
  struct
  {
    const char *name;
    int expect_ok;
  } cases[] = {
    { "valid 240x320", 1 },
    { "config NULL", 0 },
    { "handle NULL", 0 },
    { "hspi NULL", 0 },
    { "dc_port NULL", 0 },
    { "16-bit SPI", 0 },
    { "width 0", 0 },
    { "width 241", 0 },
    { "height 0", 0 },
    { "height 321", 0 },
    { "width + x_offset 241", 0 },
    { "height + y_offset 321", 0 },
    { "use_dma without hdmatx", 0 },
    { "x_offset 65535 (sum wraps in 16 bits)", 0 },
    { "1x1 at 239,319", 1 },
    { "240x320 without CS, RST, BL", 1 },
    { "use_dma with hdmatx", 1 },
  };
  for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; i++)
  {
    prepare(&panels[0], (i == 15u) ? (OPT_NO_CS | OPT_NO_RST | OPT_NO_BL) : 0u);
    ST7789HAL_Config c = cfg;
    ST7789HAL_Handle *hp = &H;
    const ST7789HAL_Config *cp = &c;
    panel_t p = panels[0];
    switch (i)
    {
      case 1: cp = NULL; break;
      case 2: hp = NULL; break;
      case 3: c.hspi = NULL; break;
      case 4: c.dc_port = NULL; break;
      case 5: spi.Init.DataSize = SPI_DATASIZE_16BIT; break;
      case 6: c.width = 0; break;
      case 7: c.width = 241; break;
      case 8: c.height = 0; break;
      case 9: c.height = 321; break;
      case 10: c.width = 200; c.x_offset = 41; break;
      case 11: c.height = 300; c.y_offset = 21; break;
      case 12: c.use_dma = 1; spi.hdmatx = NULL; break;
      case 13: c.width = 10; c.x_offset = 65535u; break;
      case 14: c.width = 1; c.height = 1; c.x_offset = 239; c.y_offset = 319; break;
      case 16: c.use_dma = 1; break;
      default: break;
    }
    p.w = c.width;
    p.h = c.height;
    p.xo = c.x_offset;
    p.yo = c.y_offset;
    cur = p;
    cfg = c;
    ST7789HAL_Status st = ST7789HAL_Init(hp, cp);
    if (cases[i].expect_ok)
    {
      CHECK(st == ST7789HAL_OK, "%s: Init returned %d", cases[i].name, (int)st);
      CHECK(H.initialized == 1u, "%s: not initialized", cases[i].name);
      ref_begin(p.w, p.h, ST7789HAL_BLACK);
      check_draw(cases[i].name);
    }
    else
    {
      CHECK(st == ST7789HAL_ERR_PARAM, "%s: Init returned %d, not ST7789HAL_ERR_PARAM",
            cases[i].name, (int)st);
      check_no_hal_use(cases[i].name);
      if (hp != NULL && cp != NULL)
      {
        CHECK(H.initialized == 0u, "%s: the handle is initialized", cases[i].name);
        call_all(&H);
        check_no_hal_use(cases[i].name);
      }
    }
  }

  /* A second ST7789HAL_Init() with an incorrect configuration stops the
     handle */
  setup(&panels[0], 0);
  ST7789HAL_Config bad = cfg;
  bad.width = 0;
  CHECK(ST7789HAL_Init(&H, &bad) == ST7789HAL_ERR_PARAM, "second Init with width 0");
  CHECK(ST7789HAL_Width(&H) == 0u, "the handle operates after an Init with an error");
  clear_counts();
  call_all(&H);
  check_no_hal_use("after an Init with an error");

  /* ST7789HAL_Init(h, NULL) after a correct ST7789HAL_Init() */
  setup(&panels[0], 0);
  CHECK(ST7789HAL_Init(&H, NULL) == ST7789HAL_ERR_PARAM, "Init(h, NULL)");
  if (H.initialized)
  {
    note("ST7789HAL_Init(h, NULL) returns ST7789HAL_ERR_PARAM but the handle stays initialized "
         "(other parameter errors set initialized = 0)");
  }
}

static void check_init_log(const char *name, unsigned opts)
{
  static const uint8_t base[] = { MODEL_SLPOUT, MODEL_COLMOD, MODEL_MADCTL, 0 /* INVON or INVOFF */,
                                  MODEL_NORON, MODEL_CASET, MODEL_RASET, MODEL_RAMWR,
                                  MODEL_DISPON };
  unsigned first = (opts & OPT_NO_RST) ? 1u : 0u;
  CHECK(model.log_len == first + 9u, "%s: %u commands, not %u", name, model.log_len, first + 9u);
  if (model.log_len != first + 9u)
  {
    for (unsigned i = 0; i < model.log_len && i < 20u; i++)
    {
      printf("      command %u: 0x%02X\n", i, model.log[i].cmd);
    }
    return;
  }
  if (first)
  {
    CHECK(model.log[0].cmd == MODEL_SWRESET, "%s: the first command is not SWRESET", name);
    CHECK(model.sw_resets == 1u && model.hw_resets == 0u, "%s: resets", name);
  }
  else
  {
    CHECK(model.sw_resets == 0u && model.hw_resets == 1u, "%s: resets", name);
  }
  for (unsigned i = 0; i < 9u; i++)
  {
    uint8_t e = base[i];
    if (e == 0u)
    {
      e = (opts & OPT_INVERT) ? MODEL_INVON : MODEL_INVOFF;
    }
    CHECK(model.log[first + i].cmd == e, "%s: command %u is 0x%02X, not 0x%02X", name,
          first + i, model.log[first + i].cmd, e);
  }
  const model_cmd *L = &model.log[first];
  CHECK(L[1].nparam == 1u && L[1].param[0] == 0x55u, "%s: COLMOD parameter", name);
  CHECK(L[2].nparam == 1u && L[2].param[0] == ((opts & OPT_BGR) ? 0x08u : 0x00u),
        "%s: MADCTL parameter 0x%02X", name, L[2].param[0]);
  int c0 = cur.xo, c1 = cur.xo + cur.w - 1, r0 = cur.yo, r1 = cur.yo + cur.h - 1;
  CHECK(L[5].nparam == 4u && ((L[5].param[0] << 8) | L[5].param[1]) == c0 &&
        ((L[5].param[2] << 8) | L[5].param[3]) == c1, "%s: CASET", name);
  CHECK(L[6].nparam == 4u && ((L[6].param[0] << 8) | L[6].param[1]) == r0 &&
        ((L[6].param[2] << 8) | L[6].param[3]) == r1, "%s: RASET", name);
  CHECK(L[7].pixels == (uint32_t)cur.w * cur.h, "%s: RAMWR with %u pixels", name, L[7].pixels);
  if (!(opts & OPT_NO_BL))
  {
    uint8_t off = (opts & OPT_BL_LOW) ? 1u : 0u;
    for (unsigned i = 0; i < 9u; i++)
    {
      CHECK(L[i].bl_level == off, "%s: the backlight is on at command %u", name, first + i);
    }
    CHECK(model_bl_level() == (int)(off ^ 1u), "%s: the backlight is not on after Init", name);
  }
  CHECK(L[0].tick - (first ? model.log[0].tick : L[0].tick) >= (first ? 120u : 0u),
        "%s: SLPOUT too soon after SWRESET", name);
  CHECK(model.colmod == 0x55u && model.display_on && !model.sleeping &&
        model.inverted == ((opts & OPT_INVERT) ? 1u : 0u) && model.normal_mode,
        "%s: controller state after Init", name);
  if (cfg.rst_port != NULL)
  {
    CHECK((port_b.ODR & RST_PIN) != 0u, "%s: RST is low after Init", name);
  }
}

static void test_init_sequence(void)
{
  static const unsigned opts_list[] = {
    0, OPT_NO_RST, OPT_NO_CS, OPT_NO_BL, OPT_BL_LOW, OPT_BGR, OPT_INVERT,
    OPT_DMA, OPT_NO_RST | OPT_NO_CS | OPT_BGR | OPT_INVERT | OPT_BL_LOW | OPT_DMA,
  };
  for (unsigned pi = 0; pi < N_PANELS; pi++)
  {
    for (unsigned oi = 0; oi < sizeof opts_list / sizeof opts_list[0]; oi++)
    {
      unsigned opts = opts_list[oi];
      const char *name = msg("panel %ux%u options 0x%02X", panels[pi].w, panels[pi].h, opts);
      char nm[128];
      snprintf(nm, sizeof nm, "%s", name);
      prepare(&panels[pi], opts);
      if (oi == 1u)
      {
        fake.tick = 0xFFFFFF80u;         /* the tick goes past 0xFFFFFFFF */
      }
      uint32_t t0 = fake.tick;
      ST7789HAL_Status st = ST7789HAL_Init(&H, &cfg);
      CHECK(st == ST7789HAL_OK, "%s: Init returned %d", nm, (int)st);
      CHECK(fake.tick - t0 >= 240u && fake.tick - t0 < 400u, "%s: Init took %u ms", nm,
            fake.tick - t0);
      check_init_log(nm, opts);
      CHECK(H.initialized == 1u && H.errors == 0u && H.text_wrap == 1u &&
            H.text_transparent == 0u && H.rotation == 0u, "%s: handle fields", nm);
      CHECK(ST7789HAL_Width(&H) == panels[pi].w && ST7789HAL_Height(&H) == panels[pi].h &&
            ST7789HAL_GetRotation(&H) == ST7789HAL_PORTRAIT, "%s: size after Init", nm);
      ref_begin(panels[pi].w, panels[pi].h, ST7789HAL_BLACK);
      check_draw(nm);
    }
  }
}

/* ST7789HAL_Init() with an SPI error at each of its SPI calls */
static void test_init_errors(void)
{
  static const HAL_StatusTypeDef st_list[] = { HAL_ERROR, HAL_TIMEOUT, HAL_BUSY };
  for (unsigned opts = 0; opts < 2u; opts++)
  {
    unsigned o = opts ? (OPT_NO_RST | OPT_DMA) : 0u;
    prepare(&panels[2], o);
    (void)ST7789HAL_Init(&H, &cfg);
    uint32_t n = fake.n_spi;
    for (uint32_t k = 1; k <= n; k++)
    {
      prepare(&panels[2], o);
      fake.fail_at = k;
      fake.fail_status = st_list[k % 3u];
      ST7789HAL_Status st = ST7789HAL_Init(&H, &cfg);
      CHECK(st == ST7789HAL_ERR_SPI, "SPI error at call %u of Init: Init returned %d", k, (int)st);
      CHECK(ST7789HAL_GetErrors(&H) == ST7789HAL_ERROR_SPI, "call %u: error bits 0x%02X", k,
            ST7789HAL_GetErrors(&H));
      CHECK(H.initialized == 0u, "call %u: the handle is initialized", k);
      CHECK(cs_is_high(), "call %u: CS is low after Init", k);
      CHECK(model.data_without_cmd == 0u, "call %u: the controller got data without a command",
            k);
      CHECK(fake.errors == 0u, "call %u: HAL: %s", k, fake.first_error);
      clear_counts();
      ST7789HAL_FillScreen(&H, 0x1234);
      CHECK(fake.n_spi == 0u, "call %u: FillScreen operates after an Init error", k);
    }
    /* A subsequent ST7789HAL_Init() is correct */
    prepare(&panels[2], o);
    fake.fail_at = 3;
    (void)ST7789HAL_Init(&H, &cfg);
    clear_counts();
    CHECK(ST7789HAL_Init(&H, &cfg) == ST7789HAL_OK, "Init after an Init with an error");
    CHECK(ST7789HAL_GetErrors(&H) == 0u, "Init does not set the error bits to 0");
  }

  /* A DMA transfer in ST7789HAL_Init() that does not complete */
  prepare(&panels[0], OPT_DMA);
  fake.dma_polls = FAKE_DMA_NEVER;
  ST7789HAL_Status st = ST7789HAL_Init(&H, &cfg);
  if (sizeof st7789hal_buf >= ST7789HAL_DMA_MIN_BYTES)
  {
    CHECK(st == ST7789HAL_ERR_TIMEOUT, "Init with a stopped DMA returned %d", (int)st);
    CHECK(ST7789HAL_GetErrors(&H) & ST7789HAL_ERROR_TIMEOUT, "no TIMEOUT error bit");
    CHECK(fake.n_abort == 1u, "HAL_SPI_Abort calls: %u", fake.n_abort);
    CHECK(cs_is_high(), "CS is low after the DMA timeout");
  }
  else
  {
    CHECK(st == ST7789HAL_OK, "Init without DMA transfers returned %d", (int)st);
  }
}

/* The screen corner (0, 0) for each rotation, in panel coordinates */
static void test_rotation(void)
{
  static const uint8_t madctl[4] = { 0x00u, 0x60u, 0xC0u, 0xA0u };
  for (unsigned pi = 0; pi < N_PANELS; pi++)
  {
    for (unsigned bgr = 0; bgr < 2u; bgr++)
    {
      for (int r = 0; r < 4; r++)
      {
        const panel_t *p = &panels[pi];
        if (!setup(p, bgr ? OPT_BGR : 0u))
        {
          return;
        }
        ST7789HAL_SetRotation(&H, (ST7789HAL_Rotation)r);
        ref_set_rotation(r);
        CHECK(model.log_len == 1u && model.log[0].cmd == MODEL_MADCTL &&
              model.log[0].param[0] == (madctl[r] | (bgr ? 0x08u : 0u)),
              "panel %ux%u rotation %d: MADCTL 0x%02X", p->w, p->h, r, model.log[0].param[0]);
        after_call("SetRotation");
        uint16_t sw = (r & 1) ? p->h : p->w, sh = (r & 1) ? p->w : p->h;
        CHECK(ST7789HAL_Width(&H) == sw && ST7789HAL_Height(&H) == sh &&
              ST7789HAL_GetRotation(&H) == (ST7789HAL_Rotation)r,
              "panel %ux%u rotation %d: size %ux%u", p->w, p->h, r, ST7789HAL_Width(&H),
              ST7789HAL_Height(&H));

        /* An image with a different value in each pixel */
        uint32_t n = (uint32_t)sw * sh;
        uint16_t *img = malloc(n * sizeof *img);
        for (uint32_t i = 0; i < n; i++)
        {
          img[i] = (uint16_t)(i * 2654435761u >> 7);
        }
        ST7789HAL_DrawImage(&H, 0, 0, (int16_t)sw, (int16_t)sh, img);
        ref_image(0, 0, sw, sh, img);
        check_draw(msg("full image, panel %ux%u rotation %d", p->w, p->h, r));
        free(img);

        /* The library in `Lib/st7789` gives the position of the corner
           (0, 0) of the screen on the panel:
           - ST7789HAL_PORTRAIT: top left
           - ST7789HAL_LANDSCAPE: top right
           - ST7789HAL_PORTRAIT_FLIPPED: bottom right
           - ST7789HAL_LANDSCAPE_FLIPPED: bottom left */
        ST7789HAL_DrawPixel(&H, 0, 0, 0xF00Du);
        ref_pixel(0, 0, 0xF00Du);
        static const int corner_x[4] = { 0, 1, 1, 0 }, corner_y[4] = { 0, 0, 1, 1 };
        uint32_t mc = p->xo + (corner_x[r] ? p->w - 1u : 0u);
        uint32_t mr = p->yo + (corner_y[r] ? p->h - 1u : 0u);
        CHECK(model.mem[mr][mc] == 0xF00Du, "panel %ux%u rotation %d: pixel (0, 0) not at "
              "the expected corner", p->w, p->h, r);
        /* The pixel (1, 0) is next to (0, 0), in the direction of the
           screen x axis. */
        ST7789HAL_DrawPixel(&H, 1, 0, 0xBEEFu);
        ref_pixel(1, 0, 0xBEEFu);
        static const int step_x[4] = { 1, 0, -1, 0 }, step_y[4] = { 0, 1, 0, -1 };
        CHECK(model.mem[(int32_t)mr + step_y[r]][(int32_t)mc + step_x[r]] == 0xBEEFu,
              "panel %ux%u rotation %d: the x axis has an incorrect direction", p->w, p->h, r);
        check_draw("corner pixels");

        /* A different rotation does not change the pixels */
        for (int r2 = 0; r2 < 4; r2++)
        {
          set_rotation(r2);
          (void)compare_screen(msg("rotation %d after %d", r2, r));
          ST7789HAL_FillRect(&H, 3, 5, 7, 2, (uint16_t)(0x1000u + (unsigned)r2));
          ref_fill_rect(3, 5, 7, 2, (uint16_t)(0x1000u + (unsigned)r2));
          check_draw(msg("FillRect in rotation %d", r2));
        }
      }
    }
  }
  /* Values out of the enum use the 2 low bits */
  setup(&panels[2], 0);
  ST7789HAL_SetRotation(&H, (ST7789HAL_Rotation)5);
  CHECK(ST7789HAL_GetRotation(&H) == ST7789HAL_LANDSCAPE, "rotation 5");
  ref_set_rotation(1);
  after_call("SetRotation 5");
  ST7789HAL_FillRect(&H, 0, 0, 30, 4, ST7789HAL_RED);
  ref_fill_rect(0, 0, 30, 4, ST7789HAL_RED);
  check_draw("FillRect after rotation 5");
}

static const int32_t special16[] = { -32768, -32767, -1000, -300, -2, -1, 0, 1, 2, 32766, 32767 };
#define N_SPECIAL16 (sizeof special16 / sizeof special16[0])

/* Coordinates near the edges of a screen with `n` pixels */
static int32_t edge_value(int32_t n, unsigned i)
{
  const int32_t v[] = { -n - 1, -n, -n + 1, -10, -1, 0, 1, n / 2, n - 2, n - 1, n, n + 1, 2 * n };
  return v[i % (sizeof v / sizeof v[0])];
}
#define N_EDGE 13u

static void test_fill_rect(void)
{
  static const unsigned pl[] = { 0, 2, 3 };
  for (unsigned k = 0; k < 3u; k++)
  {
    for (int r = 0; r < 4; r++)
    {
      if (!setup(&panels[pl[k]], (k == 1u) ? OPT_DMA : 0u))
      {
        return;
      }
      set_rotation(r);
      int32_t W = ref.w, Hh = ref.h;
      ST7789HAL_FillScreen(&H, 0x0841u);
      ref_fill_rect(0, 0, W, Hh, 0x0841u);
      check_draw("FillScreen");
      /* The x coordinates and the widths */
      for (unsigned i = 0; i < N_EDGE + N_SPECIAL16; i++)
      {
        int32_t x = (i < N_EDGE) ? edge_value(W, i) : special16[i - N_EDGE];
        for (unsigned j = 0; j < N_EDGE + N_SPECIAL16; j++)
        {
          int32_t w = (j < N_EDGE) ? edge_value(W, j) : special16[j - N_EDGE];
          int32_t y = rnd_range(-3, Hh + 2), hh = rnd_range(-1, 6);
          uint16_t c = next_color();
          ST7789HAL_FillRect(&H, (int16_t)x, (int16_t)y, (int16_t)w, (int16_t)hh, c);
          ref_fill_rect(x, y, w, hh, c);
          check_draw(msg("FillRect(%d, %d, %d, %d) rotation %d", (int)x, (int)y, (int)w,
                         (int)hh, r));
        }
      }
      /* The y coordinates and the heights */
      for (unsigned i = 0; i < N_EDGE + N_SPECIAL16; i++)
      {
        int32_t y = (i < N_EDGE) ? edge_value(Hh, i) : special16[i - N_EDGE];
        for (unsigned j = 0; j < N_EDGE + N_SPECIAL16; j++)
        {
          int32_t hh = (j < N_EDGE) ? edge_value(Hh, j) : special16[j - N_EDGE];
          int32_t x = rnd_range(-3, W + 2), w = rnd_range(-1, 6);
          uint16_t c = next_color();
          ST7789HAL_FillRect(&H, (int16_t)x, (int16_t)y, (int16_t)w, (int16_t)hh, c);
          ref_fill_rect(x, y, w, hh, c);
          check_draw(msg("FillRect(%d, %d, %d, %d) rotation %d", (int)x, (int)y, (int)w,
                         (int)hh, r));
        }
      }
      /* A width or a height of 0 or less: no SPI calls */
      ST7789HAL_FillRect(&H, 5, 5, 0, 10, 0xFFFF);
      ST7789HAL_FillRect(&H, 5, 5, 10, -1, 0xFFFF);
      ST7789HAL_FillRect(&H, -32768, -32768, 32767, 32767, 0xFFFF);   /* ends at -1 */
      ST7789HAL_FillRect(&H, 32767, 0, 32767, 10, 0xFFFF);
      CHECK(fake.n_spi == 0u, "a rectangle with no pixels on the screen used the SPI");
      after_call("empty rectangles");
    }
  }
}

static void test_pixels_lines_rects(void)
{
  for (unsigned pi = 0; pi < N_PANELS; pi += 2u)
  {
    for (int r = 0; r < 4; r++)
    {
      if (!setup(&panels[pi], 0))
      {
        return;
      }
      set_rotation(r);
      int32_t W = ref.w, Hh = ref.h;
      /* Pixels */
      for (unsigned i = 0; i < N_EDGE + N_SPECIAL16; i++)
      {
        for (unsigned j = 0; j < N_EDGE + N_SPECIAL16; j += 3u)
        {
          int32_t x = (i < N_EDGE) ? edge_value(W, i) : special16[i - N_EDGE];
          int32_t y = (j < N_EDGE) ? edge_value(Hh, j) : special16[j - N_EDGE];
          uint16_t c = next_color();
          ST7789HAL_DrawPixel(&H, (int16_t)x, (int16_t)y, c);
          ref_pixel(x, y, c);
          check_draw(msg("DrawPixel(%d, %d)", (int)x, (int)y));
        }
      }
      /* Horizontal and vertical lines */
      for (unsigned i = 0; i < 60u; i++)
      {
        int32_t x = (i % 3u) ? rnd_range(-20, W + 20) : special16[rnd() % N_SPECIAL16];
        int32_t y = rnd_range(-3, Hh + 2);
        int32_t n = (i % 4u) ? rnd_range(-3, W + 30) : special16[rnd() % N_SPECIAL16];
        uint16_t c = next_color();
        ST7789HAL_DrawHLine(&H, (int16_t)x, (int16_t)y, (int16_t)n, c);
        ref_fill_rect(x, y, n, 1, c);
        check_draw(msg("DrawHLine(%d, %d, %d)", (int)x, (int)y, (int)n));
        ST7789HAL_DrawVLine(&H, (int16_t)y, (int16_t)x, (int16_t)n, c);
        ref_fill_rect(y, x, 1, n, c);
        check_draw(msg("DrawVLine(%d, %d, %d)", (int)y, (int)x, (int)n));
      }
      /* Lines between all the pairs of a set of points */
      int32_t pts[][2] = {
        { 0, 0 }, { W - 1, Hh - 1 }, { W - 1, 0 }, { 0, Hh - 1 }, { W / 2, Hh / 2 },
        { 10, 13 }, { 11, 40 }, { 50, 14 }, { -30, 20 }, { 20, -30 }, { W + 25, Hh / 3 },
        { W / 3, Hh + 40 }, { -32768, -32768 }, { 32767, 32767 }, { -32768, 32767 },
        { 32767, 5 }, { 7, -32768 }, { 3, 3 },
      };
      unsigned np = sizeof pts / sizeof pts[0];
      for (unsigned a = 0; a < np; a++)
      {
        for (unsigned b = 0; b < np; b++)
        {
          uint16_t c = next_color();
          ST7789HAL_DrawLine(&H, (int16_t)pts[a][0], (int16_t)pts[a][1], (int16_t)pts[b][0],
                             (int16_t)pts[b][1], c);
          ref_line(pts[a][0], pts[a][1], pts[b][0], pts[b][1], c);
          check_draw(msg("DrawLine(%d, %d, %d, %d) rotation %d", (int)pts[a][0],
                         (int)pts[a][1], (int)pts[b][0], (int)pts[b][1], r));
        }
      }
      /* Short lines in all directions */
      for (int dx = -9; dx <= 9; dx++)
      {
        for (int dy = -9; dy <= 9; dy++)
        {
          uint16_t c = next_color();
          int32_t x0 = 20 + (dx + 9) % 5, y0 = 30 + (dy + 9) % 3;
          ST7789HAL_DrawLine(&H, (int16_t)x0, (int16_t)y0, (int16_t)(x0 + dx),
                             (int16_t)(y0 + dy), c);
          ref_line(x0, y0, x0 + dx, y0 + dy, c);
          check_draw(msg("DrawLine(%d, %d, %d, %d)", (int)x0, (int)y0, (int)(x0 + dx),
                         (int)(y0 + dy)));
        }
      }
      /* Rectangles */
      for (unsigned i = 0; i < 120u; i++)
      {
        int32_t x = (i % 5u) ? rnd_range(-20, W + 5) : special16[rnd() % N_SPECIAL16];
        int32_t y = (i % 7u) ? rnd_range(-20, Hh + 5) : special16[rnd() % N_SPECIAL16];
        int32_t w = (i % 3u) ? rnd_range(-2, 40) : rnd_range(-2, W + 40);
        int32_t hh = (i % 4u) ? rnd_range(-2, 40) : rnd_range(-2, Hh + 40);
        if (i % 11u == 0u)
        {
          w = special16[rnd() % N_SPECIAL16];
        }
        uint16_t c = next_color();
        ST7789HAL_DrawRect(&H, (int16_t)x, (int16_t)y, (int16_t)w, (int16_t)hh, c);
        ref_rect(x, y, w, hh, c);
        check_draw(msg("DrawRect(%d, %d, %d, %d)", (int)x, (int)y, (int)w, (int)hh));
      }
    }
  }
}

static void test_circles(void)
{
  static const int32_t radii[] = { -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 17, 25,
                                   31, 50, 64, 100, 119, 120, 121, 160, 200, 1000, 32767 };
  for (unsigned pi = 0; pi < N_PANELS; pi += 2u)
  {
    for (int r = 0; r < 4; r += (pi == 0u) ? 1 : 3)
    {
      if (!setup(&panels[pi], 0))
      {
        return;
      }
      set_rotation(r);
      int32_t W = ref.w, Hh = ref.h;
      int32_t centers[][2] = {
        { W / 2, Hh / 2 }, { 0, 0 }, { W - 1, Hh - 1 }, { -5, Hh / 2 }, { W / 2, Hh + 3 },
        { 30, 40 }, { -32768, -32768 }, { 32767, 32767 }, { W + 100, -100 },
      };
      for (unsigned ci = 0; ci < sizeof centers / sizeof centers[0]; ci++)
      {
        for (unsigned ri = 0; ri < sizeof radii / sizeof radii[0]; ri++)
        {
          int32_t cx = centers[ci][0], cy = centers[ci][1], rad = radii[ri];
          uint16_t c = next_color();
          ST7789HAL_DrawCircle(&H, (int16_t)cx, (int16_t)cy, (int16_t)rad, c);
          ref_circle(cx, cy, rad, c);
          check_draw(msg("DrawCircle(%d, %d, %d) rotation %d", (int)cx, (int)cy, (int)rad, r));
          c = next_color();
          ST7789HAL_FillCircle(&H, (int16_t)cx, (int16_t)cy, (int16_t)rad, c);
          ref_fill_circle(cx, cy, rad, c);
          check_draw(msg("FillCircle(%d, %d, %d) rotation %d", (int)cx, (int)cy, (int)rad, r));
        }
      }
      /* Each value of `r` from 0 to 70, with the middle near the middle of the
         screen */
      for (int32_t rad = 0; rad <= 70; rad++)
      {
        int32_t cx = W / 2 + (rad % 3) - 1, cy = Hh / 2 + (rad % 5) - 2;
        uint16_t c = next_color();
        ST7789HAL_FillCircle(&H, (int16_t)cx, (int16_t)cy, (int16_t)rad, c);
        ref_fill_circle(cx, cy, rad, c);
        check_draw(msg("FillCircle(%d, %d, %d)", (int)cx, (int)cy, (int)rad));
        c = next_color();
        ST7789HAL_DrawCircle(&H, (int16_t)cx, (int16_t)cy, (int16_t)rad, c);
        ref_circle(cx, cy, rad, c);
        check_draw(msg("DrawCircle(%d, %d, %d)", (int)cx, (int)cy, (int)rad));
      }
    }
  }
}

static void test_triangles(void)
{
  for (unsigned pi = 0; pi < N_PANELS; pi++)
  {
    for (int r = 0; r < 4; r += 1 + (int)(pi & 1u))
    {
      if (!setup(&panels[pi], 0))
      {
        return;
      }
      set_rotation(r);
      int32_t W = ref.w, Hh = ref.h;
      for (unsigned i = 0; i < 160u; i++)
      {
        int32_t v[6];
        for (unsigned k = 0; k < 6u; k++)
        {
          int32_t n = (k & 1u) ? Hh : W;
          uint32_t sel = rnd() % 16u;
          if (sel < 11u)
          {
            v[k] = rnd_range(-30, n + 30);
          }
          else if (sel < 14u)
          {
            v[k] = rnd_range(-3000, 3000);
          }
          else
          {
            v[k] = special16[rnd() % N_SPECIAL16];
          }
        }
        if (i % 10u == 1u)
        {
          v[2] = v[0];                     /* two equal corners */
          v[3] = v[1];
        }
        if (i % 10u == 2u)
        {
          v[3] = v[1];                     /* a horizontal side */
        }
        if (i % 10u == 3u)
        {
          v[4] = v[0] + 2 * (v[2] - v[0]) / 2;   /* almost on one line */
          v[5] = v[1] + 2 * (v[3] - v[1]) / 2;
          if (v[4] < -32768 || v[4] > 32767 || v[5] < -32768 || v[5] > 32767)
          {
            v[4] = v[0];
            v[5] = v[1];
          }
        }
        if (i % 10u == 4u)
        {
          v[2] = v[4] = v[0];              /* all the corners on one point */
          v[3] = v[5] = v[1];
        }
        uint16_t c = next_color();
        ST7789HAL_DrawTriangle(&H, (int16_t)v[0], (int16_t)v[1], (int16_t)v[2], (int16_t)v[3],
                               (int16_t)v[4], (int16_t)v[5], c);
        ref_triangle(v[0], v[1], v[2], v[3], v[4], v[5], c);
        check_draw(msg("DrawTriangle(%d,%d, %d,%d, %d,%d) rotation %d", (int)v[0], (int)v[1],
                       (int)v[2], (int)v[3], (int)v[4], (int)v[5], r));
        c = next_color();
        ST7789HAL_FillTriangle(&H, (int16_t)v[0], (int16_t)v[1], (int16_t)v[2], (int16_t)v[3],
                               (int16_t)v[4], (int16_t)v[5], c);
        ref_fill_triangle(v[0], v[1], v[2], v[3], v[4], v[5], c);
        check_draw(msg("FillTriangle(%d,%d, %d,%d, %d,%d) rotation %d", (int)v[0], (int)v[1],
                       (int)v[2], (int)v[3], (int)v[4], (int)v[5], r));
      }
      /* The largest triangles */
      static const int32_t big[][6] = {
        { -32768, -32768, 32767, -32768, 0, 32767 },
        { -32768, -32768, 32767, 32767, -32768, 32767 },
        { 32767, -32768, -32768, 32767, 32767, 32767 },
        { -32768, 0, 32767, 1, 0, 2 },
        { 0, -32768, 1, 32767, 2, 0 },
        { -32768, -32768, 32767, 32767, 32767, -32768 },
      };
      for (unsigned i = 0; i < sizeof big / sizeof big[0]; i++)
      {
        const int32_t *v = big[i];
        uint16_t c = next_color();
        ST7789HAL_FillTriangle(&H, (int16_t)v[0], (int16_t)v[1], (int16_t)v[2], (int16_t)v[3],
                               (int16_t)v[4], (int16_t)v[5], c);
        ref_fill_triangle(v[0], v[1], v[2], v[3], v[4], v[5], c);
        check_draw(msg("FillTriangle large %u", i));
        c = next_color();
        ST7789HAL_DrawTriangle(&H, (int16_t)v[0], (int16_t)v[1], (int16_t)v[2], (int16_t)v[3],
                               (int16_t)v[4], (int16_t)v[5], c);
        ref_triangle(v[0], v[1], v[2], v[3], v[4], v[5], c);
        check_draw(msg("DrawTriangle large %u", i));
      }
    }
  }
}

static void draw_string(int32_t x, int32_t y, const char *s, uint16_t fg, uint16_t bg,
                        int32_t size, const char *what)
{
  guard_mem g;
  const char *gs = (s != NULL) ? guard_string(s, &g) : NULL;
  ST7789HAL_DrawString(&H, (int16_t)x, (int16_t)y, gs, fg, bg, (uint8_t)size);
  ref_string(x, y, s, fg, bg, size, H.text_wrap, H.text_transparent);
  check_draw(what);
  if (gs != NULL)
  {
    guard_free(&g);
  }
}

static void test_text(void)
{
  /* All the 256 byte values, with a background color and transparent */
  if (!setup(&panels[0], 0))
  {
    return;
  }
  for (unsigned tr = 0; tr < 2u; tr++)
  {
    ST7789HAL_SetTextTransparent(&H, (uint8_t)tr);
    CHECK(H.text_transparent == tr, "SetTextTransparent");
    after_call("SetTextTransparent");
    for (unsigned b = 0; b < 256u; b++)
    {
      int32_t x = (int32_t)(b % 16u) * 14 + 1, y = (int32_t)(b / 16u) * 19 + 3;
      uint16_t fg = next_color(), bg = next_color();
      ST7789HAL_DrawChar(&H, (int16_t)x, (int16_t)y, (char)b, fg, bg, (uint8_t)(1u + tr));
      ref_char(x, y, (char)b, fg, bg, (int32_t)(1u + tr), (int)tr);
      check_draw(msg("DrawChar byte 0x%02X transparent %u", b, tr));
    }
  }

  /* The values 0..12 of `size` at positions on the edges, in all the
     rotations */
  for (unsigned pi = 0; pi < N_PANELS; pi += 2u)
  {
    for (int r = 0; r < 4; r++)
    {
      if (!setup(&panels[pi], 0))
      {
        return;
      }
      set_rotation(r);
      int32_t W = ref.w, Hh = ref.h;
      for (int32_t size = 0; size <= 12; size++)
      {
        for (unsigned tr = 0; tr < 2u; tr++)
        {
          ST7789HAL_SetTextTransparent(&H, (uint8_t)tr);
          after_call("SetTextTransparent");
          int32_t xs[] = { -7, -1, 0, 3, W - 5, W - 1, W, -32768, 32767, -6 * size + 1 };
          int32_t ys[] = { -5, 0, 7, Hh - 9, Hh - 1, Hh, -32768, 32767, -8 * size + 1 };
          for (unsigned a = 0; a < sizeof xs / sizeof xs[0]; a++)
          {
            for (unsigned b = 0; b < sizeof ys / sizeof ys[0]; b += 2u)
            {
              char ch = (char)('!' + (a * 7u + b * 3u + (unsigned)size) % 94u);
              uint16_t fg = next_color(), bg = next_color();
              ST7789HAL_DrawChar(&H, (int16_t)xs[a], (int16_t)ys[b], ch, fg, bg, (uint8_t)size);
              ref_char(xs[a], ys[b], ch, fg, bg, size, (int)tr);
              check_draw(msg("DrawChar('%c', %d, %d) size %d transparent %u rotation %d", ch,
                             (int)xs[a], (int)ys[b], (int)size, tr, r));
            }
          }
        }
      }
    }
  }

  /* Strings */
  static const char *const strings[] = {
    "",
    "A",
    "Hello, World!",
    "The quick brown fox jumps over the lazy dog 0123456789 !\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~",
    "line 1\nline 2\r\nline 3\n\nline 5\r",
    "\n\n\nX",
    "\r\r\rabc\r",
    "\x01\x1f\x7f\x80\xff high bytes \x90\xa0",
    "a very long string without any newline that must wrap at the right edge of the screen "
    "a very long string without any newline that must wrap at the right edge of the screen",
    "W\nWW\nWWW\nWWWW\nWWWWW\nWWWWWW\nWWWWWWW\nWWWWWWWW\nWWWWWWWWW\nWWWWWWWWWW",
  };
  for (unsigned pi = 0; pi < N_PANELS; pi++)
  {
    for (int r = 0; r < 4; r += 1 + (int)(pi & 1u))
    {
      if (!setup(&panels[pi], (pi == 3u) ? OPT_NO_CS : 0u))
      {
        return;
      }
      set_rotation(r);
      int32_t W = ref.w, Hh = ref.h;
      for (unsigned wrap = 0; wrap < 2u; wrap++)
      {
        ST7789HAL_SetTextWrap(&H, (uint8_t)(wrap ? 7u : 0u));
        CHECK(H.text_wrap == wrap, "SetTextWrap");
        after_call("SetTextWrap");
        for (unsigned tr = 0; tr < 2u; tr++)
        {
          ST7789HAL_SetTextTransparent(&H, (uint8_t)(tr ? 9u : 0u));
          after_call("SetTextTransparent");
          for (unsigned si = 0; si < sizeof strings / sizeof strings[0]; si++)
          {
            static const int32_t sizes[] = { 0, 1, 2, 3, 5, 8, 9, 255 };
            int32_t size = sizes[(si + wrap * 3u + tr) % 8u];
            int32_t xs[] = { 0, 5, -13, W - 20, W - 6 * (size < 1 ? 1 : (size > 8 ? 8 : size)),
                             W + 1, -32768, 32767 };
            int32_t ys[] = { 0, -11, Hh - 20, 20 };
            int32_t x = xs[(si + (unsigned)r) % 8u], y = ys[(si + wrap) % 4u];
            uint16_t fg = next_color(), bg = next_color();
            draw_string(x, y, strings[si], fg, bg, size,
                        msg("DrawString #%u at (%d, %d) size %d wrap %u transparent %u "
                            "rotation %d panel %ux%u", si, (int)x, (int)y, (int)size, wrap, tr,
                            r, panels[pi].w, panels[pi].h));
          }
        }
      }
      /* Long text to the bottom of the screen and more */
      static char longtext[3000];
      for (unsigned i = 0; i < sizeof longtext - 1u; i++)
      {
        longtext[i] = (char)(' ' + (i * 7u) % 95u);
      }
      longtext[sizeof longtext - 1u] = '\0';
      ST7789HAL_SetTextWrap(&H, 1);
      ST7789HAL_SetTextTransparent(&H, 0);
      after_call("settings");
      draw_string(-3, -4, longtext, 0xFFFF, 0x0010, 1, "long text size 1");
      draw_string(2, 1, longtext, 0xF800, 0x07E0, 3, "long text size 3");
      ST7789HAL_SetTextTransparent(&H, 1);
      after_call("settings");
      draw_string(0, 0, longtext, 0x001F, 0x0000, 2, "long transparent text size 2");
    }
  }
  /* NULL string */
  setup(&panels[2], 0);
  ST7789HAL_DrawString(&H, 0, 0, NULL, 0xFFFF, 0, 1);
  CHECK(fake.n_spi == 0u, "DrawString(NULL) used the SPI");
  after_call("DrawString NULL");
}

static void fill_pattern(uint16_t *p, uint32_t n, uint32_t seed)
{
  for (uint32_t i = 0; i < n; i++)
  {
    p[i] = (uint16_t)((i + seed) * 0x9E3779B1u >> 13);
  }
}

static void draw_image_guarded(int32_t x, int32_t y, int32_t w, int32_t h, int at_end,
                               const char *what)
{
  uint32_t n = (w > 0 && h > 0) ? (uint32_t)w * (uint32_t)h : 0u;
  guard_mem g;
  uint16_t *img = guard_alloc(n * 2u, at_end, &g);
  fill_pattern(img, n, (uint32_t)(x * 3 + y));
  ST7789HAL_DrawImage(&H, (int16_t)x, (int16_t)y, (int16_t)w, (int16_t)h, img);
  ref_image(x, y, w, h, img);
  check_draw(what);
  guard_free(&g);
}

static void test_image(void)
{
  for (unsigned pi = 0; pi < N_PANELS; pi++)
  {
    for (int r = 0; r < 4; r += 1 + (int)(pi & 1u))
    {
      if (!setup(&panels[pi], (pi == 2u) ? OPT_DMA : 0u))
      {
        return;
      }
      set_rotation(r);
      int32_t W = ref.w, Hh = ref.h;
      int32_t pos_x[] = { -40, -1, 0, 1, W / 2, W - 30, W - 1, W };
      int32_t pos_y[] = { -25, -1, 0, Hh / 3, Hh - 20, Hh - 1, Hh };
      int32_t dims[][2] = { { 1, 1 }, { 33, 21 }, { 7, 90 }, { 64, 1 }, { 1, 64 } };
      for (unsigned a = 0; a < sizeof pos_x / sizeof pos_x[0]; a++)
      {
        for (unsigned b = 0; b < sizeof pos_y / sizeof pos_y[0]; b++)
        {
          for (unsigned d = 0; d < sizeof dims / sizeof dims[0]; d++)
          {
            draw_image_guarded(pos_x[a], pos_y[b], dims[d][0], dims[d][1], (int)((a + b) & 1u),
                               msg("DrawImage(%d, %d, %d, %d) rotation %d", (int)pos_x[a],
                                   (int)pos_y[b], (int)dims[d][0], (int)dims[d][1], r));
          }
        }
      }
      /* Larger than the screen, on all the edges */
      draw_image_guarded(-10, -10, W + 20, Hh + 20, 1, "image larger than the screen");
      draw_image_guarded(-10, -10, W + 20, Hh + 20, 0, "image larger than the screen");
      draw_image_guarded(0, 0, W, Hh, 1, "full screen image");
      draw_image_guarded(-32768, -2, 32767, 3, 1, "image from x -32768 to -1");
      draw_image_guarded(-32760, 1, 32767, 2, 0, "image from x -32760 with w 32767");
      draw_image_guarded(32767, 0, 5, 5, 1, "image at x 32767");
      draw_image_guarded(-3, -32768, 4, 32767 - 10, 0, "tall image above the screen");
      draw_image_guarded(-3, -32760, 4, 32767, 1, "tall image into the screen");
      /* No pixels: the library must not read the image */
      draw_image_guarded(5, 5, 0, 10, 1, "image w 0");
      draw_image_guarded(5, 5, 10, 0, 1, "image h 0");
      draw_image_guarded(5, 5, -3, 10, 1, "image w -3");
      draw_image_guarded(5, 5, 10, -32768, 1, "image h -32768");
      ST7789HAL_DrawImage(&H, 0, 0, 10, 10, NULL);
      CHECK(fake.n_spi == 0u, "DrawImage(NULL) used the SPI");
      after_call("DrawImage NULL");
    }
  }
}

/* SetWindow and WritePixels */
static void write_window(int32_t x, int32_t y, int32_t w, int32_t h, const uint32_t *chunks,
                         unsigned n_chunks, const char *what)
{
  uint32_t n = (uint32_t)w * (uint32_t)h;
  guard_mem g;
  uint16_t *px = guard_alloc(n * 2u, 1, &g);
  fill_pattern(px, n, (uint32_t)(w + 7 * h));
  ST7789HAL_SetWindow(&H, (uint16_t)x, (uint16_t)y, (uint16_t)w, (uint16_t)h);
  CHECK(model.log_len == 2u && model.log[0].cmd == MODEL_CASET && model.log[1].cmd == MODEL_RASET,
        "%s: SetWindow sends %u commands", what, model.log_len);
  model.log_len = 0;
  uint32_t done = 0;
  unsigned k = 0;
  while (done < n)
  {
    uint32_t c = chunks[k % n_chunks];
    k++;
    if (c > n - done)
    {
      c = n - done;
    }
    uint32_t log0 = model.log_len;
    ST7789HAL_WritePixels(&H, px + done, c);
    CHECK(model.log_len == log0 + 1u &&
          model.log[log0].cmd == (done == 0u ? MODEL_RAMWR : MODEL_RAMWRC) &&
          model.log[log0].pixels == c, "%s: WritePixels %u (%u pixels) sends 0x%02X with %u "
          "pixels", what, k, c, model.log[log0].cmd, model.log[log0].pixels);
    done += c;
    CHECK(cs_is_high(), "%s: CS low after WritePixels", what);
    CHECK(model.errors == 0u, "%s: controller: %s", what, model.first_error);
  }
  ref_image(x, y, w, h, px);
  check_draw(what);
  guard_free(&g);
}

static void test_window(void)
{
  static const uint32_t one[] = { 0xFFFFFFFFu };
  static const uint32_t mixed[] = { 1, 3, 7, 16, 17, 31, 100, 513, 2, 1000 };
  static const uint32_t rows[] = { 37 };
  for (unsigned pi = 0; pi < N_PANELS; pi++)
  {
    for (int r = 0; r < 4; r += 1 + (int)(pi & 1u))
    {
      for (unsigned cs = 0; cs < 2u; cs++)
      {
        if (!setup(&panels[pi], cs ? OPT_NO_CS : (pi == 1u ? OPT_DMA : 0u)))
        {
          return;
        }
        set_rotation(r);
        int32_t W = ref.w, Hh = ref.h;
        write_window(0, 0, W, Hh, one, 1, "full screen window, one call");
        write_window(3, 4, 37, 11, rows, 1, "window, one call for each row");
        write_window(W - 50, Hh - 30, 50, 30, mixed, 10, "window, calls of different sizes");
        write_window(W - 1, Hh - 1, 1, 1, one, 1, "1 pixel window at the corner");
        write_window(0, Hh - 1, W, 1, mixed, 10, "1 row window");
        write_window(W - 1, 0, 1, Hh, mixed, 10, "1 column window");
      }
    }
  }

  if (!setup(&panels[2], 0))
  {
    return;
  }
  static uint16_t px[600];
  fill_pattern(px, 600, 5);
  int32_t W = ref.w, Hh = ref.h;

  /* Incorrect windows: no SPI calls. WritePixels has no effect. */
  int32_t bad[][4] = {
    { 0, 0, 0, 5 }, { 0, 0, 5, 0 }, { W - 4, 0, 5, 1 }, { 0, Hh - 2, 1, 3 }, { W, 0, 1, 1 },
    { 0, Hh, 1, 1 }, { 65535, 0, 2, 1 }, { 0, 65535, 1, 2 }, { 0, 0, 65535, 1 },
    { 1, 1, 65535, 65535 },
  };
  for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++)
  {
    ST7789HAL_SetWindow(&H, 0, 0, 4, 4);   /* a correct window first */
    after_call("SetWindow");
    ST7789HAL_SetWindow(&H, (uint16_t)bad[i][0], (uint16_t)bad[i][1], (uint16_t)bad[i][2],
                        (uint16_t)bad[i][3]);
    ST7789HAL_WritePixels(&H, px, 16);
    CHECK(fake.n_spi == 0u, "SetWindow(%d, %d, %d, %d): %u SPI calls", (int)bad[i][0],
          (int)bad[i][1], (int)bad[i][2], (int)bad[i][3], fake.n_spi);
    after_call("incorrect window");
  }
  (void)compare_screen("after the incorrect windows");

  /* WritePixels without a window, with NULL and with count 0 */
  setup(&panels[2], 0);
  ST7789HAL_WritePixels(&H, px, 10);
  ST7789HAL_SetWindow(&H, 0, 0, 2, 2);
  after_call("SetWindow");
  ST7789HAL_WritePixels(&H, NULL, 4);
  ST7789HAL_WritePixels(&H, px, 0);
  CHECK(fake.n_spi == 0u, "WritePixels with NULL, 0 pixels or no window used the SPI");
  ST7789HAL_WritePixels(&H, px, 4);
  CHECK(model.log_len == 1u && model.log[0].cmd == MODEL_RAMWR, "the first WritePixels after "
        "WritePixels(NULL) and count 0 does not send RAMWR");
  ref_image(0, 0, 2, 2, px);
  check_draw("2x2 window after WritePixels NULL");

  /* Functions that end the window */
  for (unsigned f = 0; f < 12u; f++)
  {
    ST7789HAL_SetWindow(&H, 10, 10, 5, 5);
    ST7789HAL_WritePixels(&H, px, 5);
    ref_image(10, 10, 5, 1, px);
    after_call_ex("window before the other function", 1);
    switch (f)
    {
      case 0: ST7789HAL_DrawPixel(&H, 0, 0, 0x1111); ref_pixel(0, 0, 0x1111); break;
      case 1: ST7789HAL_FillScreen(&H, 0x2222); ref_fill_rect(0, 0, W, Hh, 0x2222); break;
      case 2: ST7789HAL_SetRotation(&H, ST7789HAL_PORTRAIT); break;
      case 3: ST7789HAL_DisplayOn(&H, 1); break;
      case 4: ST7789HAL_InvertColors(&H, 0); break;
      case 5: ST7789HAL_Sleep(&H, 1); ST7789HAL_Sleep(&H, 0); break;
      case 6: ST7789HAL_DrawString(&H, 0, 0, "ab", 0x3333, 0x4444, 1);
              ref_string(0, 0, "ab", 0x3333, 0x4444, 1, 1, 0); break;
      case 7: ST7789HAL_SetTextTransparent(&H, 1);
              ST7789HAL_DrawChar(&H, 0, 0, 'x', 0x5555, 0, 1);
              ref_char(0, 0, 'x', 0x5555, 0, 1, 1);
              ST7789HAL_SetTextTransparent(&H, 0); break;
      case 8: ST7789HAL_DrawImage(&H, 50, 50, 3, 3, px); ref_image(50, 50, 3, 3, px); break;
      case 9: ST7789HAL_DrawLine(&H, 0, 0, 30, 9, 0x6666); ref_line(0, 0, 30, 9, 0x6666); break;
      case 10: ST7789HAL_SetWindow(&H, 0, 0, 0, 0); break;     /* incorrect window */
      default: ST7789HAL_FillCircle(&H, 40, 40, 3, 0x7777); ref_fill_circle(40, 40, 3, 0x7777);
               break;
    }
    after_call_ex(msg("function %u after a window", f), 1);
    ST7789HAL_WritePixels(&H, px + 5, 5);
    CHECK(fake.n_spi == 0u, "function %u: WritePixels continues the window after a command",
          f);
    after_call_ex(msg("function %u", f), 1);
    (void)compare_screen(msg("function %u", f));
  }

  /* Functions that send no command do not end the window */
  for (unsigned f = 0; f < 6u; f++)
  {
    ST7789HAL_SetWindow(&H, 20, 30, 4, 3);
    ST7789HAL_WritePixels(&H, px, 5);
    after_call_ex("window", 1);
    switch (f)
    {
      case 0: ST7789HAL_Backlight(&H, 1); break;
      case 1: ST7789HAL_SetTextWrap(&H, 1); break;
      case 2: ST7789HAL_FillRect(&H, 5, 5, 0, 5, 0xFFFF); break;
      case 3: ST7789HAL_DrawPixel(&H, -1, 0, 0xFFFF); break;
      case 4: (void)ST7789HAL_GetErrors(&H); ST7789HAL_ClearErrors(&H); break;
      default: ST7789HAL_DrawString(&H, W + 10, 0, "zz", 0xFFFF, 0, 1); break;
    }
    ST7789HAL_WritePixels(&H, px + 5, 7);
    CHECK(model.log_len == 1u && model.log[0].cmd == MODEL_RAMWRC, "case %u: the window "
          "stopped (0x%02X)", f, model.log[0].cmd);
    ref_image(20, 30, 4, 3, px);
    check_draw(msg("window continues after case %u", f));
  }

  /* More pixels than the window */
  ST7789HAL_SetWindow(&H, 60, 70, 4, 4);
  after_call("SetWindow");
  ST7789HAL_WritePixels(&H, px, 20);
  CHECK(cs_is_high(), "CS low after WritePixels");
  if (model.overflow != 0u)
  {
    note("WritePixels(20 pixels) after SetWindow(4 x 4) sends all 20 pixels. The controller "
         "writes pixels 16..19 again at the start of the window.");
  }
  /* The controller writes pixels 16..19 again at the window start */
  ref_image(60, 70, 4, 4, px);
  ref_image(60, 70, 4, 1, px + 16);
  clear_counts();
  (void)compare_screen("more pixels than the window");

  /* A large WritePixels call (more than 65535 bytes) */
  setup(&panels[0], OPT_DMA);
  uint32_t n = 240u * 320u;
  guard_mem g;
  uint16_t *big = guard_alloc(n * 2u, 1, &g);
  fill_pattern(big, n, 77);
  ST7789HAL_SetWindow(&H, 0, 0, 240, 320);
  ST7789HAL_WritePixels(&H, big, n);
  ref_image(0, 0, 240, 320, big);
  check_draw("one WritePixels with 76800 pixels");
  guard_free(&g);
}

static void test_dma(void)
{
  const uint32_t min_px = ST7789HAL_DMA_MIN_BYTES / 2;
  int dma_possible = sizeof st7789hal_buf >= ST7789HAL_DMA_MIN_BYTES;
  if (!setup(&panels[0], OPT_DMA))
  {
    return;
  }
  uint32_t log0 = fake.log_len;
  ST7789HAL_FillScreen(&H, 0x1234);
  ref_fill_rect(0, 0, 240, 320, 0x1234);
  CHECK(dma_possible ? (fake.n_dma > 0u && fake.n_long_tx == 0u) : fake.n_dma == 0u,
        "FillScreen: %u DMA transfers, %u long HAL_SPI_Transmit calls", fake.n_dma,
        fake.n_long_tx);
  (void)log0;
  check_draw("FillScreen with DMA");

  /* Short transfers do not use the DMA */
  ST7789HAL_DrawPixel(&H, 3, 3, 0xAAAA);
  ref_pixel(3, 3, 0xAAAA);
  CHECK(fake.n_dma == 0u && fake.n_tx > 0u, "DrawPixel used the DMA");
  check_draw("DrawPixel with DMA");
  ST7789HAL_FillRect(&H, 0, 10, (int16_t)(min_px - 1u), 1, 0xBBBB);
  ref_fill_rect(0, 10, (int32_t)min_px - 1, 1, 0xBBBB);
  CHECK(fake.n_dma == 0u, "a fill shorter than ST7789HAL_DMA_MIN_BYTES used the DMA");
  check_draw("FillRect, 1 pixel less than the DMA limit");
  ST7789HAL_FillRect(&H, 0, 11, (int16_t)min_px, 1, 0xCCCC);
  ref_fill_rect(0, 11, (int32_t)min_px, 1, 0xCCCC);
  CHECK(fake.n_dma == (dma_possible ? 1u : 0u), "a fill of ST7789HAL_DMA_MIN_BYTES bytes: "
        "%u DMA transfers", fake.n_dma);
  check_draw("FillRect at the DMA limit");

  /* Text and images with the DMA */
  static uint16_t img[100 * 60];
  fill_pattern(img, 100 * 60, 3);
  ST7789HAL_DrawImage(&H, -5, 7, 100, 60, img);
  ref_image(-5, 7, 100, 60, img);
  check_draw("DrawImage with DMA");
  ST7789HAL_DrawString(&H, 0, 100, "DMA text 123", 0xFFFF, 0x001F, 4);
  ref_string(0, 100, "DMA text 123", 0xFFFF, 0x001F, 4, 1, 0);
  check_draw("DrawString with DMA");

  /* The DMA transfer completes after 7 calls of HAL_SPI_GetState() */
  fake.dma_polls = 7;
  ST7789HAL_FillRect(&H, 1, 1, 100, 100, 0x0F0F);
  ref_fill_rect(1, 1, 100, 100, 0x0F0F);
  check_draw("DMA after 7 polls");
  fake.dma_polls = 1000;              /* 1000 ms: the limit, not a timeout */
  ST7789HAL_FillRect(&H, 0, 0, 40, 40, 0x0FF0);
  ref_fill_rect(0, 0, 40, 40, 0x0FF0);
  check_draw("DMA after 1000 polls of 1 ms");

  if (!dma_possible)
  {
    note("ST7789HAL_BUFFER_PIXELS = %d: all transfers are shorter than "
         "ST7789HAL_DMA_MIN_BYTES, no DMA tests", ST7789HAL_BUFFER_PIXELS);
    return;
  }

  /* Timeout: the DMA transfer does not complete */
  static const uint32_t start_ticks[] = { 1000u, 0xFFFFFF00u, 0xFFFFFFFFu };
  for (unsigned t = 0; t < 3u; t++)
  {
    if (!setup(&panels[2], OPT_DMA))
    {
      return;
    }
    fake.tick = start_ticks[t];
    fake.dma_polls = FAKE_DMA_NEVER;
    uint32_t t0 = fake.tick;
    ST7789HAL_FillRect(&H, 0, 0, 100, 100, 0xF0F0);
    uint32_t dt = fake.tick - t0;
    CHECK(ST7789HAL_GetErrors(&H) == ST7789HAL_ERROR_TIMEOUT, "start tick 0x%08X: error bits "
          "0x%02X", start_ticks[t], ST7789HAL_GetErrors(&H));
    CHECK(fake.n_abort == 1u, "HAL_SPI_Abort calls: %u", fake.n_abort);
    CHECK(fake.n_dma == 1u, "DMA transfers after the timeout: %u", fake.n_dma);
    CHECK(dt >= 1000u && dt <= 1010u, "timeout after %u ms", dt);
    CHECK(cs_is_high(), "CS is low after the timeout");
    CHECK(!fake_hal_dma_busy(), "DMA is busy after the timeout");
    /* The library operates again */
    ST7789HAL_ClearErrors(&H);
    CHECK(ST7789HAL_GetErrors(&H) == 0u, "ClearErrors");
    clear_counts();
    model_drop_window();
    ST7789HAL_FillScreen(&H, 0x3C3C);
    ref_fill_rect(0, 0, ref.w, ref.h, 0x3C3C);
    check_draw("FillScreen after a DMA timeout");
  }

  /* The DMA transfer completes with an error */
  if (!setup(&panels[0], OPT_DMA))
  {
    return;
  }
  fake.dma_error_at = 1;
  ST7789HAL_FillRect(&H, 0, 0, 200, 200, 0x5555);
  CHECK(ST7789HAL_GetErrors(&H) == ST7789HAL_ERROR_SPI, "DMA error: error bits 0x%02X",
        ST7789HAL_GetErrors(&H));
  CHECK(fake.n_dma == 1u, "DMA transfers after a DMA error: %u", fake.n_dma);
  CHECK(cs_is_high(), "CS is low after a DMA error");
  ST7789HAL_ClearErrors(&H);
  clear_counts();
  model_drop_window();
  ST7789HAL_FillScreen(&H, 0x1111);
  ref_fill_rect(0, 0, 240, 320, 0x1111);
  check_draw("FillScreen after a DMA error");

  /* HAL_SPI_Transmit_DMA returns an error */
  static const HAL_StatusTypeDef sts[] = { HAL_ERROR, HAL_BUSY, HAL_TIMEOUT };
  for (unsigned i = 0; i < 3u; i++)
  {
    uint32_t calls_before_dma = 6u;    /* CASET, 4 bytes, RASET, 4 bytes, RAMWR, pixels */
    fake.fail_at = calls_before_dma;
    fake.fail_status = sts[i];
    ST7789HAL_FillRect(&H, 0, 0, 200, 200, 0x5555);
    CHECK(ST7789HAL_GetErrors(&H) == ST7789HAL_ERROR_SPI, "HAL_SPI_Transmit_DMA status %d: "
          "error bits 0x%02X", (int)sts[i], ST7789HAL_GetErrors(&H));
    CHECK(fake.n_spi == calls_before_dma && fake.log[5].kind == FAKE_CALL_DMA,
          "SPI calls after the DMA error: %u", fake.n_spi);
    CHECK(fake.n_getstate == 0u, "HAL_SPI_GetState after HAL_SPI_Transmit_DMA error");
    CHECK(cs_is_high(), "CS is low after a DMA start error");
    ST7789HAL_ClearErrors(&H);
    clear_counts();
    model_drop_window();
    ST7789HAL_FillScreen(&H, (uint16_t)(0x2222u * (i + 1u)));
    ref_fill_rect(0, 0, 240, 320, (uint16_t)(0x2222u * (i + 1u)));
    check_draw("FillScreen after a DMA start error");
  }
}

/* An SPI error at each SPI call of one drawing operation */
static void test_spi_errors(void)
{
  if (!setup(&panels[3], 0))
  {
    return;
  }
  static uint16_t img[50 * 40];
  fill_pattern(img, 50 * 40, 9);
  for (unsigned op = 0; op < 4u; op++)
  {
    /* The number of SPI calls without an error */
    clear_counts();
    switch (op)
    {
      case 0: ST7789HAL_FillRect(&H, 3, 4, 100, 50, 0x1234); break;
      case 1: ST7789HAL_DrawImage(&H, -3, 4, 50, 40, img); break;
      case 2: ST7789HAL_DrawChar(&H, 10, 10, 'Q', 0xFFFF, 0x0000, 8); break;
      default: ST7789HAL_DrawString(&H, 10, 10, "ABC", 0xFFFF, 0x0000, 2); break;
    }
    uint32_t n = fake.n_spi;
    uint32_t step = (n > 60u) ? n / 30u : 1u;
    for (uint32_t k = 1; k <= n; k += step)
    {
      clear_counts();
      fake.fail_at = k;
      fake.fail_status = (k & 1u) ? HAL_ERROR : HAL_TIMEOUT;
      switch (op)
      {
        case 0: ST7789HAL_FillRect(&H, 3, 4, 100, 50, 0x1234); break;
        case 1: ST7789HAL_DrawImage(&H, -3, 4, 50, 40, img); break;
        case 2: ST7789HAL_DrawChar(&H, 10, 10, 'Q', 0xFFFF, 0x0000, 8); break;
        default: ST7789HAL_DrawString(&H, 10, 10, "ABC", 0xFFFF, 0x0000, 2); break;
      }
      CHECK(ST7789HAL_GetErrors(&H) == ST7789HAL_ERROR_SPI, "operation %u, error at call %u: "
            "error bits 0x%02X", op, k, ST7789HAL_GetErrors(&H));
      CHECK(fake.n_spi == k, "operation %u, error at call %u: %u SPI calls (the operation must "
            "stop at the error)", op, k, fake.n_spi);
      CHECK(cs_is_high(), "operation %u, error at call %u: CS is low", op, k);
      CHECK(fake.errors == 0u, "operation %u: HAL: %s", op, fake.first_error);
      /* The error bit stays set after a correct call */
      clear_counts();
      ST7789HAL_DrawPixel(&H, 0, 0, 0x0001);
      ref_pixel(0, 0, 0x0001);
      CHECK(ST7789HAL_GetErrors(&H) == ST7789HAL_ERROR_SPI, "the error bit is not sticky");
      ST7789HAL_ClearErrors(&H);
      clear_counts();
      /* The next operations are correct */
      uint16_t c = next_color();
      ST7789HAL_FillScreen(&H, c);
      ref_fill_rect(0, 0, ref.w, ref.h, c);
      check_draw(msg("FillScreen after an error at call %u of operation %u", k, op));
    }
  }

  /* An error in the first of more operations: the other operations
     continue (DrawLine with more horizontal lines) */
  clear_counts();
  fake.fail_at = 1;
  ST7789HAL_DrawLine(&H, 0, 0, 50, 10, 0x4321);
  CHECK(ST7789HAL_GetErrors(&H) == ST7789HAL_ERROR_SPI, "DrawLine with an error");
  CHECK(cs_is_high(), "CS low after DrawLine with an error");
  CHECK(fake.n_spi > 1u, "the operations after the error did not occur");
}

static void test_control(void)
{
  for (unsigned o = 0; o < 4u; o++)
  {
    unsigned opts = (o == 1u) ? OPT_BL_LOW : (o == 2u) ? OPT_NO_BL : (o == 3u) ? OPT_INVERT : 0u;
    if (!setup(&panels[2], opts))
    {
      return;
    }
    /* Backlight */
    for (unsigned on = 0; on < 2u; on++)
    {
      ST7789HAL_Backlight(&H, (uint8_t)(on ? 5u : 0u));
      if (opts & OPT_NO_BL)
      {
        CHECK(fake.n_gpio == 0u, "Backlight without a pin used a pin");
      }
      else
      {
        int level = model_bl_level();
        CHECK(level == (int)(on ^ ((opts & OPT_BL_LOW) ? 1u : 0u)), "Backlight(%u): pin level %d",
              on, level);
      }
      CHECK(fake.n_spi == 0u, "Backlight used the SPI");
      after_call("Backlight");
    }
    /* DisplayOn */
    ST7789HAL_DisplayOn(&H, 0);
    CHECK(model.log_len == 1u && model.log[0].cmd == MODEL_DISPOFF && !model.display_on,
          "DisplayOn(0)");
    after_call("DisplayOn(0)");
    ST7789HAL_DisplayOn(&H, 3);
    CHECK(model.log_len == 1u && model.log[0].cmd == MODEL_DISPON && model.display_on,
          "DisplayOn(3)");
    after_call("DisplayOn(3)");
    /* InvertColors */
    for (unsigned inv = 0; inv < 2u; inv++)
    {
      ST7789HAL_InvertColors(&H, (uint8_t)(inv ? 2u : 0u));
      unsigned expect = inv ^ ((opts & OPT_INVERT) ? 1u : 0u);
      CHECK(model.log_len == 1u && model.inverted == expect, "InvertColors(%u): controller "
            "inversion %u, not %u", inv, model.inverted, expect);
      after_call("InvertColors");
    }
    /* Sleep: the simulated controller examines the times */
    uint32_t t0 = fake.tick;
    ST7789HAL_Sleep(&H, 1);
    CHECK(model.sleeping && model.log_len == 1u && model.log[0].cmd == MODEL_SLPIN, "Sleep(1)");
    CHECK(fake.tick - t0 >= 120u, "Sleep(1) waits only %u ms", fake.tick - t0);
    after_call("Sleep(1)");
    ST7789HAL_Sleep(&H, 0);
    CHECK(!model.sleeping && model.log_len == 1u && model.log[0].cmd == MODEL_SLPOUT,
          "Sleep(0)");
    after_call("Sleep(0)");
    ST7789HAL_FillRect(&H, 0, 0, 10, 10, 0xFFFF);  /* a command immediately after SLPOUT */
    ref_fill_rect(0, 0, 10, 10, 0xFFFF);
    check_draw("FillRect after Sleep(0)");
    ST7789HAL_Sleep(&H, 1);
    ST7789HAL_Sleep(&H, 1);
    ST7789HAL_Sleep(&H, 0);
    ST7789HAL_Sleep(&H, 0);
    after_call("Sleep sequence");
    /* The text configuration */
    ST7789HAL_SetTextWrap(&H, 0);
    ST7789HAL_SetTextTransparent(&H, 200);
    CHECK(H.text_wrap == 0u && H.text_transparent == 1u, "text settings");
    CHECK(fake.n_spi == 0u && fake.n_gpio == 0u, "text settings used the HAL");
    after_call("text settings");
    /* GetErrors and ClearErrors */
    H.errors = 3u;
    CHECK(ST7789HAL_GetErrors(&H) == 3u, "GetErrors");
    ST7789HAL_ClearErrors(&H);
    CHECK(ST7789HAL_GetErrors(&H) == 0u, "ClearErrors");
    after_call("errors");
  }
}

/* Random calls of all the drawing functions, with checks after each call */
static void random_op(unsigned *counts)
{
  int32_t W = ref.w, Hh = ref.h;
  int32_t v[6];
  for (unsigned k = 0; k < 6u; k++)
  {
    int32_t n = (k & 1u) ? Hh : W;
    uint32_t sel = rnd() % 20u;
    v[k] = (sel < 15u) ? rnd_range(-20, n + 20) : (sel < 18u) ? rnd_range(-700, 700)
           : special16[rnd() % N_SPECIAL16];
  }
  int32_t sw = (rnd() % 10u < 8u) ? rnd_range(-3, W + 10) : special16[rnd() % N_SPECIAL16];
  int32_t sh = (rnd() % 10u < 8u) ? rnd_range(-3, Hh + 10) : special16[rnd() % N_SPECIAL16];
  uint16_t c = next_color(), c2 = next_color();
  unsigned op = rnd() % 19u;
  counts[op]++;
  switch (op)
  {
    case 0:
      ST7789HAL_FillScreen(&H, c);
      ref_fill_rect(0, 0, W, Hh, c);
      check_draw("random FillScreen");
      break;
    case 1:
      ST7789HAL_FillRect(&H, (int16_t)v[0], (int16_t)v[1], (int16_t)sw, (int16_t)sh, c);
      ref_fill_rect(v[0], v[1], sw, sh, c);
      check_draw(msg("random FillRect(%d, %d, %d, %d)", (int)v[0], (int)v[1], (int)sw, (int)sh));
      break;
    case 2:
      ST7789HAL_DrawPixel(&H, (int16_t)v[0], (int16_t)v[1], c);
      ref_pixel(v[0], v[1], c);
      check_draw("random DrawPixel");
      break;
    case 3:
      ST7789HAL_DrawHLine(&H, (int16_t)v[0], (int16_t)v[1], (int16_t)sw, c);
      ref_fill_rect(v[0], v[1], sw, 1, c);
      check_draw("random DrawHLine");
      break;
    case 4:
      ST7789HAL_DrawVLine(&H, (int16_t)v[0], (int16_t)v[1], (int16_t)sh, c);
      ref_fill_rect(v[0], v[1], 1, sh, c);
      check_draw("random DrawVLine");
      break;
    case 5:
      ST7789HAL_DrawLine(&H, (int16_t)v[0], (int16_t)v[1], (int16_t)v[2], (int16_t)v[3], c);
      ref_line(v[0], v[1], v[2], v[3], c);
      check_draw(msg("random DrawLine(%d, %d, %d, %d)", (int)v[0], (int)v[1], (int)v[2],
                     (int)v[3]));
      break;
    case 6:
      ST7789HAL_DrawRect(&H, (int16_t)v[0], (int16_t)v[1], (int16_t)sw, (int16_t)sh, c);
      ref_rect(v[0], v[1], sw, sh, c);
      check_draw(msg("random DrawRect(%d, %d, %d, %d)", (int)v[0], (int)v[1], (int)sw, (int)sh));
      break;
    case 7:
    case 8:
    {
      int32_t rad = (rnd() % 10u < 8u) ? rnd_range(-2, 80) : special16[rnd() % N_SPECIAL16];
      if (op == 7u)
      {
        ST7789HAL_DrawCircle(&H, (int16_t)v[0], (int16_t)v[1], (int16_t)rad, c);
        ref_circle(v[0], v[1], rad, c);
      }
      else
      {
        ST7789HAL_FillCircle(&H, (int16_t)v[0], (int16_t)v[1], (int16_t)rad, c);
        ref_fill_circle(v[0], v[1], rad, c);
      }
      check_draw(msg("random %sCircle(%d, %d, %d)", op == 7u ? "Draw" : "Fill", (int)v[0],
                     (int)v[1], (int)rad));
      break;
    }
    case 9:
      ST7789HAL_DrawTriangle(&H, (int16_t)v[0], (int16_t)v[1], (int16_t)v[2], (int16_t)v[3],
                             (int16_t)v[4], (int16_t)v[5], c);
      ref_triangle(v[0], v[1], v[2], v[3], v[4], v[5], c);
      check_draw("random DrawTriangle");
      break;
    case 10:
      ST7789HAL_FillTriangle(&H, (int16_t)v[0], (int16_t)v[1], (int16_t)v[2], (int16_t)v[3],
                             (int16_t)v[4], (int16_t)v[5], c);
      ref_fill_triangle(v[0], v[1], v[2], v[3], v[4], v[5], c);
      check_draw(msg("random FillTriangle(%d,%d, %d,%d, %d,%d)", (int)v[0], (int)v[1],
                     (int)v[2], (int)v[3], (int)v[4], (int)v[5]));
      break;
    case 11:
    {
      int32_t w = rnd_range(-2, W + 30), h = rnd_range(-2, Hh + 30);
      draw_image_guarded(v[0], v[1], w, h, (int)(rnd() & 1u), msg("random DrawImage(%d, %d, %d, "
                         "%d)", (int)v[0], (int)v[1], (int)w, (int)h));
      break;
    }
    case 12:
    {
      int32_t size = rnd_range(0, 10);
      char ch = (char)(rnd() & 0xFFu);
      ST7789HAL_DrawChar(&H, (int16_t)v[0], (int16_t)v[1], ch, c, c2, (uint8_t)size);
      ref_char(v[0], v[1], ch, c, c2, size, H.text_transparent);
      check_draw(msg("random DrawChar(%d, %d, 0x%02X) size %d", (int)v[0], (int)v[1],
                     (unsigned)(unsigned char)ch, (int)size));
      break;
    }
    case 13:
    case 14:
    {
      char s[64];
      unsigned len = rnd() % 60u;
      for (unsigned i = 0; i < len; i++)
      {
        uint32_t k = rnd() % 20u;
        s[i] = (k == 0u) ? '\n' : (k == 1u) ? '\r' : (k == 2u) ? (char)(0x80u + rnd() % 128u)
               : (char)(' ' + rnd() % 95u);
      }
      s[len] = '\0';
      int32_t size = (rnd() % 4u == 0u) ? rnd_range(0, 255) : rnd_range(1, 4);
      draw_string(v[0], v[1], s, c, c2, size, msg("random DrawString at (%d, %d) size %d "
                  "length %u wrap %u transparent %u", (int)v[0], (int)v[1], (int)size, len,
                  H.text_wrap, H.text_transparent));
      break;
    }
    case 15:
      ST7789HAL_SetTextWrap(&H, (uint8_t)(rnd() & 1u));
      ST7789HAL_SetTextTransparent(&H, (uint8_t)(rnd() % 3u == 0u));
      after_call("random text settings");
      break;
    case 16:
      set_rotation((int)(rnd() & 3u));
      break;
    case 17:
    {
      int32_t x = rnd_range(0, W - 1), y = rnd_range(0, Hh - 1);
      int32_t w = rnd_range(1, W - x), h = rnd_range(1, Hh - y);
      static const uint32_t chunks[] = { 1, 5, 64, 300, 7, 2000 };
      write_window(x, y, w, h, chunks, 6, msg("random window (%d, %d, %d, %d)", (int)x, (int)y,
                   (int)w, (int)h));
      break;
    }
    default:
      ST7789HAL_DisplayOn(&H, 1);
      ST7789HAL_InvertColors(&H, (uint8_t)(rnd() & 1u));
      after_call("random control");
      break;
  }
}

static void test_random(void)
{
  static const unsigned opts_list[] = { 0, OPT_DMA, OPT_NO_CS | OPT_BGR, OPT_NO_RST | OPT_DMA };
  unsigned counts[19];
  memset(counts, 0, sizeof counts);
  for (unsigned pi = 0; pi < N_PANELS; pi++)
  {
    for (unsigned oi = 0; oi < sizeof opts_list / sizeof opts_list[0]; oi++)
    {
      if (!setup(&panels[pi], opts_list[oi]))
      {
        return;
      }
      for (unsigned i = 0; i < 300u && g_test_fails < 20u; i++)
      {
        random_op(counts);
      }
    }
  }
  printf("    random calls of each function:");
  for (unsigned i = 0; i < 19u; i++)
  {
    printf(" %u", counts[i]);
  }
  printf("\n");
}

int main(void)
{
  setvbuf(stdout, NULL, _IOLBF, 0);
  printf("ST7789 HAL library host tests, ST7789HAL_BUFFER_PIXELS = %d\n", ST7789HAL_BUFFER_PIXELS);
  run_test("font", test_font);
  run_test("null_and_uninitialized", test_null_and_uninitialized);
  run_test("init_parameters", test_init_parameters);
  run_test("init_sequence", test_init_sequence);
  run_test("init_errors", test_init_errors);
  run_test("rotation", test_rotation);
  run_test("fill_rect", test_fill_rect);
  run_test("pixels_lines_rects", test_pixels_lines_rects);
  run_test("circles", test_circles);
  run_test("triangles", test_triangles);
  run_test("text", test_text);
  run_test("image", test_image);
  run_test("window", test_window);
  run_test("dma", test_dma);
  run_test("spi_errors", test_spi_errors);
  run_test("control", test_control);
  run_test("random", test_random);
  printf("\n%u tests, %u checks, %u failed checks, %u failed tests, %u notes "
         "(ST7789HAL_BUFFER_PIXELS = %d)\n", g_tests, g_checks, g_fails, g_failed_tests, g_notes,
         ST7789HAL_BUFFER_PIXELS);
  printf("%s\n", g_fails == 0u ? "RESULT: PASS" : "RESULT: FAIL");
  return g_fails == 0u ? 0 : 1;
}

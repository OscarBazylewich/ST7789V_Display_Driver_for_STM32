/**
 * Benchmark for the two ST7789 libraries.
 *
 * One source file is for the two libraries:
 * - Without USE_ST7789_HAL, the benchmark uses `Lib/st7789` in direct
 *   mode (no framebuffer).
 * - With USE_ST7789_HAL, the benchmark uses `Lib/st7789_hal`. If
 *   HAL_DEMO_USE_DMA is 1, the benchmark prepares DMA2 Stream 0 for SPI1,
 *   as `hal_demo.c` does.
 *
 * The functions at the top of this file (the lcd_* functions) call the
 * functions of the selected library. All the scenes below them are the same
 * for the two libraries. The scenes do the same operations as the scenes in
 * `hal_demo.c`, with the same random numbers. The benchmark has no test
 * pattern and no ROTATION scene. The rotation is always portrait.
 *
 * Scenes, each for SCENE_MS:
 *  - FILL: one full screen fill for each operation.
 *  - RECTS: random filled rectangles.
 *  - LINES: random lines.
 *  - CIRCLES: random circles, contours and filled circles.
 *  - TRIANGLES: random triangles, contours and filled triangles.
 *  - TEXT: text with a background color, `size` 1 to 3.
 *  - TRANSPARENT: transparent text at random positions.
 *  - IMAGE: a full screen image. The benchmark sends it with DrawImage in
 *    rectangles of 240 x 16 pixels. One frame is one operation.
 *
 * Before each scene, the benchmark fills the screen with black and draws
 * the scene name at the top. This occurs before the start time of the
 * scene. Thus, the measured time does not include it.
 *
 * `Lib/st7789` sends large areas in background transfers. Thus, at the end
 * of each scene, the benchmark calls ST7789_WaitIdle before it reads the end
 * time. Then the measured time includes all the pixels of the scene.
 *
 * The benchmark sends this text to the ST-LINK virtual COM port:
 *  - At the start: `BENCH START lib=<st7789|st7789_hal_dma|st7789_hal_poll>`
 *  - After each scene:
 *    `BENCH <scene>: <ops> ops, <time> ms, <operations in each second> ops/s`
 *  - At the end of each cycle: `BENCH errors=0x..` and `BENCH CYCLE DONE`.
 *    Then the next cycle starts.
 */
#include "lcd_bench.h"
#include "main.h"
#include <stdio.h>

#define SCENE_MS   4000u     /* the time of each scene */
#define LABEL_H    20        /* the height of the scene name at the top */
#define IMG_ROWS   16u       /* the number of rows in each DrawImage call */
#define IMG_WIDTH  240u      /* the width of the screen in portrait */

/* ---- calls to the selected library ----------------------------------------- */

#ifdef USE_ST7789_HAL

#include "st7789_hal.h"

/* 1: the benchmark prepares DMA2 Stream 0 for SPI1, and the library uses
 * HAL_SPI_Transmit_DMA for long transfers. 0: the library uses only
 * HAL_SPI_Transmit. */
#ifndef HAL_DEMO_USE_DMA
#define HAL_DEMO_USE_DMA  1
#endif

/* The SPI handle from `main.c` (STM32CubeMX) */
extern SPI_HandleTypeDef hspi1;

static ST7789HAL_Handle lcd;

#define LCD_BLACK    ST7789HAL_BLACK
#define LCD_WHITE    ST7789HAL_WHITE
#define LCD_RED      ST7789HAL_RED
#define LCD_GREEN    ST7789HAL_GREEN
#define LCD_BLUE     ST7789HAL_BLUE
#define LCD_YELLOW   ST7789HAL_YELLOW
#define LCD_CYAN     ST7789HAL_CYAN
#define LCD_MAGENTA  ST7789HAL_MAGENTA
#define LCD_RGB(r, g, b)  ST7789HAL_RGB565(r, g, b)

/* The library copies each image into its transmit buffer. The CPU reads the
 * image. Thus, the image buffers can be in all RAM areas. The benchmark puts
 * them in the same section as for `Lib/st7789` (AXI SRAM). Thus, the CPU
 * writes the pixels to the same memory with the two libraries. */
#define LCD_BUFFER_ATTR  ST7789HAL_BUFFER_ATTR

static inline int16_t lcd_width(void)  { return (int16_t)ST7789HAL_Width(&lcd); }
static inline int16_t lcd_height(void) { return (int16_t)ST7789HAL_Height(&lcd); }
static inline void lcd_fill_screen(uint16_t c) { ST7789HAL_FillScreen(&lcd, c); }
static inline void lcd_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c)
{
  ST7789HAL_FillRect(&lcd, x, y, w, h, c);
}
static inline void lcd_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t c)
{
  ST7789HAL_DrawLine(&lcd, x0, y0, x1, y1, c);
}
static inline void lcd_draw_circle(int16_t cx, int16_t cy, int16_t r, uint16_t c)
{
  ST7789HAL_DrawCircle(&lcd, cx, cy, r, c);
}
static inline void lcd_fill_circle(int16_t cx, int16_t cy, int16_t r, uint16_t c)
{
  ST7789HAL_FillCircle(&lcd, cx, cy, r, c);
}
static inline void lcd_draw_triangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                                     int16_t x2, int16_t y2, uint16_t c)
{
  ST7789HAL_DrawTriangle(&lcd, x0, y0, x1, y1, x2, y2, c);
}
static inline void lcd_fill_triangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                                     int16_t x2, int16_t y2, uint16_t c)
{
  ST7789HAL_FillTriangle(&lcd, x0, y0, x1, y1, x2, y2, c);
}
static inline void lcd_draw_image(int16_t x, int16_t y, int16_t w, int16_t h,
                                  const uint16_t *pixels)
{
  ST7789HAL_DrawImage(&lcd, x, y, w, h, pixels);
}
static inline void lcd_draw_string(int16_t x, int16_t y, const char *s, uint16_t fg,
                                   uint16_t bg, uint8_t size)
{
  ST7789HAL_DrawString(&lcd, x, y, s, fg, bg, size);
}
static inline void lcd_text_wrap(uint8_t wrap) { ST7789HAL_SetTextWrap(&lcd, wrap); }
static inline void lcd_text_transparent(uint8_t t) { ST7789HAL_SetTextTransparent(&lcd, t); }
/* Each function of this library returns after the SPI sent all the data.
 * Thus, it is not necessary to wait. */
static inline void lcd_wait_idle(void) {}
static inline uint8_t lcd_errors(void) { return ST7789HAL_GetErrors(&lcd); }

#if HAL_DEMO_USE_DMA
static DMA_HandleTypeDef hdma_spi1_tx;

/* This function does the same work as `dma_prepare()` in `hal_demo.c`:
 * the configuration of DMA2 Stream 0 for "SPI1_TX", the link to hspi1 and
 * the NVIC configuration. The HAL of the STM32H7 completes a DMA transfer in
 * the SPI1 interrupt (EOT flag). Thus, the SPI1 interrupt is also
 * necessary. The result is 1 if the configuration is correct. */
static int dma_prepare(void)
{
  __HAL_RCC_DMA2_CLK_ENABLE();

  hdma_spi1_tx.Instance                 = DMA2_Stream0;
  hdma_spi1_tx.Init.Request             = DMA_REQUEST_SPI1_TX;
  hdma_spi1_tx.Init.Direction           = DMA_MEMORY_TO_PERIPH;
  hdma_spi1_tx.Init.PeriphInc           = DMA_PINC_DISABLE;
  hdma_spi1_tx.Init.MemInc              = DMA_MINC_ENABLE;
  hdma_spi1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
  hdma_spi1_tx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
  hdma_spi1_tx.Init.Mode                = DMA_NORMAL;
  hdma_spi1_tx.Init.Priority            = DMA_PRIORITY_LOW;
  hdma_spi1_tx.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
  if (HAL_DMA_Init(&hdma_spi1_tx) != HAL_OK)
  {
    return 0;
  }
  __HAL_LINKDMA(&hspi1, hdmatx, hdma_spi1_tx);

  /* SysTick has the priority 0 (TICK_INT_PRIORITY). Thus, HAL_GetTick
     continues to increase in these interrupt handlers. */
  HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);
  HAL_NVIC_SetPriority(SPI1_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(SPI1_IRQn);
  return 1;
}

/* The interrupt handlers replace the weak symbols in the startup file.
 * When LCD_BENCH is ON, CMake does not compile `hal_demo.c`. Thus, these
 * interrupt handlers are the only ones with these names. */
void DMA2_Stream0_IRQHandler(void)
{
  HAL_DMA_IRQHandler(&hdma_spi1_tx);
}

void SPI1_IRQHandler(void)
{
  HAL_SPI_IRQHandler(&hspi1);
}
#endif /* HAL_DEMO_USE_DMA */

/* The result is 1 if ST7789HAL_Init returned ST7789HAL_OK. `name` receives
 * the name of the library for the START text. */
static int lcd_init(const char **name)
{
  ST7789HAL_Config cfg = {
    .hspi          = &hspi1,
    .dc_port       = LCD_DC_GPIO_Port,  .dc_pin  = LCD_DC_Pin,
    .cs_port       = LCD_CS_GPIO_Port,  .cs_pin  = LCD_CS_Pin,
    .rst_port      = LCD_RST_GPIO_Port, .rst_pin = LCD_RST_Pin,
    .bl_port       = LCD_BL_GPIO_Port,  .bl_pin  = LCD_BL_Pin,
    .bl_active_low = 0,
    .width         = 240,
    .height        = 320,
    .x_offset      = 0,
    .y_offset      = 0,
    .invert_colors = 1,
    .bgr_order     = 0,
    .use_dma       = 0,
  };

#if HAL_DEMO_USE_DMA
  if (dma_prepare())
  {
    cfg.use_dma = 1;
  }
  else
  {
    printf("BENCH HAL_DMA_Init failed, the library uses HAL_SPI_Transmit only\r\n");
  }
#endif
  *name = cfg.use_dma ? "st7789_hal_dma" : "st7789_hal_poll";

  ST7789HAL_Status st = ST7789HAL_Init(&lcd, &cfg);
  if (st != ST7789HAL_OK)
  {
    printf("BENCH ST7789HAL_Init: status %d, errors 0x%02X\r\n", (int)st,
           (unsigned)ST7789HAL_GetErrors(&lcd));
    return 0;
  }
  ST7789HAL_SetRotation(&lcd, ST7789HAL_PORTRAIT);
  return 1;
}

#else /* `Lib/st7789` */

#include "st7789.h"

#define LCD_BLACK    ST7789_BLACK
#define LCD_WHITE    ST7789_WHITE
#define LCD_RED      ST7789_RED
#define LCD_GREEN    ST7789_GREEN
#define LCD_BLUE     ST7789_BLUE
#define LCD_YELLOW   ST7789_YELLOW
#define LCD_CYAN     ST7789_CYAN
#define LCD_MAGENTA  ST7789_MAGENTA
#define LCD_RGB(r, g, b)  ST7789_RGB(r, g, b)

/* The DMA must read the image buffers. Thus, they must be DMA buffers. */
#define LCD_BUFFER_ATTR  ST7789_DMA_BUFFER

static inline int16_t lcd_width(void)  { return (int16_t)ST7789_Width(); }
static inline int16_t lcd_height(void) { return (int16_t)ST7789_Height(); }
static inline void lcd_fill_screen(uint16_t c) { ST7789_FillScreen(c); }
static inline void lcd_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c)
{
  ST7789_FillRect(x, y, w, h, c);
}
static inline void lcd_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t c)
{
  ST7789_DrawLine(x0, y0, x1, y1, c);
}
static inline void lcd_draw_circle(int16_t cx, int16_t cy, int16_t r, uint16_t c)
{
  ST7789_DrawCircle(cx, cy, r, c);
}
static inline void lcd_fill_circle(int16_t cx, int16_t cy, int16_t r, uint16_t c)
{
  ST7789_FillCircle(cx, cy, r, c);
}
static inline void lcd_draw_triangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                                     int16_t x2, int16_t y2, uint16_t c)
{
  ST7789_DrawTriangle(x0, y0, x1, y1, x2, y2, c);
}
static inline void lcd_fill_triangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                                     int16_t x2, int16_t y2, uint16_t c)
{
  ST7789_FillTriangle(x0, y0, x1, y1, x2, y2, c);
}
static inline void lcd_draw_image(int16_t x, int16_t y, int16_t w, int16_t h,
                                  const uint16_t *pixels)
{
  ST7789_DrawImage(x, y, w, h, pixels);
}
static inline void lcd_draw_string(int16_t x, int16_t y, const char *s, uint16_t fg,
                                   uint16_t bg, uint8_t size)
{
  ST7789_DrawString(x, y, s, fg, bg, size);
}
static inline void lcd_text_wrap(uint8_t wrap) { ST7789_SetTextWrap(wrap); }
static inline void lcd_text_transparent(uint8_t t) { ST7789_SetTextTransparent(t); }
/* This function waits for the end of all the background transfers */
static inline void lcd_wait_idle(void) { ST7789_WaitIdle(); }
static inline uint8_t lcd_errors(void) { return ST7789_GetErrors(); }

/* The result is always 1, because ST7789_Init returns no status. `name`
 * receives the name of the library for the START text. */
static int lcd_init(const char **name)
{
  *name = "st7789";
  ST7789_Init();
  ST7789_SetRotation(ST7789_PORTRAIT);
  return 1;
}

#endif /* USE_ST7789_HAL */

/* ---- data of the benchmark ------------------------------------------------- */

static uint8_t  failed;              /* 1 if lcd_init returned 0 */
static uint32_t scene;               /* the number of the scene in scenes */
static uint32_t scene_start;         /* HAL_GetTick() when the scene started */
static uint32_t steps;               /* the number of steps in this scene */
static uint32_t ops;                 /* the number of operations in this scene */
static int16_t  text_y;              /* the next row of the TEXT scene */

/* Two buffers, each for IMG_ROWS rows of the IMAGE scene. The CPU writes the
 * pixels into one buffer while `Lib/st7789` sends the other buffer. */
static uint16_t img_rows[2][IMG_WIDTH * IMG_ROWS] LCD_BUFFER_ATTR;

static const uint16_t palette[8] = {
  LCD_RED, LCD_GREEN, LCD_BLUE, LCD_WHITE,
  LCD_YELLOW, LCD_CYAN, LCD_MAGENTA, LCD_BLACK,
};

/* ---- internal functions ------------------------------------------------- */

/* rng() makes random numbers (xorshift32). At the start of each scene, the
 * benchmark sets rng_state to RNG_SEED again. RNG_SEED is the same as in
 * `hal_demo.c`. Thus, each cycle and each library draws the same shapes. */
#define RNG_SEED  0x12345678u
static uint32_t rng_state = RNG_SEED;
static uint32_t rng(void)
{
  rng_state ^= rng_state << 13;
  rng_state ^= rng_state >> 17;
  rng_state ^= rng_state << 5;
  return rng_state;
}

/* The range from `lo` to `hi` includes `lo` and `hi` */
static int16_t rng_range(int32_t lo, int32_t hi)
{
  return (int16_t)(lo + (int32_t)(rng() % (uint32_t)(hi - lo + 1)));
}

/* The result is the number of operations in each second, in units of 0.1,
 * to the nearest integer. If the time is 0, the result is 0. */
static uint32_t rate_x10(uint32_t count, uint32_t ms)
{
  return ms ? (uint32_t)(((uint64_t)count * 10000u + ms / 2u) / ms) : 0u;
}

/* ---- scenes ---------------------------------------------------------------- */

/* Each step function draws a small quantity of shapes and returns the
 * number of operations that it did. `n` is the number of the step in the
 * scene, from 0. */

static uint32_t step_fill(uint32_t n)
{
  lcd_fill_screen(palette[n % 8u]);
  return 1;
}

static uint32_t step_rects(uint32_t n)
{
  (void)n;
  for (int i = 0; i < 10; i++)
  {
    int16_t rw = rng_range(10, 80), rh = rng_range(10, 80);
    int16_t x  = rng_range(0, lcd_width() - rw);
    int16_t y  = rng_range(LABEL_H, lcd_height() - rh);
    lcd_fill_rect(x, y, rw, rh, (uint16_t)rng());
  }
  return 10;
}

static uint32_t step_lines(uint32_t n)
{
  (void)n;
  for (int i = 0; i < 20; i++)
  {
    lcd_draw_line(rng_range(0, lcd_width() - 1), rng_range(LABEL_H, lcd_height() - 1),
                  rng_range(0, lcd_width() - 1), rng_range(LABEL_H, lcd_height() - 1),
                  (uint16_t)rng());
  }
  return 20;
}

static uint32_t step_circles(uint32_t n)
{
  (void)n;
  for (int i = 0; i < 6; i++)
  {
    int16_t r  = rng_range(5, 40);
    int16_t cx = rng_range(r, lcd_width() - 1 - r);
    int16_t cy = rng_range(LABEL_H + r, lcd_height() - 1 - r);
    if (i & 1)
    {
      lcd_fill_circle(cx, cy, r, (uint16_t)rng());
    }
    else
    {
      lcd_draw_circle(cx, cy, r, (uint16_t)rng());
    }
  }
  return 6;
}

static uint32_t step_triangles(uint32_t n)
{
  (void)n;
  for (int i = 0; i < 6; i++)
  {
    int16_t x0 = rng_range(0, lcd_width() - 1), y0 = rng_range(LABEL_H, lcd_height() - 1);
    int16_t x1 = rng_range(0, lcd_width() - 1), y1 = rng_range(LABEL_H, lcd_height() - 1);
    int16_t x2 = rng_range(0, lcd_width() - 1), y2 = rng_range(LABEL_H, lcd_height() - 1);
    if (i & 1)
    {
      lcd_fill_triangle(x0, y0, x1, y1, x2, y2, (uint16_t)rng());
    }
    else
    {
      lcd_draw_triangle(x0, y0, x1, y1, x2, y2, (uint16_t)rng());
    }
  }
  return 6;
}

/* One row of text with a background color for each step. The `size`
 * changes from 1 to 3. The text does not continue on the next row
 * (`wrap` = 0). */
static void begin_text(void)
{
  text_y = LABEL_H;
  lcd_text_wrap(0);
}

static uint32_t step_text(uint32_t n)
{
  static const char *const text[3] = {
    "SIZE 1 ABCDEFGHIJKLMNOPQRSTUVWXYZ 0123456789",
    "SIZE 2 abcdefghijklm",
    "SIZE 3 !?#%&*",
  };
  uint8_t size = (uint8_t)(1u + n % 3u);
  if (text_y + 8 * size > lcd_height())
  {
    text_y = LABEL_H;
  }
  lcd_draw_string(0, text_y, text[size - 1u], palette[n % 7u], LCD_BLACK, size);
  text_y = (int16_t)(text_y + 8 * size);
  return 1;
}

static void end_text(void)
{
  lcd_text_wrap(1);
}

/* Transparent text at random positions. You can see the previous text
 * between the characters. */
static void begin_transparent(void)
{
  lcd_text_transparent(1);
  lcd_text_wrap(0);
}

static uint32_t step_transparent(uint32_t n)
{
  (void)n;
  for (int i = 0; i < 5; i++)
  {
    uint8_t size = (uint8_t)rng_range(1, 3);
    int16_t x = rng_range(0, lcd_width() - 1);
    int16_t y = rng_range(LABEL_H, lcd_height() - 8 * size);
    lcd_draw_string(x, y, "ST7789 HAL", (uint16_t)rng(), LCD_BLACK, size);
  }
  return 5;
}

static void end_transparent(void)
{
  lcd_text_transparent(0);
  lcd_text_wrap(1);
}

/* One full screen image for each step. The pattern is the same as in
 * `hal_demo.c`, and it moves a small distance in each step. The benchmark
 * calculates IMG_ROWS rows at a time and sends them with one DrawImage call.
 * It uses the two buffers in sequence.
 *
 * `Lib/st7789` can send a buffer after DrawImage returns. But each drawing
 * function first waits for the end of the previous transfer. Thus, when
 * DrawImage returns, the CPU can write into the other buffer. */
static uint32_t step_image(uint32_t n)
{
  uint32_t h = (uint32_t)lcd_height();
  uint32_t b = 0;
  for (uint32_t y0 = 0; y0 < h; y0 += IMG_ROWS)
  {
    uint32_t count = (h - y0 < IMG_ROWS) ? h - y0 : IMG_ROWS;
    uint16_t *p = img_rows[b];
    for (uint32_t y = y0; y < y0 + count; y++)
    {
      for (uint32_t x = 0; x < IMG_WIDTH; x++)
      {
        *p++ = LCD_RGB(x + n * 4u, y + n * 2u, x ^ y);
      }
    }
    lcd_draw_image(0, (int16_t)y0, (int16_t)IMG_WIDTH, (int16_t)count, img_rows[b]);
    b ^= 1u;
  }
  return 1;
}

typedef struct
{
  const char *name;
  void (*begin)(void);              /* NULL: no function */
  uint32_t (*step)(uint32_t n);
  void (*end)(void);                /* NULL: no function */
} Scene;

static const Scene scenes[] = {
  { "FILL",        NULL,              step_fill,        NULL            },
  { "RECTS",       NULL,              step_rects,       NULL            },
  { "LINES",       NULL,              step_lines,       NULL            },
  { "CIRCLES",     NULL,              step_circles,     NULL            },
  { "TRIANGLES",   NULL,              step_triangles,   NULL            },
  { "TEXT",        begin_text,        step_text,        end_text        },
  { "TRANSPARENT", begin_transparent, step_transparent, end_transparent },
  { "IMAGE",       NULL,              step_image,       NULL            },
};
#define SCENE_COUNT  (sizeof(scenes) / sizeof(scenes[0]))

/* The benchmark draws the scene name before the start time of the scene.
 * It waits for the end of all the transfers before it reads the start time.
 * Thus, the measured time of the scene does not include the scene name. */
static void begin_scene(uint32_t i)
{
  const Scene *s = &scenes[i];
  scene     = i;
  rng_state = RNG_SEED;
  steps     = 0;
  ops       = 0;
  lcd_fill_screen(LCD_BLACK);
  lcd_draw_string(2, 2, s->name, LCD_WHITE, LCD_BLACK, 2);
  if (s->begin != NULL)
  {
    s->begin();
  }
  lcd_wait_idle();
  scene_start = HAL_GetTick();
}

/* ---- API --------------------------------------------------------------------- */

void LcdBench_Start(void)
{
  const char *name = "";

  if (!lcd_init(&name))
  {
    printf("BENCH START lib=%s FAILED, the benchmark stops\r\n", name);
    failed = 1;
    return;
  }
  printf("\r\nBENCH START lib=%s\r\n", name);
  begin_scene(0);
}

void LcdBench_Update(void)
{
  if (failed)
  {
    return;
  }

  const Scene *s = &scenes[scene];
  ops += s->step(steps);
  steps++;

  uint32_t now = HAL_GetTick();
  if (now - scene_start < SCENE_MS)
  {
    return;
  }

  /* The end of the scene. Wait for the end of all the background transfers
     first. Thus, the measured time includes all the pixels. */
  lcd_wait_idle();
  uint32_t elapsed = HAL_GetTick() - scene_start;
  uint32_t rate = rate_x10(ops, elapsed);
  printf("BENCH %s: %lu ops, %lu ms, %lu.%lu ops/s\r\n", s->name, (unsigned long)ops,
         (unsigned long)elapsed, (unsigned long)(rate / 10u), (unsigned long)(rate % 10u));
  if (s->end != NULL)
  {
    s->end();
  }

  if (scene + 1u < SCENE_COUNT)
  {
    begin_scene(scene + 1u);
    return;
  }

  printf("BENCH errors=0x%02X\r\n", (unsigned)lcd_errors());
  printf("BENCH CYCLE DONE\r\n");
  begin_scene(0);
}

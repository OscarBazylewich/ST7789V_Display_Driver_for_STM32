/**
 * Test for the ST7789 HAL library (`Lib/st7789_hal`).
 *
 * Sequence of one cycle:
 *  1. The test pattern for CHECK_MS. Use it to examine the rotation and the
 *     colors. The test pattern has these items:
 *     - A contour (ST7789HAL_WHITE) at the edges of the screen
 *     - The characters TL, TR, BL and BR in the corners
 *     - 5 rectangles in different colors, each with the name of its color
 *     - The text "ST7789 HAL"
 *     - A filled circle, a filled triangle and a small image with a gradient.
 *  2. The scenes, each for SCENE_MS. Each call to HalDemo_Update does one
 *     step of the scene at this time.
 *
 * After each scene, the test sends this text to the ST-LINK virtual COM
 * port: `HAL <scene>: <operations> ops, <operations in each second> ops/s`.
 * At the end of each cycle, the test sends the error bits of the library
 * and the text "HAL DEMO CYCLE DONE".
 *
 * Scenes:
 *  - FILL: one full screen fill for each operation.
 *  - RECTS: random filled rectangles.
 *  - LINES: random lines.
 *  - CIRCLES: random circles, contours and filled circles.
 *  - TRIANGLES: random triangles, contours and filled triangles.
 *  - TEXT: text with a background color, `size` 1 to 3.
 *  - TRANSPARENT: transparent text at random positions.
 *  - IMAGE: a full screen image with ST7789HAL_SetWindow and
 *    ST7789HAL_WritePixels (one frame for each operation).
 *  - ROTATION: the test pattern in the 4 rotations, 1 second for each
 *    rotation.
 */
#include "hal_demo.h"
#include "st7789_hal.h"
#include "main.h"
#include <stdio.h>

/* 1: the test prepares DMA2 Stream 0 for SPI1, and the library uses
 * HAL_SPI_Transmit_DMA for long transfers. 0: the library uses only
 * HAL_SPI_Transmit. */
#ifndef HAL_DEMO_USE_DMA
#define HAL_DEMO_USE_DMA  1
#endif

#define CHECK_MS   5000u     /* the time to show the test pattern */
#define SCENE_MS   4000u     /* the time of each scene */
#define LABEL_H    20        /* the height of the scene name at the top */
#define IMG_SIZE   48        /* the width and the height of the small image */
#define ROWS_EACH  16u       /* the number of rows in each WritePixels call */

/* The SPI handle from `main.c` (STM32CubeMX) */
extern SPI_HandleTypeDef hspi1;

#if HAL_DEMO_USE_DMA
static DMA_HandleTypeDef hdma_spi1_tx;
#endif

static ST7789HAL_Handle lcd;
static uint8_t  failed;              /* 1 if ST7789HAL_Init did not return
                                        ST7789HAL_OK */
static uint8_t  checking;            /* 1 while the screen shows the test
                                        pattern at the start of a cycle */
static uint32_t scene;               /* the number of the scene in scenes */
static uint32_t scene_start;         /* HAL_GetTick() when the scene started */
static uint32_t cycle_start;         /* HAL_GetTick() when the cycle started */
static uint32_t cycle;               /* the number of completed cycles */
static uint32_t steps;               /* the number of steps in this scene */
static uint32_t ops;                 /* the number of operations in this scene */
static int16_t  text_y;              /* the next row of the TEXT scene */
static uint8_t  shown_rotation;      /* the rotation that the ROTATION scene
                                        shows at this time */

static uint16_t small_image[IMG_SIZE * IMG_SIZE];
static uint16_t rows[240u * ROWS_EACH];   /* the rows of the IMAGE scene for
                                             one WritePixels call */

static const uint16_t palette[8] = {
  ST7789HAL_RED, ST7789HAL_GREEN, ST7789HAL_BLUE, ST7789HAL_WHITE,
  ST7789HAL_YELLOW, ST7789HAL_CYAN, ST7789HAL_MAGENTA, ST7789HAL_BLACK,
};

/* ---- internal functions ------------------------------------------------- */

/* rng() makes random numbers (xorshift32). At the start of each scene, the
 * test sets rng_state to RNG_SEED again. Thus, each cycle draws the same
 * shapes. */
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

static int16_t width(void)
{
  return (int16_t)ST7789HAL_Width(&lcd);
}

static int16_t height(void)
{
  return (int16_t)ST7789HAL_Height(&lcd);
}

#if HAL_DEMO_USE_DMA
/* This function does the work that STM32CubeMX does for an "SPI1_TX" DMA
 * request on DMA2 Stream 0: the DMA configuration, the link to hspi1 and
 * the NVIC configuration. The HAL of the STM32H7 completes a DMA transfer
 * in the SPI1 interrupt (EOT flag). Thus, the SPI1 interrupt is also
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
 * `Lib/st7789` has interrupt handlers for SPI1 and DMA1 Stream 0. Thus, the
 * project must not link `Lib/st7789` together with this file. */
void DMA2_Stream0_IRQHandler(void)
{
  HAL_DMA_IRQHandler(&hdma_spi1_tx);
}

void SPI1_IRQHandler(void)
{
  HAL_SPI_IRQHandler(&hspi1);
}
#endif /* HAL_DEMO_USE_DMA */

/* The small image: red increases from left to right, green increases from
 * top to bottom, and blue decreases from left to right. */
static void make_small_image(void)
{
  for (uint32_t y = 0; y < IMG_SIZE; y++)
  {
    for (uint32_t x = 0; x < IMG_SIZE; x++)
    {
      uint32_t r = x * 255u / (IMG_SIZE - 1u);
      uint32_t g = y * 255u / (IMG_SIZE - 1u);
      uint32_t b = 255u - r;
      small_image[y * IMG_SIZE + x] = ST7789HAL_RGB565(r, g, b);
    }
  }
}

/* The test pattern for the rotation at this time. The coordinates of each
 * item come from the width and the height of the screen. Thus, the test
 * pattern is correct in all 4 rotations. `label` shows at the top, between
 * TL and TR. */
static void draw_check(const char *label)
{
  static const struct
  {
    const char *name;
    uint16_t color, text;
  } colors[5] = {
    { "RED",   ST7789HAL_RED,   ST7789HAL_WHITE },
    { "GREEN", ST7789HAL_GREEN, ST7789HAL_BLACK },
    { "BLUE",  ST7789HAL_BLUE,  ST7789HAL_WHITE },
    { "WHITE", ST7789HAL_WHITE, ST7789HAL_BLACK },
    { "BLACK", ST7789HAL_BLACK, ST7789HAL_WHITE },
  };
  int16_t w = width(), h = height();

  ST7789HAL_FillScreen(&lcd, ST7789HAL_BLACK);
  ST7789HAL_DrawRect(&lcd, 0, 0, w, h, ST7789HAL_WHITE);

  /* The characters in the corners, `size` 2 (12 x 16 pixels for each
     character) */
  ST7789HAL_DrawString(&lcd, 3, 3, "TL", ST7789HAL_WHITE, ST7789HAL_BLACK, 2);
  ST7789HAL_DrawString(&lcd, (int16_t)(w - 27), 3, "TR", ST7789HAL_WHITE, ST7789HAL_BLACK, 2);
  ST7789HAL_DrawString(&lcd, 3, (int16_t)(h - 19), "BL", ST7789HAL_WHITE, ST7789HAL_BLACK, 2);
  ST7789HAL_DrawString(&lcd, (int16_t)(w - 27), (int16_t)(h - 19), "BR",
                       ST7789HAL_WHITE, ST7789HAL_BLACK, 2);
  ST7789HAL_DrawString(&lcd, 36, 7, label, ST7789HAL_YELLOW, ST7789HAL_BLACK, 1);

  /* The text in the middle, `size` 2: 10 characters of 12 pixels */
  ST7789HAL_DrawString(&lcd, (int16_t)((w - 120) / 2), 24, "ST7789 HAL",
                       ST7789HAL_WHITE, ST7789HAL_BLACK, 2);

  /* 5 rectangles in different colors, each with the name of its color. The
     BLACK rectangle has a contour (ST7789HAL_WHITE). */
  for (int16_t i = 0; i < 5; i++)
  {
    int16_t y = (int16_t)(48 + i * 22);
    ST7789HAL_FillRect(&lcd, 20, y, (int16_t)(w - 40), 18, colors[i].color);
    if (colors[i].color == ST7789HAL_BLACK)
    {
      ST7789HAL_DrawRect(&lcd, 20, y, (int16_t)(w - 40), 18, ST7789HAL_WHITE);
    }
    ST7789HAL_DrawString(&lcd, 26, (int16_t)(y + 5), colors[i].name,
                         colors[i].text, colors[i].color, 1);
  }

  /* A filled circle, a filled triangle and the small image, near the
     bottom of the screen */
  int16_t top = (int16_t)(h - 80);
  ST7789HAL_FillCircle(&lcd, (int16_t)(w / 4), (int16_t)(top + 25), 25, ST7789HAL_CYAN);
  ST7789HAL_FillTriangle(&lcd, (int16_t)(w / 2), top,
                         (int16_t)(w / 2 - 25), (int16_t)(top + 50),
                         (int16_t)(w / 2 + 25), (int16_t)(top + 50), ST7789HAL_MAGENTA);
  ST7789HAL_DrawImage(&lcd, (int16_t)(3 * w / 4 - IMG_SIZE / 2), (int16_t)(top + 1),
                      IMG_SIZE, IMG_SIZE, small_image);
}

/* ---- scenes ---------------------------------------------------------------- */

/* Each step function draws a small quantity of shapes and returns the
 * number of operations that it did. `n` is the number of the step in the
 * scene, from 0. */

static uint32_t step_fill(uint32_t n)
{
  ST7789HAL_FillScreen(&lcd, palette[n % 8u]);
  return 1;
}

static uint32_t step_rects(uint32_t n)
{
  (void)n;
  for (int i = 0; i < 10; i++)
  {
    int16_t rw = rng_range(10, 80), rh = rng_range(10, 80);
    int16_t x  = rng_range(0, width() - rw);
    int16_t y  = rng_range(LABEL_H, height() - rh);
    ST7789HAL_FillRect(&lcd, x, y, rw, rh, (uint16_t)rng());
  }
  return 10;
}

static uint32_t step_lines(uint32_t n)
{
  (void)n;
  for (int i = 0; i < 20; i++)
  {
    ST7789HAL_DrawLine(&lcd, rng_range(0, width() - 1), rng_range(LABEL_H, height() - 1),
                       rng_range(0, width() - 1), rng_range(LABEL_H, height() - 1),
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
    int16_t cx = rng_range(r, width() - 1 - r);
    int16_t cy = rng_range(LABEL_H + r, height() - 1 - r);
    if (i & 1)
    {
      ST7789HAL_FillCircle(&lcd, cx, cy, r, (uint16_t)rng());
    }
    else
    {
      ST7789HAL_DrawCircle(&lcd, cx, cy, r, (uint16_t)rng());
    }
  }
  return 6;
}

static uint32_t step_triangles(uint32_t n)
{
  (void)n;
  for (int i = 0; i < 6; i++)
  {
    int16_t x0 = rng_range(0, width() - 1), y0 = rng_range(LABEL_H, height() - 1);
    int16_t x1 = rng_range(0, width() - 1), y1 = rng_range(LABEL_H, height() - 1);
    int16_t x2 = rng_range(0, width() - 1), y2 = rng_range(LABEL_H, height() - 1);
    if (i & 1)
    {
      ST7789HAL_FillTriangle(&lcd, x0, y0, x1, y1, x2, y2, (uint16_t)rng());
    }
    else
    {
      ST7789HAL_DrawTriangle(&lcd, x0, y0, x1, y1, x2, y2, (uint16_t)rng());
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
  ST7789HAL_SetTextWrap(&lcd, 0);
}

static uint32_t step_text(uint32_t n)
{
  static const char *const text[3] = {
    "SIZE 1 ABCDEFGHIJKLMNOPQRSTUVWXYZ 0123456789",
    "SIZE 2 abcdefghijklm",
    "SIZE 3 !?#%&*",
  };
  uint8_t size = (uint8_t)(1u + n % 3u);
  if (text_y + 8 * size > height())
  {
    text_y = LABEL_H;
  }
  ST7789HAL_DrawString(&lcd, 0, text_y, text[size - 1u], palette[n % 7u],
                       ST7789HAL_BLACK, size);
  text_y = (int16_t)(text_y + 8 * size);
  return 1;
}

static void end_text(void)
{
  ST7789HAL_SetTextWrap(&lcd, 1);
}

/* Transparent text at random positions. You can see the previous text
 * between the characters. */
static void begin_transparent(void)
{
  ST7789HAL_SetTextTransparent(&lcd, 1);
  ST7789HAL_SetTextWrap(&lcd, 0);
}

static uint32_t step_transparent(uint32_t n)
{
  (void)n;
  for (int i = 0; i < 5; i++)
  {
    uint8_t size = (uint8_t)rng_range(1, 3);
    int16_t x = rng_range(0, width() - 1);
    int16_t y = rng_range(LABEL_H, height() - 8 * size);
    ST7789HAL_DrawString(&lcd, x, y, "ST7789 HAL", (uint16_t)rng(), ST7789HAL_BLACK, size);
  }
  return 5;
}

static void end_transparent(void)
{
  ST7789HAL_SetTextTransparent(&lcd, 0);
  ST7789HAL_SetTextWrap(&lcd, 1);
}

/* One full screen image for each step. The test calculates ROWS_EACH rows
 * at a time and sends them with ST7789HAL_WritePixels. The pattern moves
 * a small distance in each step. */
static uint32_t step_image(uint32_t n)
{
  uint32_t w = (uint32_t)width(), h = (uint32_t)height();
  uint32_t each = (sizeof rows / sizeof rows[0]) / w;   /* ROWS_EACH in portrait */
  ST7789HAL_SetWindow(&lcd, 0, 0, (uint16_t)w, (uint16_t)h);
  for (uint32_t y0 = 0; y0 < h; y0 += each)
  {
    uint32_t count = (h - y0 < each) ? h - y0 : each;
    uint16_t *p = rows;
    for (uint32_t y = y0; y < y0 + count; y++)
    {
      for (uint32_t x = 0; x < w; x++)
      {
        *p++ = ST7789HAL_RGB565(x + n * 4u, y + n * 2u, x ^ y);
      }
    }
    ST7789HAL_WritePixels(&lcd, rows, w * count);
  }
  return 1;
}

/* The test pattern in each rotation for SCENE_MS / 4. The function draws
 * only when the rotation changes. Thus, the number of operations is 4. */
static void begin_rotation(void)
{
  shown_rotation = 0xFFu;
}

static uint32_t step_rotation(uint32_t n)
{
  static const char *const names[4] = {
    "ROTATION 0", "ROTATION 1", "ROTATION 2", "ROTATION 3",
  };
  (void)n;
  uint32_t r = (HAL_GetTick() - scene_start) / (SCENE_MS / 4u);
  if (r > 3u)
  {
    r = 3u;
  }
  if (r == shown_rotation)
  {
    return 0;
  }
  shown_rotation = (uint8_t)r;
  ST7789HAL_SetRotation(&lcd, (ST7789HAL_Rotation)r);
  draw_check(names[r]);
  return 1;
}

static void end_rotation(void)
{
  ST7789HAL_SetRotation(&lcd, ST7789HAL_PORTRAIT);
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
  { "ROTATION",    begin_rotation,    step_rotation,    end_rotation    },
};
#define SCENE_COUNT  (sizeof(scenes) / sizeof(scenes[0]))

static void begin_check(void)
{
  draw_check("CHECK");
  printf("HAL CHECK screen (%lu ms)\r\n", (unsigned long)CHECK_MS);
  checking    = 1;
  cycle_start = scene_start = HAL_GetTick();
}

static void begin_scene(uint32_t i)
{
  const Scene *s = &scenes[i];
  scene     = i;
  rng_state = RNG_SEED;
  steps     = 0;
  ops       = 0;
  ST7789HAL_FillScreen(&lcd, ST7789HAL_BLACK);
  ST7789HAL_DrawString(&lcd, 2, 2, s->name, ST7789HAL_WHITE, ST7789HAL_BLACK, 2);
  if (s->begin != NULL)
  {
    s->begin();
  }
  scene_start = HAL_GetTick();
}

/* ---- API --------------------------------------------------------------------- */

void HalDemo_Start(void)
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

  printf("\r\nST7789 HAL library test (HAL_DEMO_USE_DMA=%d)\r\n", HAL_DEMO_USE_DMA);
#if HAL_DEMO_USE_DMA
  if (dma_prepare())
  {
    cfg.use_dma = 1;
  }
  else
  {
    printf("HAL DMA: HAL_DMA_Init failed, the test uses HAL_SPI_Transmit only\r\n");
  }
#endif

  make_small_image();

  uint32_t t0 = HAL_GetTick();
  ST7789HAL_Status st = ST7789HAL_Init(&lcd, &cfg);
  uint32_t t1 = HAL_GetTick();
  printf("HAL init: status %d, errors 0x%02X, %lu ms, use_dma %u\r\n", (int)st,
         (unsigned)ST7789HAL_GetErrors(&lcd), (unsigned long)(t1 - t0),
         (unsigned)cfg.use_dma);
  if (st != ST7789HAL_OK)
  {
    printf("HAL init FAILED, the test stops\r\n");
    failed = 1;
    return;
  }

  begin_check();
}

void HalDemo_Update(void)
{
  if (failed)
  {
    return;
  }

  uint32_t now = HAL_GetTick();
  if (checking)
  {
    if (now - scene_start >= CHECK_MS)
    {
      checking = 0;
      begin_scene(0);
    }
    return;
  }

  const Scene *s = &scenes[scene];
  ops += s->step(steps);
  steps++;

  now = HAL_GetTick();
  if (now - scene_start < SCENE_MS)
  {
    return;
  }

  /* The end of the scene */
  uint32_t rate = rate_x10(ops, now - scene_start);
  printf("HAL %s: %lu ops, %lu.%lu ops/s\r\n", s->name, (unsigned long)ops,
         (unsigned long)(rate / 10u), (unsigned long)(rate % 10u));
  if (s->end != NULL)
  {
    s->end();
  }

  if (scene + 1u < SCENE_COUNT)
  {
    begin_scene(scene + 1u);
    return;
  }

  cycle++;
  printf("HAL cycle %lu: %lu ms, errors 0x%02X, HAL DEMO CYCLE DONE\r\n",
         (unsigned long)cycle, (unsigned long)(HAL_GetTick() - cycle_start),
         (unsigned)ST7789HAL_GetErrors(&lcd));
  begin_check();
}

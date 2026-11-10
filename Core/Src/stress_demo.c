/**
 * Stress test for the display module.
 *
 * The stress test shows each scene two times, first in direct mode and then
 * in framebuffer mode:
 *  - In direct mode, the library sends the pixels to the controller
 *    immediately.
 *  - In framebuffer mode, the library draws into the framebuffer in RAM. At
 *    the end of each frame, the stress test calls ST7789_Flush.
 *
 * Each scene draws in the area below the status bar as quickly as possible
 * for some seconds. In each frame, FILL, TEXT and PIXELS draw the full area
 * again. CIRCLES, LINES and SCATTER draw on the previous frame.
 *
 * During each scene, the status bar shows the frame rate at that time. It
 * shows a new value at intervals of BAR_UPDATE_MS. The stress test sends the
 * average frame rate of each scene to the ST-LINK virtual COM port (USART3,
 * 115200 8N1). At the end, the stress test shows a table with the average
 * frame rates of all the scenes in the two modes.
 *
 * The coordinates are for a 240x320 panel in portrait. StressDemo_Start sets
 * the rotation.
 *
 * Scenes:
 *   FILL    one filled rectangle
 *   TEXT    1480 small characters
 *   CIRCLES 40 random filled circles
 *   LINES   300 random lines
 *   PIXELS  all the pixels, one pixel at a time, row by row
 *   SCATTER 72,000 pixels at random positions: in direct mode, a different
 *           window is necessary for each pixel (the worst condition)
 */
#include "stress_demo.h"
#include "st7789.h"
#include "main.h"
#include <stdio.h>

/* The coordinates of the scenes and of the test pattern are for this panel
 * in portrait */
_Static_assert(ST7789_PANEL_WIDTH == 240 && ST7789_PANEL_HEIGHT == 320,
               "stress_demo assumes a 240x320 panel in portrait");

#define BAR_H          20        /* the height of the status bar in pixels */
#define SCENE_MS       6000      /* the time to show each scene */
#define BAR_UPDATE_MS  500       /* the interval to show a new frame rate */
#define SUMMARY_MS     10000
#define CHECK_MS       5000      /* the time to show the test pattern */

enum { MODE_DIRECT, MODE_FRAMEBUFFER, MODE_COUNT };
static const char *const mode_names[MODE_COUNT] = { "direct", "framebuffer" };

typedef struct
{
  const char *name;
  void (*draw)(uint32_t frame);
  uint32_t fps_x10[MODE_COUNT];   /* the average frame rate of the full scene
                                     in the last cycle, in units of 0.1 fps */
} Scene;

static void draw_fill(uint32_t frame);
static void draw_text(uint32_t frame);
static void draw_lines(uint32_t frame);
static void draw_circles(uint32_t frame);
static void draw_pixels(uint32_t frame);
static void draw_scatter(uint32_t frame);

static Scene scenes[] = {
  { "FILL",    draw_fill,    { 0 } },
  { "TEXT",    draw_text,    { 0 } },
  { "CIRCLES", draw_circles, { 0 } },
  { "LINES",   draw_lines,   { 0 } },
  { "PIXELS",  draw_pixels,  { 0 } },
  { "SCATTER", draw_scatter, { 0 } },
};
#define SCENE_COUNT  (sizeof(scenes) / sizeof(scenes[0]))

static uint16_t framebuffer[ST7789_FRAMEBUFFER_PIXELS] ST7789_DMA_BUFFER;

static uint32_t mode;
static uint32_t scene;               /* the number of the scene in scenes,
                                        or SCENE_COUNT for the results */
static uint32_t scene_start;         /* HAL_GetTick() when the scene started */
static uint32_t frames;              /* the number of frames that the
                                        stress test drew in this scene */
static uint32_t bar_time, bar_frames;
static volatile uint8_t skip;        /* StressDemo_Tick1ms sets it when you
                                        push the button */
static uint8_t  checking;            /* the screen shows the test pattern */

/* ---- internal functions ------------------------------------------------- */

/* rng() is a small, fast function that makes random numbers (xorshift32).
 * At the start of each scene, the stress test sets rng_state to RNG_SEED
 * again. Thus, the shapes are the same in the two modes and in all the
 * cycles. */
#define RNG_SEED  0x12345678u
static uint32_t rng_state = RNG_SEED;
static uint32_t rng(void)
{
  rng_state ^= rng_state << 13;
  rng_state ^= rng_state >> 17;
  rng_state ^= rng_state << 5;
  return rng_state;
}

static int16_t rng_range(int16_t lo, int16_t hi)   /* the range from lo to
                                                      hi includes lo and hi */
{
  return (int16_t)(lo + (int16_t)(rng() % (uint32_t)(hi - lo + 1)));
}

/* The result is the frame rate in units of 0.1 fps, to the nearest integer.
 * If the time is 0, the result is 0. */
static uint32_t fps_tenths(uint32_t count, uint32_t ms)
{
  return ms ? (uint32_t)(((uint64_t)count * 10000u + ms / 2u) / ms) : 0u;
}

/* The result is SCK in units of 0.1 MHz:
 * SCK = the SPI123 kernel clock / 2^(MBR+1) */
static uint32_t spi_mhz_x10(void)
{
  uint32_t mbr = (SPI1->CFG1 & SPI_CFG1_MBR) >> SPI_CFG1_MBR_Pos;
  uint32_t sck = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SPI123) >> (mbr + 1u);
  return (sck + 50000u) / 100000u;
}

static void draw_bar(const char *name, uint32_t fps_x10)
{
  char text[32];
  char m = (mode == MODE_DIRECT) ? 'D' : 'F';
  if (fps_x10 == 0)
    snprintf(text, sizeof(text), "%c %-8s --.- fps", m, name);
  else
    snprintf(text, sizeof(text), "%c %-8s%3lu.%lu fps", m, name,
             (unsigned long)(fps_x10 / 10), (unsigned long)(fps_x10 % 10));
  ST7789_DrawString(0, 2, text, ST7789_YELLOW, ST7789_BLACK, 2);
}

static void set_mode(uint32_t m)
{
  mode = m;
  if (mode == MODE_FRAMEBUFFER)
  {
    ST7789_AttachFramebuffer(framebuffer, sizeof framebuffer / sizeof framebuffer[0]);
  }
  else
  {
    ST7789_DetachFramebuffer();
  }
}

static void begin_scene(uint32_t index)
{
  scene = index;
  ST7789_FillScreen(ST7789_BLACK);
  if (scene < SCENE_COUNT)
  {
    draw_bar(scenes[scene].name, 0);
  }
  ST7789_Flush();
  ST7789_WaitIdle();
  frames = 0;
  bar_frames = 0;
  rng_state = RNG_SEED;   /* to draw the same shapes in each mode and in each cycle */
  scene_start = bar_time = HAL_GetTick();
}

/* The test pattern is an image that does not move. The stress test shows it
 * at the start of each mode. Incorrect bits show as incorrect colors, as
 * errors in the checkerboard, or as shapes at incorrect positions. */
static uint16_t check_row_a[240] ST7789_DMA_BUFFER;
static uint16_t check_row_b[240] ST7789_DMA_BUFFER;
static uint16_t check_image[48 * 48] ST7789_DMA_BUFFER;
/* This buffer is NOT a DMA buffer, because the stress test must also use the
 * library without DMA. The buffer is in DTCM, and the DMA cannot read DTCM.
 * Thus, when the library draws the image that is in this buffer, the CPU
 * sends the data. */
static uint16_t check_small[16 * 16];

static void show_check(void)
{
  static const uint16_t bars[8] = { ST7789_WHITE, ST7789_YELLOW, ST7789_CYAN, ST7789_GREEN,
                                    ST7789_MAGENTA, ST7789_RED, ST7789_BLUE, ST7789_GREY };

  /* Step 1. In framebuffer mode, the flush of this step sends the full
   * screen in one DMA transfer. */
  ST7789_FillScreen(ST7789_BLACK);
  ST7789_DrawString(0, 2, (mode == MODE_DIRECT) ? "D CHECK" : "F CHECK",
                    ST7789_YELLOW, ST7789_BLACK, 2);

  for (int i = 0; i < 8; i++)                       /* color rectangles */
  {
    ST7789_FillRect((int16_t)(i * 30), BAR_H, 30, 50, bars[i]);
  }
  /* Transparent text: the rectangles must show around the characters. The
   * background color (red) must not show. */
  ST7789_SetTextTransparent(1);
  ST7789_DrawString(4, 38, "Clear", ST7789_BLACK, ST7789_RED, 2);
  ST7789_SetTextTransparent(0);

  for (int x = 0; x < 240; x++)                     /* 1-pixel checkerboard */
  {
    check_row_a[x] = (x & 1) ? ST7789_WHITE : ST7789_BLACK;
    check_row_b[x] = (x & 1) ? ST7789_BLACK : ST7789_WHITE;
  }
  for (int y = 70; y < 110; y++)
  {
    ST7789_DrawImage(0, (int16_t)y, 240, 1, (y & 1) ? check_row_b : check_row_a);
  }

  for (int y = 0; y < 48; y++)                      /* gradient image */
  {
    for (int x = 0; x < 48; x++)
    {
      check_image[y * 48 + x] = ST7789_RGB(x * 5, y * 5, 255 - x * 5);
    }
  }
  for (int y = 0; y < 16; y++)
  {
    for (int x = 0; x < 16; x++)
    {
      check_small[y * 16 + x] = check_image[(y * 3) * 48 + x * 3];
    }
  }
  ST7789_DrawImage(-24, 116, 48, 48, check_image);  /* clipped left half */
  ST7789_DrawImage(216, 116, 48, 48, check_image);  /* clipped right half */
  ST7789_DrawImage(52, 132, 16, 16, check_small);   /* not clipped: in direct mode,
                                                       the library sends it in one transfer */

  ST7789_DrawString(0, 172, "Size 1: The quick brown fox 0123", ST7789_WHITE, ST7789_BLACK, 1);
  ST7789_DrawString(0, 184, "Size 2: AaBb 789", ST7789_CYAN, ST7789_BLACK, 2);
  ST7789_DrawString(-3, 302, "Clipped", ST7789_WHITE, ST7789_BLACK, 1);   /* clipping at the left edge */
  /* The difference in y between the two ends of this line is half of the
     difference in x. Thus, in each column in which the x coordinate is an
     odd number, two pixels are equally near the line. In these columns, the
     library must select the same pixel in the two modes. If the library
     selects different pixels in the two modes, this line is different in the
     two modes. */
  ST7789_DrawLine(130, 205, 190, 235, ST7789_CYAN);
  /* A filled circle with its contour on it: a pixel of the filled circle must
   * not show out of the contour. */
  ST7789_FillCircle(215, 218, 12, ST7789_RED);
  ST7789_DrawCircle(215, 218, 12, ST7789_WHITE);
  ST7789_Flush();
  ST7789_WaitIdle();

  /* Step 2. The shapes of this step are on the areas that are black after
   * step 1. In framebuffer mode, these shapes make 10 different changed
   * areas, and no area becomes part of a different area. Thus, the flush is
   * a sequence of DMA transfers in which the transfer interrupt starts the
   * next DMA transfer. If the library records a changed area incorrectly, a
   * part of a shape is missing.
   *
   * For the two thin shapes, the library uses its two methods for thin
   * areas:
   * - The library copies the 1 px line into the gather buffer. Then the DMA
   *   sends the gather buffer, and it does not stop between the rows.
   * - The library sends the 3 px rectangle row by row. The DMA starts again
   *   for each row.
   *
   * If you change the positions of the shapes, keep a large distance between
   * them. */
  ST7789_DrawImage(96, 116, 48, 48, check_image);   /* not clipped */
  ST7789_DrawVLine(80, 116, 48, ST7789_YELLOW);     /* thin areas */
  ST7789_FillRect(84, 116, 3, 48, ST7789_MAGENTA);
  ST7789_DrawLine(150, 120, 205, 160, ST7789_YELLOW);
  ST7789_DrawImage(232, 172, 16, 16, check_small);  /* clipped half: in direct mode,
                                                       the library sends it row by row */
  ST7789_DrawString(0, 204, "Sz3 #@!", ST7789_ORANGE, ST7789_BLUE, 3);

  ST7789_DrawCircle(40, 270, 30, ST7789_GREEN);
  ST7789_FillCircle(120, 270, 30, ST7789_RED);
  ST7789_DrawRect(170, 240, 60, 60, ST7789_WHITE);
  ST7789_DrawLine(170, 240, 229, 299, ST7789_YELLOW);
  ST7789_FillTriangle(180, 300, 200, 250, 220, 300, ST7789_MAGENTA);
  /* The filled triangle includes all the pixels of the contour, and no
   * pixels out of it. Thus, magenta must not show out of the contour. */
  ST7789_DrawTriangle(180, 300, 200, 250, 220, 300, ST7789_WHITE);
  for (int x = 0; x < 240; x++)                     /* thin bottom gradient */
  {
    ST7789_DrawPixel((int16_t)x, 312, ST7789_RGB(x, 255 - x, 128));
    ST7789_DrawVLine((int16_t)x, 313, 7, ST7789_RGB(x, 255 - x, 128));
  }
  ST7789_Flush();
  ST7789_WaitIdle();
}

static void show_summary(void)
{
  char line[40];
  uint32_t mhz = spi_mhz_x10();
  snprintf(line, sizeof(line), "fps @ %lu.%lu MHz SPI", (unsigned long)(mhz / 10),
           (unsigned long)(mhz % 10));
  ST7789_DrawString(0, 2, line, ST7789_YELLOW, ST7789_BLACK, 2);
  ST7789_DrawString(0, 30, "       direct   fbuf", ST7789_GREY, ST7789_BLACK, 2);
  printf("---- summary (fps @ %lu.%lu MHz SPI) ----\r\n         direct  framebuffer\r\n",
         (unsigned long)(mhz / 10), (unsigned long)(mhz % 10));
  for (uint32_t i = 0; i < SCENE_COUNT; i++)
  {
    const Scene *s = &scenes[i];
    unsigned long d = s->fps_x10[MODE_DIRECT], f = s->fps_x10[MODE_FRAMEBUFFER];
    snprintf(line, sizeof(line), "%-7s%4lu.%lu%5lu.%lu", s->name, d / 10, d % 10, f / 10, f % 10);
    ST7789_DrawString(0, (int16_t)(58 + i * 26), line, ST7789_WHITE, ST7789_BLACK, 2);
    printf("%-8s %4lu.%lu  %9lu.%lu\r\n", s->name, d / 10, d % 10, f / 10, f % 10);
  }
  ST7789_Flush();
}

/* ---- scenes ------------------------------------------------------------- */

static void draw_fill(uint32_t frame)
{
  static const uint16_t colors[] = { ST7789_RED, ST7789_GREEN, ST7789_BLUE,
                                     ST7789_CYAN, ST7789_MAGENTA, ST7789_ORANGE };
  ST7789_FillRect(0, BAR_H, 240, 320 - BAR_H, colors[frame % 6]);
}

static void draw_text(uint32_t frame)
{
  /* The text has 37 rows of 40 characters, and each character uses 6x8
   * pixels. In each frame, the characters move by one position. */
  static char page[40 * 37 + 1];
  for (uint32_t i = 0; i < 40 * 37; i++)
  {
    page[i] = (char)('!' + ((i + frame) % 94));
  }
  page[40 * 37] = '\0';
  uint16_t fg = (frame & 1) ? ST7789_WHITE : ST7789_GREEN;
  ST7789_DrawString(0, BAR_H + 4, page, fg, ST7789_BLACK, 1);
}

static void draw_lines(uint32_t frame)
{
  (void)frame;
  for (int i = 0; i < 300; i++)
  {
    ST7789_DrawLine(rng_range(0, 239), rng_range(BAR_H, 319),
                    rng_range(0, 239), rng_range(BAR_H, 319), (uint16_t)rng());
  }
}

static void draw_circles(uint32_t frame)
{
  (void)frame;
  for (int i = 0; i < 40; i++)
  {
    int16_t r  = rng_range(5, 40);
    int16_t cx = rng_range(r, 239 - r);
    int16_t cy = rng_range(BAR_H + r, 319 - r);
    ST7789_FillCircle(cx, cy, r, (uint16_t)rng());
  }
}

static void draw_pixels(uint32_t frame)
{
  (void)frame;
  for (int16_t y = BAR_H; y < 320; y++)
  {
    for (int16_t x = 0; x < 240; x++)
    {
      ST7789_DrawPixel(x, y, (uint16_t)rng());
    }
  }
}

static void draw_scatter(uint32_t frame)
{
  (void)frame;
  for (uint32_t i = 0; i < 240u * (320u - BAR_H); i++)
  {
    uint32_t r = rng();
    ST7789_DrawPixel((int16_t)(r % 240u), (int16_t)(BAR_H + (r >> 8) % (320u - BAR_H)),
                     (uint16_t)(r >> 16));
  }
}

/* ---- functions in stress_demo.h ----------------------------------------- */

static void start_mode(uint32_t m)
{
  set_mode(m);
  show_check();
  checking = 1;
  scene_start = HAL_GetTick();
}

void StressDemo_Start(void)
{
  ST7789_SetRotation(ST7789_PORTRAIT);
  printf("Display stress test (button = next scene)\r\n");
  start_mode(MODE_DIRECT);
}

/* SysTick_Handler calls this function at intervals of 1 ms. While you push
 * the USER button, its signal is high. The function accepts a new level only
 * after it reads that level 5 times in sequence. Thus, if contact bounce
 * occurs when you push and when you release the button, the function sets
 * the variable skip to 1 only one time. */
void StressDemo_Tick1ms(void)
{
  static uint8_t stable, count;
  uint8_t level = (BSP_PB_GetState(BUTTON_USER) != 0) ? 1u : 0u;
  if (level == stable)
  {
    count = 0;
    return;
  }
  if (++count >= 5u)
  {
    stable = level;
    count  = 0;
    if (stable)
    {
      skip = 1;
    }
  }
}

void StressDemo_Update(void)
{
  uint32_t now = HAL_GetTick();

  if (checking)
  {
    if (skip || now - scene_start >= CHECK_MS)
    {
      skip = 0;
      checking = 0;
      begin_scene(0);
    }
    return;
  }

  if (scene >= SCENE_COUNT)
  {
    /* Results screen: the stress test waits, and then it starts again */
    if (skip || now - scene_start >= SUMMARY_MS)
    {
      skip = 0;
      start_mode(MODE_DIRECT);
    }
    return;
  }

  Scene *s = &scenes[scene];
  s->draw(frames);
  ST7789_Flush();                           /* no effect in direct mode */
  frames++;
  bar_frames++;

  now = HAL_GetTick();
  if (frames == 1)
  {
    /* This code starts the interval for the frame rate in the status bar
       after the first frame. Thus, each interval starts and stops at the
       same point of the loop. This code does not wait for the transfer.
       Thus, in framebuffer mode, the stress test continues to draw during
       the flush. */
    bar_frames = 0;
    bar_time   = now;
  }
  else if (now - bar_time >= BAR_UPDATE_MS)
  {
    draw_bar(s->name, fps_tenths(bar_frames, now - bar_time));
    bar_frames = 0;
    bar_time   = now;
  }

  if (skip || now - scene_start >= SCENE_MS)
  {
    ST7789_WaitIdle();                      /* to count only the completed frames */
    now = HAL_GetTick();
    s->fps_x10[mode] = fps_tenths(frames, now - scene_start);
    printf("%-11s %-8s %4lu.%lu fps  (%lu frames)\r\n", mode_names[mode], s->name,
           (unsigned long)(s->fps_x10[mode] / 10), (unsigned long)(s->fps_x10[mode] % 10),
           (unsigned long)frames);
    skip = 0;

    if (scene + 1 < SCENE_COUNT)
    {
      begin_scene(scene + 1);
    }
    else if (mode + 1 < MODE_COUNT)
    {
      start_mode(mode + 1);
    }
    else
    {
      set_mode(MODE_DIRECT);
      begin_scene(SCENE_COUNT);
      show_summary();
    }
  }
}

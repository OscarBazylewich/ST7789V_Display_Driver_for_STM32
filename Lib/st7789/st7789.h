/**
 * Library for the ST7789 controller.
 *
 * The library has two drawing modes. The application can change the
 * drawing mode while it operates:
 *
 * - In direct mode (the default), each drawing function sends its pixels
 *   directly to the controller. The DMA does each large transfer as a
 *   background transfer. Thus, most calls return before the screen shows
 *   their pixels. It is not necessary to wait between two calls, because
 *   each drawing function first waits for the end of the previous transfer.
 *   It also does this when it draws no pixels.
 *
 *   A framebuffer is not necessary. The library uses this RAM:
 *   - Approximately 350 bytes for its internal data.
 *   - A gather buffer of 640 bytes for changed areas that are 1 pixel wide
 *     (ST7789_GATHER_WIDTH). If ST7789_GATHER_WIDTH is 0, the library has
 *     no gather buffer.
 *   - Two text buffers when the application draws text. With the defaults,
 *     these buffers use 15 KB (refer to st7789_config.h).
 *
 * - In framebuffer mode (ST7789_AttachFramebuffer), the drawing functions
 *   change only a copy of the screen in RAM (the framebuffer). Each call
 *   records the bounding box of its pixels as a changed area.
 *   ST7789_Flush() waits for the end of the previous flush. Then it sends
 *   only these changed areas as a background transfer.
 *
 *   The application can call the drawing functions during a flush. The
 *   next flush sends the areas that changed during this time. If the screen
 *   must show only completed frames, call ST7789_WaitIdle() before you call
 *   the next drawing function. Framebuffer mode is best for frames that
 *   change quickly and for many small shapes.
 *
 * The coordinates can be less than 0 or out of the screen. The library
 * clips all the parts that are out of the screen. The colors are RGB565
 * values.
 *
 * Instructions for calls to the library:
 *
 * - After you prepare the SPI and the pins, call ST7789_Init() before the
 *   other functions of the library.
 * - Call the functions of the library from only one context, usually the
 *   loop in main() or one RTOS task. If two or more tasks use the
 *   library, use a mutex. A call to the library must not start before the
 *   end of the previous call, also in framebuffer mode.
 * - Make sure that the interrupts of the port and the interrupt of its
 *   millisecond counter can preempt this context. On the STM32H7 port,
 *   these interrupts are SPI1 and DMA1_Stream0 at
 *   ST7789_PORT_IRQ_PRIORITY, and SysTick.
 * - Thus, do not call a function of the library:
 *   - When the interrupts are disabled
 *   - From an interrupt handler that has the same priority as one of these
 *     interrupts, or a higher priority.
 *
 *   In these conditions, a call that must wait continues to wait until a
 *   timeout occurs (ST7789_IDLE_TIMEOUT_MS). Then it stops the transfer
 *   before its end. If the millisecond counter also cannot increase, the
 *   call does not return.
 *
 * - To draw when an interrupt occurs, set a flag to 1 in the interrupt
 *   handler. Then call the drawing functions from the loop in main().
 *
 * The code for the hardware (the SPI, the DMA, the pins and the time) is in
 * a port file. Refer to st7789_port.h. The configuration macros are in
 * st7789_config.h.
 */
#ifndef ST7789_H
#define ST7789_H

#include <stdint.h>
#include "st7789_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- colors (RGB565) ---------------------------------------------------- */

#define ST7789_BLACK    0x0000
#define ST7789_WHITE    0xFFFF
#define ST7789_RED      0xF800
#define ST7789_GREEN    0x07E0
#define ST7789_BLUE     0x001F
#define ST7789_YELLOW   0xFFE0
#define ST7789_CYAN     0x07FF
#define ST7789_MAGENTA  0xF81F
#define ST7789_ORANGE   0xFD20
#define ST7789_GREY     0x8410

/* This macro makes an RGB565 color from 8-bit red, green and blue values */
#define ST7789_RGB(r, g, b) \
  ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | (((b) & 0xFF) >> 3)))

/* The number of pixels in a framebuffer for the full screen */
#define ST7789_FRAMEBUFFER_PIXELS  ((uint32_t)ST7789_PANEL_WIDTH * ST7789_PANEL_HEIGHT)

/* ST7789_DMA_BUFFER (in st7789_config.h) puts a buffer in memory that the
 * DMA can read, for example:
 *   static uint16_t fb[ST7789_FRAMEBUFFER_PIXELS] ST7789_DMA_BUFFER;
 * The library can also use a buffer that the DMA cannot read. In this
 * condition, the CPU sends the data, not the DMA. */

typedef enum
{
  ST7789_PORTRAIT = 0,         /* portrait, not turned */
  ST7789_LANDSCAPE,            /* turned 90 degrees */
  ST7789_PORTRAIT_FLIPPED,     /* turned 180 degrees */
  ST7789_LANDSCAPE_FLIPPED,    /* turned 270 degrees */
} ST7789_Rotation;

/* ---- configuration and control of the display module -------------------- */

/* After you prepare the SPI and the pins (MX_SPI1_Init()), call this
 * function before the other functions of the library. It does a reset of
 * the controller, sets its configuration and fills the screen with black.
 * Then it sets the panel to ON. When it returns, the rotation is portrait,
 * there is no color inversion and no scrolling, and the library is in
 * direct mode. If the library has a framebuffer, this function disconnects
 * it. If you use a framebuffer, attach it again after this call. */
void     ST7789_Init(void);
void     ST7789_SetRotation(ST7789_Rotation rotation);
/* This function returns the rotation that the library uses: the last
 * rotation that you set. After ST7789_Init, it is ST7789_PORTRAIT.
 * (If two rotations have 180 degrees between them, ST7789_Width and
 * ST7789_Height cannot show the difference.) */
ST7789_Rotation ST7789_GetRotation(void);
uint16_t ST7789_Width(void);
uint16_t ST7789_Height(void);
void     ST7789_Backlight(uint8_t on);
void     ST7789_DisplayOn(uint8_t on);       /* 0: the image stays in memory */
/* In sleep mode, the panel shows no image, and the controller uses only a
 * very small quantity of the electrical supply. The controller memory keeps
 * its data, and the panel shows the image again after Sleep(0). Sleep mode
 * does not change the backlight, which uses more than half of the electrical
 * supply of the display module. To set the backlight to ON or OFF, use
 * ST7789_Backlight. The time for this function is approximately 5 ms. If
 * you call Sleep(1) less than 120 ms after the end of the previous sleep
 * mode, the time is a maximum of 120 ms. */
void     ST7789_Sleep(uint8_t sleep);
void     ST7789_InvertColors(uint8_t invert);

/* This function sets the refresh rate of the panel in Hz, from 39 to 119.
 * After ST7789_Init, the refresh rate is 60 Hz. The library sets the
 * nearest refresh rate that the controller can use. With a higher refresh
 * rate, you can see less tearing. */
void     ST7789_SetRefreshRate(uint8_t hz);

/* Hardware scrolling moves the rows of the scroll area (the rows between the
 * top fixed area and the bottom fixed area). A row that moves out at one end
 * of the scroll area shows again at the other end. For this, the library
 * sends only one short command, and it does not draw the pixels again.
 * Hardware scrolling is for direct mode.
 * - `top_fixed` and `bottom_fixed` are the numbers of rows in the top fixed
 *   area and in the bottom fixed area. These rows of the panel do not move.
 *   The scroll area between them must contain a minimum of one row.
 * - ScrollTo(n) moves the contents of the scroll area n rows up from their
 *   start position (n modulo the height of the scroll area). SetScrollArea
 *   sets n to 0.
 * - For all rotations, the library uses the rows of the panel in portrait
 *   for the areas and the direction. In ST7789_PORTRAIT_FLIPPED, the top
 *   fixed area is at the bottom of the screen, and the contents move in the
 *   opposite direction. In the landscape rotations, the scrolling is
 *   horizontal. SetRotation does not change the scroll area or the offset n.
 * - The library does not add the offset n to the coordinates of the drawing
 *   functions. */
void     ST7789_SetScrollArea(uint16_t top_fixed, uint16_t bottom_fixed);
void     ST7789_ScrollTo(uint16_t line);

/* ---- background transfers ----------------------------------------------- */

/* This function waits for the end of all the transfers, for a maximum of
 * ST7789_IDLE_TIMEOUT_MS. If a transfer does not complete in this time, the
 * function stops it and sets the ST7789_ERR_TIMEOUT bit to 1. In
 * framebuffer mode, the next flush sends the areas that this transfer did
 * not fully send. For this function, the interrupts of the port must
 * operate (refer to the instructions for calls at the top of this file). */
void    ST7789_WaitIdle(void);
uint8_t ST7789_IsBusy(void);       /* 1 while a transfer is in progress */

/* The errors that occurred after the last call of ST7789_Init or
 * ST7789_ClearErrors, as a value with one bit for each error */
#define ST7789_ERR_TIMEOUT  0x01u  /* the library stopped a transfer after
                                      ST7789_IDLE_TIMEOUT_MS */
#define ST7789_ERR_DMA      0x02u  /* the DMA could not read a source */
#define ST7789_ERR_LINK     0x04u  /* the SPI stopped (other code changed its
                                      configuration, or its clock stopped),
                                      and the port started it again. It is
                                      possible that the controller did not
                                      receive some pixels or a value of its
                                      configuration. Draw the contents of the
                                      screen again. As an alternative, call
                                      ST7789_Init. */
uint8_t ST7789_GetErrors(void);
void    ST7789_ClearErrors(void);

/* These callbacks are optional. While the library waits, they can stop the
 * CPU until the next interrupt, or let the RTOS stop the task. Without
 * them, the library waits in a wait loop. The README shows the two methods.
 * NULL removes a callback.
 * - The library calls the wait hook again and again while it waits for a
 *   background transfer. The wait hook can stop the CPU until the next
 *   interrupt, or let the RTOS stop the task. But it must return in
 *   approximately 1 millisecond or less. Thus, a timeout also occurs for a
 *   transfer that does not complete.
 * - The library calls the done callback after the end of each background
 *   transfer (a DMA transfer that a drawing function started, or a full
 *   flush). The done callback always operates in the interrupt handler of
 *   the transfer interrupt (on the STM32H7 port, SPI1 at
 *   ST7789_PORT_IRQ_PRIORITY). At this time, ST7789_IsBusy() returns 0.
 *   The done callback must be short, and it must not call a function of
 *   the library. The done callback does not operate for a transfer that
 *   the library stopped after a timeout (ST7789_ERR_TIMEOUT), or that
 *   ST7789_Init stopped. */
void    ST7789_SetWaitHook(void (*hook)(void));
void    ST7789_SetDoneCallback(void (*callback)(void));

/* ---- framebuffer mode ----------------------------------------------------
 * The framebuffer must contain a minimum of ST7789_FRAMEBUFFER_PIXELS
 * pixels. We recommend a DMA buffer (a buffer with ST7789_DMA_BUFFER) for
 * the framebuffer. Give the number of pixels of the framebuffer, for
 * example:
 *   ST7789_AttachFramebuffer(fb, sizeof fb / sizeof fb[0]);
 * ST7789_AttachFramebuffer rejects a framebuffer that is NULL or too small.
 * Then it returns 0 and makes no change.
 *
 * The framebuffer contains the screen in the rotation that the library
 * uses, row by row: pixel (x, y) is `framebuffer[y * ST7789_Width() + x]`.
 * Each pixel is an RGB565 value, as the color macros in this file (the same
 * format as for DrawImage).
 *
 * ST7789_AttachFramebuffer records the full screen as a changed area. Thus,
 * the next flush sends all the contents of the framebuffer. If the
 * framebuffer can contain unwanted data, fill it with ST7789_FillScreen.
 *
 * ST7789_SetRotation does not turn the contents of the framebuffer. It
 * records the full screen as a changed area, and the next flush reads the
 * framebuffer with the new width. Thus, after a change between portrait and
 * landscape, draw the full screen again. (After a change of the rotation by
 * 180 degrees, the contents have the correct dimensions.)
 *
 * Each drawing function records the clipped bounding box of its pixels as a
 * changed area. For a line or a contour (DrawLine, DrawRect, DrawCircle,
 * DrawTriangle), it records the full clipped bounding box, not only the
 * drawn pixels. Thus, the next flush also sends the inner pixels of the
 * bounding box again.
 */
uint8_t   ST7789_AttachFramebuffer(uint16_t *framebuffer, uint32_t pixels);
/* This function sets the library to direct mode again. It waits for the end
 * of a flush that is in progress. But it discards the changes that you made
 * after the last ST7789_Flush(). To show these changes, call ST7789_Flush()
 * first. */
void      ST7789_DetachFramebuffer(void);
uint16_t *ST7789_GetFramebuffer(void);       /* NULL in direct mode */
/* This function waits for the end of the previous flush. Then it starts to
 * send the changed areas and returns. In direct mode, it has no effect. */
void      ST7789_Flush(void);
/* After you write into the framebuffer without the drawing functions,
 * record the changed area with this function. Use the coordinates of the
 * rotation that the library uses. If a different bus master (DMA2D, MDMA)
 * writes into the framebuffer when the data cache is on, refer to the
 * README. */
void      ST7789_MarkDirty(int16_t x, int16_t y, int16_t w, int16_t h);

/* ---- shapes ------------------------------------------------------------- */

void ST7789_FillScreen(uint16_t color);
void ST7789_FillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void ST7789_DrawPixel(int16_t x, int16_t y, uint16_t color);
void ST7789_DrawHLine(int16_t x, int16_t y, int16_t w, uint16_t color);
void ST7789_DrawVLine(int16_t x, int16_t y, int16_t h, uint16_t color);
void ST7789_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
void ST7789_DrawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void ST7789_DrawCircle(int16_t cx, int16_t cy, int16_t r, uint16_t color);
void ST7789_FillCircle(int16_t cx, int16_t cy, int16_t r, uint16_t color);
void ST7789_DrawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                         int16_t x2, int16_t y2, uint16_t color);
/* The filled area is the same as the contour that DrawTriangle draws with the
 * same corners, together with all the pixels in this contour. Thus, if you
 * draw a contour on a filled triangle (or a filled triangle on a contour),
 * there is no empty space between them and no unwanted pixel. */
void ST7789_FillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                         int16_t x2, int16_t y2, uint16_t color);

/* This function draws an RGB565 image of `w` x `h` pixels. The image is in
 * memory row by row, from the top-left pixel. `w` is also the stride. If
 * `w` or `h` is 0 or less, or if `pixels` is NULL, the function draws no
 * pixels.
 *
 * In direct mode, the library sends an image that the DMA can read (a DMA
 * buffer, or `const` data in flash memory) as a background transfer. This
 * transfer continues after the function returns. Thus, do not change the
 * buffer or use it again until the next drawing function or
 * ST7789_WaitIdle() returns. When ST7789_IsBusy() returns 0, you can also
 * change the buffer or use it again. The library sends other buffers
 * before the function returns. Keep constant images `const`, without
 * ST7789_DMA_BUFFER, because the DMA reads flash memory directly.
 *
 * In framebuffer mode, the library copies the image before the function
 * returns. The image can also be in the framebuffer. Thus, you can move full
 * rows (`w` equal to ST7789_Width()), for example to move the image up or
 * down. */
void ST7789_DrawImage(int16_t x, int16_t y, int16_t w, int16_t h,
                      const uint16_t *pixels);

/* ---- text (5x7 font) -----------------------------------------------------
 * Each character uses a rectangle of 6x8 pixels (with one empty column and
 * one empty row between characters). The parameter `size` multiplies these
 * dimensions. The range of `size` is 1 to ST7789_MAX_TEXT_SIZE (the default
 * of ST7789_MAX_TEXT_SIZE is 8). The library changes other values to the
 * nearest limit of this range.
 *
 * The library also draws the background color, but not if the last value
 * that you gave to ST7789_SetTextTransparent is 1. Thus, new text fully
 * replaces the previous text.
 *
 * The font includes the ASCII characters from ' ' to '~'. The library draws
 * all other bytes as '?'. This includes each byte of a UTF-8 sequence.
 */
#define ST7789_CHAR_W          6
#define ST7789_CHAR_H          8

void ST7789_DrawChar(int16_t x, int16_t y, char c,
                     uint16_t fg, uint16_t bg, uint8_t size);

/* This function draws a string. `'\n'` starts a new row of text at `x`. The
 * library ignores `'\r'`. Thus, `"\r\n"` has the same result as `"\n"`.
 *
 * If the next character does not have sufficient space at the right side of
 * the screen, the text continues on the next row at `x`. But if there is
 * not sufficient space for one character at `x`, the library clips the
 * remaining part of the row. The function stops at the bottom of the screen.
 * If `str` is NULL, the function draws no pixels. */
void ST7789_DrawString(int16_t x, int16_t y, const char *str,
                       uint16_t fg, uint16_t bg, uint8_t size);

/* 1 (the default): at the right side of the screen, the text of DrawString
 * continues on the next row. 0: the text does not continue on the next row,
 * and the library clips it at the right side. This is applicable to text
 * that moves across the screen and to labels that do not move. */
void ST7789_SetTextWrap(uint8_t wrap);

/* - 1: DrawChar and DrawString draw only the pixels of the characters. The
 *   other pixels in the rectangle of each character do not change (the
 *   library ignores `bg`). Thus, in direct mode, the library must set a
 *   window for each horizontal line of character pixels. At `size` 1, this
 *   is approximately three times slower than with 0. At `size` 2 or more,
 *   it is approximately as fast as with 0 or faster than with 0.
 * - 0 (the default): the library also draws the background color. */
void ST7789_SetTextTransparent(uint8_t transparent);

#ifdef __cplusplus
}
#endif

#endif /* ST7789_H */

/**
 * A simulated ST7789 controller for the tests. It receives the bytes of the
 * SPI transfers and the pin changes from the HAL functions in fake_hal. It
 * writes the pixels into a simulated controller memory (240 x 320 pixels).
 *
 * The simulated controller records each incorrect use as an error. Examples
 * are data without a command, or a command less than 5 ms after a reset.
 */
#ifndef ST7789_MODEL_H
#define ST7789_MODEL_H

#include <stdint.h>
#include "main.h"

#define MODEL_COLS     240
#define MODEL_ROWS     320
#define MODEL_LOG_MAX  4096u

/* The commands that the simulated controller knows */
#define MODEL_SWRESET  0x01u
#define MODEL_SLPIN    0x10u
#define MODEL_SLPOUT   0x11u
#define MODEL_NORON    0x13u
#define MODEL_INVOFF   0x20u
#define MODEL_INVON    0x21u
#define MODEL_DISPOFF  0x28u
#define MODEL_DISPON   0x29u
#define MODEL_CASET    0x2Au
#define MODEL_RASET    0x2Bu
#define MODEL_RAMWR    0x2Cu
#define MODEL_MADCTL   0x36u
#define MODEL_COLMOD   0x3Au
#define MODEL_RAMWRC   0x3Cu

/* The pins of the controller. A NULL port: the pin is not connected (CS
 * connects to GND, RST stays high). */
typedef struct
{
  GPIO_TypeDef *dc_port, *cs_port, *rst_port, *bl_port;
  uint16_t dc_pin, cs_pin, rst_pin, bl_pin;
} model_pins;

/* One command that the controller received */
typedef struct
{
  uint8_t  cmd;
  uint8_t  nparam;
  uint8_t  param[4];
  uint32_t tick;
  uint32_t pixels;          /* RAMWR and RAMWRC: the number of pixels */
  uint8_t  bl_level;        /* the level of the backlight pin at the command */
} model_cmd;

typedef struct
{
  uint16_t mem[MODEL_ROWS][MODEL_COLS];     /* [row][column], in portrait */
  uint8_t  written[MODEL_ROWS][MODEL_COLS]; /* 1: the controller wrote this pixel */

  uint8_t  madctl, colmod, inverted, display_on, sleeping, normal_mode;
  uint16_t xs, xe, ys, ye;                  /* CASET and RASET */

  uint32_t errors;                          /* incorrect uses of the controller */
  char     first_error[256];
  uint32_t data_without_cmd;                /* data bytes without a command */
  uint32_t ramwr_short;                     /* RAMWR sequences that did not fill the window */
  uint32_t overflow;                        /* pixels after the end of the window */
  uint32_t pixels_total;
  uint32_t hw_resets, sw_resets;

  uint32_t log_len;                         /* the number of commands */
  model_cmd log[MODEL_LOG_MAX];             /* a list of the first MODEL_LOG_MAX commands */
} model_t;

extern model_t model;

/* The start of the simulated controller: all the controller memory gets
 * `fill`, `written` gets 0, and the controller gets its reset values. */
void model_power_on(const model_pins *pins, uint16_t fill);

/* This function sets the errors, the counters and the list of commands
 * to 0. */
void model_clear_counts(void);

/* For fake_hal_set_sink() */
void model_bytes(const uint8_t *data, uint32_t n);
void model_pin(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState state);

/* 1 if a RAMWR sequence started and did not fill its window */
int model_window_open(void);

/* After an SPI error: the controller stops the window that did not get
 * all its pixels. Thus, the next RAMWR does not increase ramwr_short. */
void model_drop_window(void);

/* The level of the backlight pin (1: high), or -1 if there is no pin */
int model_bl_level(void);

#endif /* ST7789_MODEL_H */

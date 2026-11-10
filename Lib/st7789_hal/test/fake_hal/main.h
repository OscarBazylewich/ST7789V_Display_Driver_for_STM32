/**
 * The HAL types and the 7 HAL functions that the ST7789 HAL library uses.
 * The tests use these functions, not the HAL of an MCU.
 *
 * The functions do not use hardware. They send the bytes of each SPI
 * transfer and each pin change to two functions. The test sets these
 * functions with fake_hal_set_sink(). The tick is a variable. HAL_Delay()
 * adds to it.
 *
 * The test can tell a HAL function to return an error. Refer to
 * fake_hal_state.
 */
#ifndef MAIN_H
#define MAIN_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- HAL types ----------------------------------------------------------- */

typedef enum
{
  HAL_OK = 0x00,
  HAL_ERROR = 0x01,
  HAL_BUSY = 0x02,
  HAL_TIMEOUT = 0x03
} HAL_StatusTypeDef;

typedef enum
{
  GPIO_PIN_RESET = 0,
  GPIO_PIN_SET
} GPIO_PinState;

typedef struct
{
  volatile uint32_t ODR;      /* the output level of each pin */
  const char *name;
} GPIO_TypeDef;

typedef struct
{
  uint32_t dummy;
} DMA_HandleTypeDef;

typedef enum
{
  HAL_SPI_STATE_RESET = 0x00,
  HAL_SPI_STATE_READY = 0x01,
  HAL_SPI_STATE_BUSY = 0x02,
  HAL_SPI_STATE_BUSY_TX = 0x03,
  HAL_SPI_STATE_ERROR = 0x06,
  HAL_SPI_STATE_ABORT = 0x07
} HAL_SPI_StateTypeDef;

typedef struct
{
  uint32_t DataSize;
} SPI_InitTypeDef;

typedef struct
{
  SPI_InitTypeDef Init;
  DMA_HandleTypeDef *hdmatx;
  volatile uint32_t ErrorCode;
  volatile HAL_SPI_StateTypeDef State;
} SPI_HandleTypeDef;

#define SPI_DATASIZE_8BIT    0x00000007u
#define SPI_DATASIZE_16BIT   0x0000000Fu
#define HAL_SPI_ERROR_NONE   0x00000000u
#define HAL_SPI_ERROR_DMA    0x00000010u

#define GPIO_PIN_0   0x0001u
#define GPIO_PIN_1   0x0002u
#define GPIO_PIN_2   0x0004u
#define GPIO_PIN_3   0x0008u
#define GPIO_PIN_4   0x0010u
#define GPIO_PIN_5   0x0020u
#define GPIO_PIN_15  0x8000u

/* Some HAL drivers have `const uint8_t *pData`, and other HAL drivers have
 * `uint8_t *pData`. To get `const`, compile with FAKE_HAL_CONST_PDATA. */
#ifdef FAKE_HAL_CONST_PDATA
#define FAKE_HAL_PDATA const uint8_t *
#else
#define FAKE_HAL_PDATA uint8_t *
#endif

/* ---- the 7 HAL functions ------------------------------------------------- */

HAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef *hspi, FAKE_HAL_PDATA pData, uint16_t Size,
                                   uint32_t Timeout);
HAL_StatusTypeDef HAL_SPI_Transmit_DMA(SPI_HandleTypeDef *hspi, FAKE_HAL_PDATA pData, uint16_t Size);
HAL_SPI_StateTypeDef HAL_SPI_GetState(SPI_HandleTypeDef *hspi);
HAL_StatusTypeDef HAL_SPI_Abort(SPI_HandleTypeDef *hspi);
void HAL_GPIO_WritePin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin, GPIO_PinState PinState);
void HAL_Delay(uint32_t Delay);
uint32_t HAL_GetTick(void);

/* ---- control of the HAL functions for the tests --------------------------- */

#define FAKE_DMA_NEVER   0xFFFFFFFFu   /* the DMA transfer does not complete */
#define FAKE_LOG_MAX     8192u

typedef enum
{
  FAKE_CALL_TX = 0,                     /* HAL_SPI_Transmit */
  FAKE_CALL_DMA                         /* HAL_SPI_Transmit_DMA */
} fake_call_kind;

/* One SPI call */
typedef struct
{
  fake_call_kind kind;
  uint16_t size;
  const uint8_t *data;
  HAL_StatusTypeDef status;
} fake_call;

typedef struct
{
  /* The tick. HAL_Delay() and the wait in HAL_SPI_GetState() add to it. */
  uint32_t tick;

  /* The number of calls of each HAL function */
  uint32_t n_tx, n_dma, n_getstate, n_abort, n_gpio, n_delay, n_gettick;
  uint32_t n_spi;                      /* n_tx + n_dma */
  uint32_t n_failed;                   /* calls that did not return HAL_OK */
  uint32_t delay_total;                /* the sum of the HAL_Delay() parameters */
  uint32_t bytes_total;                /* the bytes that the SPI sent */
  uint32_t max_size;                   /* the largest `Size` parameter */
  uint32_t n_long_tx;                  /* HAL_SPI_Transmit calls with `Size >= long_size` */
  uint32_t long_size;                  /* the default is 64 */

  /* Incorrect use of the HAL functions. `first_error` gives the first. */
  uint32_t errors;
  char first_error[256];

  /* The errors that the HAL functions make:
   * - The SPI calls (HAL_SPI_Transmit and HAL_SPI_Transmit_DMA) from number
   *   `fail_at` to `fail_at + fail_count - 1` return `fail_status`. They do
   *   not send data. The first call after fake_hal_reset() is number 1.
   *   `fail_at` = 0: no error.
   * - A DMA transfer completes after `dma_polls` HAL_SPI_GetState() calls.
   *   FAKE_DMA_NEVER: it does not complete.
   * - The DMA transfer number `dma_error_at` completes with ErrorCode =
   *   HAL_SPI_ERROR_DMA. The first DMA transfer is number 1. 0: no error.
   * - Each HAL_SPI_GetState() call during a DMA transfer adds `poll_ms` to
   *   the tick (default 1). */
  uint32_t fail_at, fail_count;
  HAL_StatusTypeDef fail_status;
  uint32_t dma_polls;
  uint32_t dma_error_at;
  uint32_t poll_ms;

  /* If not NULL: the DMA must read only from this area. */
  const void *dma_area;
  size_t dma_area_size;

  /* The list of the SPI calls. It keeps the first FAKE_LOG_MAX calls. */
  uint32_t log_len;
  fake_call log[FAKE_LOG_MAX];
} fake_hal_state;

extern fake_hal_state fake;

typedef void (*fake_bytes_fn)(const uint8_t *data, uint32_t n);
typedef void (*fake_pin_fn)(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState state);

/* This function sets all the counters and the errors to 0. It keeps the
 * tick and the functions of fake_hal_set_sink(). */
void fake_hal_reset(void);
void fake_hal_set_sink(fake_bytes_fn bytes, fake_pin_fn pin);

/* 1 if a DMA transfer did not complete and did not stop */
int fake_hal_dma_busy(void);

#ifdef __cplusplus
}
#endif

#endif /* MAIN_H */

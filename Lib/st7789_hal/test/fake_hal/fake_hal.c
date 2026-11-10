/**
 * The HAL functions for the tests. Refer to `main.h`.
 */
#include "main.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

fake_hal_state fake;

static fake_bytes_fn sink_bytes;
static fake_pin_fn   sink_pin;

/* The DMA transfer at this time */
static int dma_busy;
static SPI_HandleTypeDef *dma_spi;
static const uint8_t *dma_data;
static uint16_t dma_size;
static uint32_t dma_left;
static int dma_error;
static uint8_t dma_copy[65536];

static void fake_error(const char *fmt, ...)
{
  if (fake.errors++ == 0u)
  {
    va_list ap;
    va_start(ap, fmt);
    (void)vsnprintf(fake.first_error, sizeof fake.first_error, fmt, ap);
    va_end(ap);
  }
}

void fake_hal_reset(void)
{
  uint32_t tick = fake.tick;
  memset(&fake, 0, sizeof fake);
  fake.tick = tick;
  fake.fail_status = HAL_ERROR;
  fake.fail_count = 1u;
  fake.poll_ms = 1u;
  fake.long_size = 64u;
  dma_busy = 0;
}

void fake_hal_set_sink(fake_bytes_fn bytes, fake_pin_fn pin)
{
  sink_bytes = bytes;
  sink_pin = pin;
}

int fake_hal_dma_busy(void)
{
  return dma_busy;
}

/* This function records an SPI call. It returns 1 if the call must give
 * an error. */
static int spi_call(fake_call_kind kind, const uint8_t *data, uint16_t size, HAL_StatusTypeDef *st)
{
  fake.n_spi++;
  *st = HAL_OK;
  if (fake.fail_at != 0u && fake.n_spi >= fake.fail_at &&
      fake.n_spi - fake.fail_at < fake.fail_count)
  {
    *st = fake.fail_status;
    fake.n_failed++;
  }
  if (fake.log_len < FAKE_LOG_MAX)
  {
    fake.log[fake.log_len].kind = kind;
    fake.log[fake.log_len].size = size;
    fake.log[fake.log_len].data = data;
    fake.log[fake.log_len].status = *st;
  }
  fake.log_len++;
  if (size > fake.max_size)
  {
    fake.max_size = size;
  }
  return *st != HAL_OK;
}

HAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef *hspi, FAKE_HAL_PDATA pData, uint16_t Size,
                                   uint32_t Timeout)
{
  HAL_StatusTypeDef st;
  fake.n_tx++;
  if (hspi == NULL || pData == NULL || Size == 0u)
  {
    fake_error("HAL_SPI_Transmit: hspi %p, pData %p, Size %u", (void *)hspi,
               (const void *)pData, (unsigned)Size);
    return HAL_ERROR;
  }
  if (Timeout == 0u)
  {
    fake_error("HAL_SPI_Transmit: Timeout 0");
  }
  if (dma_busy)
  {
    fake_error("HAL_SPI_Transmit during a DMA transfer");
  }
  if (Size >= fake.long_size)
  {
    fake.n_long_tx++;
  }
  if (spi_call(FAKE_CALL_TX, pData, Size, &st))
  {
    return st;
  }
  hspi->ErrorCode = HAL_SPI_ERROR_NONE;
  fake.bytes_total += Size;
  if (sink_bytes != NULL)
  {
    sink_bytes(pData, Size);
  }
  return HAL_OK;
}

HAL_StatusTypeDef HAL_SPI_Transmit_DMA(SPI_HandleTypeDef *hspi, FAKE_HAL_PDATA pData, uint16_t Size)
{
  HAL_StatusTypeDef st;
  fake.n_dma++;
  if (hspi == NULL || pData == NULL || Size == 0u)
  {
    fake_error("HAL_SPI_Transmit_DMA: hspi %p, pData %p, Size %u", (void *)hspi,
               (const void *)pData, (unsigned)Size);
    return HAL_ERROR;
  }
  if (hspi->hdmatx == NULL)
  {
    fake_error("HAL_SPI_Transmit_DMA without hdmatx");
  }
  if (dma_busy)
  {
    fake_error("HAL_SPI_Transmit_DMA during a DMA transfer");
  }
  if (fake.dma_area != NULL)
  {
    const uint8_t *a = (const uint8_t *)fake.dma_area;
    if ((const uint8_t *)pData < a || (const uint8_t *)pData + Size > a + fake.dma_area_size)
    {
      fake_error("HAL_SPI_Transmit_DMA: the data (%p, %u bytes) is not in the transmit buffer",
                 (const void *)pData, (unsigned)Size);
    }
  }
  if (spi_call(FAKE_CALL_DMA, pData, Size, &st))
  {
    return st;
  }
  hspi->ErrorCode = HAL_SPI_ERROR_NONE;
  hspi->State = HAL_SPI_STATE_BUSY_TX;
  dma_busy = 1;
  dma_spi = hspi;
  dma_data = pData;
  dma_size = Size;
  dma_left = fake.dma_polls;
  dma_error = (fake.dma_error_at != 0u && fake.dma_error_at == fake.n_dma);
  memcpy(dma_copy, pData, Size);
  return HAL_OK;
}

HAL_SPI_StateTypeDef HAL_SPI_GetState(SPI_HandleTypeDef *hspi)
{
  fake.n_getstate++;
  if (hspi == NULL)
  {
    fake_error("HAL_SPI_GetState: hspi NULL");
    return HAL_SPI_STATE_RESET;
  }
  if (dma_busy && hspi == dma_spi)
  {
    if (dma_left == 0u)
    {
      /* The DMA transfer completes. Here, the DMA reads all the data at
         the end of the transfer. Thus, the data must be the same as at the
         start. */
      dma_busy = 0;
      if (memcmp(dma_copy, dma_data, dma_size) != 0)
      {
        fake_error("the library changed the data of a DMA transfer before its end");
      }
      fake.bytes_total += dma_size;
      if (sink_bytes != NULL)
      {
        sink_bytes(dma_data, dma_size);
      }
      if (dma_error)
      {
        hspi->ErrorCode |= HAL_SPI_ERROR_DMA;
      }
      hspi->State = HAL_SPI_STATE_READY;
      return HAL_SPI_STATE_READY;
    }
    if (dma_left != FAKE_DMA_NEVER)
    {
      dma_left--;
    }
    fake.tick += fake.poll_ms;
    return HAL_SPI_STATE_BUSY_TX;
  }
  return HAL_SPI_STATE_READY;
}

HAL_StatusTypeDef HAL_SPI_Abort(SPI_HandleTypeDef *hspi)
{
  fake.n_abort++;
  if (hspi == NULL)
  {
    fake_error("HAL_SPI_Abort: hspi NULL");
    return HAL_ERROR;
  }
  if (dma_busy && hspi == dma_spi)
  {
    dma_busy = 0;                      /* the controller gets no data */
  }
  hspi->State = HAL_SPI_STATE_READY;
  return HAL_OK;
}

void HAL_GPIO_WritePin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin, GPIO_PinState PinState)
{
  fake.n_gpio++;
  if (GPIOx == NULL || GPIO_Pin == 0u)
  {
    fake_error("HAL_GPIO_WritePin: port %p, pin 0x%04X", (void *)GPIOx, (unsigned)GPIO_Pin);
    return;
  }
  if (PinState != GPIO_PIN_RESET && PinState != GPIO_PIN_SET)
  {
    fake_error("HAL_GPIO_WritePin: PinState %d", (int)PinState);
  }
  if (dma_busy)
  {
    fake_error("HAL_GPIO_WritePin during a DMA transfer");
  }
  if (PinState == GPIO_PIN_SET)
  {
    GPIOx->ODR |= GPIO_Pin;
  }
  else
  {
    GPIOx->ODR &= ~(uint32_t)GPIO_Pin;
  }
  if (sink_pin != NULL)
  {
    sink_pin(GPIOx, GPIO_Pin, PinState);
  }
}

void HAL_Delay(uint32_t Delay)
{
  fake.n_delay++;
  if (dma_busy)
  {
    fake_error("HAL_Delay during a DMA transfer");
  }
  fake.delay_total += Delay;
  fake.tick += Delay;
}

uint32_t HAL_GetTick(void)
{
  fake.n_gettick++;
  return fake.tick;
}

/**
 * ST7789 port for the STM32H7. It controls SPI1 and DMA1 Stream 0 directly
 * through their registers.
 *
 * Set this configuration in STM32CubeMX:
 * - SPI1: "Transmit Only Master" (mode 0), with "master keep IO state" on
 * - The pins LCD_CS, LCD_DC, LCD_RST and LCD_BL in output mode
 *
 * Call MX_SPI1_Init() before ST7789_Init(). After ST7789_Init(), only the
 * port uses SPI1. Do not call the HAL_SPI_* functions for SPI1. If one of
 * these conditions occurs, call ST7789_Init() again:
 * - The application calls MX_SPI1_Init() again.
 * - SCK or the clock of the CPU changes (for example, after Stop mode).
 *
 * If a wait loop continues for much longer than the time of one SPI frame,
 * the port starts SPI1 again. SPI1 then has the configuration of this port.
 * This is a precaution: if other code disables SPI1 or changes its
 * configuration, the CPU does not stay in the wait loop. The library then
 * records ST7789_ERR_LINK.
 *
 * Only this file controls DMA1 Stream 0, DMAMUX1 channel 0, the SPI1
 * interrupt and the DMA1_Stream0 interrupt. Do not also enable them in
 * STM32CubeMX. STM32CubeMX does not know this. It gives DMA1 Stream 0 to
 * the first DMA request that you add. It also adds a DMA1_Stream0_IRQHandler
 * that causes an error when the linker makes the executable. Thus, move this
 * DMA request to a different DMA stream in its "DMA Settings" tab.
 *
 * SCK has the value that STM32CubeMX sets. In this project, SCK is 100 MHz.
 * This is the limit of the MCU, and it is more than the rating of the
 * controller (refer to "Speed" in the README).
 *
 * How the port stays fast:
 * - While the library draws, SPI1 operates with 16-bit SPI frames and with
 *   no transfer length (TSIZE = 0, endless mode). Thus, it is not necessary
 *   to stop SPI1 and start it again between bytes. SCK operates only while
 *   the FIFO of the SPI contains data.
 * - The port sends a command as one 16-bit SPI frame: 0x00 and then the
 *   command byte. 0x00 is the NOP command of the controller. Thus, only the
 *   command byte has an effect. The window coordinates and the pixels are
 *   also 16-bit values.
 * - Before DC changes, the port waits for the TXC flag. The SPI sets this
 *   flag when its FIFO is empty and the SPI sent the last bit.
 * - The DMA supplies the data of a background transfer in pieces (a maximum
 *   of 65535 pixels, or one row of a rectangle). The "transfer complete"
 *   interrupt of the DMA starts the next piece. If this start does not occur
 *   immediately, the SPI only stops SCK for a short time. Thus, no data is
 *   missing. After the last piece, the transfer interrupt (the TXC interrupt
 *   of the SPI) tells the library about the end.
 * - The port sends each configuration command (st7789_port_command) in a
 *   different CS cycle, as 8-bit SPI frames. For this, the port stops the
 *   SPI and sets it to 8 bits. Then it sets the SPI to 16-bit endless mode
 *   again. CS is high during the two changes. st7789_port_dma_abort and the
 *   procedure for a stopped SPI also start the SPI again. While the library
 *   draws, the port does not stop the SPI.
 * - If the data cache is on, the port first cleans the cache lines that a
 *   DMA transfer reads. In all conditions, a barrier makes sure that the CPU
 *   wrote all its data to memory before the DMA starts.
 * - The pins and the SPI are on different buses. Each transfer starts with
 *   a change of DC. Then the port reads the GPIO port of DC, and a barrier
 *   follows. Thus, the pin has its new value before the SPI starts to send
 *   data. If CS is on the same GPIO port, it is not necessary to read the
 *   GPIO port again for the change of CS.
 */
#include "../st7789_port.h"
#include "main.h"

/* The interrupt handlers for the vector table (refer to the startup file).
 * They are not `static`. Thus, they replace the `.weak` handlers of the
 * startup file */
void DMA1_Stream0_IRQHandler(void);
void SPI1_IRQHandler(void);

#define LCD_SPI         SPI1
#define LCD_SPI_IRQn    SPI1_IRQn
#define LCD_DMA         DMA1
#define LCD_DMA_STREAM  DMA1_Stream0
#define LCD_DMA_IRQn    DMA1_Stream0_IRQn
#define LCD_DMAMUX      DMAMUX1_Channel0      /* DMAMUX1 channel 0 supplies the DMA
                                                 requests to DMA1 Stream 0 */
#define LCD_DMA_FLAGS   (DMA_LIFCR_CTCIF0 | DMA_LIFCR_CHTIF0 | DMA_LIFCR_CTEIF0 | \
                         DMA_LIFCR_CDMEIF0 | DMA_LIFCR_CFEIF0)

/* The NVIC priority of the SPI1 interrupt and of the DMA1_Stream0 interrupt.
 * It must be possible for these interrupts to preempt the code that calls
 * the library functions. Thus, call these functions only in thread mode or
 * in an interrupt handler with a higher priority value. To change the
 * priority, set ST7789_PORT_IRQ_PRIORITY for all the files that you compile.
 * With CMake, set it as a compile definition of the st7789 target (refer to
 * Lib/st7789/CMakeLists.txt). */
#ifndef ST7789_PORT_IRQ_PRIORITY
#define ST7789_PORT_IRQ_PRIORITY  5
#endif

#define DMA_MAX_ITEMS   0xFFFFu               /* NDTR has 16 bits */

/* The configuration of the DMA stream:
 * - Memory to SPI1 TX
 * - 16 bits on the two sides
 * - DMDIS = 0 in FCR ("direct mode" in RM0433)
 * - High priority
 * - Interrupts for "transfer complete" and "transfer error"
 * For each DMA transfer, the port adds MINC if necessary. */
#define LCD_DMA_CR      (DMA_SxCR_DIR_0 | DMA_SxCR_PSIZE_0 | DMA_SxCR_MSIZE_0 | \
                         DMA_SxCR_PL_1 | DMA_SxCR_TCIE | DMA_SxCR_TEIE)

/* ---- variables for the background transfer ------------------------------ */

static const uint16_t *volatile job_src;      /* the next piece starts here */
static volatile uint32_t job_left;            /* the number of remaining pixels (one area of memory)
                                                 or rows (rectangle) to start */
static volatile uint32_t job_row_w;           /* 0 = a DMA transfer from one area of memory */
static volatile uint32_t job_stride;
static volatile uint8_t  job_increment;
static volatile uint32_t job_cr;              /* CR without EN for each piece of the DMA transfer */

/* When the port writes to the FIFO, TXC goes to 0 after some kernel clock
 * cycles and SCK cycles. This variable contains the maximum number of times
 * that the wait loop reads SR until TXC goes to 0. st7789_port_init
 * calculates it from the clocks that it reads. The CPU reads SR this number
 * of times in approximately the time of one SPI frame. */
static uint32_t txc_drop_spins = 16u;

/* If the wait loop for space in the FIFO reads SR this number of times, the
 * port records a stopped SPI (other code disabled SPI1 or changed its
 * configuration). In correct operation, each wait loop is much shorter. The
 * port does not measure time for this limit. It counts the number of times
 * that it reads SR. Thus, the time in interrupt handlers has no effect on
 * this limit. */
static uint32_t stall_spins = 1024u;
static uint8_t  spi_stalled;          /* SPI1 stopped: the next transfer starts it again first */

#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
static uint32_t dcache_lines;         /* the number of cache lines in the data cache
                                         (st7789_port_init sets this value) */
#endif

/* ---- pins (the port writes BSRR with one atomic operation) -------------- */

/* This function makes sure that a pin change occurs before the next access
 * to the SPI. The CPU reads the GPIO port of the pin, and it gets the value
 * only after the GPIO port changed the pin. The barrier makes sure that the
 * SPI access (on a different bus) does not occur before the CPU gets this
 * value */
static inline void pin_settle(GPIO_TypeDef *port)
{
  (void)port->ODR;
  __DMB();
}

static inline void dc_command(void)
{
  LCD_DC_GPIO_Port->BSRR = (uint32_t)LCD_DC_Pin << 16;
  pin_settle(LCD_DC_GPIO_Port);
}

static inline void dc_data(void)
{
  LCD_DC_GPIO_Port->BSRR = LCD_DC_Pin;
  pin_settle(LCD_DC_GPIO_Port);
}

/* When the port sets CS high, it does not wait for the pin change. The SPI
 * sent the last bit, and no SPI operation uses CS. When the port sets CS
 * low, the DC change at the start of each transfer also completes the CS
 * change. This is applicable when the two pins are on the same GPIO port
 * (GPIOD on this board). */
void st7789_port_select(uint8_t selected)
{
  LCD_CS_GPIO_Port->BSRR = selected ? ((uint32_t)LCD_CS_Pin << 16) : LCD_CS_Pin;
  if (selected && (uintptr_t)LCD_CS_GPIO_Port != (uintptr_t)LCD_DC_GPIO_Port)
  {
    pin_settle(LCD_CS_GPIO_Port);
  }
}

void st7789_port_backlight(uint8_t on)
{
  LCD_BL_GPIO_Port->BSRR = on ? LCD_BL_Pin : ((uint32_t)LCD_BL_Pin << 16);
}

void st7789_port_delay_ms(uint32_t ms) { HAL_Delay(ms); }
uint32_t st7789_port_millis(void)      { return HAL_GetTick(); }

void st7789_port_hw_reset(void)
{
  LCD_RST_GPIO_Port->BSRR = (uint32_t)LCD_RST_Pin << 16;   /* low for a minimum of 10 us */
  HAL_Delay(1);
  LCD_RST_GPIO_Port->BSRR = LCD_RST_Pin;
  HAL_Delay(5);                                           /* 5 ms before the first command */
}

/* ---- SPI ---------------------------------------------------------------- */

/* This function starts SPI1 (or starts it again) in endless mode, with the
 * given number of bits in each SPI frame. The CPU can write CFG1 only while
 * SPE = 0. Thus, each time, the function writes all these values again:
 * - The number of bits in each SPI frame
 * - A FIFO threshold of one SPI frame (FTHLV = 0, which is necessary for
 *   endless mode)
 * - The bit that enables the TX DMA request (TXDMAEN)
 * This is also a precaution if other code changed the configuration of
 * SPI1. */
__STATIC_FORCEINLINE void spi_start_inline(uint32_t bits)
{
  spi_stalled = 0;
  CLEAR_BIT(LCD_SPI->CR1, SPI_CR1_SPE);
  MODIFY_REG(LCD_SPI->CFG1, SPI_CFG1_DSIZE | SPI_CFG1_FTHLV | SPI_CFG1_TXDMAEN,
             ((bits - 1u) << SPI_CFG1_DSIZE_Pos) | SPI_CFG1_TXDMAEN);
  MODIFY_REG(LCD_SPI->CR2, SPI_CR2_TSIZE, 0u);          /* endless mode */
  SET_BIT(LCD_SPI->CR1, SPI_CR1_SPE);
  SET_BIT(LCD_SPI->CR1, SPI_CR1_CSTART);
}

static void spi_start(uint32_t bits)
{
  spi_start_inline(bits);
}

/* The wait loops that follow have a limit and call no functions. Thus, they
 * do not make the fast functions slower. A wait loop that continues for much
 * longer than the time of one SPI frame shows that SPI1 does not operate. For
 * example, other code called HAL_SPI_Init for SPI1 again, or the clock of
 * SPI1 stopped. The function with the wait loop then only records this
 * condition and returns. Thus, the CPU does not stay in the wait loop.
 *
 * Then data16 and repeat16 discard their remaining pixels (some pixels are
 * missing, also if SPI1 operates again during this time). They call
 * spi_recover() before they return (spi_recover() follows the wait loops in
 * this file). Other calls let the next transfer start SPI1 again. The
 * controller did not receive the data that the port sent while SPI1 did not
 * operate. ST7789_ERR_LINK tells the application to draw the pixels again. */
static void spi_recover(void);
#define SPI_RESTART_IF_STALLED()                                  \
  do { if (spi_stalled) { spi_recover(); } } while (0)

/* Each fast function goes to the procedure for a stopped SPI with a tail
 * call to a function with the `cold` attribute. These `cold` functions are
 * cmd_fast_recover, data16_recover and repeat16_recover. Each `cold`
 * function calls spi_recover(). Then it calls its fast function again with
 * the same parameters. Thus, in the usual condition, the fast functions
 * operate without a stack frame */
static void cmd_fast_recover(uint8_t cmd);
static void data16_recover(const uint16_t *src, uint32_t count);
static void repeat16_recover(uint16_t value, uint32_t count);

/* The function returns 0 if the wait loop stopped at its limit */
static inline uint32_t spi_wait_txp(void)
{
  if ((LCD_SPI->SR & SPI_SR_TXP) == 0)   /* usually there is space immediately */
  {
    for (uint32_t n = stall_spins; (LCD_SPI->SR & SPI_SR_TXP) == 0; )
    {
      if (--n == 0u)
      {
        spi_stalled = 1;
        return 0u;
      }
    }
  }
  return 1u;
}

/* The function returns 0 if the SPI stopped. Then the controller does not
 * receive the value */
static inline uint32_t spi_put16(uint16_t value)
{
  uint32_t ok = spi_wait_txp();
  *(__IO uint16_t *)&LCD_SPI->TXDR = value;
  return ok;
}

static inline void spi_put8(uint8_t value)
{
  (void)spi_wait_txp();
  *(__IO uint8_t *)&LCD_SPI->TXDR = value;
}

/* At the start of each call of a fast function, the TX FIFO (space for 8 SPI
 * frames) is empty. Each port function calls spi_wait_sent() or spi_start()
 * at its end. The library calls the fast functions only while no DMA
 * transfer is in progress. Thus, the port writes the first SPI frame without
 * a check. */
static inline void spi_first16(uint16_t value)
{
  *(__IO uint16_t *)&LCD_SPI->TXDR = value;
}

/* TXC goes to 0 some kernel clock cycles and SCK cycles after the port
 * writes to the FIFO. TXC goes to 1 again when the SPI sent the last bit.
 * The port waits for TXC = 0 first, and then for TXC = 1. Thus, the port
 * cannot incorrectly read "done" immediately after it writes to the FIFO. If
 * TXC does not go to 0 in approximately the time of one SPI frame, the SPI
 * sent all the data. */
static inline void spi_wait_txc_drop(void)
{
  for (uint32_t n = 0; (LCD_SPI->SR & SPI_SR_TXC) && n < txc_drop_spins; n++) { }
}

/* This function waits until the SPI sent all the data that the port wrote
 * until this time. The FIFO contains a maximum of 8 SPI frames. Thus, if the
 * wait loop continues for much longer than the time of 8 SPI frames, the SPI
 * stopped. The function returns 0 if the wait loop stopped at its limit. */
static inline uint32_t spi_wait_sent(void)
{
  spi_wait_txc_drop();
  for (uint32_t n = stall_spins * 8u; (LCD_SPI->SR & SPI_SR_TXC) == 0; )
  {
    if (--n == 0u)
    {
      spi_stalled = 1;
      return 0u;
    }
  }
  return 1u;
}

/* This function starts SPI1 again with the configuration of this port. Then
 * it sends a NOP to stop the command that was in progress in the controller.
 * Thus, if the controller did not receive the command for some data, it
 * ignores this data and does not draw it. The library sets its window
 * again.
 *
 * If SPI1 stopped in the middle of a byte, the controller has a part of this
 * byte. When the port sets CS high, the controller discards these bits (if
 * CS goes high before the last bit of a byte, this causes a reset of the
 * serial interface). Thus, the NOP cannot complete this byte as a different
 * command.
 *
 * The function has the `noinline` and `cold` attributes. Thus, it adds no
 * time to the fast functions. */
static void __attribute__((noinline, cold)) spi_recover(void)
{
  uint32_t selected = (LCD_CS_GPIO_Port->ODR & LCD_CS_Pin) == 0u;
  if (selected)
  {
    st7789_port_select(0);
    pin_settle(LCD_CS_GPIO_Port);  /* CS is high before SPI1 starts again */
  }
  spi_start(16);
  if (selected)
  {
    st7789_port_select(1);
  }
  dc_command();
  spi_first16(0x0000u);            /* NOP, NOP */
  spi_wait_sent();
  spi_stalled = 0;                 /* also if SPI1 stopped again: the next wait loop finds this */
  st7789_link_reset();
}

void st7789_port_command(uint8_t cmd, const uint8_t *params, uint32_t len)
{
  SPI_RESTART_IF_STALLED();
  spi_start(8);
  st7789_port_select(1);
  dc_command();
  spi_put8(cmd);
  spi_wait_sent();
  if (len > 0)
  {
    dc_data();
    for (uint32_t i = 0; i < len; i++)
    {
      spi_put8(params[i]);
    }
    spi_wait_sent();
  }
  uint8_t lost = spi_stalled;      /* spi_start sets it to 0 */
  st7789_port_select(0);
  spi_start(16);
  if (lost)
  {
    st7789_link_reset();
  }
}

void st7789_port_cmd_fast(uint8_t cmd)
{
  if (spi_stalled)
  {
    cmd_fast_recover(cmd);
    return;
  }
  dc_command();
  spi_first16(cmd);                /* the high byte is 0x00 (NOP) */
  spi_wait_sent();
}

static void __attribute__((noinline, cold)) cmd_fast_recover(uint8_t cmd)
{
  spi_recover();
  st7789_port_cmd_fast(cmd);       /* spi_recover set spi_stalled to 0 */
}

void st7789_port_data16(const uint16_t *src, uint32_t count)
{
  if (spi_stalled)
  {
    data16_recover(src, count);
    return;
  }
  dc_data();
  uint32_t ok = 1u;
  if (count > 0u)
  {
    spi_first16(*src++);
    while (--count)
    {
      if (!spi_put16(*src++))
      {
        ok = 0u;
        break;
      }
    }
  }
  if (!ok || !spi_wait_sent())
  {
    spi_recover();                 /* SPI1 stopped: the port starts SPI1 again
                                      immediately. Thus, the library records
                                      ST7789_ERR_LINK in this call */
  }
}

void st7789_port_repeat16(uint16_t value, uint32_t count)
{
  if (spi_stalled)
  {
    repeat16_recover(value, count);
    return;
  }
  dc_data();
  uint32_t ok = 1u;
  if (count > 0u)
  {
    spi_first16(value);
    while (--count)
    {
      if (!spi_put16(value))
      {
        ok = 0u;
        break;
      }
    }
  }
  if (!ok || !spi_wait_sent())
  {
    spi_recover();                 /* SPI1 stopped: the port starts SPI1 again
                                      immediately. Thus, the library records
                                      ST7789_ERR_LINK in this call */
  }
}

static void __attribute__((noinline, cold)) data16_recover(const uint16_t *src, uint32_t count)
{
  spi_recover();
  st7789_port_data16(src, count);
}

static void __attribute__((noinline, cold)) repeat16_recover(uint16_t value, uint32_t count)
{
  spi_recover();
  st7789_port_repeat16(value, count);
}

/* ---- DMA ---------------------------------------------------------------- */

uint8_t st7789_port_dma_can_read(const void *src)
{
  uint32_t a = (uint32_t)src;
  if (a < 0x00040000u)                      return 0;   /* ITCM: 64 KB, H72x/H73x: a maximum of 256 KB */
  if (a >= 0x20000000u && a < 0x20020000u)  return 0;   /* DTCM */
  return 1;
}

#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
/* This function cleans the cache lines that contain h rows of w pixels, with
 * a stride of `stride` pixels. Flash memory and ITCM have no changed cache
 * lines. Thus, the function ignores them. If a DMA transfer uses more cache
 * lines than the data cache has, the function cleans the full data cache. */
static void dcache_clean_rect(const uint16_t *src, uint32_t w, uint32_t h, uint32_t stride)
{
  uintptr_t a = (uintptr_t)src;
  if (a < 0x20000000u)
  {
    return;
  }
  uint32_t bytes = w * 2u, step = stride * 2u;
  if (h == 1u || step - bytes < 32u)        /* spaces shorter than a cache line: one range */
  {
    bytes += (h - 1u) * step;
    h = 1u;
  }
  uint32_t per_row = (((uint32_t)a & 31u) + bytes + 31u) / 32u;
  if (h * per_row > dcache_lines)
  {
    SCB_CleanDCache();
    return;
  }
  for (; h > 0u; h--, a += step)
  {
    for (uintptr_t p = a & ~(uintptr_t)31u; p < a + bytes; p += 32u)
    {
      SCB->DCCMVAC = (uint32_t)p;
    }
  }
}
#endif

/* This function starts a piece. It writes the full CR value, and it does not
 * read CR before this (one access less on the AHB bus). The flags of the
 * stream are 0: st7789_port_dma16 sets them to 0 before the first piece. The
 * interrupt handler sets the flags that it read to 0 before it starts the
 * next piece. */
static void dma_go(const uint16_t *src, uint32_t items)
{
  LCD_DMA_STREAM->M0AR = (uint32_t)src;
  LCD_DMA_STREAM->NDTR = items;
  LCD_DMA_STREAM->CR   = job_cr | DMA_SxCR_EN;
}

/* This function starts the next piece of the DMA transfer in progress. It
 * reads each job_* variable only one time */
static void job_next_piece(void)
{
  const uint16_t *src = job_src;
  uint32_t left = job_left, row_w = job_row_w;
  if (row_w == 0u)
  {
    uint32_t n = (left > DMA_MAX_ITEMS) ? DMA_MAX_ITEMS : left;
    job_left = left - n;
    if (job_increment)
    {
      job_src = src + n;
    }
    dma_go(src, n);
  }
  else
  {
    job_left = left - 1u;
    job_src  = src + job_stride;
    dma_go(src, row_w);
  }
}

void st7789_port_dma16(const uint16_t *src, uint32_t w, uint32_t h,
                       uint32_t stride, uint8_t increment)
{
  SPI_RESTART_IF_STALLED();
#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
  if (SCB->CCR & SCB_CCR_DC_Msk)
  {
    if (increment)
    {
      dcache_clean_rect(src, w, h, stride);
    }
    else
    {
      dcache_clean_rect(src, 1u, 1u, 1u);
    }
  }
#endif

  job_cr        = increment ? (LCD_DMA_CR | DMA_SxCR_MINC) : LCD_DMA_CR;
  job_src       = src;
  job_increment = increment;
  job_stride    = stride;
  if (!increment || w == stride || h == 1u)
  {
    job_row_w = 0;              /* one area of memory */
    job_left  = w * h;
  }
  else
  {
    job_row_w = w;              /* rows that are not adjacent in memory */
    job_left  = h;
  }

  __DSB();                      /* the CPU completed its memory accesses to the source */
  dc_data();
  LCD_DMA->LIFCR = LCD_DMA_FLAGS;   /* no remaining flag before the first piece */
  job_next_piece();
}

/* This function stops the stream, and it prevents an unwanted interrupt
 * after this. If the port disables an active stream, the stream sets TCIF.
 * Thus, the function sets TCIE, TEIE and EN to 0 at the same time. Before
 * you call this function, disable the two interrupts. */
static void dma_stop(void)
{
  LCD_SPI->IER = 0;
  CLEAR_BIT(LCD_DMA_STREAM->CR, DMA_SxCR_TCIE | DMA_SxCR_TEIE | DMA_SxCR_EN);
  while (LCD_DMA_STREAM->CR & DMA_SxCR_EN) { }
  LCD_DMA->LIFCR = LCD_DMA_FLAGS;
  job_left = 0;
}

/* The NVIC can set pending bits while the two interrupts are off. This
 * function sets these bits to 0, and then it enables the two interrupts
 * again */
static void irqs_enable(void)
{
  (void)LCD_DMA->LISR;          /* to make sure that the CPU completed the flag and IER changes: */
  (void)LCD_SPI->IER;           /* the CPU cannot set a pending bit to 0 while its signal is high */
  __DSB();
  HAL_NVIC_ClearPendingIRQ(LCD_DMA_IRQn);
  HAL_NVIC_ClearPendingIRQ(LCD_SPI_IRQn);
  HAL_NVIC_EnableIRQ(LCD_DMA_IRQn);
  HAL_NVIC_EnableIRQ(LCD_SPI_IRQn);
}

void st7789_port_dma_abort(void)
{
  HAL_NVIC_DisableIRQ(LCD_DMA_IRQn);
  HAL_NVIC_DisableIRQ(LCD_SPI_IRQn);
  dma_stop();
  spi_start(16);                /* this also makes the FIFO empty */
  LCD_DMA_STREAM->CR = LCD_DMA_CR;
  irqs_enable();
}

/* The DMA sent a piece to the SPI */
void DMA1_Stream0_IRQHandler(void)
{
  /* The handler sets only the flags that it read to 0. Thus, if the stream
     sets TC during this time, TC stays set */
  uint32_t flags = LCD_DMA->LISR & LCD_DMA_FLAGS;
  LCD_DMA->LIFCR = flags;
  if ((flags & (DMA_LISR_TCIF0 | DMA_LISR_TEIF0)) == 0u)
  {
    return;                     /* previous or unwanted interrupt */
  }

  if (flags & DMA_LISR_TEIF0)
  {
    job_left = 0;               /* bus error: the port stops the transfer and then
                                   tells the library about the end as usual */
    st7789_transfer_error();
  }
  if (job_left > 0)
  {
    job_next_piece();
    (void)LCD_DMA_STREAM->CR;   /* the CPU completes the flag change (LIFCR) before the handler returns */
  }
  else
  {
    /* The SPI can be without data when the DMA writes the last piece into
       it. Then, when this handler starts, TXC can stay 1 for a short time.
       Thus, the handler waits until TXC goes to 0 before it arms TXC */
    spi_wait_txc_drop();
    LCD_SPI->IER = SPI_IER_EOTIE;   /* the transfer interrupt occurs when the SPI sent the last bit (TXC) */
  }
}

/* The SPI sent all the data (TXC) */
void SPI1_IRQHandler(void)
{
  if ((LCD_SPI->IER & SPI_IER_EOTIE) && (LCD_SPI->SR & SPI_SR_TXC))
  {
    LCD_SPI->IER = 0;
    st7789_transfer_done();
  }
}

/* ---- configuration ------------------------------------------------------ */

void st7789_port_init(void)
{
  /* The function first disables the two interrupts. Thus, if the
     application calls ST7789_Init() again, the function immediately stops a
     transfer in progress, and no pending bit stays set */
  HAL_NVIC_DisableIRQ(LCD_DMA_IRQn);
  HAL_NVIC_DisableIRQ(LCD_SPI_IRQn);

  /* DMA1 Stream 0 (refer to LCD_DMA_CR) */
  __HAL_RCC_DMA1_CLK_ENABLE();
  dma_stop();
  LCD_DMAMUX->CCR      = DMA_REQUEST_SPI1_TX;
  LCD_DMA_STREAM->CR   = LCD_DMA_CR;
  job_cr               = LCD_DMA_CR;
  LCD_DMA_STREAM->FCR  = 0;
  LCD_DMA_STREAM->PAR  = (uint32_t)&LCD_SPI->TXDR;
  LCD_DMA->LIFCR       = LCD_DMA_FLAGS;

  /* SPI1: 16-bit endless mode.

     The TX DMA request of SPI1 is always enabled. This is different from the
     procedure in RM0433 (SPI chapter, "DMA communication") to start and stop
     a transfer. This procedure enables the DMA request for each transfer.
     The DMA request is only important while the stream is enabled. If the
     port enables and disables the DMA request for each piece, the interrupt
     handler must write to the registers more times between rows.

     When the port enables the stream and the DMA request is active, the
     stream can set its FIFO error flag (FEIF). With DMDIS = 0 ("direct mode"
     in RM0433), this is safe: FEIE stays 0, and for the port, only TEIF
     shows an error. */
  CLEAR_BIT(LCD_SPI->CR1, SPI_CR1_SPE);
  LCD_SPI->IER = 0;
  st7789_port_select(0);
  spi_start(16);

  /* The function calculates the limit of the wait loop for TXC = 0. The
     limit is the number of times that the CPU reads SR in approximately the
     time of one 16-bit SPI frame (18 SCK cycles). This value is correct if
     the CPU uses a minimum of 8 CPU cycles to read SR one time */
  uint32_t ker = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SPI123);   /* 0 if its PLL is off */
  uint32_t div = 2u << ((LCD_SPI->CFG1 & SPI_CFG1_MBR) >> SPI_CFG1_MBR_Pos);
  if (ker != 0u)
  {
    uint32_t spins = (uint32_t)(((uint64_t)SystemCoreClock * div * 18u) / ((uint64_t)ker * 8u));
    txc_drop_spins = (spins > 16u) ? spins : 16u;
  }
  /* The limit for a stopped SPI: the wait loop reads SR for the time of 64
     SPI frames, and a minimum of 1024 times */
  stall_spins = (64u * txc_drop_spins > 1024u) ? 64u * txc_drop_spins : 1024u;

#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
  SCB->CSSELR = 0u;                      /* the data cache (level 1) */
  __DSB();
  uint32_t ccsidr = SCB->CCSIDR;
  dcache_lines = (CCSIDR_SETS(ccsidr) + 1u) * (CCSIDR_WAYS(ccsidr) + 1u);
#endif

  HAL_NVIC_SetPriority(LCD_DMA_IRQn, ST7789_PORT_IRQ_PRIORITY, 0);
  HAL_NVIC_SetPriority(LCD_SPI_IRQn, ST7789_PORT_IRQ_PRIORITY, 0);
  irqs_enable();
}

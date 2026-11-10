/**
 * The hardware functions that are necessary for the ST7789 library.
 *
 * For one board, one port file contains these functions. All the other files
 * of the library are C11 (they use GCC attributes if the compiler has them).
 * port/st7789_port_stm32h7.c is the port for the STM32H7.
 *
 * The format on the SPI bus: the controller reads a byte as a command when DC
 * is low, and as data when DC is high. The library keeps CS low from the
 * start to the end of a group of commands and data. The 16-bit values that
 * the library gives to data16, repeat16 and dma16 are pixels and the
 * CASET/RASET coordinates. The port must send the high byte of each of these
 * values first.
 *
 * The library keeps the pixels as uint16_t values in the byte sequence of the
 * CPU (the color of a filled rectangle, the text buffers, the framebuffer and
 * the images of the application). It gives these buffers to dma16 without a
 * change. Thus:
 * - Some DMAs send the bytes in the sequence of their addresses (for
 *   example, an 8-bit SPI DMA). If the MCU has the low byte at the lower
 *   address, these DMAs send the low byte first. In this condition, the port
 *   must interchange the 2 bytes of each pixel in a bounce buffer.
 * - Some DMAs do not have a mode with a constant memory address. For
 *   `increment == 0`, a port with this type of DMA must use a small buffer
 *   with copies of the pixel.
 *
 * The contexts of the calls: the library also calls select, cmd_fast, data16
 * and dma16 from st7789_transfer_done(). These calls occur in the transfer
 * interrupt of the port, when a flush continues with its next changed area.
 * Thus, these functions must be safe in an interrupt handler, and they must
 * wait only in wait loops (no RTOS functions that wait, no mutexes and no HAL
 * functions that use `__HAL_LOCK()`).
 *
 * The library calls all the other port functions only in the context of the
 * application. In this context, the library waits for the end of a transfer
 * in progress before it calls the port again. But in these three conditions,
 * the library does not wait for the end of a transfer in progress:
 * - st7789_port_backlight: ST7789_Backlight does not wait. Thus, the transfer
 *   interrupt can call select() while st7789_port_backlight operates. The
 *   port must write the pins with atomic operations (for example, with BSRR).
 * - The procedure after a timeout: the library calls dma_abort and then
 *   select(0).
 * - st7789_port_init: it must stop a transfer that is in progress.
 *
 * It must be possible for the interrupts of the port and of its millisecond
 * counter to preempt the application.
 *
 * You can write a port in C++: this header file gives C linkage to the
 * functions. The interrupt handlers of the port must also use `extern "C"`.
 * If they do not use it, they do not replace the `.weak` handlers of the
 * startup file. Thus, the first interrupt goes to Default_Handler.
 */
#ifndef ST7789_PORT_H
#define ST7789_PORT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Prepare the SPI, the DMA and the interrupts. Also stop a transfer that is
 * in progress. The pins have their configuration before this call. */
void     st7789_port_init(void);

/* Set the reset pin low for a minimum of 10 us. Then set it high. Then wait
 * for 5 ms. The library waits for the 120 ms that are necessary for the
 * controller before Sleep Out. If a port does not have a reset pin, this
 * function can be empty. */
void     st7789_port_hw_reset(void);

/* Wait for `ms` milliseconds (ST7789_Init and ST7789_Sleep use it) */
void     st7789_port_delay_ms(uint32_t ms);

/* Return the value of the millisecond counter (for example, HAL_GetTick()).
 * The initial value of the counter is not important. The value is a uint32_t
 * that increases continuously and goes back to 0 (modulo 2^32). If your
 * counter has less than 32 bits, extend it to 32 bits. The counter must
 * continue to increase while the library waits for a transfer. The library
 * uses it to measure the timeout (ST7789_IDLE_TIMEOUT_MS) and the intervals
 * before and after Sleep Out. */
uint32_t st7789_port_millis(void);

void     st7789_port_backlight(uint8_t on);
void     st7789_port_select(uint8_t selected); /* CS is low if selected != 0 */

/* Send a command and all its parameter bytes (0 or more) in a CS cycle that
 * contains only this command. Return only after the end of the CS cycle. The
 * library uses this function for configuration commands. Thus, speed is not
 * important. */
void     st7789_port_command(uint8_t cmd, const uint8_t *params, uint32_t len);

/* The library calls these fast functions only when CS is low (selected) and
 * no background transfer is in progress. Each call returns after the SPI sent
 * the last bit of its data through the pin (a wait loop). The parameter
 * `count` can be the number of pixels of the full screen (76,800 pixels on a
 * 240x320 panel). If the SPI driver uses a 16-bit length (for example,
 * HAL_SPI_Transmit), divide the transfer into smaller parts. CS and DC must
 * be correct at the pins before the next change of SCK: */
void     st7789_port_cmd_fast(uint8_t cmd);                     /* DC low */
void     st7789_port_data16(const uint16_t *src, uint32_t count); /* DC high */
void     st7789_port_repeat16(uint16_t value, uint32_t count);  /* DC high */

/* If dma16 can do a DMA transfer that starts at this address, return 1. If
 * this function always returns 0, the library does not call dma16. */
uint8_t  st7789_port_dma_can_read(const void *src);

/* Start to send `h` rows of `w` pixels in a background transfer. The stride
 * of the rows is `stride` pixels. If `increment == 0`, send `w*h` copies of
 * the pixel at `src`.
 * - CS is low (selected), and DC is low (the library sent RAMWR immediately
 *   before this call). First, set DC high.
 * - `w` and `h` are each 320 or less. But `w*h` can be as large as the full
 *   screen (76,800 pixels), also when `increment == 0`. Divide the transfer
 *   into parts that are not larger than the limit of the DMA counter.
 * - `src` is only 2-byte aligned, and `stride` can be an odd number. All the
 *   pixels of the transfer are in one variable or in one buffer. The CPU
 *   wrote them a short time before this call. If there is a data cache,
 *   first clean their cache lines (only one pixel if `increment` is 0).
 * - Set all the variables before you start the hardware. The call to
 *   st7789_transfer_done(), and a call from it to dma16(), can occur before
 *   this call returns.
 * - When the SPI sent the last bit through the pin, call
 *   st7789_transfer_done() one time only. Also do this after a DMA error. If
 *   you do not call it, the next call to the library waits for
 *   ST7789_IDLE_TIMEOUT_MS and then calls st7789_port_dma_abort(). Before you
 *   call st7789_transfer_done(), disarm the transfer interrupt or set its flag
 *   to 0. This is necessary because st7789_transfer_done() can start the next
 *   DMA transfer and arm the interrupt again.
 *
 * A port without DMA can send the pixels in this function and call
 * st7789_transfer_done() before this function returns. In this condition,
 * during a flush, st7789_transfer_done() calls dma16() again for each area
 * before the previous call returns. Thus, a maximum of ST7789_MAX_DIRTY_RECTS
 * of these calls are on the stack at the same time. */
void     st7789_port_dma16(const uint16_t *src, uint32_t w, uint32_t h,
                           uint32_t stride, uint8_t increment);

/* Stop a background transfer (the library uses this function when a transfer
 * does not complete). After this function returns, no call to
 * st7789_transfer_done() or st7789_transfer_error() follows for the stopped
 * transfer. */
void     st7789_port_dma_abort(void);

/* ---- library functions that the port calls ------------------------------ */

/* When the SPI sent the last bit of a DMA transfer (one dma16 call) through
 * the pin, call this function one time. Call it from the transfer interrupt
 * of the port, or let a different context do the call (for example, PendSV or
 * a task with a high priority). That context must have a higher priority than
 * the application. The interrupt that starts the next part of a DMA transfer
 * and the transfer interrupt must not preempt each other. The STM32H7 port
 * gives the two interrupts the same priority. */
void     st7789_transfer_done(void);

/* If a bus error occurs during a background transfer, call this function
 * from the same context. Then call st7789_transfer_done(), as for each
 * transfer. */
void     st7789_transfer_error(void);

/* After the port finds a stopped SPI and starts it again, call this function
 * in the context of the transfer. The controller possibly did not receive the
 * data that the port sent while the SPI did not operate. The library then
 * sets its window again and records ST7789_ERR_LINK. If a port does not start
 * its SPI again, it is not necessary to call this function. */
void     st7789_link_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* ST7789_PORT_H */

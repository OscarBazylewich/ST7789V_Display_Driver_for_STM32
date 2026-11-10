/**
 * Test for the ST7789 HAL library (`Lib/st7789_hal`).
 *
 * The test uses SPI1 (hspi1 from STM32CubeMX) and the pins LCD_CS, LCD_DC,
 * LCD_RST and LCD_BL of `main.h`. It sends its results to the ST-LINK
 * virtual COM port with `printf()`.
 *
 * If HAL_DEMO_USE_DMA is 1 (the default), the test prepares DMA2 Stream 0
 * for the transmit DMA requests of SPI1. If HAL_DEMO_USE_DMA is 0, the
 * library uses only HAL_SPI_Transmit.
 */
#ifndef HAL_DEMO_H
#define HAL_DEMO_H

#ifdef __cplusplus
extern "C" {
#endif

/* Call this function one time, after MX_SPI1_Init and BSP_COM_Init. It
 * calls ST7789HAL_Init. Then it shows the test pattern for 5 seconds. */
void HalDemo_Start(void);

/* Call this function in the loop in main(). Each call does one step of the
 * scene at this time and then returns. */
void HalDemo_Update(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_DEMO_H */

/**
 * Benchmark for the two ST7789 libraries (`Lib/st7789` and
 * `Lib/st7789_hal`).
 *
 * The benchmark does the same operations with the two libraries. Thus, you
 * can compare the speeds. The compile definition USE_ST7789_HAL selects the
 * library. The benchmark uses SPI1 (hspi1 from STM32CubeMX) and the pins
 * LCD_CS, LCD_DC, LCD_RST and LCD_BL of `main.h`. It sends its results to
 * the ST-LINK virtual COM port with `printf()`.
 */
#ifndef LCD_BENCH_H
#define LCD_BENCH_H

#ifdef __cplusplus
extern "C" {
#endif

/* Call this function one time, after MX_SPI1_Init and BSP_COM_Init. It
 * calls ST7789_Init or ST7789HAL_Init. Then it starts the first scene. */
void LcdBench_Start(void);

/* Call this function in the loop in main(). Each call does one step of the
 * scene at this time and then returns. */
void LcdBench_Update(void);

#ifdef __cplusplus
}
#endif

#endif /* LCD_BENCH_H */

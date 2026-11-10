/**
 * This is the usual 5 x 7 pixel font for the ASCII characters from ' ' to
 * '~' (0x20 to 0x7E).
 *
 * For each character, the font gives 5 columns of 8 bits. Bit 0 is the top
 * row, and bit 6 is the bottom row. The font does not use bit 7.
 */
#ifndef ST7789_FONT5X7_H
#define ST7789_FONT5X7_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FONT5X7_FIRST_CHAR  ' '
#define FONT5X7_LAST_CHAR   '~'
#define FONT5X7_GLYPH_W     5
#define FONT5X7_GLYPH_H     7

extern const uint8_t st7789_font5x7[FONT5X7_LAST_CHAR - FONT5X7_FIRST_CHAR + 1][FONT5X7_GLYPH_W];

#ifdef __cplusplus
}
#endif

#endif /* ST7789_FONT5X7_H */

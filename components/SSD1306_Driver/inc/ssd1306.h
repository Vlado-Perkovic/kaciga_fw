#ifndef SSD1306_H
#define SSD1306_H 100

/* C++ detection */
#ifdef __cplusplus
extern "C" {
#endif

#include "fonts.h"  // For FontDef_t
#include "stdlib.h" // For NULL, size_t (used by string.h indirectly)
#include "string.h" // For memset, strlen
#include <stdint.h> // For uint8_t, uint16_t etc.

/* I2C address and GPIO Configuration */
// These are used by ssd1306.c to initialize the I2C peripheral
#define I2C_MASTER_SDA_IO   26
#define I2C_MASTER_SCL_IO   27
#define I2C_MASTER_FREQ_HZ  100000 // 100kHz, common for SSD1306. Can be 400000 for faster.

#define SSD1306_I2C_ADDR    0x78 // 8-bit address (0x3C for 7-bit)

/* SSD1306 settings */
/* SSD1306 width in pixels */
#ifndef SSD1306_WIDTH
#define SSD1306_WIDTH       128
#endif
/* SSD1306 LCD height in pixels */
#ifndef SSD1306_HEIGHT
#define SSD1306_HEIGHT      64
#endif

/**
 * @brief  SSD1306 color enumeration
 */
typedef enum {
	SSD1306_COLOR_BLACK = 0x00, /*!< Black color, no pixel */
	SSD1306_COLOR_WHITE = 0x01  /*!< Pixel is set. Color depends on LCD */
} SSD1306_COLOR_t;


/* Public API Functions for SSD1306 Display Control */

/**
 * @brief  Initializes SSD1306 LCD (including its I2C communication).
 * @param  None
 * @retval Initialization status:
 * - 0: LCD initialization failed (e.g., I2C setup or device not detected)
 * - 1: LCD initialized OK and ready to use
 */
uint8_t SSD1306_Init(void);

/** * @brief  Updates buffer from internal RAM to LCD.
 * @note   This function must be called each time you do some changes to LCD,
 * to transfer the buffer from RAM to the LCD.
 * @param  None
 * @retval None
 */
void SSD1306_UpdateScreen(void);

/**
 * @brief  Toggles pixels inversion inside internal RAM.
 * @note   SSD1306_UpdateScreen() must be called after that in order to see updated LCD screen.
 * @param  None
 * @retval None
 */
void SSD1306_ToggleInvert(void);

/** * @brief  Fills entire LCD with desired color.
 * @note   SSD1306_UpdateScreen() must be called after that in order to see updated LCD screen.
 * @param  Color: Color to be used for screen fill. This parameter can be a value of @ref SSD1306_COLOR_t enumeration.
 * @retval None
 */
void SSD1306_Fill(SSD1306_COLOR_t Color);

/**
 * @brief  Draws pixel at desired location.
 * @note   SSD1306_UpdateScreen() must called after that in order to see updated LCD screen.
 * @param  x: X location. This parameter can be a value between 0 and SSD1306_WIDTH - 1.
 * @param  y: Y location. This parameter can be a value between 0 and SSD1306_HEIGHT - 1.
 * @param  color: Color to be used for screen fill. This parameter can be a value of @ref SSD1306_COLOR_t enumeration.
 * @retval None
 */
void SSD1306_DrawPixel(uint16_t x, uint16_t y, SSD1306_COLOR_t color);

/**
 * @brief  Sets cursor pointer to desired location for strings.
 * @param  x: X location. This parameter can be a value between 0 and SSD1306_WIDTH - 1.
 * @param  y: Y location. This parameter can be a value between 0 and SSD1306_HEIGHT - 1.
 * @retval None
 */
void SSD1306_GotoXY(uint16_t x, uint16_t y);

/**
 * @brief  Puts character to internal RAM.
 * @note   SSD1306_UpdateScreen() must be called after that in order to see updated LCD screen.
 * @param  ch: Character to be written.
 * @param  Font: Pointer to @ref FontDef_t structure with used font.
 * @param  color: Color used for drawing. This parameter can be a value of @ref SSD1306_COLOR_t enumeration.
 * @retval Character written.
 */
char SSD1306_Putc(char ch, FontDef_t* Font, SSD1306_COLOR_t color);

/**
 * @brief  Puts string to internal RAM.
 * @note   SSD1306_UpdateScreen() must be called after that in order to see updated LCD screen.
 * @param  str: String to be written.
 * @param  Font: Pointer to @ref FontDef_t structure with used font.
 * @param  color: Color used for drawing. This parameter can be a value of @ref SSD1306_COLOR_t enumeration.
 * @retval Zero on success or character value when function failed.
 */
char SSD1306_Puts(char* str, FontDef_t* Font, SSD1306_COLOR_t color);

/**
 * @brief  Draws line on LCD.
 * @note   SSD1306_UpdateScreen() must be called after that in order to see updated LCD screen.
 * @param  x0: Line X start point. Valid input is 0 to SSD1306_WIDTH - 1.
 * @param  y0: Line Y start point. Valid input is 0 to SSD1306_HEIGHT - 1.
 * @param  x1: Line X end point. Valid input is 0 to SSD1306_WIDTH - 1.
 * @param  y1: Line Y end point. Valid input is 0 to SSD1306_HEIGHT - 1.
 * @param  c: Color to be used. This parameter can be a value of @ref SSD1306_COLOR_t enumeration.
 * @retval None
 */
void SSD1306_DrawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, SSD1306_COLOR_t c);

/**
 * @brief  Draws rectangle on LCD.
 * @note   SSD1306_UpdateScreen() must be called after that in order to see updated LCD screen.
 * @param  x: Top left X start point. Valid input is 0 to SSD1306_WIDTH - 1.
 * @param  y: Top left Y start point. Valid input is 0 to SSD1306_HEIGHT - 1.
 * @param  w: Rectangle width in units of pixels.
 * @param  h: Rectangle height in units of pixels.
 * @param  c: Color to be used. This parameter can be a value of @ref SSD1306_COLOR_t enumeration.
 * @retval None
 */
void SSD1306_DrawRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, SSD1306_COLOR_t c);

/**
 * @brief  Draws filled rectangle on LCD.
 * @note   SSD1306_UpdateScreen() must be called after that in order to see updated LCD screen.
 * @param  x: Top left X start point. Valid input is 0 to SSD1306_WIDTH - 1.
 * @param  y: Top left Y start point. Valid input is 0 to SSD1306_HEIGHT - 1.
 * @param  w: Rectangle width in units of pixels.
 * @param  h: Rectangle height in units of pixels.
 * @param  c: Color to be used. This parameter can be a value of @ref SSD1306_COLOR_t enumeration.
 * @retval None
 */
void SSD1306_DrawFilledRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, SSD1306_COLOR_t c);

/**
 * @brief  Draws triangle on LCD.
 * @note   SSD1306_UpdateScreen() must be called after that in order to see updated LCD screen.
 * @param  x1: First coordinate X location. Valid input is 0 to SSD1306_WIDTH - 1.
 * @param  y1: First coordinate Y location. Valid input is 0 to SSD1306_HEIGHT - 1.
 * @param  x2: Second coordinate X location. Valid input is 0 to SSD1306_WIDTH - 1.
 * @param  y2: Second coordinate Y location. Valid input is 0 to SSD1306_HEIGHT - 1.
 * @param  x3: Third coordinate X location. Valid input is 0 to SSD1306_WIDTH - 1.
 * @param  y3: Third coordinate Y location. Valid input is 0 to SSD1306_HEIGHT - 1.
 * @param  color: Color to be used. This parameter can be a value of @ref SSD1306_COLOR_t enumeration.
 * @retval None
 */
void SSD1306_DrawTriangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3, SSD1306_COLOR_t color);

/**
 * @brief  Draws circle to internal buffer.
 * @note   SSD1306_UpdateScreen() must be called after that in order to see updated LCD screen.
 * @param  x0: X location for center of circle. Valid input is 0 to SSD1306_WIDTH - 1.
 * @param  y0: Y location for center of circle. Valid input is 0 to SSD1306_HEIGHT - 1.
 * @param  r: Circle radius in units of pixels.
 * @param  c: Color to be used. This parameter can be a value of @ref SSD1306_COLOR_t enumeration.
 * @retval None
 */
void SSD1306_DrawCircle(int16_t x0, int16_t y0, int16_t r, SSD1306_COLOR_t c);

/**
 * @brief  Draws filled circle to internal buffer.
 * @note   SSD1306_UpdateScreen() must be called after that in order to see updated LCD screen.
 * @param  x0: X location for center of circle. Valid input is 0 to SSD1306_WIDTH - 1.
 * @param  y0: Y location for center of circle. Valid input is 0 to SSD1306_HEIGHT - 1.
 * @param  r: Circle radius in units of pixels.
 * @param  c: Color to be used. This parameter can be a value of @ref SSD1306_COLOR_t enumeration.
 * @retval None
 */
void SSD1306_DrawFilledCircle(int16_t x0, int16_t y0, int16_t r, SSD1306_COLOR_t c);

/**
 * @brief  Draws the Bitmap.
 * @param  x:  X location to start the Drawing.
 * @param  y:  Y location to start the Drawing.
 * @param  bitmap : Pointer to the bitmap data (expects 1 bit per pixel).
 * @param  w : width of the image.
 * @param  h : Height of the image.
 * @param  color : Color for '1' bits in bitmap (SSD1306_COLOR_WHITE or SSD1306_COLOR_BLACK).
 * @retval None
 */
void SSD1306_DrawBitmap(int16_t x, int16_t y, const unsigned char* bitmap, int16_t w, int16_t h, uint16_t color);


// --- Scrolling Functions ---
void SSD1306_ScrollRight(uint8_t start_row, uint8_t end_row);
void SSD1306_ScrollLeft(uint8_t start_row, uint8_t end_row);
void SSD1306_Scrolldiagright(uint8_t start_row, uint8_t end_row);
void SSD1306_Scrolldiagleft(uint8_t start_row, uint8_t end_row);
void SSD1306_Stopscroll(void);

/**
 * @brief Inverts the display.
 * @param i: 1 to invert, 0 for normal display.
 */
void SSD1306_InvertDisplay (int i);

/**
 * @brief Clears the display buffer (fills with black).
 * @note  SSD1306_UpdateScreen() must be called to see the change on the display.
 */
void SSD1306_Clear (void);

/**
 * @brief Turns the SSD1306 display panel ON.
 * (Sends display ON command and enables charge pump)
 */
void SSD1306_ON(void);

/**
 * @brief Turns the SSD1306 display panel OFF.
 * (Sends display OFF command and may disable charge pump)
 */
void SSD1306_OFF(void);


/* C++ detection */
#ifdef __cplusplus
}
#endif

#endif // SSD1306_H
// ```
//
// **Key changes made in this `ssd1306.h`:**
//
// * Removed the declarations for `ssd1306_I2C_Init`, `ssd1306_I2C_Write`, and `ssd1306_I2C_WriteMulti` as these are now internal to `ssd1306.c`.
// * Removed the `#include "driver/i2c.h"` as it's no longer needed for the public API. The new `driver/i2c_master.h` is included within `ssd1306.c`.
// * Added `<stdint.h>` for explicit integer types like `uint8_t`, `uint16_t`, which is good practice.
// * The I2C configuration macros (`I2C_MASTER_SDA_IO`, etc.) are kept as they are essential for configuring how this driver component initializes I2C.
// * The public function `SSD1306_Init(void)` remains, and its implementation in `ssd1306.c` now handles the setup using the `i2c_master` API.
// * The Doxygen comments for the removed I2C functions were also removed. The comment for `SSD1306_Init` was slightly updated to reflect it handles I2C setup.
// * Removed the `ssd1306_I2C_TIMEOUT` macro as the timeout is now handled within the `i2c_master_transmit` calls in `ssd1306.c`.
//
// This revised header file is cleaner and only exposes the necessary public API for controlling the SSD1306 displ

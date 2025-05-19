#include "ssd1306.h"
#include "driver/i2c_master.h" // Modern ESP-IDF I2C master driver
#include "esp_log.h"           // For ESP_LOGx logging
uint8_t ssd1306_I2C_Init();

void ssd1306_I2C_Write(uint8_t control_byte, uint8_t data_byte);
void ssd1306_I2C_WriteMulti(uint8_t control_byte, uint8_t* data_buffer, uint16_t count);
// --- Macros for I2C communication ---
// Updated macros to reflect new function signatures for ssd1306_I2C_Write
#define SSD1306_WRITECOMMAND(command)      ssd1306_I2C_Write(0x00, (command)) // 0x00 for command
#define SSD1306_WRITEDATA(data)            ssd1306_I2C_Write(0x40, (data))    // 0x40 for data

/* Absolute value */
#define ABS(x)   ((x) > 0 ? (x) : -(x))

/* SSD1306 data buffer */
static uint8_t SSD1306_Buffer[SSD1306_WIDTH * SSD1306_HEIGHT / 8];

/* Private SSD1306 structure */
typedef struct {
	uint16_t CurrentX;
	uint16_t CurrentY;
	uint8_t Inverted;
	uint8_t Initialized;
} SSD1306_t;

/* Private variable */
static SSD1306_t SSD1306;

// --- I2C Master Device Handle ---
static i2c_master_dev_handle_t g_ssd1306_dev_handle = NULL;
static const char *I2C_TAG = "SSD1306_I2C"; // Tag for logging

#define SSD1306_RIGHT_HORIZONTAL_SCROLL              0x26
#define SSD1306_LEFT_HORIZONTAL_SCROLL               0x27
#define SSD1306_VERTICAL_AND_RIGHT_HORIZONTAL_SCROLL 0x29
#define SSD1306_VERTICAL_AND_LEFT_HORIZONTAL_SCROLL  0x2A
#define SSD1306_DEACTIVATE_SCROLL                    0x2E // Stop scroll
#define SSD1306_ACTIVATE_SCROLL                      0x2F // Start scroll
#define SSD1306_SET_VERTICAL_SCROLL_AREA             0xA3 // Set scroll range

#define SSD1306_NORMALDISPLAY       0xA6
#define SSD1306_INVERTDISPLAY       0xA7


void SSD1306_ScrollRight(uint8_t start_row, uint8_t end_row)
{
  SSD1306_WRITECOMMAND (SSD1306_RIGHT_HORIZONTAL_SCROLL);  // send 0x26
  SSD1306_WRITECOMMAND (0x00);  // send dummy
  SSD1306_WRITECOMMAND(start_row);  // start page address
  SSD1306_WRITECOMMAND(0X00);  // time interval 5 frames
  SSD1306_WRITECOMMAND(end_row);  // end page address
  SSD1306_WRITECOMMAND(0X00);
  SSD1306_WRITECOMMAND(0XFF);
  SSD1306_WRITECOMMAND (SSD1306_ACTIVATE_SCROLL); // start scroll
}


void SSD1306_ScrollLeft(uint8_t start_row, uint8_t end_row)
{
  SSD1306_WRITECOMMAND (SSD1306_LEFT_HORIZONTAL_SCROLL);  // send 0x26
  SSD1306_WRITECOMMAND (0x00);  // send dummy
  SSD1306_WRITECOMMAND(start_row);  // start page address
  SSD1306_WRITECOMMAND(0X00);  // time interval 5 frames
  SSD1306_WRITECOMMAND(end_row);  // end page address
  SSD1306_WRITECOMMAND(0X00);
  SSD1306_WRITECOMMAND(0XFF);
  SSD1306_WRITECOMMAND (SSD1306_ACTIVATE_SCROLL); // start scroll
}


void SSD1306_Scrolldiagright(uint8_t start_row, uint8_t end_row)
{
  SSD1306_WRITECOMMAND(SSD1306_SET_VERTICAL_SCROLL_AREA);  // sect the area
  SSD1306_WRITECOMMAND (0x00);   // write dummy
  SSD1306_WRITECOMMAND(SSD1306_HEIGHT);

  SSD1306_WRITECOMMAND(SSD1306_VERTICAL_AND_RIGHT_HORIZONTAL_SCROLL);
  SSD1306_WRITECOMMAND (0x00);
  SSD1306_WRITECOMMAND(start_row);
  SSD1306_WRITECOMMAND(0X00);
  SSD1306_WRITECOMMAND(end_row);
  SSD1306_WRITECOMMAND (0x01);
  SSD1306_WRITECOMMAND (SSD1306_ACTIVATE_SCROLL);
}


void SSD1306_Scrolldiagleft(uint8_t start_row, uint8_t end_row)
{
  SSD1306_WRITECOMMAND(SSD1306_SET_VERTICAL_SCROLL_AREA);  // sect the area
  SSD1306_WRITECOMMAND (0x00);   // write dummy
  SSD1306_WRITECOMMAND(SSD1306_HEIGHT);

  SSD1306_WRITECOMMAND(SSD1306_VERTICAL_AND_LEFT_HORIZONTAL_SCROLL);
  SSD1306_WRITECOMMAND (0x00);
  SSD1306_WRITECOMMAND(start_row);
  SSD1306_WRITECOMMAND(0X00);
  SSD1306_WRITECOMMAND(end_row);
  SSD1306_WRITECOMMAND (0x01);
  SSD1306_WRITECOMMAND (SSD1306_ACTIVATE_SCROLL);
}


void SSD1306_Stopscroll(void)
{
	SSD1306_WRITECOMMAND(SSD1306_DEACTIVATE_SCROLL);
}



void SSD1306_InvertDisplay (int i)
{
  if (i) SSD1306_WRITECOMMAND (SSD1306_INVERTDISPLAY);
  else SSD1306_WRITECOMMAND (SSD1306_NORMALDISPLAY);
}


void SSD1306_DrawBitmap(int16_t x, int16_t y, const unsigned char* bitmap, int16_t w, int16_t h, uint16_t color)
{
    int16_t byteWidth = (w + 7) / 8; // Bitmap scanline pad = whole byte
    uint8_t byte = 0;

    for(int16_t j=0; j<h; j++, y++)
    {
        for(int16_t i=0; i<w; i++)
        {
            if(i & 7)
            {
               byte <<= 1;
            }
            else
            {
               byte = (*(const unsigned char *)(&bitmap[j * byteWidth + i / 8]));
            }
			uint8_t tmp = byte & 0x80; // Check MSB
            if( tmp ) SSD1306_DrawPixel(x+i, y, (SSD1306_COLOR_t)color);
            // else: if you want to draw the background color, add SSD1306_DrawPixel(x+i, y, (SSD1306_COLOR_t)!color);
        }
    }
}


uint8_t SSD1306_Init(void) {
	/* Init I2C */
	if (!ssd1306_I2C_Init()) {
        ESP_LOGE(I2C_TAG, "I2C initialization failed for SSD1306");
        return 0; // Indicate failure
    }

	/* A little delay */
	uint32_t p = 250000; // Increased delay slightly for stability after I2C init
	while(p>0)
		p--;
	
	/* Init LCD */
	SSD1306_WRITECOMMAND(0xAE); //display off
	SSD1306_WRITECOMMAND(0x20); //Set Memory Addressing Mode   
	SSD1306_WRITECOMMAND(0x10); //00,Horizontal Addressing Mode;01,Vertical Addressing Mode;10,Page Addressing Mode (RESET);11,Invalid
	SSD1306_WRITECOMMAND(0xB0); //Set Page Start Address for Page Addressing Mode,0-7
	SSD1306_WRITECOMMAND(0xC8); //Set COM Output Scan Direction
	SSD1306_WRITECOMMAND(0x00); //---set low column address
	SSD1306_WRITECOMMAND(0x10); //---set high column address
	SSD1306_WRITECOMMAND(0x40); //--set start line address
	SSD1306_WRITECOMMAND(0x81); //--set contrast control register
	SSD1306_WRITECOMMAND(0xFF); // Max contrast
	SSD1306_WRITECOMMAND(0xA1); //--set segment re-map 0 to 127 (POR = 0xA0)
	SSD1306_WRITECOMMAND(0xA6); //--set normal display (POR)
	SSD1306_WRITECOMMAND(0xA8); //--set multiplex ratio(1 to 64)
	SSD1306_WRITECOMMAND(0x3F); // 1/64 duty (POR) for 64-line display
	SSD1306_WRITECOMMAND(0xA4); //0xa4,Output follows RAM content;0xa5,Output ignores RAM content (POR = 0xA4)
	SSD1306_WRITECOMMAND(0xD3); //-set display offset
	SSD1306_WRITECOMMAND(0x00); //-not offset (POR)
	SSD1306_WRITECOMMAND(0xD5); //--set display clock divide ratio/oscillator frequency
	SSD1306_WRITECOMMAND(0xF0); // values from 00 to FF (POR = 0x80)
	SSD1306_WRITECOMMAND(0xD9); //--set pre-charge period
	SSD1306_WRITECOMMAND(0x22); // values from 00 to FF (POR = 0x22)
	SSD1306_WRITECOMMAND(0xDA); //--set com pins hardware configuration
	SSD1306_WRITECOMMAND(0x12); // (POR = 0x12 for 64-line display)
	SSD1306_WRITECOMMAND(0xDB); //--set vcomh
	SSD1306_WRITECOMMAND(0x20); //0x20,0.77xVcc (POR = 0x20)
	SSD1306_WRITECOMMAND(0x8D); //--set DC-DC enable
	SSD1306_WRITECOMMAND(0x14); //
	SSD1306_WRITECOMMAND(0xAF); //--turn on SSD1306 panel
	
	SSD1306_WRITECOMMAND(SSD1306_DEACTIVATE_SCROLL);

	/* Clear screen */
	SSD1306_Fill(SSD1306_COLOR_BLACK);
	
	/* Update screen */
	SSD1306_UpdateScreen();
	
	/* Set default values */
	SSD1306.CurrentX = 0;
	SSD1306.CurrentY = 0;
	SSD1306.Inverted = 0;
	
	/* Initialized OK */
	SSD1306.Initialized = 1;
	
	ESP_LOGI(I2C_TAG, "SSD1306 Panel Initialized");
	return 1; // Indicate success
}

void SSD1306_UpdateScreen(void) {
	if (!SSD1306.Initialized || g_ssd1306_dev_handle == NULL) return;
	uint8_t m;
	
	for (m = 0; m < 8; m++) { // 8 pages for 64 rows
		SSD1306_WRITECOMMAND(0xB0 + m); // Set page address
		SSD1306_WRITECOMMAND(0x00);     // Set lower column start address
		SSD1306_WRITECOMMAND(0x10);     // Set higher column start address
		
		/* Write multi data */
		// The control byte 0x40 indicates data stream
		ssd1306_I2C_WriteMulti(0x40, &SSD1306_Buffer[SSD1306_WIDTH * m], SSD1306_WIDTH);
	}
}

void SSD1306_ToggleInvert(void) {
	uint16_t i;
	
	/* Toggle invert */
	SSD1306.Inverted = !SSD1306.Inverted;
	
	/* Do memory toggle */
	for (i = 0; i < sizeof(SSD1306_Buffer); i++) {
		SSD1306_Buffer[i] = ~SSD1306_Buffer[i];
	}
}

void SSD1306_Fill(SSD1306_COLOR_t color) {
	/* Set memory */
	memset(SSD1306_Buffer, (color == SSD1306_COLOR_BLACK) ? 0x00 : 0xFF, sizeof(SSD1306_Buffer));
}

void SSD1306_DrawPixel(uint16_t x, uint16_t y, SSD1306_COLOR_t color) {
	if (
		x >= SSD1306_WIDTH ||
		y >= SSD1306_HEIGHT
	) {
		/* Error */
		return;
	}
	
	/* Check if pixels are inverted */
	if (SSD1306.Inverted) {
		color = (SSD1306_COLOR_t)!color;
	}
	
	/* Set color */
	if (color == SSD1306_COLOR_WHITE) {
		SSD1306_Buffer[x + (y / 8) * SSD1306_WIDTH] |= 1 << (y % 8);
	} else {
		SSD1306_Buffer[x + (y / 8) * SSD1306_WIDTH] &= ~(1 << (y % 8));
	}
}

void SSD1306_GotoXY(uint16_t x, uint16_t y) {
	/* Set write pointers */
	SSD1306.CurrentX = x;
	SSD1306.CurrentY = y;
}

char SSD1306_Putc(char ch, FontDef_t* Font, SSD1306_COLOR_t color) {
	uint32_t i, b, j;
	
	/* Check available space in LCD */
	if (
		SSD1306_WIDTH <= (SSD1306.CurrentX + Font->FontWidth) ||
		SSD1306_HEIGHT <= (SSD1306.CurrentY + Font->FontHeight)
	) {
		/* Error */
		return 0;
	}
	
	/* Go through font */
	// for (i = 0; i < Font->FontHeight; i++) {
	// 	b = Font->data[(ch - 32) * Font->FontHeight + i];
	// 	for (j = 0; j < Font->FontWidth; j++) {
	// 		if ((b << j) & 0x8000) { // Font data is 16-bit
	// 			SSD1306_DrawPixel(SSD1306.CurrentX + j, (SSD1306.CurrentY + i), color);
	// 		} else {
	// 			SSD1306_DrawPixel(SSD1306.CurrentX + j, (SSD1306.CurrentY + i), (SSD1306_COLOR_t)!color);
	// 		}
	// 	}
	// }
	    for (i = 0; i < Font->FontHeight; i++) {
		b = Font->data[(ch - 32) * Font->FontHeight + i];

		for (j = 0; j < Font->FontWidth; j++) {
		    uint16_t targetScreenY = SSD1306.CurrentY + (Font->FontHeight - 1 - i);

		    if ((b << j) & 0x8000) {
			SSD1306_DrawPixel(SSD1306.CurrentX + j, targetScreenY, color);
		    } else {
			SSD1306_DrawPixel(SSD1306.CurrentX + j, targetScreenY, (SSD1306_COLOR_t)!color);
		    }
		}
	    }
	
	/* Increase pointer */
	SSD1306.CurrentX += Font->FontWidth;
	
	/* Return character written */
	return ch;
}

char SSD1306_Puts(char* str, FontDef_t* Font, SSD1306_COLOR_t color) {
	/* Write characters */
	while (*str) {
		/* Write character by character */
		if (SSD1306_Putc(*str, Font, color) != *str) {
			/* Return error */
			return *str;
		}
		
		/* Increase string pointer */
		str++;
	}
	
	/* Everything OK, zero should be returned */
	return *str;
}
 

void SSD1306_DrawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, SSD1306_COLOR_t c) {
	int16_t dx, dy, sx, sy, err, e2; 
	
	/* Check for overflow */
	if (x0 >= SSD1306_WIDTH) x0 = SSD1306_WIDTH - 1;
	if (x1 >= SSD1306_WIDTH) x1 = SSD1306_WIDTH - 1;
	if (y0 >= SSD1306_HEIGHT) y0 = SSD1306_HEIGHT - 1;
	if (y1 >= SSD1306_HEIGHT) y1 = SSD1306_HEIGHT - 1;
	
	dx = ABS(x1 - x0);
	dy = ABS(y1 - y0);
	sx = (x0 < x1) ? 1 : -1;
	sy = (y0 < y1) ? 1 : -1;
	err = ((dx > dy) ? dx : -dy) / 2;

	while (1) {
		SSD1306_DrawPixel(x0, y0, c);
		if (x0 == x1 && y0 == y1) break;
		e2 = err;
		if (e2 > -dx) { err -= dy; x0 += sx; }
		if (e2 < dy) { err += dx; y0 += sy; }
	}
}

void SSD1306_DrawRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, SSD1306_COLOR_t c) {
	if (x >= SSD1306_WIDTH || y >= SSD1306_HEIGHT) return;
	if ((x + w) >= SSD1306_WIDTH) w = SSD1306_WIDTH - x -1; // Adjust width
    if ((y + h) >= SSD1306_HEIGHT) h = SSD1306_HEIGHT - y -1; // Adjust height
	
	SSD1306_DrawLine(x, y, x + w, y, c);         /* Top line */
	SSD1306_DrawLine(x, y + h, x + w, y + h, c); /* Bottom line */
	SSD1306_DrawLine(x, y, x, y + h, c);         /* Left line */
	SSD1306_DrawLine(x + w, y, x + w, y + h, c); /* Right line */
}

void SSD1306_DrawFilledRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, SSD1306_COLOR_t c) {
	if (x >= SSD1306_WIDTH || y >= SSD1306_HEIGHT) return;
	if ((x + w) >= SSD1306_WIDTH) w = SSD1306_WIDTH - x - 1; // Adjust width
    if ((y + h) >= SSD1306_HEIGHT) h = SSD1306_HEIGHT - y - 1; // Adjust height
	
	for (uint16_t i = 0; i <= h; i++) {
		SSD1306_DrawLine(x, y + i, x + w, y + i, c);
	}
}

void SSD1306_DrawTriangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3, SSD1306_COLOR_t color) {
	SSD1306_DrawLine(x1, y1, x2, y2, color);
	SSD1306_DrawLine(x2, y2, x3, y3, color);
	SSD1306_DrawLine(x3, y3, x1, y1, color);
}


void SSD1306_DrawFilledTriangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3, SSD1306_COLOR_t color) {
	int16_t deltax = 0, deltay = 0, x = 0, y = 0, xinc1 = 0, xinc2 = 0, 
	yinc1 = 0, yinc2 = 0, den = 0, num = 0, numadd = 0, numpixels = 0, 
	curpixel = 0;
	
    // Simplified for brevity, a proper fill algorithm is more complex
    // This is a basic line-by-line fill, not efficient or perfect
    int16_t t1x, t2x, y_min, y_max;

    // Sort vertices by y-coordinate
    if (y1 > y2) { int16_t temp = y1; y1 = y2; y2 = temp; temp = x1; x1 = x2; x2 = temp; }
    if (y1 > y3) { int16_t temp = y1; y1 = y3; y3 = temp; temp = x1; x1 = x3; x3 = temp; }
    if (y2 > y3) { int16_t temp = y2; y2 = y3; y3 = temp; temp = x2; x2 = x3; x3 = temp; }

    y_min = y1;
    y_max = y3;

    if (y_max == y_min) { // Horizontal line
        SSD1306_DrawLine(x1,y1,x2,y2,color);
        SSD1306_DrawLine(x2,y2,x3,y3,color);
        return;
    }
    
    for (y = y_min; y <= y_max; y++) {
        t1x = x1 + (x2 - x1) * (y - y1) / (y2 - y1 +1); // Avoid division by zero
        t2x = x1 + (x3 - x1) * (y - y1) / (y3 - y1 +1); // Avoid division by zero
        if (y < y2) {
             t2x = x1 + (x3 - x1) * (y - y1) / (y3 - y1 +1) ;
        } else {
             if (y3 == y2) t1x = x2; // Handle horizontal bottom edge
             else t1x = x2 + (x3 - x2) * (y - y2) / (y3 - y2 +1);
             t2x = x1 + (x3 - x1) * (y - y1) / (y3 - y1 +1);
        }
        if (t1x > t2x) { int16_t temp = t1x; t1x = t2x; t2x = temp; }
        SSD1306_DrawLine(t1x, y, t2x, y, color);
    }
}

void SSD1306_DrawCircle(int16_t x0, int16_t y0, int16_t r, SSD1306_COLOR_t c) {
	int16_t f = 1 - r;
	int16_t ddF_x = 1;
	int16_t ddF_y = -2 * r;
	int16_t x = 0;
	int16_t y = r;

    SSD1306_DrawPixel(x0, y0 + r, c);
    SSD1306_DrawPixel(x0, y0 - r, c);
    SSD1306_DrawPixel(x0 + r, y0, c);
    SSD1306_DrawPixel(x0 - r, y0, c);

    while (x < y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;

        SSD1306_DrawPixel(x0 + x, y0 + y, c);
        SSD1306_DrawPixel(x0 - x, y0 + y, c);
        SSD1306_DrawPixel(x0 + x, y0 - y, c);
        SSD1306_DrawPixel(x0 - x, y0 - y, c);

        SSD1306_DrawPixel(x0 + y, y0 + x, c);
        SSD1306_DrawPixel(x0 - y, y0 + x, c);
        SSD1306_DrawPixel(x0 + y, y0 - x, c);
        SSD1306_DrawPixel(x0 - y, y0 - x, c);
    }
}

void SSD1306_DrawFilledCircle(int16_t x0, int16_t y0, int16_t r, SSD1306_COLOR_t c) {
	int16_t f = 1 - r;
	int16_t ddF_x = 1;
	int16_t ddF_y = -2 * r;
	int16_t x = 0;
	int16_t y = r;

    SSD1306_DrawLine(x0 - r, y0, x0 + r, y0, c); // Center line

    while (x < y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;

        SSD1306_DrawLine(x0 - x, y0 + y, x0 + x, y0 + y, c);
        SSD1306_DrawLine(x0 - x, y0 - y, x0 + x, y0 - y, c);
        SSD1306_DrawLine(x0 - y, y0 + x, x0 + y, y0 + x, c);
        SSD1306_DrawLine(x0 - y, y0 - x, x0 + y, y0 - x, c);
    }
}
 
void SSD1306_Clear (void)
{
	SSD1306_Fill (SSD1306_COLOR_BLACK);
    // SSD1306_UpdateScreen(); // UpdateScreen should be called explicitly after Clear if needed immediately
}

void SSD1306_ON(void) {
	SSD1306_WRITECOMMAND(0x8D);  
	SSD1306_WRITECOMMAND(0x14);  // Enable charge pump
	SSD1306_WRITECOMMAND(0xAF);  // Display ON
}
void SSD1306_OFF(void) {
	SSD1306_WRITECOMMAND(0x8D);  
	SSD1306_WRITECOMMAND(0x10); // Disable charge pump (specific value might vary)
	SSD1306_WRITECOMMAND(0xAE);  // Display OFF
}

// --- I2C HAL ---
// Uses modern ESP-IDF i2c_master driver
// The I2C port number (e.g., I2C_NUM_0)
#define I2C_MASTER_NUM I2C_NUM_0 

uint8_t ssd1306_I2C_Init() {
    if (g_ssd1306_dev_handle != NULL) {
        ESP_LOGW(I2C_TAG, "I2C already initialized for SSD1306");
        return 1; // Already initialized
    }

    i2c_master_bus_config_t i2c_mst_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_1,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .glitch_ignore_cnt = 7, // Default glitch filter
        .flags.enable_internal_pullup = true, // Enable internal pullups if external are not present
    };
    i2c_master_bus_handle_t bus_handle;
    esp_err_t err = i2c_new_master_bus(&i2c_mst_config, &bus_handle);
    if (err != ESP_OK) {
        ESP_LOGE(I2C_TAG, "i2c_new_master_bus failed: %s", esp_err_to_name(err));
        return 0;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = SSD1306_I2C_ADDR >> 1, // 7-bit address
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };
    err = i2c_master_bus_add_device(bus_handle, &dev_cfg, &g_ssd1306_dev_handle);
    if (err != ESP_OK) {
        ESP_LOGE(I2C_TAG, "i2c_master_bus_add_device failed: %s", esp_err_to_name(err));
        i2c_del_master_bus(bus_handle); // Clean up bus if device add fails
        g_ssd1306_dev_handle = NULL;
        return 0;
    }

    ESP_LOGI(I2C_TAG, "I2C master initialized successfully for SSD1306.");
    return 1;
}

// New signature: void ssd1306_I2C_Write(uint8_t control_byte, uint8_t data_byte)
void ssd1306_I2C_Write(uint8_t control_byte, uint8_t data_byte) {
    if (g_ssd1306_dev_handle == NULL) {
        ESP_LOGE(I2C_TAG, "SSD1306 device handle not initialized!");
        return;
    }
    uint8_t buffer[2];
    buffer[0] = control_byte; // Control byte (0x00 for command, 0x40 for data)
    buffer[1] = data_byte;    // Actual command or data

    esp_err_t err = i2c_master_transmit(g_ssd1306_dev_handle, buffer, sizeof(buffer), (100)); // 100ms timeout
    if (err != ESP_OK) {
        ESP_LOGE(I2C_TAG, "I2C transmit failed (single byte): %s", esp_err_to_name(err));
    }
}

// New signature: void ssd1306_I2C_WriteMulti(uint8_t control_byte, uint8_t* data_buffer, uint16_t count)
void ssd1306_I2C_WriteMulti(uint8_t control_byte, uint8_t* data_buffer, uint16_t count) {
    if (g_ssd1306_dev_handle == NULL) {
        ESP_LOGE(I2C_TAG, "SSD1306 device handle not initialized!");
        return;
    }
    if (data_buffer == NULL || count == 0) {
        ESP_LOGE(I2C_TAG, "Invalid data for I2C_WriteMulti");
        return;
    }

    uint8_t* tx_buffer = (uint8_t*)malloc(count + 1);
    if (tx_buffer == NULL) {
        ESP_LOGE(I2C_TAG, "Failed to allocate buffer for I2C_WriteMulti");
        return;
    }

    tx_buffer[0] = control_byte; // Control byte (usually 0x40 for data stream)
    memcpy(tx_buffer + 1, data_buffer, count);

    esp_err_t err = i2c_master_transmit(g_ssd1306_dev_handle, tx_buffer, count + 1, (100)); // 100ms timeout
    if (err != ESP_OK) {
        ESP_LOGE(I2C_TAG, "I2C transmit failed (multi byte): %s", esp_err_to_name(err));
    }
    free(tx_buffer);
}
// ```
//
// **Important considerations and explanations:**
//
// * **`ssd1306.h`**:
//     * The definitions like `I2C_MASTER_SDA_IO`, `I2C_MASTER_SCL_IO`, `I2C_MASTER_FREQ_HZ`, and `SSD1306_I2C_ADDR` are assumed to be correct in your `ssd1306.h`.
//     * `SSD1306_I2C_ADDR` (e.g., `0x78`) is treated as an 8-bit address. The 7-bit address (`0x3C`) is derived by `SSD1306_I2C_ADDR >> 1` for the new API.
// * **`ssd1306_I2C_Init()`**:
//     * Initializes the I2C master bus for `I2C_NUM_0` (you can change `I2C_MASTER_NUM` if needed).
//     * Adds the SSD1306 as a device on that bus. The handle `g_ssd1306_dev_handle` is stored.
//     * Internal pullups are enabled via `flags.enable_internal_pullup = true`. If you have external pull-ups, you might set this to `false`.
// * **`ssd1306_I2C_Write()` and `ssd1306_I2C_WriteMulti()`**:
//     * Their signatures have changed. They now take `control_byte` (0x00 for command, 0x40 for data) and the payload.
//     * They use `i2c_master_transmit()` with the `g_ssd1306_dev_handle`.
//     * A timeout of 100ms (`pdMS_TO_TICKS(100)`) is used for the transmissions. You can adjust this.
//     * For `ssd1306_I2C_WriteMulti`, dynamic memory allocation (`malloc`/`free`) is used for the transmit buffer to prepend the control byte.
// * **Macros `SSD1306_WRITECOMMAND` and `SSD1306_WRITEDATA`**: These are updated within `ssd1306.c` to call the modified `ssd1306_I2C_Write` with the correct control byte.
// * **`SSD1306_UpdateScreen()`**: This function is updated to call the new `ssd1306_I2C_WriteMulti` with `0x40` as the control byte.
// * **Error Handling**: `ESP_LOGx` is used for logging errors and information.
// * **Bitmap Drawing**: Corrected a minor logic error in `SSD1306_DrawBitmap` for checking the MSB of the font byte.
// * **Filled Triangle**: The `SSD1306_DrawFilledTriangle` function was very basic; I've added a slightly more robust (though still simplified) scanline-based approach. A proper, efficient triangle rasterizer is more complex.
//
// Make sure your `ssd1306.h` has the necessary pin and address definitions. This updated `ssd1306.c` should now work with the current ESP-IDF I2C master driv

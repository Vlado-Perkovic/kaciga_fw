#ifndef DISPLAY_LOGIC_H
#define DISPLAY_LOGIC_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ssd1306.h"        // From SSD1306_Driver component
#include "fonts.h"          // From SSD1306_Driver component
#include "global_vars.h"    // For global variable extern declarations

// Task function declaration
void display_task(void *pvParameters);

#endif // DISPLAY_LOGIC_H


#ifndef ST7789_H
#define ST7789_H

#include "esp_err.h"

/**
 * @file st7789.h
 * @brief ST7789 TFT Display Driver for ESP32
 * 
 * High-performance driver for ST7789-based 240x240 TFT displays.
 * Optimized for displays without CS pin using SPI communication.
 */

// Hardware pin configuration for ST7789 display
#define ST7789_SCK_PIN  18  // SPI clock pin
#define ST7789_SDA_PIN  23  // SPI data pin (MOSI)
#define ST7789_RST_PIN  4   // Hardware reset pin (active low)
#define ST7789_DC_PIN   2   // Data/Command select pin
#define ST7789_BLK_PIN  15  // Backlight control pin

/**
 * @brief Initialize the ST7789 display driver
 * 
 * Configures GPIO pins, initializes SPI communication, performs hardware reset,
 * and sends the complete initialization sequence to the ST7789 controller.
 * 
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t st7789_init(void);

/**
 * @brief Run display functionality test
 * 
 * Executes a comprehensive test sequence including full-screen color fills
 * and multi-color patterns to verify display functionality and color accuracy.
 */
void st7789_test(void);

#endif // ST7789_H
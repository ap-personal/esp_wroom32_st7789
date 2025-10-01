#include "st7789.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "ST7789";

// ST7789 Display Controller Commands
#define ST7789_SWRESET  0x01  // Software reset
#define ST7789_SLPOUT   0x11  // Sleep out
#define ST7789_COLMOD   0x3A  // Color mode
#define ST7789_MADCTL   0x36  // Memory access control
#define ST7789_INVON    0x21  // Display inversion on
#define ST7789_NORON    0x13  // Normal display mode
#define ST7789_DISPON   0x29  // Display on
#define ST7789_CASET    0x2A  // Column address set
#define ST7789_RASET    0x2B  // Row address set
#define ST7789_RAMWR    0x2C  // Memory write

// Color definitions (16-bit RGB565)
#define RED     0xF800
#define GREEN   0x07E0
#define BLUE    0x001F
#define WHITE   0xFFFF
#define BLACK   0x0000
#define YELLOW  0xFFE0

// Precise millisecond delay using FreeRTOS task delay
static void delay_ms(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

// GPIO helper function
static void digitalWrite(int pin, int value) {
    gpio_set_level((gpio_num_t)pin, value);
}

// Bit-banging SPI functions for maximum compatibility
static void spi_write_byte_bitbang(uint8_t data) {
    for (int i = 7; i >= 0; i--) {
        // Set data bit on MOSI
        digitalWrite(ST7789_SDA_PIN, (data >> i) & 1);
        
        // Clock pulse
        digitalWrite(ST7789_SCK_PIN, 0);
        esp_rom_delay_us(1); // Small delay for stability
        digitalWrite(ST7789_SCK_PIN, 1);
        esp_rom_delay_us(1);
    }
}

static void spi_write_word_bitbang(uint16_t data) {
    spi_write_byte_bitbang(data >> 8);   // High byte first
    spi_write_byte_bitbang(data & 0xFF); // Low byte
}

// Data/Command pin control for ST7789 protocol
static inline void set_dc_command(void) {
    gpio_set_level(ST7789_DC_PIN, 0);  // DC low = command mode
}

static inline void set_dc_data(void) {
    gpio_set_level(ST7789_DC_PIN, 1);  // DC high = data mode
}

// ST7789 command and data transmission functions
static void write_command(uint8_t cmd) {
    ESP_LOGD(TAG, "Sending command: 0x%02X", cmd);
    set_dc_command();
    spi_write_byte_bitbang(cmd);
    set_dc_data();  // Ready for data mode
}

static void write_data(uint8_t data) {
    ESP_LOGD(TAG, "Sending data: 0x%02X", data);
    set_dc_data();
    spi_write_byte_bitbang(data);
}

static void write_data_word(uint16_t data) {
    ESP_LOGD(TAG, "Sending 16-bit data: 0x%04X", data);
    set_dc_data();
    spi_write_word_bitbang(data);
}

// Set display memory address window for pixel writing
static void set_address_window(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    uint16_t x_end = x + w - 1;
    uint16_t y_end = y + h - 1;
    
    write_command(ST7789_CASET);   // Column address set
    write_data_word(x);            // X start
    write_data_word(x_end);        // X end
    
    write_command(ST7789_RASET);   // Row address set
    write_data_word(y);            // Y start
    write_data_word(y_end);        // Y end
    
    write_command(ST7789_RAMWR);   // Write to RAM
}

// Fill rectangular area with specified color
static void fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    set_address_window(x, y, w, h);
    
    uint32_t pixels = (uint32_t)w * h;
    
    // Set data mode and stream color data to display memory
    set_dc_data();
    for (uint32_t i = 0; i < pixels; i++) {
        spi_write_word_bitbang(color);
    }
}

esp_err_t st7789_init(void) {
    ESP_LOGI(TAG, "===========================================");
    ESP_LOGI(TAG, "     ST7789 Display Driver Initialization");
    ESP_LOGI(TAG, "        Using Bit-banging SPI");
    ESP_LOGI(TAG, "===========================================");
    
    // Configure GPIO pins for bit-banging SPI and display control
    gpio_config_t io_conf = {};
    
    // Configure MOSI pin
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << ST7789_SDA_PIN);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);
    
    // Configure SCLK pin
    io_conf.pin_bit_mask = (1ULL << ST7789_SCK_PIN);
    gpio_config(&io_conf);
    
    // Configure DC pin
    io_conf.pin_bit_mask = (1ULL << ST7789_DC_PIN);
    gpio_config(&io_conf);
    
    // Configure RST pin
    io_conf.pin_bit_mask = (1ULL << ST7789_RST_PIN);
    gpio_config(&io_conf);
    
    // Configure backlight pin
    io_conf.pin_bit_mask = (1ULL << ST7789_BLK_PIN);
    gpio_config(&io_conf);

    // Set initial pin states
    digitalWrite(ST7789_SCK_PIN, 1);  // SPI Mode 0: CLK idle high
    digitalWrite(ST7789_SDA_PIN, 0);  // MOSI idle low
    digitalWrite(ST7789_DC_PIN, 1);   // Data mode default
    digitalWrite(ST7789_BLK_PIN, 1);  // Turn on backlight
    
    ESP_LOGI(TAG, "GPIO pins configured for bit-banging SPI");
    ESP_LOGI(TAG, "Pin configuration: RST=%d, DC=%d, SDA=%d, SCK=%d, BLK=%d", 
             ST7789_RST_PIN, ST7789_DC_PIN, ST7789_SDA_PIN, ST7789_SCK_PIN, ST7789_BLK_PIN);
    
    // Perform hardware reset sequence for reliable initialization
    ESP_LOGI(TAG, "Performing hardware reset sequence...");
    digitalWrite(ST7789_RST_PIN, 0);  // Assert reset
    delay_ms(10);                     // Hold reset for 10ms
    digitalWrite(ST7789_RST_PIN, 1);  // Release reset
    delay_ms(120);                    // Wait for display stabilization (matching working code)
    ESP_LOGI(TAG, "Hardware reset sequence completed");
    
    // Send ST7789 initialization command sequence (matching working C++ code)
    ESP_LOGI(TAG, "Sending display initialization commands...");
    
    write_command(ST7789_SWRESET);  // Software reset
    delay_ms(150);                  // Wait for reset completion
    
    write_command(ST7789_SLPOUT);   // Exit sleep mode
    delay_ms(255);                  // Wait for sleep exit
    
    write_command(ST7789_COLMOD);   // Set color format
    write_data(0x55);               // 16-bit RGB565 color mode
    delay_ms(10);                   // Additional delay for stability
    
    write_command(ST7789_MADCTL);   // Memory access control
    write_data(0x00);               // Normal scan direction, RGB order
    
    write_command(ST7789_INVON);    // Enable display inversion
    delay_ms(10);                   // Additional delay for stability
    
    write_command(ST7789_NORON);    // Normal display mode
    delay_ms(10);                   // Additional delay for stability
    
    write_command(ST7789_DISPON);   // Turn display on
    delay_ms(100);                  // Allow display to stabilize
    
    ESP_LOGI(TAG, "ST7789 display initialization completed successfully!");
    ESP_LOGI(TAG, "===========================================");
    
    return ESP_OK;
}

void st7789_test(void) {
    ESP_LOGI(TAG, "Starting display functionality test...");
    
    // Test 1: Full screen color fill - Red
    ESP_LOGI(TAG, "Display Test 1: Full screen red fill");
    fill_rect(0, 0, 240, 240, RED);
    delay_ms(1000);
    
    // Test 2: Full screen color fill - Green
    ESP_LOGI(TAG, "Display Test 2: Full screen green fill");
    fill_rect(0, 0, 240, 240, GREEN);
    delay_ms(1000);
    
    // Test 3: Full screen color fill - Blue
    ESP_LOGI(TAG, "Display Test 3: Full screen blue fill");
    fill_rect(0, 0, 240, 240, BLUE);
    delay_ms(1000);
    
    // Test 4: Full screen color fill - White
    ESP_LOGI(TAG, "Display Test 4: Full screen white fill");
    fill_rect(0, 0, 240, 240, WHITE);
    delay_ms(1000);
    
    // Test 5: Full screen color fill - Black
    ESP_LOGI(TAG, "Display Test 5: Full screen black fill");
    fill_rect(0, 0, 240, 240, BLACK);
    delay_ms(1000);
    
    // Test 6: Multi-color pattern test
    ESP_LOGI(TAG, "Display Test 6: Multi-color pattern");
    fill_rect(0, 0, 240, 240, BLACK);      // Clear screen to black
    delay_ms(500);
    
    // Draw colored squares to test RGB color accuracy
    fill_rect(10, 10, 50, 50, RED);        // Red square (top-left)
    fill_rect(180, 10, 50, 50, GREEN);     // Green square (top-right)
    fill_rect(10, 180, 50, 50, BLUE);      // Blue square (bottom-left)
    fill_rect(180, 180, 50, 50, YELLOW);   // Yellow square (bottom-right)
    fill_rect(95, 95, 50, 50, WHITE);      // White square (center)
    
    ESP_LOGI(TAG, "Display test sequence completed successfully!");
    ESP_LOGI(TAG, "All color patterns should be visible on the display");
}
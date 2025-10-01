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

// Font definitions - 8x8 pixel font
#define FONT_WIDTH  8
#define FONT_HEIGHT 8

// Simple 8x8 bitmap font for basic ASCII characters (32-126)
static const uint8_t font8x8[95][8] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // Space (32)
    {0x18, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x18, 0x00}, // ! (33)
    {0x36, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // " (34)
    {0x36, 0x36, 0x7F, 0x36, 0x7F, 0x36, 0x36, 0x00}, // # (35)
    {0x0C, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x0C, 0x00}, // $ (36)
    {0x00, 0x63, 0x33, 0x18, 0x0C, 0x66, 0x63, 0x00}, // % (37)
    {0x1C, 0x36, 0x1C, 0x6E, 0x3B, 0x33, 0x6E, 0x00}, // & (38)
    {0x06, 0x06, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00}, // ' (39)
    {0x18, 0x0C, 0x06, 0x06, 0x06, 0x0C, 0x18, 0x00}, // ( (40)
    {0x06, 0x0C, 0x18, 0x18, 0x18, 0x0C, 0x06, 0x00}, // ) (41)
    {0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00}, // * (42)
    {0x00, 0x0C, 0x0C, 0x3F, 0x0C, 0x0C, 0x00, 0x00}, // + (43)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x06, 0x00}, // , (44)
    {0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x00}, // - (45)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x00}, // . (46)
    {0x60, 0x30, 0x18, 0x0C, 0x06, 0x03, 0x01, 0x00}, // / (47)
    {0x3E, 0x63, 0x73, 0x7B, 0x6F, 0x67, 0x3E, 0x00}, // 0 (48)
    {0x0C, 0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x3F, 0x00}, // 1 (49)
    {0x1E, 0x33, 0x30, 0x1C, 0x06, 0x33, 0x3F, 0x00}, // 2 (50)
    {0x1E, 0x33, 0x30, 0x1C, 0x30, 0x33, 0x1E, 0x00}, // 3 (51)
    {0x38, 0x3C, 0x36, 0x33, 0x7F, 0x30, 0x78, 0x00}, // 4 (52)
    {0x3F, 0x03, 0x1F, 0x30, 0x30, 0x33, 0x1E, 0x00}, // 5 (53)
    {0x1C, 0x06, 0x03, 0x1F, 0x33, 0x33, 0x1E, 0x00}, // 6 (54)
    {0x3F, 0x33, 0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x00}, // 7 (55)
    {0x1E, 0x33, 0x33, 0x1E, 0x33, 0x33, 0x1E, 0x00}, // 8 (56)
    {0x1E, 0x33, 0x33, 0x3E, 0x30, 0x18, 0x0E, 0x00}, // 9 (57)
    {0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x00}, // : (58)
    {0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x06, 0x00}, // ; (59)
    {0x18, 0x0C, 0x06, 0x03, 0x06, 0x0C, 0x18, 0x00}, // < (60)
    {0x00, 0x00, 0x3F, 0x00, 0x00, 0x3F, 0x00, 0x00}, // = (61)
    {0x06, 0x0C, 0x18, 0x30, 0x18, 0x0C, 0x06, 0x00}, // > (62)
    {0x1E, 0x33, 0x30, 0x18, 0x0C, 0x00, 0x0C, 0x00}, // ? (63)
    {0x3E, 0x63, 0x7B, 0x7B, 0x7B, 0x03, 0x1E, 0x00}, // @ (64)
    {0x0C, 0x1E, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x00}, // A (65)
    {0x3F, 0x66, 0x66, 0x3E, 0x66, 0x66, 0x3F, 0x00}, // B (66)
    {0x3C, 0x66, 0x03, 0x03, 0x03, 0x66, 0x3C, 0x00}, // C (67)
    {0x1F, 0x36, 0x66, 0x66, 0x66, 0x36, 0x1F, 0x00}, // D (68)
    {0x7F, 0x46, 0x16, 0x1E, 0x16, 0x46, 0x7F, 0x00}, // E (69)
    {0x7F, 0x46, 0x16, 0x1E, 0x16, 0x06, 0x0F, 0x00}, // F (70)
    {0x3C, 0x66, 0x03, 0x03, 0x73, 0x66, 0x7C, 0x00}, // G (71)
    {0x33, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x33, 0x00}, // H (72)
    {0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, // I (73)
    {0x78, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E, 0x00}, // J (74)
    {0x67, 0x66, 0x36, 0x1E, 0x36, 0x66, 0x67, 0x00}, // K (75)
    {0x0F, 0x06, 0x06, 0x06, 0x46, 0x66, 0x7F, 0x00}, // L (76)
    {0x63, 0x77, 0x7F, 0x7F, 0x6B, 0x63, 0x63, 0x00}, // M (77)
    {0x63, 0x67, 0x6F, 0x7B, 0x73, 0x63, 0x63, 0x00}, // N (78)
    {0x1C, 0x36, 0x63, 0x63, 0x63, 0x36, 0x1C, 0x00}, // O (79)
    {0x3F, 0x66, 0x66, 0x3E, 0x06, 0x06, 0x0F, 0x00}, // P (80)
    {0x1E, 0x33, 0x33, 0x33, 0x3B, 0x1E, 0x38, 0x00}, // Q (81)
    {0x3F, 0x66, 0x66, 0x3E, 0x36, 0x66, 0x67, 0x00}, // R (82)
    {0x1E, 0x33, 0x07, 0x0E, 0x38, 0x33, 0x1E, 0x00}, // S (83)
    {0x3F, 0x2D, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, // T (84)
    {0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x3F, 0x00}, // U (85)
    {0x33, 0x33, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00}, // V (86)
    {0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00}, // W (87)
    {0x63, 0x63, 0x36, 0x1C, 0x1C, 0x36, 0x63, 0x00}, // X (88)
    {0x33, 0x33, 0x33, 0x1E, 0x0C, 0x0C, 0x1E, 0x00}, // Y (89)
    {0x7F, 0x63, 0x31, 0x18, 0x4C, 0x66, 0x7F, 0x00}, // Z (90)
    {0x1E, 0x06, 0x06, 0x06, 0x06, 0x06, 0x1E, 0x00}, // [ (91)
    {0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x40, 0x00}, // \ (92)
    {0x1E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1E, 0x00}, // ] (93)
    {0x08, 0x1C, 0x36, 0x63, 0x00, 0x00, 0x00, 0x00}, // ^ (94)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF}, // _ (95)
    {0x0C, 0x0C, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00}, // ` (96)
    {0x00, 0x00, 0x1E, 0x30, 0x3E, 0x33, 0x6E, 0x00}, // a (97)
    {0x07, 0x06, 0x06, 0x3E, 0x66, 0x66, 0x3B, 0x00}, // b (98)
    {0x00, 0x00, 0x1E, 0x33, 0x03, 0x33, 0x1E, 0x00}, // c (99)
    {0x38, 0x30, 0x30, 0x3e, 0x33, 0x33, 0x6E, 0x00}, // d (100)
    {0x00, 0x00, 0x1E, 0x33, 0x3f, 0x03, 0x1E, 0x00}, // e (101)
    {0x1C, 0x36, 0x06, 0x0f, 0x06, 0x06, 0x0F, 0x00}, // f (102)
    {0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x1F}, // g (103)
    {0x07, 0x06, 0x36, 0x6E, 0x66, 0x66, 0x67, 0x00}, // h (104)
    {0x0C, 0x00, 0x0E, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, // i (105)
    {0x30, 0x00, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E}, // j (106)
    {0x07, 0x06, 0x66, 0x36, 0x1E, 0x36, 0x67, 0x00}, // k (107)
    {0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, // l (108)
    {0x00, 0x00, 0x33, 0x7F, 0x7F, 0x6B, 0x63, 0x00}, // m (109)
    {0x00, 0x00, 0x1F, 0x33, 0x33, 0x33, 0x33, 0x00}, // n (110)
    {0x00, 0x00, 0x1E, 0x33, 0x33, 0x33, 0x1E, 0x00}, // o (111)
    {0x00, 0x00, 0x3B, 0x66, 0x66, 0x3E, 0x06, 0x0F}, // p (112)
    {0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x78}, // q (113)
    {0x00, 0x00, 0x3B, 0x6E, 0x66, 0x06, 0x0F, 0x00}, // r (114)
    {0x00, 0x00, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x00}, // s (115)
    {0x08, 0x0C, 0x3E, 0x0C, 0x0C, 0x2C, 0x18, 0x00}, // t (116)
    {0x00, 0x00, 0x33, 0x33, 0x33, 0x33, 0x6E, 0x00}, // u (117)
    {0x00, 0x00, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00}, // v (118)
    {0x00, 0x00, 0x63, 0x6B, 0x7F, 0x7F, 0x36, 0x00}, // w (119)
    {0x00, 0x00, 0x63, 0x36, 0x1C, 0x36, 0x63, 0x00}, // x (120)
    {0x00, 0x00, 0x33, 0x33, 0x33, 0x3E, 0x30, 0x1F}, // y (121)
    {0x00, 0x00, 0x3F, 0x19, 0x0C, 0x26, 0x3F, 0x00}, // z (122)
    {0x38, 0x0C, 0x0C, 0x07, 0x0C, 0x0C, 0x38, 0x00}, // { (123)
    {0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x00}, // | (124)
    {0x07, 0x0C, 0x0C, 0x38, 0x0C, 0x0C, 0x07, 0x00}, // } (125)
    {0x6E, 0x3B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // ~ (126)
};

// Precise millisecond delay using FreeRTOS task delay
static void delay_ms(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

// GPIO helper function
static void digitalWrite(int pin, int value) {
    gpio_set_level((gpio_num_t)pin, value);
}

// Bit-banging SPI functions optimized for maximum speed
static void spi_write_byte_bitbang(uint8_t data) {
    for (int i = 7; i >= 0; i--) {
        // Set data bit on MOSI
        digitalWrite(ST7789_SDA_PIN, (data >> i) & 1);
        
        // Clock pulse - maximum speed, no delays
        digitalWrite(ST7789_SCK_PIN, 0);
        digitalWrite(ST7789_SCK_PIN, 1);
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

// Fill rectangular area with specified color - optimized for cooperative multitasking
static void fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    set_address_window(x, y, w, h);
    
    uint32_t pixels = (uint32_t)w * h;
    
    // Set data mode and stream color data to display memory
    set_dc_data();
    for (uint32_t i = 0; i < pixels; i++) {
        spi_write_word_bitbang(color);
        
        // Yield to other tasks occasionally for large operations (every 500 pixels)
        if (pixels > 1000 && (i % 500) == 0) {
            taskYIELD(); // Brief yield without delay
        }
    }
}

// Draw a single pixel at specified coordinates
static void draw_pixel(uint16_t x, uint16_t y, uint16_t color) {
    if (x >= 240 || y >= 240) return;  // Bounds check
    
    set_address_window(x, y, 1, 1);
    write_data_word(color);
}

// Draw a single character at specified position - optimized for performance
static void draw_char(uint16_t x, uint16_t y, char c, uint16_t color, uint16_t bg_color) {
    if (c < 32 || c > 126) return;  // Only printable ASCII characters
    
    uint8_t char_index = c - 32;  // Convert to font array index
    
    // Set address window for entire character to minimize SPI overhead
    set_address_window(x, y, FONT_WIDTH, FONT_HEIGHT);
    set_dc_data();  // Switch to data mode once
    
    // Stream entire character as pixel data - fast enough not to need yields
    for (uint8_t row = 0; row < FONT_HEIGHT; row++) {
        uint8_t font_row = font8x8[char_index][row];
        
        for (uint8_t col = 0; col < FONT_WIDTH; col++) {
            // Fix bit order - read from LSB to MSB to correct character reversal
            if (font_row & (0x01 << col)) {
                spi_write_word_bitbang(color);     // Foreground
            } else {
                spi_write_word_bitbang(bg_color);  // Background
            }
        }
    }
}

// Draw a string at specified position - optimized with minimal task cooperation
static void draw_string(uint16_t x, uint16_t y, const char* str, uint16_t color, uint16_t bg_color) {
    uint16_t cur_x = x;
    uint16_t cur_y = y;
    uint16_t char_count = 0;
    
    while (*str) {
        if (*str == '\n') {
            // New line
            cur_x = x;
            cur_y += FONT_HEIGHT + 2;  // Add 2 pixels line spacing
        } else if (*str == '\r') {
            // Carriage return
            cur_x = x;
        } else {
            // Bounds check before drawing character
            if (cur_x + FONT_WIDTH <= 240 && cur_y + FONT_HEIGHT <= 240) {
                draw_char(cur_x, cur_y, *str, color, bg_color);
            }
            cur_x += FONT_WIDTH + 1;  // Add 1 pixel character spacing
            
            // Wrap to next line if text exceeds display width
            if (cur_x + FONT_WIDTH > 240) {
                cur_x = x;
                cur_y += FONT_HEIGHT + 2;
            }
        }
        str++;
        char_count++;
        
        // Only yield for very long strings (more than 20 characters)
        if ((char_count % 20) == 0) {
            taskYIELD(); // Brief yield without delay
        }
        
        // Stop if text exceeds display height
        if (cur_y + FONT_HEIGHT > 240) break;
    }
}

// Public API functions for external use

void st7789_draw_pixel(uint16_t x, uint16_t y, uint16_t color) {
    draw_pixel(x, y, color);
}

void st7789_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    fill_rect(x, y, w, h, color);
}

void st7789_draw_char(uint16_t x, uint16_t y, char c, uint16_t color, uint16_t bg_color) {
    draw_char(x, y, c, color, bg_color);
}

void st7789_draw_string(uint16_t x, uint16_t y, const char* str, uint16_t color, uint16_t bg_color) {
    draw_string(x, y, str, color, bg_color);
}

void st7789_clear_screen(uint16_t color) {
    fill_rect(0, 0, 240, 240, color);
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
    delay_ms(255);                  // Wait for sleep exit - this is the longest delay
    
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
    
    // Clear display memory to prevent showing previous content
    ESP_LOGI(TAG, "Clearing display memory...");
    fill_rect(0, 0, 240, 240, BLACK);
    delay_ms(50);                   // Allow clear operation to complete
    
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
    
    delay_ms(2000);
    
    // Test 7: Text rendering demonstration
    ESP_LOGI(TAG, "Display Test 7: Text rendering");
    fill_rect(0, 0, 240, 240, BLACK);      // Clear screen to black
    
    // Display title
    draw_string(60, 20, "ST7789 ESP32", WHITE, BLACK);
    draw_string(80, 35, "Display", WHITE, BLACK);
    
    // Display different colored text
    draw_string(10, 60, "Red Text", RED, BLACK);
    draw_string(10, 75, "Green Text", GREEN, BLACK);
    draw_string(10, 90, "Blue Text", BLUE, BLACK);
    draw_string(10, 105, "Yellow Text", YELLOW, BLACK);
    
    // Display numbers and special characters
    draw_string(10, 130, "Numbers: 0123456789", WHITE, BLACK);
    draw_string(10, 145, "Special: !@#$%^&*()", WHITE, BLACK);
    
    // Display multiline text
    draw_string(10, 170, "Line 1\nLine 2\nLine 3", WHITE, BLACK);
    
    // Display status message
    draw_string(50, 220, "Text Demo!", 0x07FF, BLACK);  // Cyan color
    
    ESP_LOGI(TAG, "Display test sequence completed successfully!");
    ESP_LOGI(TAG, "All color patterns and text should be visible on the display");
}
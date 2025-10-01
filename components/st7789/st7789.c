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

// Large font definitions - 16x16 pixel font
#define LARGE_FONT_WIDTH  16
#define LARGE_FONT_HEIGHT 16

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

// Large 16x16 font for sensor displays - focused character set
// Includes: space, digits 0-9, colon, uppercase letters A-Z
// Character mapping: 32(space), 48-57(0-9), 58(:), 65-90(A-Z)
static const uint16_t large_font16x16[][16] = {
    // Space (32)
    {0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
     
    // 0 (48)
    {0x0000, 0x07E0, 0x1FF8, 0x3C3C, 0x701E, 0x600F, 0x600F, 0x600F,
     0x600F, 0x600F, 0x600F, 0x701E, 0x3C3C, 0x1FF8, 0x07E0, 0x0000},
     
    // 1 (49) - Fixed orientation
    {0x0000, 0x01C0, 0x03C0, 0x07C0, 0x0DC0, 0x01C0, 0x01C0, 0x01C0,
     0x01C0, 0x01C0, 0x01C0, 0x01C0, 0x01C0, 0x7FF0, 0x7FF0, 0x0000},
     
    // 2 (50)
    {0x0000, 0x07E0, 0x1FF8, 0x3C3C, 0x701E, 0x600F, 0x6007, 0x0007,
     0x000E, 0x001C, 0x0038, 0x0070, 0x00E0, 0x7FFF, 0x7FFF, 0x0000},
     
    // 3 (51)
    {0x0000, 0x07E0, 0x1FF8, 0x3C3C, 0x700E, 0x0007, 0x0007, 0x03FE,
     0x03FE, 0x0007, 0x0007, 0x700E, 0x3C3C, 0x1FF8, 0x07E0, 0x0000},
     
    // 4 (52)
    {0x0000, 0x000E, 0x001E, 0x003E, 0x006E, 0x00CE, 0x018E, 0x030E,
     0x060E, 0x7FFF, 0x7FFF, 0x000E, 0x000E, 0x000E, 0x000E, 0x0000},
     
    // 5 (53)
    {0x0000, 0x7FFF, 0x7FFF, 0x7000, 0x7000, 0x7000, 0x7FE0, 0x7FF8,
     0x003C, 0x000E, 0x0007, 0x700E, 0x3C3C, 0x1FF8, 0x07E0, 0x0000},
     
    // 6 (54)
    {0x0000, 0x07E0, 0x1FF8, 0x3C3C, 0x700E, 0x7007, 0x7000, 0x7FE0,
     0x7FF8, 0x703C, 0x700E, 0x700E, 0x3C3C, 0x1FF8, 0x07E0, 0x0000},
     
    // 7 (55)
    {0x0000, 0x7FFF, 0x7FFF, 0x0007, 0x000E, 0x000E, 0x001C, 0x001C,
     0x0038, 0x0038, 0x0070, 0x0070, 0x00E0, 0x00E0, 0x01C0, 0x0000},
     
    // 8 (56)
    {0x0000, 0x07E0, 0x1FF8, 0x3C3C, 0x700E, 0x700E, 0x3C3C, 0x1FF8,
     0x1FF8, 0x3C3C, 0x700E, 0x700E, 0x3C3C, 0x1FF8, 0x07E0, 0x0000},
     
    // 9 (57)
    {0x0000, 0x07E0, 0x1FF8, 0x3C3C, 0x700E, 0x700E, 0x3C0E, 0x1FFE,
     0x07FE, 0x0007, 0x0007, 0x700E, 0x3C3C, 0x1FF8, 0x07E0, 0x0000},
     
    // : (58)
    {0x0000, 0x0000, 0x0000, 0x0000, 0x01C0, 0x03E0, 0x03E0, 0x01C0,
     0x0000, 0x01C0, 0x03E0, 0x03E0, 0x01C0, 0x0000, 0x0000, 0x0000},
     
    // A (65)
    {0x0000, 0x0380, 0x07C0, 0x0EE0, 0x1C70, 0x1C70, 0x3838, 0x381C,
     0x701C, 0x7FFE, 0x7FFE, 0xE007, 0xE007, 0xE007, 0xE007, 0x0000},
     
    // C (67)
    {0x0000, 0x07E0, 0x1FF8, 0x3C3C, 0x701E, 0x700F, 0x7007, 0x7000,
     0x7000, 0x7007, 0x700F, 0x701E, 0x3C3C, 0x1FF8, 0x07E0, 0x0000},
     
    // D (68)
    {0x0000, 0x7FE0, 0x7FF8, 0x703C, 0x701E, 0x700F, 0x700F, 0x700F,
     0x700F, 0x700F, 0x700F, 0x701E, 0x703C, 0x7FF8, 0x7FE0, 0x0000},
     
    // E (69)
    {0x0000, 0x7FFF, 0x7FFF, 0x7000, 0x7000, 0x7000, 0x7FE0, 0x7FE0,
     0x7FE0, 0x7000, 0x7000, 0x7000, 0x7000, 0x7FFF, 0x7FFF, 0x0000},
     
    // H (72)
    {0x0000, 0xE007, 0xE007, 0xE007, 0xE007, 0xE007, 0xFFFF, 0xFFFF,
     0xFFFF, 0xE007, 0xE007, 0xE007, 0xE007, 0xE007, 0xE007, 0x0000},
     
    // I (73)
    {0x0000, 0x0FF0, 0x0FF0, 0x01C0, 0x01C0, 0x01C0, 0x01C0, 0x01C0,
     0x01C0, 0x01C0, 0x01C0, 0x01C0, 0x01C0, 0x0FF0, 0x0FF0, 0x0000},
     
    // M (77)
    {0x0000, 0xE007, 0xF00F, 0xF81F, 0xFC3F, 0xEE77, 0xE7E7, 0xE3C7,
     0xE187, 0xE007, 0xE007, 0xE007, 0xE007, 0xE007, 0xE007, 0x0000},
     
    // N (78)
    {0x0000, 0xE007, 0xF007, 0xF807, 0xFC07, 0xEE07, 0xE707, 0xE387,
     0xE1C7, 0xE0E7, 0xE077, 0xE03F, 0xE01F, 0xE00F, 0xE007, 0x0000},
     
    // P (80)
    {0x0000, 0x7FE0, 0x7FF8, 0x703C, 0x700E, 0x700E, 0x703C, 0x7FF8,
     0x7FE0, 0x7000, 0x7000, 0x7000, 0x7000, 0x7000, 0x7000, 0x0000},
     
    // R (82)
    {0x0000, 0x7FE0, 0x7FF8, 0x703C, 0x700E, 0x700E, 0x703C, 0x7FF8,
     0x7FE0, 0x7380, 0x71C0, 0x70E0, 0x7070, 0x7038, 0x701C, 0x0000},
     
    // S (83)
    {0x0000, 0x07E0, 0x1FF8, 0x3C3C, 0x700E, 0x7007, 0x3800, 0x1FE0,
     0x07F8, 0x001C, 0x700E, 0x700E, 0x3C3C, 0x1FF8, 0x07E0, 0x0000},
     
    // T (84)
    {0x0000, 0x7FFF, 0x7FFF, 0x01C0, 0x01C0, 0x01C0, 0x01C0, 0x01C0,
     0x01C0, 0x01C0, 0x01C0, 0x01C0, 0x01C0, 0x01C0, 0x01C0, 0x0000},
     
    // U (85)
    {0x0000, 0xE007, 0xE007, 0xE007, 0xE007, 0xE007, 0xE007, 0xE007,
     0xE007, 0xE007, 0xE007, 0x700E, 0x3C3C, 0x1FF8, 0x07E0, 0x0000},
     
    // Y (89)
    {0x0000, 0xE007, 0x700E, 0x381C, 0x1C38, 0x0E70, 0x07E0, 0x03C0,
     0x0180, 0x0180, 0x0180, 0x0180, 0x0180, 0x0180, 0x0180, 0x0000},
     
    // . (46) - Period
    {0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
     0x0000, 0x0000, 0x0000, 0x0000, 0x03C0, 0x07E0, 0x07E0, 0x03C0},
     
    // % (37) - Percent
    {0x0000, 0x7003, 0xF807, 0xDC0E, 0xDC1C, 0xF838, 0x7070, 0x00E0,
     0x01C0, 0x0380, 0x0700, 0x0E3E, 0x1C7E, 0x387B, 0x707B, 0xE03E},
};

/**
 * @brief Get array index for large font character
 * 
 * Maps supported characters to their corresponding index in the large_font16x16 array.
 * Returns -1 for unsupported characters to enable graceful error handling.
 * 
 * Supported character mapping:
 * - Space: index 0
 * - Numbers 0-9: indices 1-10  
 * - Colon (:): index 11
 * - Letters A,C,D,E,H,I,M,N,P,R,S,T,U,Y: indices 12-25
 * - Period (.): index 26
 * - Percent (%): index 27
 * 
 * @param c Character to look up
 * @return Array index (0-27) for supported characters, -1 for unsupported
 */
static int get_large_font_index(char c) {
    if (c == ' ') return 0;           // Space
    if (c >= '0' && c <= '9') return 1 + (c - '0');  // Numbers 0-9
    if (c == ':') return 11;          // Colon
    if (c == 'A') return 12;          // A
    if (c == 'C') return 13;          // C  
    if (c == 'D') return 14;          // D
    if (c == 'E') return 15;          // E
    if (c == 'H') return 16;          // H
    if (c == 'I') return 17;          // I
    if (c == 'M') return 18;          // M
    if (c == 'N') return 19;          // N
    if (c == 'P') return 20;          // P
    if (c == 'R') return 21;          // R
    if (c == 'S') return 22;          // S
    if (c == 'T') return 23;          // T
    if (c == 'U') return 24;          // U
    if (c == 'Y') return 25;          // Y
    if (c == '.') return 26;          // Period
    if (c == '%') return 27;          // Percent
    return -1; // Unsupported character
}

/**
 * @brief Precise millisecond delay using FreeRTOS
 * 
 * Provides accurate timing delays required for ST7789 initialization and
 * operation. Uses FreeRTOS task delay to avoid blocking other system tasks.
 * 
 * @param ms Delay duration in milliseconds
 */
static void delay_ms(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

/**
 * @brief Set GPIO pin output level
 * 
 * Helper function for cleaner GPIO control. Provides Arduino-style
 * digitalWrite interface for better code readability.
 * 
 * @param pin GPIO pin number
 * @param value Output level (0 = low, 1 = high)
 */
static void digitalWrite(int pin, int value) {
    gpio_set_level((gpio_num_t)pin, value);
}

/**
 * @brief Send single byte via bit-banging SPI
 * 
 * Implements software SPI transmission for maximum compatibility with ST7789
 * displays that don't have CS pins. Optimized for speed with no delays between
 * clock cycles. Uses SPI Mode 0 (CPOL=0, CPHA=0).
 * 
 * @param data 8-bit data byte to transmit (MSB first)
 */
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

/**
 * @brief Send command to ST7789 controller
 * 
 * Transmits a command byte to the ST7789 with proper DC pin control.
 * Sets DC low for command mode, sends the command, then switches back
 * to data mode for subsequent data transmission.
 * 
 * @param cmd ST7789 command byte
 */
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

/**
 * @brief Set display memory address window
 * 
 * Configures the ST7789 to accept pixel data for a specific rectangular region.
 * Essential for efficient drawing operations as it allows streaming pixel data
 * without individual coordinate commands.
 * 
 * @param x Starting X coordinate
 * @param y Starting Y coordinate  
 * @param w Width of the window in pixels
 * @param h Height of the window in pixels
 */
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

// Draw a single large character (16x16) at specified position
static void draw_large_char(uint16_t x, uint16_t y, char c, uint16_t color, uint16_t bg_color) {
    int char_index = get_large_font_index(c);
    if (char_index < 0) return;  // Unsupported character
    
    // Set address window for entire character to minimize SPI overhead
    set_address_window(x, y, LARGE_FONT_WIDTH, LARGE_FONT_HEIGHT);
    set_dc_data();  // Switch to data mode once
    
    // Stream entire character as pixel data
    for (uint8_t row = 0; row < LARGE_FONT_HEIGHT; row++) {
        uint16_t font_row = large_font16x16[char_index][row];
        
        for (uint8_t col = 0; col < LARGE_FONT_WIDTH; col++) {
            // Read bit from font data (MSB first for 16x16)
            if (font_row & (0x8000 >> col)) {
                spi_write_word_bitbang(color);     // Foreground
            } else {
                spi_write_word_bitbang(bg_color);  // Background
            }
        }
        
        // Reset task occasionally for large characters
        if ((row % 8) == 0) {
            taskYIELD();
        }
    }
}

// Draw a string with large font (16x16)
static void draw_large_string(uint16_t x, uint16_t y, const char* str, uint16_t color, uint16_t bg_color) {
    uint16_t cur_x = x;
    uint16_t cur_y = y;
    uint16_t char_count = 0;
    
    while (*str) {
        if (*str == '\n') {
            // New line
            cur_x = x;
            cur_y += LARGE_FONT_HEIGHT + 4;  // Add 4 pixels line spacing for large font
        } else if (*str == '\r') {
            // Carriage return
            cur_x = x;
        } else {
            // Bounds check before drawing character
            if (cur_x + LARGE_FONT_WIDTH <= 240 && cur_y + LARGE_FONT_HEIGHT <= 240) {
                draw_large_char(cur_x, cur_y, *str, color, bg_color);
            }
            cur_x += LARGE_FONT_WIDTH + 2;  // Add 2 pixels character spacing for large font
            
            // Wrap to next line if text exceeds display width
            if (cur_x + LARGE_FONT_WIDTH > 240) {
                cur_x = x;
                cur_y += LARGE_FONT_HEIGHT + 4;
            }
        }
        str++;
        char_count++;
        
        // Only yield for very long strings
        if ((char_count % 5) == 0) {
            taskYIELD(); // Brief yield for large font operations
        }
        
        // Stop if text exceeds display height
        if (cur_y + LARGE_FONT_HEIGHT > 240) break;
    }
}

// Public API functions for external use

/**
 * @brief Draw a single pixel at specified coordinates
 * 
 * Sets a single pixel on the display to the specified color. Includes bounds
 * checking to prevent drawing outside the 240x240 display area.
 * 
 * @param x X coordinate (0-239)
 * @param y Y coordinate (0-239)
 * @param color 16-bit RGB565 color value
 */
void st7789_draw_pixel(uint16_t x, uint16_t y, uint16_t color) {
    draw_pixel(x, y, color);
}

/**
 * @brief Fill a rectangular area with specified color
 * 
 * Efficiently fills a rectangular region with a solid color using optimized
 * SPI streaming. Includes cooperative multitasking yields for large rectangles.
 * 
 * @param x X coordinate of top-left corner
 * @param y Y coordinate of top-left corner
 * @param w Width of rectangle in pixels
 * @param h Height of rectangle in pixels
 * @param color 16-bit RGB565 color value
 */
void st7789_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    fill_rect(x, y, w, h, color);
}

/**
 * @brief Draw a single character using 8x8 font
 * 
 * Renders a single ASCII character (32-126) using the built-in 8x8 pixel font.
 * Optimized for speed with minimal SPI overhead by setting the address window once.
 * 
 * @param x X coordinate for character placement
 * @param y Y coordinate for character placement
 * @param c Character to draw (printable ASCII 32-126)
 * @param color 16-bit RGB565 foreground color
 * @param bg_color 16-bit RGB565 background color
 */
void st7789_draw_char(uint16_t x, uint16_t y, char c, uint16_t color, uint16_t bg_color) {
    draw_char(x, y, c, color, bg_color);
}

/**
 * @brief Draw a text string using 8x8 font
 * 
 * Renders a null-terminated string with automatic word wrapping and newline support.
 * Includes cooperative multitasking yields for long strings to prevent watchdog timeouts.
 * 
 * @param x X coordinate for text start position
 * @param y Y coordinate for text start position
 * @param str Null-terminated string to draw
 * @param color 16-bit RGB565 foreground color
 * @param bg_color 16-bit RGB565 background color
 */
void st7789_draw_string(uint16_t x, uint16_t y, const char* str, uint16_t color, uint16_t bg_color) {
    draw_string(x, y, str, color, bg_color);
}

/**
 * @brief Clear entire 240x240 display with specified color
 * 
 * Efficiently fills the entire display memory with a single color.
 * Equivalent to fill_rect(0, 0, 240, 240, color) but more explicit.
 * 
 * @param color 16-bit RGB565 color value to fill the screen
 */
void st7789_clear_screen(uint16_t color) {
    fill_rect(0, 0, 240, 240, color);
}

/**
 * @brief Draw a single character using 16x16 large font
 * 
 * Renders a character using the large 16x16 pixel font. Supports a limited
 * character set optimized for sensor displays: numbers, symbols, and select letters.
 * Includes periodic task yields during rendering to maintain system responsiveness.
 * 
 * @param x X coordinate for character placement
 * @param y Y coordinate for character placement
 * @param c Character to draw (supported: 0-9, ., %, :, A,C,D,E,H,I,M,N,P,R,S,T,U,Y)
 * @param color 16-bit RGB565 foreground color
 * @param bg_color 16-bit RGB565 background color
 */
void st7789_draw_large_char(uint16_t x, uint16_t y, char c, uint16_t color, uint16_t bg_color) {
    draw_large_char(x, y, c, color, bg_color);
}

/**
 * @brief Draw a text string using 16x16 large font
 * 
 * Renders a string with large, easily readable characters. Perfect for sensor
 * readings and important information. Includes automatic wrapping and newline support.
 * Uses cooperative multitasking to prevent watchdog timeouts during rendering.
 * 
 * @param x X coordinate for text start position
 * @param y Y coordinate for text start position
 * @param str Null-terminated string (supported chars: 0-9, ., %, :, A,C,D,E,H,I,M,N,P,R,S,T,U,Y)
 * @param color 16-bit RGB565 foreground color
 * @param bg_color 16-bit RGB565 background color
 */
void st7789_draw_large_string(uint16_t x, uint16_t y, const char* str, uint16_t color, uint16_t bg_color) {
    draw_large_string(x, y, str, color, bg_color);
}

/**
 * @brief Initialize the ST7789 240x240 TFT display
 * 
 * Performs complete initialization sequence including GPIO configuration,
 * hardware reset, and ST7789 controller setup. Uses bit-banging SPI for
 * maximum compatibility with displays that don't have CS pins.
 * 
 * Initialization sequence:
 * 1. Configure GPIO pins for SPI and control signals
 * 2. Perform hardware reset cycle
 * 3. Send ST7789 initialization commands
 * 4. Configure display for RGB565 color mode
 * 5. Clear display memory
 * 
 * @return ESP_OK on successful initialization, ESP_FAIL on error
 */
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

    // Set initial pin states
    digitalWrite(ST7789_SCK_PIN, 1);  // SPI Mode 0: CLK idle high
    digitalWrite(ST7789_SDA_PIN, 0);  // MOSI idle low
    digitalWrite(ST7789_DC_PIN, 1);   // Data mode default
    
    ESP_LOGI(TAG, "GPIO pins configured for bit-banging SPI");
    ESP_LOGI(TAG, "Pin configuration: RST=%d, DC=%d, SDA=%d, SCK=%d",
             ST7789_RST_PIN, ST7789_DC_PIN, ST7789_SDA_PIN, ST7789_SCK_PIN);
    
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

/**
 * @brief Run comprehensive display functionality test
 * 
 * Executes a complete test sequence to verify display operation and color accuracy.
 * Tests include solid color fills, multi-color patterns, and text rendering with
 * the 8x8 font. Useful for validating display connections and driver functionality.
 * 
 * Test sequence:
 * 1. Full-screen color fills (Red, Green, Blue, White, Black)
 * 2. Multi-color pattern with positioned squares
 * 3. Text rendering demonstration with various colors
 * 4. Special characters and multiline text display
 */
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

/**
 * @brief Test large font functionality with sensor-style display
 * 
 * Demonstrates the 16x16 large font capabilities by showing sample sensor
 * readings in a typical IoT monitoring format. Tests character rendering,
 * color coding, and layout positioning for temperature, humidity, and distance.
 * 
 * Test sequence:
 * 1. Initial sensor reading display (TEMP, HUMIDITY, DISTANCE)
 * 2. Updated values to show dynamic content capability
 * 3. Validates all supported large font characters including numbers,
 *    symbols (., %, :), and uppercase letters
 */
void st7789_large_font_test(void) {
    ESP_LOGI(TAG, "Starting large font test...");
    
    // Clear screen to black
    st7789_clear_screen(BLACK);
    delay_ms(500);
    
    // Test large font with sensor-style display
    ESP_LOGI(TAG, "Large Font Test: Sensor Display Demo");
    
    // Display temperature
    draw_large_string(10, 20, "TEMP:", WHITE, BLACK);
    draw_large_string(10, 50, "22.5C", RED, BLACK);
    
    draw_large_string(10, 90, "HUMIDITY:", WHITE, BLACK);
    draw_large_string(10, 120, "40%", BLUE, BLACK);
    
    draw_large_string(10, 150, "DISTANCE:", WHITE, BLACK);
    draw_large_string(10, 180, "10.1CM", GREEN, BLACK);
    
    delay_ms(3000);
    
    // Test with different values
    ESP_LOGI(TAG, "Large Font Test: Updated Values");
    
    // Clear and show updated values
    st7789_clear_screen(BLACK);
    
    draw_large_string(10, 20, "TEMP:", WHITE, BLACK);
    draw_large_string(10, 50, "22.1C", RED, BLACK);
    
    draw_large_string(10, 90, "HUMIDITY:", WHITE, BLACK);
    draw_large_string(10, 120, "70%", BLUE, BLACK);
    
    draw_large_string(10, 150, "DISTANCE:", WHITE, BLACK);
    draw_large_string(10, 180, "8.2CM", GREEN, BLACK);
    
    delay_ms(3000);
    
    ESP_LOGI(TAG, "Large font test completed successfully!");
}
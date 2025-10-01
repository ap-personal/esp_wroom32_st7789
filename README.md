# ESP32 ST7789 TFT Display Driver

A high-performance, pure C implementation of an ST7789 TFT display driver for ESP32 using ESP-IDF framework. This driver provides full control over 240x240 pixel ST7789 displays with optimized SPI communication.

## Features

- **Pure C Implementation**: Clean, efficient C code without external dependencies
- **High-Speed SPI**: Optimized SPI communication at 27MHz for fast refresh rates
- **No CS Pin Required**: Designed for ST7789 displays without chip select pins
- **Polling-Based Transmission**: Immediate SPI transmission for low latency
- **RGB565 Color Format**: 16-bit color support with 65,536 colors
- **Hardware Reset Support**: Proper reset sequence for reliable initialization

## Hardware Requirements

### ST7789 240x240 TFT Display Connections

| ST7789 Pin | ESP32 Pin | Function |
|------------|-----------|----------|
| GND        | GND       | Ground |
| VCC        | 3.3V      | Power Supply |
| SCK        | GPIO 18   | SPI Clock |
| SDA        | GPIO 23   | SPI Data (MOSI) |
| RES        | GPIO 4    | Reset (active low) |
| DC         | GPIO 2    | Data/Command Select |
| BLK        | GPIO 15   | Backlight Control |
| CS         | Not Used  | Chip Select (not required) |

**Note**: This driver is specifically designed for ST7789 displays that don't have a CS (Chip Select) pin or have it permanently tied low.

## Project Structure

```
02_esp32_tft_display/
├── components/
│   └── st7789/
│       ├── st7789.c         # Main driver implementation
│       ├── st7789.h         # Header file with API definitions
│       └── CMakeLists.txt   # Component build configuration
├── main/
│   ├── main.c              # Application entry point
│   └── CMakeLists.txt      # Main component build configuration
├── CMakeLists.txt          # Project-level build configuration
└── README.md               # This file
```

## How It Works

### 1. SPI Communication Architecture

The driver uses ESP-IDF's SPI master driver configured for optimal performance:

```c
spi_device_interface_config_t devcfg = {
    .clock_speed_hz = 27000000,     // 27MHz for fast data transfer
    .mode = 0,                      // SPI mode 0 (CPOL=0, CPHA=0)
    .spics_io_num = -1,             // No CS pin required
    .queue_size = 1,                // Minimal queue for immediate transmission
    .flags = SPI_DEVICE_NO_DUMMY | SPI_DEVICE_HALFDUPLEX
};
```

**Key Features:**
- **27MHz Clock**: High-speed data transfer for smooth graphics
- **No CS Pin**: Configured for displays without chip select
- **Polling Transmission**: Uses `spi_device_polling_transmit()` for immediate data send
- **Half-Duplex Mode**: Optimized for display-only communication

### 2. Display Initialization Sequence

The ST7789 requires a specific initialization sequence for proper operation:

1. **Hardware Reset**: 10ms low pulse on RST pin, followed by 150ms delay
2. **Software Reset**: `SWRESET` command with 150ms delay
3. **Sleep Out**: `SLPOUT` command with 255ms delay
4. **Color Mode**: Set to 16-bit RGB565 format (`COLMOD` = 0x55)
5. **Memory Access Control**: Configure scan direction (`MADCTL` = 0x00)
6. **Display Inversion**: Enable for correct color representation (`INVON`)
7. **Normal Mode**: Set normal display mode (`NORON`)
8. **Display On**: Final activation (`DISPON`)

### 3. Memory Addressing System

The ST7789 uses a coordinate-based addressing system:

```c
static void set_address_window(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    uint16_t x_end = x + w - 1;
    uint16_t y_end = y + h - 1;
    
    write_command(ST7789_CASET);   // Column Address Set
    write_data_word(x);            // X start position
    write_data_word(x_end);        // X end position
    
    write_command(ST7789_RASET);   // Row Address Set  
    write_data_word(y);            // Y start position
    write_data_word(y_end);        // Y end position
    
    write_command(ST7789_RAMWR);   // Memory Write command
}
```

**Process:**
1. **CASET**: Define column (X) boundaries
2. **RASET**: Define row (Y) boundaries  
3. **RAMWR**: Enter memory write mode
4. **Data Stream**: Send pixel data sequentially

### 4. Color Format (RGB565)

The driver uses 16-bit RGB565 color format:

```
Bit:  15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
      |  R  |      G     |     B    |
      Red    Green        Blue
      5-bit  6-bit        5-bit
```

**Color Calculation:**
- Red: (R & 0x1F) << 11
- Green: (G & 0x3F) << 5  
- Blue: (B & 0x1F)

**Predefined Colors:**
- RED: 0xF800, GREEN: 0x07E0, BLUE: 0x001F
- WHITE: 0xFFFF, BLACK: 0x0000, YELLOW: 0xFFE0

### 5. Data/Command Protocol

The ST7789 distinguishes between commands and data using the DC pin:

```c
static void write_command(uint8_t cmd) {
    gpio_set_level(ST7789_DC_PIN, 0);  // DC low = command
    spi_write_byte(cmd);
}

static void write_data(uint8_t data) {
    gpio_set_level(ST7789_DC_PIN, 1);  // DC high = data
    spi_write_byte(data);
}
```

### 6. Pixel Rendering Process

For drawing graphics, the driver follows this sequence:

1. **Set Address Window**: Define the rectangular area to draw
2. **Set Data Mode**: Pull DC pin high
3. **Stream Pixels**: Send RGB565 color data for each pixel
4. **Auto-Increment**: Display automatically advances to next pixel

```c
static void fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    set_address_window(x, y, w, h);
    
    uint32_t pixels = (uint32_t)w * h;
    set_dc_data();
    
    for (uint32_t i = 0; i < pixels; i++) {
        spi_write_word(color);  // Send 16-bit color data
    }
}
```

## API Reference

### Initialization Functions

#### `esp_err_t st7789_init(void)`
Initializes the ST7789 display driver.

**Returns:**
- `ESP_OK`: Successful initialization
- `ESP_FAIL`: Initialization failed

**Process:**
1. Configure GPIO pins for SPI and control signals
2. Initialize SPI bus with optimized settings
3. Perform hardware reset sequence
4. Send ST7789 initialization commands
5. Enable display and backlight

### Test Functions

#### `void st7789_test(void)`
Runs a comprehensive test sequence to verify display functionality.

**Test Sequence:**
1. **Red Screen**: Fill entire display with red color (2 seconds)
2. **White Screen**: Fill entire display with white color (2 seconds)  
3. **Color Pattern**: Draw colored squares in corners and center
   - Red square (top-left)
   - Green square (top-right)
   - Blue square (bottom-left)
   - Yellow square (bottom-right)
   - White square (center)

## Building and Flashing

### Prerequisites

- ESP-IDF v5.5 or later
- ESP32 development board
- ST7789 240x240 TFT display

### Build Commands

```bash
# Set up ESP-IDF environment
C:\Espressif\frameworks\esp-idf-v5.5\export.bat

# Clean previous builds
idf.py fullclean

# Build the project
idf.py build

# Flash to ESP32
idf.py flash

# Monitor output
idf.py monitor
```

### Expected Output

When successful, you should see:

1. **Serial Output**: Initialization messages and test progress
2. **Display Output**: 
   - Brief red screen
   - Brief white screen  
   - Color pattern with squares in corners and center

## Technical Specifications

- **Display Resolution**: 240x240 pixels
- **Color Depth**: 16-bit (65,536 colors)
- **SPI Speed**: 27MHz
- **Memory Usage**: ~2KB for driver code
- **Refresh Rate**: Up to 60 FPS (depending on content)
- **Power Consumption**: ~20mA (display) + ~5mA (ESP32 SPI)

## Troubleshooting

### Display Shows Only Backlight

**Possible Causes:**
1. Incorrect wiring connections
2. Wrong pin definitions in `st7789.h`
3. SPI timing issues
4. Display variant compatibility

**Solutions:**
1. Verify all connections match the pin table
2. Check pin definitions in header file
3. Try lower SPI speeds (reduce `clock_speed_hz`)
4. Ensure your display is ST7789-compatible

### No Backlight

**Possible Causes:**
1. BLK pin not connected
2. BLK pin inverted logic
3. Power supply issues

**Solutions:**
1. Connect BLK to GPIO 15
2. Try inverting backlight logic in code
3. Verify 3.3V power supply capacity

### Incorrect Colors

**Possible Causes:**
1. RGB/BGR color order mismatch
2. Incorrect color format
3. Display variant differences

**Solutions:**
1. Try different `MADCTL` register values
2. Verify RGB565 color calculations
3. Check display datasheet for color format

## License

This project is provided as-is for educational and development purposes. Feel free to modify and adapt for your specific needs.

## Contributing

Contributions are welcome! Please ensure code follows the existing style and includes appropriate documentation.
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "st7789.h"

static const char *TAG = "MAIN";

void app_main(void)
{

    ESP_LOGI(TAG, "    ST7789 TFT Display Driver Demo");
    
    /* Initialize ST7789 display driver */
    ESP_LOGI(TAG, "Initializing ST7789 display driver...");
    esp_err_t ret = st7789_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize ST7789 display: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "Display driver initialization completed successfully!");
    
    /* Execute display functionality tests */
    ESP_LOGI(TAG, "Starting display test sequence...");
    st7789_test();
    ESP_LOGI(TAG, "   Display test sequence completed!");
}
#include "status_led.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "led";

#define GPIO_STATUS_LED  GPIO_NUM_8

/* TODO: implement LED blink patterns driven by a FreeRTOS task.
 * See docs/high-level-design.md section 3.2.1. */

esp_err_t status_led_init(void)
{
    ESP_LOGI(TAG, "Status LED init (stub)");

    gpio_config_t conf = {
        .pin_bit_mask = (1ULL << GPIO_STATUS_LED),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&conf);
    gpio_set_level(GPIO_STATUS_LED, 0);

    return ESP_OK;
}

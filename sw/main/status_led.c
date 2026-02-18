#include "status_led.h"
#include "motor_control.h"
#include "esp_log.h"
#include "led_strip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "led";

#define LED_GPIO    48
#define LED_MAX_VAL 80  /* cap brightness to avoid blinding (0-255) */
#define LED_UPDATE_INTERVAL_MS 100

static led_strip_handle_t s_led;

void status_led_update(void)
{
    if (!s_led) return;

    if (!motor_get_enable() || motor_get_current_speed() == 0) {
        led_strip_clear(s_led);
        led_strip_refresh(s_led);
        return;
    }

    /* Map speed 0-1000 → brightness 0-LED_MAX_VAL */
    uint32_t brightness = (uint32_t)motor_get_current_speed() * LED_MAX_VAL / 1000;

    uint32_t r = 0, g = 0;
    if (motor_get_direction_forward()) {
        g = brightness;  /* green = forward */
    } else {
        r = brightness;  /* red = reverse */
    }

    led_strip_set_pixel(s_led, 0, r, g, 0);
    led_strip_refresh(s_led);
}

static void led_task(void *arg)
{
    (void)arg;
    for (;;) {
        status_led_update();
        vTaskDelay(pdMS_TO_TICKS(LED_UPDATE_INTERVAL_MS));
    }
}

esp_err_t status_led_init(void)
{
    ESP_LOGI(TAG, "Status LED init (WS2812 on GPIO %d)", LED_GPIO);

    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_GPIO,
        .max_leds = 1,
    };
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000,  /* 10 MHz */
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &s_led));
    led_strip_clear(s_led);
    led_strip_refresh(s_led);

    xTaskCreate(led_task, "status_led", 2048, NULL, tskIDLE_PRIORITY + 1, NULL);

    return ESP_OK;
}

#include "status_led.h"
#include "motor_control.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "led_strip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "led";

#define LED_GPIO    7
#define LED_MAX_VAL 80  /* cap brightness to avoid blinding (0-255) */
#define LED_UPDATE_INTERVAL_MS 100

static led_strip_handle_t s_led;

static bool     s_override_active = false;
static uint8_t  s_override_r = 0, s_override_g = 0, s_override_b = 0;
static bool     s_flash_active = false;
static esp_timer_handle_t s_flash_timer;

static void write_pixel(uint8_t r, uint8_t g, uint8_t b)
{
    if (!s_led) return;
    led_strip_set_pixel(s_led, 0, r, g, b);
    led_strip_refresh(s_led);
}

static void flash_end_cb(void *arg)
{
    (void)arg;
    s_flash_active = false;
    if (s_override_active) {
        write_pixel(s_override_r, s_override_g, s_override_b);
    } else {
        status_led_update();
    }
}

void status_led_set_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    s_override_active = true;
    s_override_r = r;
    s_override_g = g;
    s_override_b = b;
    if (!s_flash_active) write_pixel(r, g, b);
}

void status_led_flash_rgb(uint8_t r, uint8_t g, uint8_t b, uint32_t duration_ms)
{
    if (!s_led) return;
    s_flash_active = true;
    write_pixel(r, g, b);
    esp_timer_stop(s_flash_timer);
    esp_timer_start_once(s_flash_timer, (uint64_t)duration_ms * 1000);
}

void status_led_clear_override(void)
{
    s_override_active = false;
    if (!s_flash_active) status_led_update();
}

void status_led_update(void)
{
    if (!s_led) return;
    if (s_override_active || s_flash_active) return;

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

    const esp_timer_create_args_t flash_args = {
        .callback = flash_end_cb,
        .name     = "led_flash",
    };
    ESP_ERROR_CHECK(esp_timer_create(&flash_args, &s_flash_timer));

    xTaskCreate(led_task, "status_led", 2048, NULL, tskIDLE_PRIORITY + 1, NULL);

    return ESP_OK;
}

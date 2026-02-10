#include <stdio.h>
#include "esp_log.h"
#include "nvs_flash.h"

#include "config.h"
#include "wifi_manager.h"
#include "motor_control.h"
#include "modbus_server.h"
#include "provisioning.h"
#include "status_led.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "Traincontrol starting");

    /* Initialise NVS — required for WiFi and config storage */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    config_init();
    status_led_init();
    motor_control_init();
    wifi_manager_init();
    provisioning_init();
    modbus_server_init();

    ESP_LOGI(TAG, "Traincontrol ready");
}

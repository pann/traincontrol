#include "wifi_manager.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"

static const char *TAG = "wifi";

/* TODO: implement WiFi station connect, reconnect logic, and event handlers. */

esp_err_t wifi_manager_init(void)
{
    ESP_LOGI(TAG, "WiFi manager init (stub)");

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    return ESP_OK;
}

#include "wifi_manager.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "event_log.h"

static const char *TAG = "wifi";

/* TODO: implement WiFi station connect, reconnect logic, and event handlers.
 *
 * Event logging reference — add these calls when implementing:
 *   event_log(EVT_WIFI, "connecting ssid=%s", ssid);
 *   event_log(EVT_WIFI, "connected ip=" IPSTR, IP2STR(&ip));
 *   event_log(EVT_WIFI, "disconnected reason=%d", reason);
 *   event_log(EVT_WIFI, "reconnecting attempt=%d", attempt);
 */

esp_err_t wifi_manager_init(void)
{
    ESP_LOGI(TAG, "WiFi manager init (stub)");

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    event_log(EVT_WIFI, "init (stub)");
    return ESP_OK;
}

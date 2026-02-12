#include "provisioning.h"
#include "esp_log.h"
#include "event_log.h"

static const char *TAG = "prov";

/* TODO: implement SoftAP captive portal and USB CDC/ACM CLI.
 * See docs/high-level-design.md section 3.4.
 *
 * Event logging reference — add these calls when implementing:
 *   event_log(EVT_SYSTEM, "softap_started ssid=%s", ap_ssid);
 *   event_log(EVT_SYSTEM, "provisioning_complete");
 *   event_log(EVT_CONFIG, "wifi_credentials_received ssid=%s", ssid);
 */

esp_err_t provisioning_init(void)
{
    ESP_LOGI(TAG, "Provisioning init (stub)");
    event_log(EVT_SYSTEM, "provisioning init (stub)");
    return ESP_OK;
}

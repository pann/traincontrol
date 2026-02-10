#include "provisioning.h"
#include "esp_log.h"

static const char *TAG = "prov";

/* TODO: implement SoftAP captive portal and USB CDC/ACM CLI.
 * See docs/high-level-design.md section 3.4. */

esp_err_t provisioning_init(void)
{
    ESP_LOGI(TAG, "Provisioning init (stub)");
    return ESP_OK;
}

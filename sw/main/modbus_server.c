#include "modbus_server.h"
#include "motor_control.h"
#include "esp_log.h"

static const char *TAG = "modbus";

/* TODO: implement Modbus TCP server using esp-modbus component.
 * Register map as defined in docs/high-level-design.md section 3.3. */

esp_err_t modbus_server_init(void)
{
    ESP_LOGI(TAG, "Modbus TCP server init (stub)");
    return ESP_OK;
}

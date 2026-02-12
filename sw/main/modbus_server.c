#include "modbus_server.h"
#include "motor_control.h"
#include "esp_log.h"
#include "event_log.h"

static const char *TAG = "modbus";

/* TODO: implement Modbus TCP server using esp-modbus component.
 * Register map as defined in docs/high-level-design.md section 3.3.
 *
 * Event logging reference — add these calls when implementing:
 *   event_log(EVT_MODBUS, "client_connected ip=" IPSTR, IP2STR(&ip));
 *   event_log(EVT_MODBUS, "write_register addr=0x%04x val=%u", addr, val);
 *   event_log(EVT_MODBUS, "read_register addr=0x%04x val=%u", addr, val);
 *   event_log(EVT_MODBUS, "client_disconnected");
 */

esp_err_t modbus_server_init(void)
{
    ESP_LOGI(TAG, "Modbus TCP server init (stub)");
    event_log(EVT_MODBUS, "init (stub)");
    return ESP_OK;
}

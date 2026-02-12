#include "event_log.h"
#include "esp_log.h"

#include <stdarg.h>
#include <stdio.h>

static const char *TAG = "event";

static const char *s_category_names[] = {
    [EVT_SYSTEM] = "SYSTEM",
    [EVT_MOTOR]  = "MOTOR",
    [EVT_WIFI]   = "WIFI",
    [EVT_MODBUS] = "MODBUS",
    [EVT_CONFIG] = "CONFIG",
};

esp_err_t event_log_init(void)
{
    ESP_LOGI(TAG, "Event logging initialised");
    return ESP_OK;
}

void event_log(event_category_t category, const char *fmt, ...)
{
    const char *cat_name = "?";
    if (category < EVT_CATEGORY_COUNT) {
        cat_name = s_category_names[category];
    }

    char buf[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    ESP_LOGI(TAG, "[%-6s] %s", cat_name, buf);
}

#include "config.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>

static const char *TAG = "config";
static const char *NVS_NAMESPACE = "trainctrl";

static char s_ssid[33]     = {0};
static char s_password[65] = {0};

esp_err_t config_init(void)
{
    ESP_LOGI(TAG, "Loading config from NVS");

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs);
    if (err == ESP_OK) {
        size_t len = sizeof(s_ssid);
        nvs_get_str(nvs, "wifi_ssid", s_ssid, &len);
        len = sizeof(s_password);
        nvs_get_str(nvs, "wifi_pass", s_password, &len);
        nvs_close(nvs);
    }

    return ESP_OK;
}

bool config_has_wifi_credentials(void)
{
    return s_ssid[0] != '\0';
}

const char *config_get_wifi_ssid(void)     { return s_ssid; }
const char *config_get_wifi_password(void) { return s_password; }

esp_err_t config_set_wifi_credentials(const char *ssid, const char *password)
{
    nvs_handle_t nvs;
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs));
    ESP_ERROR_CHECK(nvs_set_str(nvs, "wifi_ssid", ssid));
    ESP_ERROR_CHECK(nvs_set_str(nvs, "wifi_pass", password));
    ESP_ERROR_CHECK(nvs_commit(nvs));
    nvs_close(nvs);

    strncpy(s_ssid, ssid, sizeof(s_ssid) - 1);
    strncpy(s_password, password, sizeof(s_password) - 1);

    ESP_LOGI(TAG, "WiFi credentials stored");
    return ESP_OK;
}

esp_err_t config_factory_reset(void)
{
    ESP_LOGW(TAG, "Factory reset — erasing NVS namespace");
    nvs_handle_t nvs;
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs));
    ESP_ERROR_CHECK(nvs_erase_all(nvs));
    ESP_ERROR_CHECK(nvs_commit(nvs));
    nvs_close(nvs);
    memset(s_ssid, 0, sizeof(s_ssid));
    memset(s_password, 0, sizeof(s_password));
    return ESP_OK;
}

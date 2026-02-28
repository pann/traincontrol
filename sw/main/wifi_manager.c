#include "wifi_manager.h"
#include "config.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "event_log.h"
#include "mdns.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "wifi";

static esp_netif_t *s_sta_netif = NULL;
static bool s_is_connected = false;
static char s_ip_str[16] = {0};
static int  s_retry_count = 0;

#define WIFI_MAX_RETRY          10
#define WIFI_RETRY_INTERVAL_MS  5000

static char s_hostname[32] = {0};

static void build_hostname(void)
{
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(s_hostname, sizeof(s_hostname), "trainctrl-%02x%02x",
             mac[4], mac[5]);
}

static void wifi_event_handler(void *arg, esp_event_base_t base,
                                int32_t event_id, void *event_data)
{
    (void)arg;

    if (base == WIFI_EVENT) {
        switch (event_id) {
        case WIFI_EVENT_STA_START:
            esp_wifi_connect();
            break;

        case WIFI_EVENT_STA_DISCONNECTED: {
            wifi_event_sta_disconnected_t *evt = event_data;
            s_is_connected = false;
            memset(s_ip_str, 0, sizeof(s_ip_str));
            event_log(EVT_WIFI, "disconnected reason=%d", evt->reason);

            if (s_retry_count < WIFI_MAX_RETRY) {
                s_retry_count++;
                event_log(EVT_WIFI, "reconnecting attempt=%d", s_retry_count);
                vTaskDelay(pdMS_TO_TICKS(WIFI_RETRY_INTERVAL_MS));
                esp_wifi_connect();
            } else {
                event_log(EVT_WIFI, "max retries reached");
            }
            break;
        }
        default:
            break;
        }
    } else if (base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *evt = event_data;
        snprintf(s_ip_str, sizeof(s_ip_str), IPSTR, IP2STR(&evt->ip_info.ip));
        s_is_connected = true;
        s_retry_count = 0;
        event_log(EVT_WIFI, "connected ip=%s", s_ip_str);

        mdns_init();
        mdns_hostname_set(s_hostname);
        mdns_service_add(NULL, "_modbus", "_tcp", 502, NULL, 0);
        ESP_LOGI(TAG, "mDNS: %s._modbus._tcp", s_hostname);
    }
}

esp_err_t wifi_manager_init(void)
{
    ESP_LOGI(TAG, "WiFi manager init");

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    s_sta_netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    build_hostname();

    if (config_has_wifi_credentials()) {
        event_log(EVT_WIFI, "credentials found, starting STA");
        wifi_manager_start_sta();
    } else {
        event_log(EVT_WIFI, "no credentials, waiting for provisioning");
    }

    return ESP_OK;
}

esp_err_t wifi_manager_start_sta(void)
{
    const char *ssid = config_get_wifi_ssid();
    const char *pass = config_get_wifi_password();

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, pass, sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    s_retry_count = 0;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    event_log(EVT_WIFI, "connecting ssid=%s", ssid);
    return ESP_OK;
}

esp_err_t wifi_manager_stop(void)
{
    mdns_service_remove_all();
    mdns_free();
    esp_wifi_disconnect();
    esp_wifi_stop();
    s_is_connected = false;
    memset(s_ip_str, 0, sizeof(s_ip_str));
    event_log(EVT_WIFI, "stopped");
    return ESP_OK;
}

bool wifi_manager_is_connected(void)
{
    return s_is_connected;
}

const char *wifi_manager_get_ip_str(void)
{
    return s_ip_str;
}

#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    WIFI_MODE_NULL = 0,
    WIFI_MODE_STA,
    WIFI_MODE_AP,
    WIFI_MODE_APSTA,
} wifi_mode_t;

typedef enum {
    WIFI_IF_STA = 0,
    WIFI_IF_AP,
} wifi_interface_t;

typedef enum {
    WIFI_AUTH_OPEN = 0,
    WIFI_AUTH_WPA2_PSK = 3,
} wifi_auth_mode_t;

typedef struct {
    uint8_t ssid[32];
    uint8_t password[64];
    struct {
        wifi_auth_mode_t authmode;
    } threshold;
} wifi_sta_config_t;

typedef struct {
    uint8_t ssid[32];
    uint8_t password[64];
    uint8_t ssid_len;
    uint8_t channel;
    wifi_auth_mode_t authmode;
    uint8_t max_connection;
} wifi_ap_config_t;

typedef union {
    wifi_sta_config_t sta;
    wifi_ap_config_t ap;
} wifi_config_t;

typedef struct {
    int dummy;
} wifi_init_config_t;

#define WIFI_INIT_CONFIG_DEFAULT() { .dummy = 0 }

typedef struct {
    uint8_t ssid[32];
    uint8_t ssid_len;
    uint8_t bssid[6];
    uint8_t reason;
} wifi_event_sta_disconnected_t;

#define WIFI_EVENT_STA_START        2
#define WIFI_EVENT_STA_DISCONNECTED 5

esp_err_t esp_wifi_init(const wifi_init_config_t *config);
esp_err_t esp_wifi_set_mode(wifi_mode_t mode);
esp_err_t esp_wifi_set_config(wifi_interface_t interface, wifi_config_t *conf);
esp_err_t esp_wifi_start(void);
esp_err_t esp_wifi_stop(void);
esp_err_t esp_wifi_connect(void);
esp_err_t esp_wifi_disconnect(void);

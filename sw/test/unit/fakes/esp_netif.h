#pragma once

#include "esp_err.h"

struct esp_netif_obj { int dummy; };
typedef struct esp_netif_obj esp_netif_t;

esp_err_t esp_netif_init(void);
esp_netif_t *esp_netif_create_default_wifi_sta(void);
esp_netif_t *esp_netif_create_default_wifi_ap(void);
void esp_netif_destroy(esp_netif_t *netif);

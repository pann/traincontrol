#pragma once

#include "esp_err.h"
#include <stdint.h>

typedef const char *esp_event_base_t;

#define ESP_EVENT_ANY_ID (-1)

extern esp_event_base_t WIFI_EVENT;
extern esp_event_base_t IP_EVENT;

#define IP_EVENT_STA_GOT_IP 0

typedef struct {
    uint32_t addr;
} esp_ip4_addr_t;

typedef struct {
    esp_ip4_addr_t ip;
    esp_ip4_addr_t netmask;
    esp_ip4_addr_t gw;
} esp_netif_ip_info_t;

typedef struct {
    esp_netif_ip_info_t ip_info;
    uint32_t ip_changed;
} ip_event_got_ip_t;

#define IPSTR           "%d.%d.%d.%d"
#define IP2STR(ipaddr)  \
    (int)(((ipaddr)->addr >>  0) & 0xFF), \
    (int)(((ipaddr)->addr >>  8) & 0xFF), \
    (int)(((ipaddr)->addr >> 16) & 0xFF), \
    (int)(((ipaddr)->addr >> 24) & 0xFF)

typedef void (*esp_event_handler_t)(void *event_handler_arg,
                                     esp_event_base_t event_base,
                                     int32_t event_id,
                                     void *event_data);

typedef void *esp_event_handler_instance_t;

esp_err_t esp_event_loop_create_default(void);
esp_err_t esp_event_handler_instance_register(esp_event_base_t event_base,
                                               int32_t event_id,
                                               esp_event_handler_t event_handler,
                                               void *event_handler_arg,
                                               esp_event_handler_instance_t *instance);

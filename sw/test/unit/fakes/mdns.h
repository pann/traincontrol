#pragma once

#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>

typedef struct {
    const char *key;
    const char *value;
} mdns_txt_item_t;

esp_err_t mdns_init(void);
esp_err_t mdns_hostname_set(const char *hostname);
esp_err_t mdns_service_add(const char *instance_name,
                            const char *service_type,
                            const char *proto,
                            uint16_t port,
                            const mdns_txt_item_t *txt,
                            size_t num_items);
esp_err_t mdns_service_remove_all(void);
void mdns_free(void);

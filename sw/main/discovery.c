#include "discovery.h"
#include "wifi_manager.h"
#include "esp_log.h"
#include "lwip/sockets.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "discovery";

#define DISCOVERY_PORT      55502
#define BEACON_INTERVAL_MS  5000

static TaskHandle_t s_task = NULL;
static char         s_hostname[32];

static void beacon_task(void *arg)
{
    (void)arg;

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGE(TAG, "socket() failed: errno=%d", errno);
        s_task = NULL;
        vTaskDelete(NULL);
        return;
    }

    int enable = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &enable, sizeof(enable)) < 0) {
        ESP_LOGE(TAG, "SO_BROADCAST failed: errno=%d", errno);
        close(sock);
        s_task = NULL;
        vTaskDelete(NULL);
        return;
    }

    struct sockaddr_in dst = {
        .sin_family = AF_INET,
        .sin_port   = htons(DISCOVERY_PORT),
        .sin_addr.s_addr = htonl(INADDR_BROADCAST),
    };

    char payload[192];
    ESP_LOGI(TAG, "UDP broadcast beacon on port %d every %d ms",
             DISCOVERY_PORT, BEACON_INTERVAL_MS);

    for (;;) {
        const char *ip = wifi_manager_get_ip_str();
        if (ip && ip[0]) {
            int n = snprintf(payload, sizeof(payload),
                "{\"app\":\"traincontrol\",\"name\":\"%s\",\"ip\":\"%s\",\"modbus\":502}",
                s_hostname, ip);
            if (n > 0) {
                ssize_t sent = sendto(sock, payload, n, 0,
                                      (struct sockaddr *)&dst, sizeof(dst));
                if (sent < 0) {
                    ESP_LOGW(TAG, "sendto failed: errno=%d", errno);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(BEACON_INTERVAL_MS));
    }
}

esp_err_t discovery_start(const char *hostname)
{
    if (s_task) return ESP_OK;  /* already running */

    strncpy(s_hostname, hostname, sizeof(s_hostname) - 1);
    s_hostname[sizeof(s_hostname) - 1] = '\0';

    BaseType_t r = xTaskCreate(beacon_task, "discovery", 3072, NULL,
                               tskIDLE_PRIORITY + 1, &s_task);
    return (r == pdPASS) ? ESP_OK : ESP_ERR_NO_MEM;
}

void discovery_stop(void)
{
    if (s_task) {
        vTaskDelete(s_task);
        s_task = NULL;
    }
}

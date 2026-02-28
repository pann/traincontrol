#include "fakes_state.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "event_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "mdns.h"
#include "esp_http_server.h"
#include "esp_console.h"
#include "config.h"
#include "wifi_manager.h"
#include "motor_control.h"
#include "esp_system.h"
#include <string.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>

fake_state_t g_fake;

/* Event base constants (ESP-IDF uses extern pointers, not string macros) */
esp_event_base_t WIFI_EVENT = "WIFI_EVENT";
esp_event_base_t IP_EVENT   = "IP_EVENT";

/* Storage for fake event log (declared extern in fakes/event_log.h) */
fake_event_entry_t g_fake_events[FAKE_EVENT_LOG_MAX];
int g_fake_event_count;

void fake_state_reset(void)
{
    memset(&g_fake, 0, sizeof(g_fake));
    fake_event_log_reset();
}

/* --- Event log fakes --- */

esp_err_t event_log_init(void)
{
    g_fake_event_count = 0;
    return ESP_OK;
}

void fake_event_log_reset(void)
{
    g_fake_event_count = 0;
}

void event_log(event_category_t category, const char *fmt, ...)
{
    if (g_fake_event_count < FAKE_EVENT_LOG_MAX) {
        g_fake_events[g_fake_event_count].category = category;
        va_list args;
        va_start(args, fmt);
        vsnprintf(g_fake_events[g_fake_event_count].message,
                  FAKE_EVENT_MSG_LEN, fmt, args);
        va_end(args);
        g_fake_event_count++;
    }
}

void fake_nvs_preset(const char *key, const char *value)
{
    if (g_fake.nvs_store_count >= FAKE_NVS_MAX_ENTRIES) return;
    fake_nvs_entry_t *e = &g_fake.nvs_store[g_fake.nvs_store_count++];
    strncpy(e->key, key, FAKE_NVS_MAX_KEY_LEN - 1);
    strncpy(e->value, value, FAKE_NVS_MAX_VAL_LEN - 1);
}

void fake_set_mac(uint8_t b0, uint8_t b1, uint8_t b2,
                  uint8_t b3, uint8_t b4, uint8_t b5)
{
    g_fake.fake_mac[0] = b0; g_fake.fake_mac[1] = b1;
    g_fake.fake_mac[2] = b2; g_fake.fake_mac[3] = b3;
    g_fake.fake_mac[4] = b4; g_fake.fake_mac[5] = b5;
}

/* --- GPIO fakes --- */

esp_err_t gpio_config(const gpio_config_t *cfg)
{
    (void)cfg;
    return ESP_OK;
}

esp_err_t gpio_set_level(gpio_num_t pin, uint32_t level)
{
    if (g_fake.gpio_history_count < FAKE_GPIO_HISTORY_SIZE) {
        g_fake.gpio_history[g_fake.gpio_history_count].pin = pin;
        g_fake.gpio_history[g_fake.gpio_history_count].level = level;
        g_fake.gpio_history_count++;
    }
    return ESP_OK;
}

esp_err_t gpio_install_isr_service(int flags)
{
    (void)flags;
    return ESP_OK;
}

esp_err_t gpio_isr_handler_add(gpio_num_t pin, gpio_isr_t handler, void *arg)
{
    (void)pin;
    (void)handler;
    (void)arg;
    return ESP_OK;
}

/* --- esp_timer fakes --- */

struct esp_timer {
    int id;
};

static struct esp_timer s_fake_timers[4];

esp_err_t esp_timer_create(const esp_timer_create_args_t *args, esp_timer_handle_t *out)
{
    int idx = g_fake.timer_create_calls;
    g_fake.timer_create_calls++;
    if (idx < 4) {
        s_fake_timers[idx].id = idx;
        *out = &s_fake_timers[idx];
        if (args->callback && g_fake.timer_callback_count < 4) {
            g_fake.timer_callbacks[g_fake.timer_callback_count++] = args->callback;
        }
    }
    return ESP_OK;
}

esp_err_t esp_timer_start_once(esp_timer_handle_t timer, uint64_t timeout_us)
{
    (void)timer;
    g_fake.timer_start_once_calls++;
    g_fake.last_timer_timeout_us = timeout_us;
    return ESP_OK;
}

/* --- NVS fakes --- */

esp_err_t nvs_flash_init(void)
{
    return ESP_OK;
}

esp_err_t nvs_flash_erase(void)
{
    return ESP_OK;
}

esp_err_t nvs_open(const char *namespace_name, nvs_open_mode_t open_mode, nvs_handle_t *out_handle)
{
    (void)namespace_name;
    (void)open_mode;
    *out_handle = 1;
    return ESP_OK;
}

void nvs_close(nvs_handle_t handle)
{
    (void)handle;
}

esp_err_t nvs_get_str(nvs_handle_t handle, const char *key, char *out_value, size_t *length)
{
    (void)handle;
    for (int i = 0; i < g_fake.nvs_store_count; i++) {
        if (strcmp(g_fake.nvs_store[i].key, key) == 0) {
            size_t vlen = strlen(g_fake.nvs_store[i].value) + 1;
            if (*length >= vlen) {
                memcpy(out_value, g_fake.nvs_store[i].value, vlen);
            }
            *length = vlen;
            return ESP_OK;
        }
    }
    return ESP_ERR_NVS_NOT_FOUND;
}

esp_err_t nvs_set_str(nvs_handle_t handle, const char *key, const char *value)
{
    (void)handle;
    for (int i = 0; i < g_fake.nvs_store_count; i++) {
        if (strcmp(g_fake.nvs_store[i].key, key) == 0) {
            strncpy(g_fake.nvs_store[i].value, value, FAKE_NVS_MAX_VAL_LEN - 1);
            g_fake.nvs_store[i].value[FAKE_NVS_MAX_VAL_LEN - 1] = '\0';
            return ESP_OK;
        }
    }
    fake_nvs_preset(key, value);
    return ESP_OK;
}

esp_err_t nvs_commit(nvs_handle_t handle)
{
    (void)handle;
    return ESP_OK;
}

esp_err_t nvs_erase_all(nvs_handle_t handle)
{
    (void)handle;
    g_fake.nvs_store_count = 0;
    return ESP_OK;
}

/* --- WiFi fakes --- */

esp_err_t esp_wifi_init(const wifi_init_config_t *config)
{
    (void)config;
    g_fake.wifi_init_calls++;
    return ESP_OK;
}

esp_err_t esp_wifi_set_mode(wifi_mode_t mode)
{
    g_fake.wifi_mode = mode;
    return ESP_OK;
}

esp_err_t esp_wifi_set_config(wifi_interface_t iface, wifi_config_t *conf)
{
    (void)iface;
    (void)conf;
    g_fake.wifi_set_config_calls++;
    return ESP_OK;
}

esp_err_t esp_wifi_start(void)
{
    g_fake.wifi_start_calls++;
    return ESP_OK;
}

esp_err_t esp_wifi_stop(void)
{
    g_fake.wifi_stop_calls++;
    return ESP_OK;
}

esp_err_t esp_wifi_connect(void)
{
    g_fake.wifi_connect_calls++;
    return ESP_OK;
}

esp_err_t esp_wifi_disconnect(void)
{
    return ESP_OK;
}

/* --- Network interface fakes --- */

static struct esp_netif_obj s_fake_sta_netif;
static struct esp_netif_obj s_fake_ap_netif;

esp_err_t esp_netif_init(void)
{
    return ESP_OK;
}

esp_netif_t *esp_netif_create_default_wifi_sta(void)
{
    return &s_fake_sta_netif;
}

esp_netif_t *esp_netif_create_default_wifi_ap(void)
{
    return &s_fake_ap_netif;
}

void esp_netif_destroy(esp_netif_t *netif)
{
    (void)netif;
}

/* --- Event fakes --- */

esp_err_t esp_event_loop_create_default(void)
{
    return ESP_OK;
}

esp_err_t esp_event_handler_instance_register(esp_event_base_t base, int32_t event_id,
                                               esp_event_handler_t handler, void *arg,
                                               esp_event_handler_instance_t *instance)
{
    (void)base;
    (void)event_id;
    (void)arg;
    if (instance) *instance = NULL;
    if (g_fake.event_handler_count < 8) {
        g_fake.event_handlers[g_fake.event_handler_count++] = handler;
    }
    return ESP_OK;
}

/* --- MAC fakes --- */

esp_err_t esp_read_mac(uint8_t *mac, esp_mac_type_t type)
{
    (void)type;
    memcpy(mac, g_fake.fake_mac, 6);
    return ESP_OK;
}

/* --- mDNS fakes --- */

esp_err_t mdns_init(void)
{
    g_fake.mdns_init_calls++;
    return ESP_OK;
}

esp_err_t mdns_hostname_set(const char *hostname)
{
    strncpy(g_fake.mdns_hostname, hostname, sizeof(g_fake.mdns_hostname) - 1);
    return ESP_OK;
}

esp_err_t mdns_service_add(const char *instance, const char *type,
                            const char *proto, uint16_t port,
                            const mdns_txt_item_t *txt, size_t num_items)
{
    (void)instance; (void)type; (void)proto; (void)port; (void)txt; (void)num_items;
    g_fake.mdns_service_add_calls++;
    return ESP_OK;
}

esp_err_t mdns_service_remove_all(void)
{
    return ESP_OK;
}

void mdns_free(void) { }

/* --- HTTP server fakes --- */

static int s_fake_httpd_handle = 1;

esp_err_t httpd_start(httpd_handle_t *handle, const httpd_config_t *config)
{
    (void)config;
    *handle = &s_fake_httpd_handle;
    g_fake.httpd_start_calls++;
    return ESP_OK;
}

esp_err_t httpd_stop(httpd_handle_t handle)
{
    (void)handle;
    g_fake.httpd_stop_calls++;
    return ESP_OK;
}

esp_err_t httpd_register_uri_handler(httpd_handle_t handle, const httpd_uri_t *uri)
{
    (void)handle;
    (void)uri;
    return ESP_OK;
}

esp_err_t httpd_resp_set_type(httpd_req_t *req, const char *type)
{
    (void)req; (void)type;
    return ESP_OK;
}

esp_err_t httpd_resp_set_status(httpd_req_t *req, const char *status)
{
    (void)req; (void)status;
    return ESP_OK;
}

esp_err_t httpd_resp_set_hdr(httpd_req_t *req, const char *field, const char *value)
{
    (void)req; (void)field; (void)value;
    return ESP_OK;
}

esp_err_t httpd_resp_send(httpd_req_t *req, const char *buf, int buf_len)
{
    (void)req; (void)buf; (void)buf_len;
    return ESP_OK;
}

esp_err_t httpd_resp_send_err(httpd_req_t *req, const char *status, const char *msg)
{
    (void)req; (void)status; (void)msg;
    return ESP_OK;
}

int httpd_req_recv(httpd_req_t *req, char *buf, size_t buf_len)
{
    (void)req; (void)buf; (void)buf_len;
    return 0;
}

httpd_uri_match_func_t httpd_uri_match_wildcard = NULL;

/* --- Console fakes --- */

struct esp_console_repl { int dummy; };
static struct esp_console_repl s_fake_repl;

esp_err_t esp_console_new_repl_usb_serial_jtag(
    const esp_console_dev_usb_serial_jtag_config_t *dev_config,
    const esp_console_repl_config_t *repl_config,
    esp_console_repl_t **ret_repl)
{
    (void)dev_config; (void)repl_config;
    *ret_repl = &s_fake_repl;
    return ESP_OK;
}

esp_err_t esp_console_cmd_register(const esp_console_cmd_t *cmd)
{
    (void)cmd;
    g_fake.console_cmd_register_calls++;
    return ESP_OK;
}

esp_err_t esp_console_start_repl(esp_console_repl_t *repl)
{
    (void)repl;
    return ESP_OK;
}

/* --- Config fakes (weak: overridden when test includes config.c) --- */

__attribute__((weak))
esp_err_t config_init(void) { return ESP_OK; }

__attribute__((weak))
bool config_has_wifi_credentials(void) { return g_fake.config_has_creds; }

__attribute__((weak))
const char *config_get_wifi_ssid(void) { return g_fake.config_ssid; }

__attribute__((weak))
const char *config_get_wifi_password(void) { return g_fake.config_pass; }

__attribute__((weak))
esp_err_t config_set_wifi_credentials(const char *ssid, const char *password)
{
    strncpy(g_fake.config_ssid, ssid, sizeof(g_fake.config_ssid) - 1);
    strncpy(g_fake.config_pass, password, sizeof(g_fake.config_pass) - 1);
    g_fake.config_has_creds = true;
    return ESP_OK;
}

__attribute__((weak))
esp_err_t config_factory_reset(void)
{
    g_fake.config_has_creds = false;
    memset(g_fake.config_ssid, 0, sizeof(g_fake.config_ssid));
    memset(g_fake.config_pass, 0, sizeof(g_fake.config_pass));
    return ESP_OK;
}

/* --- WiFi manager fakes (weak: overridden when test includes wifi_manager.c) --- */

__attribute__((weak))
esp_err_t wifi_manager_init(void) { return ESP_OK; }

__attribute__((weak))
esp_err_t wifi_manager_start_sta(void) { return ESP_OK; }

__attribute__((weak))
esp_err_t wifi_manager_stop(void) { return ESP_OK; }

__attribute__((weak))
bool wifi_manager_is_connected(void) { return false; }

__attribute__((weak))
const char *wifi_manager_get_ip_str(void) { return ""; }

/* --- System fakes --- */

void esp_restart(void) { g_fake.restart_calls++; }

/* --- FreeRTOS task fakes --- */

#include "freertos/task.h"

BaseType_t xTaskCreate(TaskFunction_t pvTaskCode, const char *pcName,
                       uint32_t usStackDepth, void *pvParameters,
                       UBaseType_t uxPriority, TaskHandle_t *pxCreatedTask)
{
    (void)pvTaskCode; (void)pcName; (void)usStackDepth;
    (void)pvParameters; (void)uxPriority;
    if (pxCreatedTask) *pxCreatedTask = NULL;
    g_fake.task_create_calls++;
    return pdPASS;
}

/* --- Motor control fakes (weak: overridden when test includes motor_control.c) --- */

__attribute__((weak))
esp_err_t motor_control_init(void) { return ESP_OK; }

__attribute__((weak))
void motor_set_direction(bool forward) { g_fake.motor_forward = forward; }

__attribute__((weak))
void motor_set_speed(uint16_t speed) { g_fake.motor_speed = speed; }

__attribute__((weak))
void motor_set_enable(bool enable) { g_fake.motor_enabled = enable; }

__attribute__((weak))
void motor_emergency_stop(void)
{
    g_fake.motor_enabled = false;
    g_fake.motor_speed = 0;
    g_fake.motor_current_speed = 0;
    g_fake.motor_estop_calls++;
}

__attribute__((weak))
uint16_t motor_get_current_speed(void) { return g_fake.motor_current_speed; }

__attribute__((weak))
bool motor_get_enable(void) { return g_fake.motor_enabled; }

__attribute__((weak))
bool motor_get_direction_forward(void) { return g_fake.motor_forward; }

/* --- esp_timer_get_time fake --- */

int64_t esp_timer_get_time(void) { return g_fake.fake_timer_time_us; }

/* --- lwip socket fakes (minimal stubs for compilation) --- */

#include "lwip/sockets.h"

int socket(int domain, int type, int protocol)
{ (void)domain; (void)type; (void)protocol; return -1; }

int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{ (void)sockfd; (void)addr; (void)addrlen; return -1; }

int listen(int sockfd, int backlog)
{ (void)sockfd; (void)backlog; return -1; }

int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{ (void)sockfd; (void)addr; (void)addrlen; return -1; }

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{ (void)sockfd; (void)addr; (void)addrlen; return -1; }

ssize_t recv(int sockfd, void *buf, size_t len, int flags)
{ (void)sockfd; (void)buf; (void)len; (void)flags; return -1; }

ssize_t send(int sockfd, const void *buf, size_t len, int flags)
{ (void)sockfd; (void)buf; (void)len; (void)flags; return -1; }

int setsockopt(int sockfd, int level, int optname, const void *optval,
               socklen_t optlen)
{ (void)sockfd; (void)level; (void)optname; (void)optval; (void)optlen; return 0; }

int shutdown(int sockfd, int how)
{ (void)sockfd; (void)how; return 0; }

char *inet_ntoa(struct in_addr in)
{ (void)in; return "0.0.0.0"; }

/* --- Modbus server fakes (weak, for provisioning tests) --- */

__attribute__((weak))
bool modbus_server_client_connected(void) { return false; }

__attribute__((weak))
uint32_t modbus_server_get_transaction_count(void) { return 0; }

__attribute__((weak))
uint16_t modbus_read_input_reg(uint16_t addr) { (void)addr; return 0; }

__attribute__((weak))
uint16_t modbus_read_holding_reg(uint16_t addr) { (void)addr; return 0; }

__attribute__((weak))
void modbus_write_holding_reg(uint16_t addr, uint16_t value) { (void)addr; (void)value; }

__attribute__((weak))
bool modbus_read_coil(uint16_t addr) { (void)addr; return false; }

__attribute__((weak))
void modbus_write_coil(uint16_t addr, bool value) { (void)addr; (void)value; }

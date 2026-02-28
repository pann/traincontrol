#include "provisioning.h"
#include "config.h"
#include "wifi_manager.h"
#include "modbus_server.h"
#include "motor_control.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "esp_http_server.h"
#include "esp_console.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "event_log.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "prov";

/* ===== SoftAP captive portal ===== */

static httpd_handle_t s_httpd = NULL;
static esp_netif_t   *s_ap_netif = NULL;
static char           s_ap_ssid[32] = {0};

static const char PORTAL_HTML[] =
    "<!DOCTYPE html><html><head>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>TrainCtrl WiFi Setup</title>"
    "<style>"
    "body{font-family:sans-serif;max-width:400px;margin:40px auto;padding:0 20px;}"
    "input{width:100%;padding:8px;margin:8px 0;box-sizing:border-box;}"
    "button{width:100%;padding:12px;background:#2196F3;color:#fff;border:none;cursor:pointer;}"
    "</style></head><body>"
    "<h2>TrainCtrl WiFi Setup</h2>"
    "<form method='POST' action='/save'>"
    "<label>SSID:<input name='ssid' required></label>"
    "<label>Password:<input name='password' type='password'></label>"
    "<button type='submit'>Connect</button>"
    "</form></body></html>";

static const char PORTAL_DONE_HTML[] =
    "<!DOCTYPE html><html><head>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>TrainCtrl</title></head><body>"
    "<h2>Credentials saved!</h2>"
    "<p>The device will reboot and connect to your WiFi network. "
    "You can close this page.</p></body></html>";

/**
 * URL-decode a single form field value from URL-encoded POST body.
 * Handles %XX hex encoding and '+' → space.
 * Returns ESP_OK if field found, ESP_ERR_NOT_FOUND otherwise.
 */
static esp_err_t parse_form_field(const char *body, const char *field,
                                   char *out, size_t out_size)
{
    size_t field_len = strlen(field);
    const char *p = body;

    while ((p = strstr(p, field)) != NULL) {
        /* Verify this is a complete field name match:
         * must be at start of string or after '&', and followed by '=' */
        if (p != body && *(p - 1) != '&') {
            p += field_len;
            continue;
        }
        if (p[field_len] != '=') {
            p += field_len;
            continue;
        }

        const char *val = p + field_len + 1;
        size_t i = 0;

        while (*val && *val != '&' && i < out_size - 1) {
            if (*val == '+') {
                out[i++] = ' ';
                val++;
            } else if (*val == '%' && val[1] && val[2]) {
                unsigned int hex;
                if (sscanf(val + 1, "%2x", &hex) == 1) {
                    out[i++] = (char)hex;
                    val += 3;
                } else {
                    out[i++] = *val++;
                }
            } else {
                out[i++] = *val++;
            }
        }
        out[i] = '\0';
        return ESP_OK;
    }
    return ESP_ERR_NOT_FOUND;
}

/* GET / — serve the portal form */
static esp_err_t portal_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, PORTAL_HTML, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/* Delayed reboot — gives HTTP response time to reach the client */
static void reboot_timer_cb(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "Rebooting after provisioning...");
    esp_restart();
}

static void schedule_reboot(void)
{
    esp_timer_handle_t timer;
    const esp_timer_create_args_t args = {
        .callback = reboot_timer_cb,
        .name = "reboot",
    };
    esp_timer_create(&args, &timer);
    esp_timer_start_once(timer, 1500 * 1000);  /* 1.5 seconds */
}

/* POST /save — receive SSID + password, store, reboot into STA */
static esp_err_t portal_save_handler(httpd_req_t *req)
{
    char buf[256] = {0};
    int received = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (received <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty body");
        return ESP_FAIL;
    }
    buf[received] = '\0';

    char ssid[64] = {0};
    char password[128] = {0};

    if (parse_form_field(buf, "ssid", ssid, sizeof(ssid)) != ESP_OK ||
        strlen(ssid) == 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing SSID");
        return ESP_FAIL;
    }
    parse_form_field(buf, "password", password, sizeof(password));

    event_log(EVT_CONFIG, "wifi_credentials_received ssid=%s", ssid);
    config_set_wifi_credentials(ssid, password);

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, PORTAL_DONE_HTML, HTTPD_RESP_USE_STRLEN);

    event_log(EVT_SYSTEM, "provisioning_complete, rebooting");
    schedule_reboot();

    return ESP_OK;
}

/* Redirect all other URLs to / (captive portal detection) */
static esp_err_t portal_redirect_handler(httpd_req_t *req)
{
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

esp_err_t provisioning_start_softap(void)
{
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    snprintf(s_ap_ssid, sizeof(s_ap_ssid), "TrainCtrl-%02X%02X", mac[4], mac[5]);

    /* Create AP netif */
    s_ap_netif = esp_netif_create_default_wifi_ap();

    wifi_config_t ap_config = {0};
    strncpy((char *)ap_config.ap.ssid, s_ap_ssid, sizeof(ap_config.ap.ssid) - 1);
    ap_config.ap.ssid_len = strlen(s_ap_ssid);
    ap_config.ap.max_connection = 4;
    ap_config.ap.authmode = WIFI_AUTH_OPEN;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    /* Start HTTP server */
    httpd_config_t http_config = HTTPD_DEFAULT_CONFIG();
    http_config.uri_match_fn = httpd_uri_match_wildcard;
    ESP_ERROR_CHECK(httpd_start(&s_httpd, &http_config));

    httpd_uri_t uri_root = {
        .uri = "/", .method = HTTP_GET, .handler = portal_get_handler,
    };
    httpd_uri_t uri_save = {
        .uri = "/save", .method = HTTP_POST, .handler = portal_save_handler,
    };
    httpd_uri_t uri_redirect = {
        .uri = "/*", .method = HTTP_GET, .handler = portal_redirect_handler,
    };
    httpd_register_uri_handler(s_httpd, &uri_root);
    httpd_register_uri_handler(s_httpd, &uri_save);
    httpd_register_uri_handler(s_httpd, &uri_redirect);

    event_log(EVT_SYSTEM, "softap_started ssid=%s", s_ap_ssid);
    ESP_LOGI(TAG, "SoftAP started: %s", s_ap_ssid);
    return ESP_OK;
}

esp_err_t provisioning_stop_softap(void)
{
    if (s_httpd) {
        httpd_stop(s_httpd);
        s_httpd = NULL;
    }
    esp_wifi_stop();
    if (s_ap_netif) {
        esp_netif_destroy(s_ap_netif);
        s_ap_netif = NULL;
    }
    ESP_LOGI(TAG, "SoftAP stopped");
    return ESP_OK;
}

/* ===== USB Serial CLI ===== */

static int cmd_wifi_set(int argc, char **argv)
{
    if (argc < 3) {
        printf("Usage: wifi_set <ssid> <password>\n");
        return 1;
    }
    config_set_wifi_credentials(argv[1], argv[2]);
    event_log(EVT_CONFIG, "wifi_credentials_received ssid=%s", argv[1]);
    printf("Credentials saved. Connecting...\n");

    wifi_manager_stop();
    wifi_manager_start_sta();
    event_log(EVT_SYSTEM, "provisioning_complete");
    return 0;
}

static int cmd_wifi_status(int argc, char **argv)
{
    (void)argc; (void)argv;
    if (wifi_manager_is_connected()) {
        printf("Connected  IP=%s  SSID=%s\n",
               wifi_manager_get_ip_str(), config_get_wifi_ssid());
    } else {
        printf("Disconnected\n");
    }
    return 0;
}

static int cmd_factory_reset(int argc, char **argv)
{
    (void)argc; (void)argv;
    printf("Factory reset: clearing credentials...\n");
    wifi_manager_stop();
    config_factory_reset();
    event_log(EVT_SYSTEM, "factory_reset");
    provisioning_start_softap();
    printf("SoftAP started. Connect to '%s' to reconfigure.\n", s_ap_ssid);
    return 0;
}

/* ===== Motor CLI commands ===== */

static int cmd_speed(int argc, char **argv)
{
    if (argc != 2) {
        printf("Usage: speed <0-1000>\n");
        return 1;
    }
    uint16_t speed = (uint16_t)atoi(argv[1]);
    motor_set_speed(speed);
    printf("OK speed=%u\n", motor_get_current_speed());
    return 0;
}

static int cmd_dir(int argc, char **argv)
{
    if (argc != 2) {
        printf("Usage: dir <fwd|rev>\n");
        return 1;
    }
    bool fwd = (strcmp(argv[1], "fwd") == 0);
    motor_set_direction(fwd);
    printf("OK dir=%s\n", fwd ? "fwd" : "rev");
    return 0;
}

static int cmd_enable(int argc, char **argv)
{
    if (argc != 2) {
        printf("Usage: enable <0|1>\n");
        return 1;
    }
    bool en = (argv[1][0] == '1');
    motor_set_enable(en);
    printf("OK enable=%d\n", en);
    return 0;
}

static int cmd_estop(int argc, char **argv)
{
    (void)argc; (void)argv;
    motor_emergency_stop();
    printf("OK emergency_stop\n");
    return 0;
}

static int cmd_motor_status(int argc, char **argv)
{
    (void)argc; (void)argv;
    printf("enabled=%d dir=%s speed=%u\n",
           motor_get_enable(),
           motor_get_direction_forward() ? "fwd" : "rev",
           motor_get_current_speed());
    return 0;
}

/* ===== Modbus CLI commands ===== */

static const char *coil_name(uint16_t addr)
{
    switch (addr) {
    case 0: return "MOTOR_ENABLE";
    case 1: return "EMERGENCY_STOP";
    default: return "?";
    }
}

static const char *hreg_name(uint16_t addr)
{
    switch (addr) {
    case 0: return "DIRECTION";
    case 1: return "SPEED";
    case 2: return "ACCEL_RATE";
    case 3: return "DECEL_RATE";
    default: return "?";
    }
}

static const char *ireg_name(uint16_t addr)
{
    switch (addr) {
    case 0: return "STATUS";
    case 1: return "RAIL_VOLTAGE";
    case 2: return "CURRENT_SPEED";
    case 3: return "UPTIME";
    default: return "?";
    }
}

static void print_all_regs(void)
{
    printf("Coils:\n");
    for (uint16_t i = 0; i < 2; i++)
        printf("  [%u] %-16s = %d\n", i, coil_name(i), modbus_read_coil(i));
    printf("Holding registers:\n");
    for (uint16_t i = 0; i < 4; i++)
        printf("  [%u] %-16s = %u\n", i, hreg_name(i), modbus_read_holding_reg(i));
    printf("Input registers:\n");
    for (uint16_t i = 0; i < 4; i++)
        printf("  [%u] %-16s = %u\n", i, ireg_name(i), modbus_read_input_reg(i));
}

static int cmd_mr(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: mr <coil|holding|input|all> [addr]\n");
        return 1;
    }
    if (strcmp(argv[1], "all") == 0) {
        print_all_regs();
        return 0;
    }
    if (argc < 3) {
        /* Print all registers of the given type */
        if (strcmp(argv[1], "coil") == 0) {
            for (uint16_t i = 0; i < 2; i++)
                printf("[%u] %-16s = %d\n", i, coil_name(i), modbus_read_coil(i));
        } else if (strcmp(argv[1], "holding") == 0) {
            for (uint16_t i = 0; i < 4; i++)
                printf("[%u] %-16s = %u\n", i, hreg_name(i), modbus_read_holding_reg(i));
        } else if (strcmp(argv[1], "input") == 0) {
            for (uint16_t i = 0; i < 4; i++)
                printf("[%u] %-16s = %u\n", i, ireg_name(i), modbus_read_input_reg(i));
        } else {
            printf("Unknown type: %s (use coil, holding, input, all)\n", argv[1]);
            return 1;
        }
        return 0;
    }
    uint16_t addr = (uint16_t)atoi(argv[2]);
    if (strcmp(argv[1], "coil") == 0) {
        if (addr >= 2) { printf("Coil address out of range (0-1)\n"); return 1; }
        printf("[%u] %s = %d\n", addr, coil_name(addr), modbus_read_coil(addr));
    } else if (strcmp(argv[1], "holding") == 0) {
        if (addr >= 4) { printf("Holding reg address out of range (0-3)\n"); return 1; }
        printf("[%u] %s = %u\n", addr, hreg_name(addr), modbus_read_holding_reg(addr));
    } else if (strcmp(argv[1], "input") == 0) {
        if (addr >= 4) { printf("Input reg address out of range (0-3)\n"); return 1; }
        printf("[%u] %s = %u\n", addr, ireg_name(addr), modbus_read_input_reg(addr));
    } else {
        printf("Unknown type: %s\n", argv[1]);
        return 1;
    }
    return 0;
}

static int cmd_mw(int argc, char **argv)
{
    if (argc < 4) {
        printf("Usage: mw <coil|holding> <addr> <value>\n");
        return 1;
    }
    uint16_t addr = (uint16_t)atoi(argv[2]);
    uint16_t value = (uint16_t)atoi(argv[3]);
    if (strcmp(argv[1], "coil") == 0) {
        if (addr >= 2) { printf("Coil address out of range (0-1)\n"); return 1; }
        modbus_write_coil(addr, value != 0);
        printf("Coil [%u] %s = %d\n", addr, coil_name(addr), value != 0);
    } else if (strcmp(argv[1], "holding") == 0) {
        if (addr >= 4) { printf("Holding reg address out of range (0-3)\n"); return 1; }
        modbus_write_holding_reg(addr, value);
        printf("Holding [%u] %s = %u\n", addr, hreg_name(addr), value);
    } else {
        printf("Unknown type: %s (use coil, holding)\n", argv[1]);
        return 1;
    }
    return 0;
}

static int cmd_modbus_status(int argc, char **argv)
{
    (void)argc; (void)argv;
    printf("Client: %s\n",
           modbus_server_client_connected() ? "connected" : "not connected");
    printf("Transactions: %lu\n",
           (unsigned long)modbus_server_get_transaction_count());
    return 0;
}

static int cmd_reset(int argc, char **argv)
{
    (void)argc; (void)argv;
    printf("Rebooting...\n");
    esp_restart();
    return 0;
}

static void register_cli_commands(void)
{
    const esp_console_cmd_t cmds[] = {
        {
            .command = "wifi_set",
            .help = "Set WiFi credentials: wifi_set <ssid> <password>",
            .hint = NULL,
            .func = cmd_wifi_set,
        },
        {
            .command = "wifi_status",
            .help = "Show WiFi connection status",
            .hint = NULL,
            .func = cmd_wifi_status,
        },
        {
            .command = "factory_reset",
            .help = "Clear credentials and restart SoftAP",
            .hint = NULL,
            .func = cmd_factory_reset,
        },
        {
            .command = "mr",
            .help = "Read Modbus registers: mr <coil|holding|input|all> [addr]",
            .hint = NULL,
            .func = cmd_mr,
        },
        {
            .command = "mw",
            .help = "Write Modbus register: mw <coil|holding> <addr> <value>",
            .hint = NULL,
            .func = cmd_mw,
        },
        {
            .command = "modbus_status",
            .help = "Show Modbus TCP server status",
            .hint = NULL,
            .func = cmd_modbus_status,
        },
        {
            .command = "reset",
            .help = "Reboot the device",
            .hint = NULL,
            .func = cmd_reset,
        },
        {
            .command = "speed",
            .help = "Set motor speed: speed <0-1000>",
            .hint = NULL,
            .func = cmd_speed,
        },
        {
            .command = "dir",
            .help = "Set motor direction: dir <fwd|rev>",
            .hint = NULL,
            .func = cmd_dir,
        },
        {
            .command = "enable",
            .help = "Enable/disable motor: enable <0|1>",
            .hint = NULL,
            .func = cmd_enable,
        },
        {
            .command = "estop",
            .help = "Emergency stop (disable + zero speed)",
            .hint = NULL,
            .func = cmd_estop,
        },
        {
            .command = "status",
            .help = "Show current motor state",
            .hint = NULL,
            .func = cmd_motor_status,
        },
    };

    for (int i = 0; i < (int)(sizeof(cmds) / sizeof(cmds[0])); i++) {
        ESP_ERROR_CHECK(esp_console_cmd_register(&cmds[i]));
    }
}

esp_err_t provisioning_init(void)
{
    ESP_LOGI(TAG, "Provisioning init");

    /* Start USB serial CLI (always available) */
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    esp_console_dev_usb_serial_jtag_config_t usb_config =
        ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    esp_console_repl_t *repl = NULL;
    ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&usb_config,
                                                          &repl_config,
                                                          &repl));
    register_cli_commands();
    ESP_ERROR_CHECK(esp_console_start_repl(repl));

    /* If no credentials, also start SoftAP portal */
    if (!config_has_wifi_credentials()) {
        provisioning_start_softap();
    }

    return ESP_OK;
}

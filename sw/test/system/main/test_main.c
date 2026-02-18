/*
 * System test firmware for traincontrol.
 *
 * Provides a serial REPL over USB for manual and automated testing.
 *
 * Motor test setup:
 *   - Function generator on GPIO 6 (100 Hz square wave, 3.3 V) for zero-crossing
 *   - Oscilloscope on GPIO 4 (FWD) and GPIO 5 (REV) to observe TRIAC gate pulses
 *
 * Visual feedback via on-board RGB LED (WS2812 on GPIO 48):
 *   - Green = forward, brightness = speed
 *   - Red   = reverse, brightness = speed
 *   - Off   = disabled or speed 0
 *
 * WiFi commands:
 *   - wifi_status: show connection state, IP, and SSID
 *   - wifi_set <ssid> <password>: store credentials and connect
 *   - factory_reset: clear credentials, start SoftAP provisioning
 *
 * Modbus commands (from provisioning CLI):
 *   - mr <coil|holding|input|all> [addr]: read registers
 *   - mw <coil|holding> <addr> <value>: write a register
 *   - modbus_status: show client connected state and transaction count
 *   - reset: reboot the device
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "esp_console.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "led_strip.h"
#include "motor_control.h"
#include "modbus_server.h"
#include "config.h"
#include "wifi_manager.h"
#include "provisioning.h"

static const char *TAG = "sys_test";

#define LED_GPIO    48
#define LED_MAX_VAL 80  /* cap brightness to avoid blinding (0-255) */

static led_strip_handle_t s_led;

/* Update the RGB LED to reflect current motor state. */
static void led_update(void)
{
    if (!motor_get_enable() || motor_get_current_speed() == 0) {
        led_strip_clear(s_led);
        led_strip_refresh(s_led);
        return;
    }

    /* Map speed 0-1000 → brightness 0-LED_MAX_VAL */
    uint32_t brightness = (uint32_t)motor_get_current_speed() * LED_MAX_VAL / 1000;

    uint32_t r = 0, g = 0;
    if (motor_get_direction_forward()) {
        g = brightness;  /* green = forward */
    } else {
        r = brightness;  /* red = reverse */
    }

    led_strip_set_pixel(s_led, 0, r, g, 0);
    led_strip_refresh(s_led);
}

/* --- Command handlers --- */

static int cmd_speed(int argc, char **argv)
{
    if (argc != 2) {
        printf("Usage: speed <0-1000>\n");
        return 1;
    }
    uint16_t speed = (uint16_t)atoi(argv[1]);
    motor_set_speed(speed);
    led_update();
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
    led_update();
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
    led_update();
    printf("OK enable=%d\n", en);
    return 0;
}

static int cmd_estop(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    motor_emergency_stop();
    led_update();
    printf("OK emergency_stop\n");
    return 0;
}

static int cmd_status(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    printf("enabled=%d dir=%s speed=%u\n",
        motor_get_enable(),
        motor_get_direction_forward() ? "fwd" : "rev",
        motor_get_current_speed());
    return 0;
}

/* --- WiFi command handlers --- */

static int cmd_wifi_status(int argc, char **argv)
{
    (void)argc; (void)argv;
    if (wifi_manager_is_connected()) {
        printf("wifi: connected ip=%s ssid=%s\n",
               wifi_manager_get_ip_str(), config_get_wifi_ssid());
    } else {
        printf("wifi: disconnected\n");
    }
    return 0;
}

static int cmd_wifi_set(int argc, char **argv)
{
    if (argc < 3) {
        printf("Usage: wifi_set <ssid> <password>\n");
        return 1;
    }
    config_set_wifi_credentials(argv[1], argv[2]);
    printf("Credentials saved. Connecting...\n");
    wifi_manager_stop();
    wifi_manager_start_sta();
    return 0;
}

static int cmd_factory_reset(int argc, char **argv)
{
    (void)argc; (void)argv;
    printf("Factory reset: clearing credentials...\n");
    wifi_manager_stop();
    config_factory_reset();
    provisioning_start_softap();
    printf("SoftAP started. Connect to the AP to reconfigure.\n");
    return 0;
}

static int cmd_reset(int argc, char **argv)
{
    (void)argc; (void)argv;
    printf("Rebooting...\n");
    esp_restart();
    return 0;  /* unreachable */
}

/* --- Command table --- */

static const esp_console_cmd_t s_commands[] = {
    {
        .command = "speed",
        .help = "Set motor speed (0-1000)",
        .hint = "<0-1000>",
        .func = &cmd_speed,
    },
    {
        .command = "dir",
        .help = "Set motor direction",
        .hint = "<fwd|rev>",
        .func = &cmd_dir,
    },
    {
        .command = "enable",
        .help = "Enable or disable motor output",
        .hint = "<0|1>",
        .func = &cmd_enable,
    },
    {
        .command = "estop",
        .help = "Emergency stop (disable + zero speed)",
        .hint = NULL,
        .func = &cmd_estop,
    },
    {
        .command = "status",
        .help = "Show current motor state",
        .hint = NULL,
        .func = &cmd_status,
    },
    {
        .command = "wifi_status",
        .help = "Show WiFi connection state, IP, and SSID",
        .hint = NULL,
        .func = &cmd_wifi_status,
    },
    {
        .command = "wifi_set",
        .help = "Set WiFi credentials and connect",
        .hint = "<ssid> <password>",
        .func = &cmd_wifi_set,
    },
    {
        .command = "factory_reset",
        .help = "Clear credentials and start SoftAP provisioning",
        .hint = NULL,
        .func = &cmd_factory_reset,
    },
    {
        .command = "reset",
        .help = "Reboot the device",
        .hint = NULL,
        .func = &cmd_reset,
    },
};

void app_main(void)
{
    ESP_LOGI(TAG, "System test firmware starting");

    /* Initialise NVS (required by config and WiFi) */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(config_init());
    ESP_ERROR_CHECK(wifi_manager_init());

    /* Initialise on-board RGB LED (WS2812 on GPIO 48) */
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_GPIO,
        .max_leds = 1,
    };
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000,  /* 10 MHz */
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &s_led));
    led_strip_clear(s_led);
    led_strip_refresh(s_led);

    ESP_ERROR_CHECK(motor_control_init());
    ESP_ERROR_CHECK(modbus_server_init());

    /* Initialise console REPL */
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = "test> ";

    esp_console_dev_usb_serial_jtag_config_t jtag_config =
        ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&jtag_config,
                                                          &repl_config,
                                                          &repl));

    /* Register all commands */
    for (int i = 0; i < (int)(sizeof(s_commands) / sizeof(s_commands[0])); i++) {
        ESP_ERROR_CHECK(esp_console_cmd_register(&s_commands[i]));
    }

    ESP_LOGI(TAG, "Ready — type 'help' for commands");
    ESP_LOGI(TAG, "Motor LED: green=fwd, red=rev, brightness=speed");
    ESP_LOGI(TAG, "WiFi: wifi_status, wifi_set, factory_reset");
    ESP_LOGI(TAG, "Modbus: mr, mw, modbus_status, reset");
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}

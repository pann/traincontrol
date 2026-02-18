#pragma once

#include "esp_err.h"
#include <stddef.h>

typedef struct esp_console_repl esp_console_repl_t;

typedef int (*esp_console_cmd_func_t)(int argc, char **argv);

typedef struct {
    const char *command;
    const char *help;
    const char *hint;
    esp_console_cmd_func_t func;
} esp_console_cmd_t;

typedef struct {
    const char *prompt;
    size_t max_cmdline_length;
    size_t max_history_len;
} esp_console_repl_config_t;

#define ESP_CONSOLE_REPL_CONFIG_DEFAULT() { \
    .prompt = "> ",                         \
    .max_cmdline_length = 256,              \
    .max_history_len = 32,                  \
}

typedef struct {
    int channel;
} esp_console_dev_usb_serial_jtag_config_t;

#define ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT() { .channel = 0 }

esp_err_t esp_console_new_repl_usb_serial_jtag(
    const esp_console_dev_usb_serial_jtag_config_t *dev_config,
    const esp_console_repl_config_t *repl_config,
    esp_console_repl_t **ret_repl);
esp_err_t esp_console_cmd_register(const esp_console_cmd_t *cmd);
esp_err_t esp_console_start_repl(esp_console_repl_t *repl);

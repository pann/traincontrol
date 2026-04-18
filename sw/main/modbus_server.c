#include "modbus_server.h"
#include "motor_control.h"
#include "wifi_manager.h"
#include "status_led.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "event_log.h"
#include "lwip/sockets.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "modbus";

/* ===== Constants ===== */

#define MODBUS_TCP_PORT     502
#define MBAP_HEADER_LEN     7
#define MAX_PDU_LEN         256
#define MAX_ADU_LEN         (MBAP_HEADER_LEN + MAX_PDU_LEN)

/* Coil addresses */
#define COIL_MOTOR_ENABLE   0x0000
#define COIL_EMERGENCY_STOP 0x0001
#define COIL_COUNT          2

/* Holding register addresses */
#define HREG_DIRECTION      0x0000
#define HREG_SPEED          0x0001
#define HREG_ACCEL_RATE     0x0002
#define HREG_DECEL_RATE     0x0003
#define HREG_COUNT          4

/* Input register addresses */
#define IREG_STATUS         0x0000
#define IREG_RAIL_VOLTAGE   0x0001
#define IREG_CURRENT_SPEED  0x0002
#define IREG_UPTIME         0x0003
#define IREG_COUNT          4

/* STATUS bit positions */
#define STATUS_BIT_WIFI      0
#define STATUS_BIT_ENABLED   1
#define STATUS_BIT_DIRECTION 2
#define STATUS_BIT_FAULT     3

/* Modbus function codes */
#define FC_READ_COILS           0x01
#define FC_READ_HOLDING_REGS    0x03
#define FC_READ_INPUT_REGS      0x04
#define FC_WRITE_SINGLE_COIL    0x05
#define FC_WRITE_SINGLE_REG     0x06

/* Modbus exception codes */
#define EX_ILLEGAL_FUNCTION     0x01
#define EX_ILLEGAL_DATA_ADDRESS 0x02
#define EX_ILLEGAL_DATA_VALUE   0x03

/* ===== State ===== */

static uint16_t s_holding_regs[HREG_COUNT] = {0, 0, 100, 100};
static uint32_t s_transaction_count = 0;
static bool     s_client_connected = false;
static int64_t  s_boot_time_us = 0;

/* ===== Layer 1: Register storage + motor bridging ===== */

static void apply_holding_reg(uint16_t addr, uint16_t value)
{
    switch (addr) {
    case HREG_DIRECTION:
        s_holding_regs[addr] = value;
        motor_set_direction(value == 0);
        event_log(EVT_MODBUS, "write dir=%s", value ? "rev" : "fwd");
        break;
    case HREG_SPEED:
        s_holding_regs[addr] = value;
        motor_set_speed(value);
        event_log(EVT_MODBUS, "write speed=%u", value);
        break;
    case HREG_ACCEL_RATE:
        s_holding_regs[addr] = value;
        event_log(EVT_MODBUS, "write accel=%u", value);
        break;
    case HREG_DECEL_RATE:
        s_holding_regs[addr] = value;
        event_log(EVT_MODBUS, "write decel=%u", value);
        break;
    }
}

static void refresh_input_regs(uint16_t *regs)
{
    uint16_t status = 0;
    if (wifi_manager_is_connected())    status |= (1 << STATUS_BIT_WIFI);
    if (motor_get_enable())             status |= (1 << STATUS_BIT_ENABLED);
    if (!motor_get_direction_forward()) status |= (1 << STATUS_BIT_DIRECTION);

    regs[IREG_STATUS]        = status;
    regs[IREG_RAIL_VOLTAGE]  = motor_get_rail_voltage_mv();
    regs[IREG_CURRENT_SPEED] = motor_get_current_speed();
    regs[IREG_UPTIME]        = (uint16_t)((esp_timer_get_time() - s_boot_time_us)
                                           / 1000000LL);
}

/* ===== Layer 2: PDU processing ===== */

static int make_exception(uint8_t fc, uint8_t ex_code,
                          uint8_t *resp, int resp_size)
{
    if (resp_size < 2) return -1;
    resp[0] = fc | 0x80;
    resp[1] = ex_code;
    return 2;
}

static int handle_read_coils(const uint8_t *req, int req_len,
                             uint8_t *resp, int resp_size)
{
    if (req_len < 5) return make_exception(FC_READ_COILS, EX_ILLEGAL_DATA_VALUE, resp, resp_size);

    uint16_t start_addr = (req[1] << 8) | req[2];
    uint16_t quantity   = (req[3] << 8) | req[4];

    if (quantity == 0 || start_addr + quantity > COIL_COUNT)
        return make_exception(FC_READ_COILS, EX_ILLEGAL_DATA_ADDRESS, resp, resp_size);

    uint8_t coil_byte = 0;
    for (uint16_t i = 0; i < quantity; i++) {
        uint16_t addr = start_addr + i;
        bool val = false;
        if (addr == COIL_MOTOR_ENABLE) val = motor_get_enable();
        /* EMERGENCY_STOP always reads as 0 */
        if (val) coil_byte |= (1 << i);
    }

    if (resp_size < 4) return -1;
    resp[0] = FC_READ_COILS;
    resp[1] = 1;  /* byte count */
    resp[2] = coil_byte;
    return 3;
}

static int handle_write_single_coil(const uint8_t *req, int req_len,
                                    uint8_t *resp, int resp_size)
{
    if (req_len < 5) return make_exception(FC_WRITE_SINGLE_COIL, EX_ILLEGAL_DATA_VALUE, resp, resp_size);

    uint16_t addr  = (req[1] << 8) | req[2];
    uint16_t value = (req[3] << 8) | req[4];

    if (addr >= COIL_COUNT)
        return make_exception(FC_WRITE_SINGLE_COIL, EX_ILLEGAL_DATA_ADDRESS, resp, resp_size);

    if (value != 0xFF00 && value != 0x0000)
        return make_exception(FC_WRITE_SINGLE_COIL, EX_ILLEGAL_DATA_VALUE, resp, resp_size);

    bool on = (value == 0xFF00);

    if (addr == COIL_MOTOR_ENABLE) {
        motor_set_enable(on);
        event_log(EVT_MODBUS, "write enable=%d", on);
    } else if (addr == COIL_EMERGENCY_STOP && on) {
        motor_emergency_stop();
        event_log(EVT_MODBUS, "emergency_stop");
    }

    /* FC05 response echoes the request */
    if (resp_size < 5) return -1;
    memcpy(resp, req, 5);
    return 5;
}

static int handle_read_holding_regs(const uint8_t *req, int req_len,
                                    uint8_t *resp, int resp_size)
{
    if (req_len < 5) return make_exception(FC_READ_HOLDING_REGS, EX_ILLEGAL_DATA_VALUE, resp, resp_size);

    uint16_t start_addr = (req[1] << 8) | req[2];
    uint16_t quantity   = (req[3] << 8) | req[4];

    if (quantity == 0 || start_addr + quantity > HREG_COUNT)
        return make_exception(FC_READ_HOLDING_REGS, EX_ILLEGAL_DATA_ADDRESS, resp, resp_size);

    int byte_count = quantity * 2;
    if (resp_size < 2 + byte_count) return -1;

    resp[0] = FC_READ_HOLDING_REGS;
    resp[1] = (uint8_t)byte_count;
    for (uint16_t i = 0; i < quantity; i++) {
        resp[2 + i * 2]     = s_holding_regs[start_addr + i] >> 8;
        resp[2 + i * 2 + 1] = s_holding_regs[start_addr + i] & 0xFF;
    }
    return 2 + byte_count;
}

static int handle_write_single_reg(const uint8_t *req, int req_len,
                                   uint8_t *resp, int resp_size)
{
    if (req_len < 5) return make_exception(FC_WRITE_SINGLE_REG, EX_ILLEGAL_DATA_VALUE, resp, resp_size);

    uint16_t addr  = (req[1] << 8) | req[2];
    uint16_t value = (req[3] << 8) | req[4];

    if (addr >= HREG_COUNT)
        return make_exception(FC_WRITE_SINGLE_REG, EX_ILLEGAL_DATA_ADDRESS, resp, resp_size);

    /* Validate ranges */
    switch (addr) {
    case HREG_DIRECTION:
        if (value > 1)
            return make_exception(FC_WRITE_SINGLE_REG, EX_ILLEGAL_DATA_VALUE, resp, resp_size);
        break;
    case HREG_SPEED:
        if (value > 1000)
            return make_exception(FC_WRITE_SINGLE_REG, EX_ILLEGAL_DATA_VALUE, resp, resp_size);
        break;
    case HREG_ACCEL_RATE:
    case HREG_DECEL_RATE:
        if (value < 1 || value > 1000)
            return make_exception(FC_WRITE_SINGLE_REG, EX_ILLEGAL_DATA_VALUE, resp, resp_size);
        break;
    }

    apply_holding_reg(addr, value);

    /* FC06 response echoes the request */
    if (resp_size < 5) return -1;
    memcpy(resp, req, 5);
    return 5;
}

static int handle_read_input_regs(const uint8_t *req, int req_len,
                                  uint8_t *resp, int resp_size)
{
    if (req_len < 5) return make_exception(FC_READ_INPUT_REGS, EX_ILLEGAL_DATA_VALUE, resp, resp_size);

    uint16_t start_addr = (req[1] << 8) | req[2];
    uint16_t quantity   = (req[3] << 8) | req[4];

    if (quantity == 0 || start_addr + quantity > IREG_COUNT)
        return make_exception(FC_READ_INPUT_REGS, EX_ILLEGAL_DATA_ADDRESS, resp, resp_size);

    uint16_t iregs[IREG_COUNT];
    refresh_input_regs(iregs);

    int byte_count = quantity * 2;
    if (resp_size < 2 + byte_count) return -1;

    resp[0] = FC_READ_INPUT_REGS;
    resp[1] = (uint8_t)byte_count;
    for (uint16_t i = 0; i < quantity; i++) {
        resp[2 + i * 2]     = iregs[start_addr + i] >> 8;
        resp[2 + i * 2 + 1] = iregs[start_addr + i] & 0xFF;
    }
    return 2 + byte_count;
}

static int process_pdu(const uint8_t *req, int req_len,
                       uint8_t *resp, int resp_size)
{
    if (req_len < 1) return -1;

    uint8_t fc = req[0];
    switch (fc) {
    case FC_READ_COILS:
        return handle_read_coils(req, req_len, resp, resp_size);
    case FC_READ_HOLDING_REGS:
        return handle_read_holding_regs(req, req_len, resp, resp_size);
    case FC_READ_INPUT_REGS:
        return handle_read_input_regs(req, req_len, resp, resp_size);
    case FC_WRITE_SINGLE_COIL:
        return handle_write_single_coil(req, req_len, resp, resp_size);
    case FC_WRITE_SINGLE_REG:
        return handle_write_single_reg(req, req_len, resp, resp_size);
    default:
        return make_exception(fc, EX_ILLEGAL_FUNCTION, resp, resp_size);
    }
}

/* ===== Layer 3: TCP server task ===== */

static void modbus_tcp_task(void *arg)
{
    (void)arg;

    int listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "Failed to create socket");
        vTaskDelete(NULL);
        return;
    }

    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in bind_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(MODBUS_TCP_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };

    if (bind(listen_sock, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0) {
        ESP_LOGE(TAG, "Failed to bind to port %d", MODBUS_TCP_PORT);
        close(listen_sock);
        vTaskDelete(NULL);
        return;
    }

    if (listen(listen_sock, 1) < 0) {
        ESP_LOGE(TAG, "Failed to listen");
        close(listen_sock);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Listening on port %d", MODBUS_TCP_PORT);

    for (;;) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client_sock = accept(listen_sock,
                                 (struct sockaddr *)&client_addr, &addr_len);
        if (client_sock < 0) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        s_client_connected = true;
        event_log(EVT_MODBUS, "client_connected ip=%s",
                  inet_ntoa(client_addr.sin_addr));
        ESP_LOGI(TAG, "Client connected from %s",
                 inet_ntoa(client_addr.sin_addr));
        /* Debug: white flash confirms TCP reachability over WiFi */
        status_led_flash_rgb(80, 80, 80, 200);

        /* Set receive timeout (5 seconds) */
        struct timeval tv = { .tv_sec = 5, .tv_usec = 0 };
        setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        /* Client session loop */
        uint8_t buf[MAX_ADU_LEN];
        while (1) {
            /* Read MBAP header (7 bytes) */
            ssize_t n = recv(client_sock, buf, MBAP_HEADER_LEN, 0);
            if (n <= 0) break;
            if (n < MBAP_HEADER_LEN) break;

            uint16_t transaction_id = (buf[0] << 8) | buf[1];
            uint16_t protocol_id    = (buf[2] << 8) | buf[3];
            uint16_t pdu_len        = (buf[4] << 8) | buf[5];
            /* buf[6] = unit_id */

            if (protocol_id != 0 || pdu_len < 2 || pdu_len > MAX_PDU_LEN) break;

            /* Read PDU (pdu_len - 1 bytes, since unit_id counts in length) */
            int pdu_data_len = pdu_len - 1;
            n = recv(client_sock, buf + MBAP_HEADER_LEN, pdu_data_len, 0);
            if (n <= 0 || n < pdu_data_len) break;

            /* Process PDU */
            uint8_t resp_pdu[MAX_PDU_LEN];
            int resp_len = process_pdu(buf + MBAP_HEADER_LEN, pdu_data_len,
                                       resp_pdu, sizeof(resp_pdu));
            if (resp_len <= 0) break;

            /* Build MBAP response header */
            uint8_t resp_adu[MAX_ADU_LEN];
            resp_adu[0] = transaction_id >> 8;
            resp_adu[1] = transaction_id & 0xFF;
            resp_adu[2] = 0; /* protocol ID */
            resp_adu[3] = 0;
            uint16_t resp_mbap_len = resp_len + 1; /* PDU + unit_id */
            resp_adu[4] = resp_mbap_len >> 8;
            resp_adu[5] = resp_mbap_len & 0xFF;
            resp_adu[6] = buf[6]; /* echo unit_id */
            memcpy(resp_adu + MBAP_HEADER_LEN, resp_pdu, resp_len);

            send(client_sock, resp_adu, MBAP_HEADER_LEN + resp_len, 0);
            s_transaction_count++;
        }

        s_client_connected = false;
        shutdown(client_sock, SHUT_RDWR);
        close(client_sock);
        event_log(EVT_MODBUS, "client_disconnected");
        ESP_LOGI(TAG, "Client disconnected");
    }
}

/* ===== Public API ===== */

esp_err_t modbus_server_init(void)
{
    ESP_LOGI(TAG, "Modbus TCP server init");
    s_boot_time_us = esp_timer_get_time();

    xTaskCreate(modbus_tcp_task, "modbus_tcp", 4096, NULL,
                tskIDLE_PRIORITY + 2, NULL);

    event_log(EVT_MODBUS, "init port=%d", MODBUS_TCP_PORT);
    return ESP_OK;
}

bool modbus_server_client_connected(void)
{
    return s_client_connected;
}

uint32_t modbus_server_get_transaction_count(void)
{
    return s_transaction_count;
}

uint16_t modbus_read_input_reg(uint16_t addr)
{
    uint16_t iregs[IREG_COUNT];
    refresh_input_regs(iregs);
    if (addr < IREG_COUNT) return iregs[addr];
    return 0;
}

uint16_t modbus_read_holding_reg(uint16_t addr)
{
    if (addr < HREG_COUNT) return s_holding_regs[addr];
    return 0;
}

void modbus_write_holding_reg(uint16_t addr, uint16_t value)
{
    if (addr < HREG_COUNT) apply_holding_reg(addr, value);
}

bool modbus_read_coil(uint16_t addr)
{
    if (addr == COIL_MOTOR_ENABLE) return motor_get_enable();
    return false;
}

void modbus_write_coil(uint16_t addr, bool value)
{
    if (addr == COIL_MOTOR_ENABLE) {
        motor_set_enable(value);
        event_log(EVT_MODBUS, "write enable=%d", value);
    } else if (addr == COIL_EMERGENCY_STOP && value) {
        motor_emergency_stop();
        event_log(EVT_MODBUS, "emergency_stop");
    }
}

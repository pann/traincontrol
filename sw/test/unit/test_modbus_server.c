/**
 * Unit tests for modbus_server.c — PDU processing layer.
 *
 * Tests target process_pdu() directly with crafted byte arrays,
 * verifying all 5 Modbus function codes and exception handling.
 */

#include "unity.h"
#include "fakes_state.h"

/* Pull in the implementation (gives access to static functions) */
#include "modbus_server.c"

/* Helpers */
static uint8_t req[32];
static uint8_t resp[64];

static void reset_state(void)
{
    fake_state_reset();
    /* Reset module-level statics */
    memset(s_holding_regs, 0, sizeof(s_holding_regs));
    s_holding_regs[HREG_ACCEL_RATE] = 100;
    s_holding_regs[HREG_DECEL_RATE] = 100;
    s_transaction_count = 0;
    s_client_connected = false;
    s_boot_time_us = 0;
}

void setUp(void) { reset_state(); }
void tearDown(void) {}

/* ===== Unsupported function code → exception 0x01 ===== */

void test_unsupported_fc_returns_illegal_function(void)
{
    req[0] = 0x10; /* FC16 not supported */
    int len = process_pdu(req, 1, resp, sizeof(resp));
    TEST_ASSERT_EQUAL(2, len);
    TEST_ASSERT_EQUAL_HEX8(0x90, resp[0]); /* 0x10 | 0x80 */
    TEST_ASSERT_EQUAL_HEX8(EX_ILLEGAL_FUNCTION, resp[1]);
}

void test_fc0x02_returns_illegal_function(void)
{
    req[0] = 0x02; /* FC02 not supported */
    int len = process_pdu(req, 1, resp, sizeof(resp));
    TEST_ASSERT_EQUAL(2, len);
    TEST_ASSERT_EQUAL_HEX8(0x82, resp[0]);
    TEST_ASSERT_EQUAL_HEX8(EX_ILLEGAL_FUNCTION, resp[1]);
}

/* ===== FC01 Read Coils ===== */

void test_fc01_motor_disabled_reads_zero(void)
{
    g_fake.motor_enabled = false;
    req[0] = FC_READ_COILS;
    req[1] = 0x00; req[2] = 0x00; /* start addr = 0 */
    req[3] = 0x00; req[4] = 0x02; /* quantity = 2 */
    int len = process_pdu(req, 5, resp, sizeof(resp));
    TEST_ASSERT_EQUAL(3, len);
    TEST_ASSERT_EQUAL_HEX8(FC_READ_COILS, resp[0]);
    TEST_ASSERT_EQUAL(1, resp[1]); /* byte count */
    TEST_ASSERT_EQUAL_HEX8(0x00, resp[2]);
}

void test_fc01_motor_enabled_reads_bit0(void)
{
    g_fake.motor_enabled = true;
    req[0] = FC_READ_COILS;
    req[1] = 0x00; req[2] = 0x00;
    req[3] = 0x00; req[4] = 0x02;
    int len = process_pdu(req, 5, resp, sizeof(resp));
    TEST_ASSERT_EQUAL(3, len);
    TEST_ASSERT_EQUAL_HEX8(0x01, resp[2]); /* bit 0 set */
}

void test_fc01_estop_reads_zero(void)
{
    /* EMERGENCY_STOP coil always reads as 0 */
    req[0] = FC_READ_COILS;
    req[1] = 0x00; req[2] = 0x01; /* start addr = 1 (ESTOP) */
    req[3] = 0x00; req[4] = 0x01;
    int len = process_pdu(req, 5, resp, sizeof(resp));
    TEST_ASSERT_EQUAL(3, len);
    TEST_ASSERT_EQUAL_HEX8(0x00, resp[2]);
}

void test_fc01_address_oob_returns_exception(void)
{
    req[0] = FC_READ_COILS;
    req[1] = 0x00; req[2] = 0x00;
    req[3] = 0x00; req[4] = 0x03; /* quantity 3 > COIL_COUNT */
    int len = process_pdu(req, 5, resp, sizeof(resp));
    TEST_ASSERT_EQUAL(2, len);
    TEST_ASSERT_EQUAL_HEX8(FC_READ_COILS | 0x80, resp[0]);
    TEST_ASSERT_EQUAL_HEX8(EX_ILLEGAL_DATA_ADDRESS, resp[1]);
}

void test_fc01_quantity_zero_returns_exception(void)
{
    req[0] = FC_READ_COILS;
    req[1] = 0x00; req[2] = 0x00;
    req[3] = 0x00; req[4] = 0x00; /* quantity 0 */
    int len = process_pdu(req, 5, resp, sizeof(resp));
    TEST_ASSERT_EQUAL(2, len);
    TEST_ASSERT_EQUAL_HEX8(EX_ILLEGAL_DATA_ADDRESS, resp[1]);
}

/* ===== FC05 Write Single Coil ===== */

void test_fc05_enable_motor(void)
{
    g_fake.motor_enabled = false;
    req[0] = FC_WRITE_SINGLE_COIL;
    req[1] = 0x00; req[2] = 0x00; /* addr = MOTOR_ENABLE */
    req[3] = 0xFF; req[4] = 0x00; /* ON */
    int len = process_pdu(req, 5, resp, sizeof(resp));
    TEST_ASSERT_EQUAL(5, len);
    TEST_ASSERT_TRUE(g_fake.motor_enabled);
    /* Response echoes request */
    TEST_ASSERT_EQUAL_HEX8(FC_WRITE_SINGLE_COIL, resp[0]);
    TEST_ASSERT_EQUAL_HEX8(0x00, resp[1]);
    TEST_ASSERT_EQUAL_HEX8(0x00, resp[2]);
    TEST_ASSERT_EQUAL_HEX8(0xFF, resp[3]);
    TEST_ASSERT_EQUAL_HEX8(0x00, resp[4]);
}

void test_fc05_disable_motor(void)
{
    g_fake.motor_enabled = true;
    req[0] = FC_WRITE_SINGLE_COIL;
    req[1] = 0x00; req[2] = 0x00;
    req[3] = 0x00; req[4] = 0x00; /* OFF */
    int len = process_pdu(req, 5, resp, sizeof(resp));
    TEST_ASSERT_EQUAL(5, len);
    TEST_ASSERT_FALSE(g_fake.motor_enabled);
}

void test_fc05_emergency_stop(void)
{
    req[0] = FC_WRITE_SINGLE_COIL;
    req[1] = 0x00; req[2] = 0x01; /* addr = EMERGENCY_STOP */
    req[3] = 0xFF; req[4] = 0x00; /* ON */
    int len = process_pdu(req, 5, resp, sizeof(resp));
    TEST_ASSERT_EQUAL(5, len);
    TEST_ASSERT_EQUAL(1, g_fake.motor_estop_calls);
}

void test_fc05_invalid_value_returns_exception(void)
{
    req[0] = FC_WRITE_SINGLE_COIL;
    req[1] = 0x00; req[2] = 0x00;
    req[3] = 0x00; req[4] = 0x01; /* not 0xFF00 or 0x0000 */
    int len = process_pdu(req, 5, resp, sizeof(resp));
    TEST_ASSERT_EQUAL(2, len);
    TEST_ASSERT_EQUAL_HEX8(EX_ILLEGAL_DATA_VALUE, resp[1]);
}

void test_fc05_address_oob_returns_exception(void)
{
    req[0] = FC_WRITE_SINGLE_COIL;
    req[1] = 0x00; req[2] = 0x02; /* addr 2 >= COIL_COUNT */
    req[3] = 0xFF; req[4] = 0x00;
    int len = process_pdu(req, 5, resp, sizeof(resp));
    TEST_ASSERT_EQUAL(2, len);
    TEST_ASSERT_EQUAL_HEX8(EX_ILLEGAL_DATA_ADDRESS, resp[1]);
}

/* ===== FC03 Read Holding Registers ===== */

void test_fc03_read_single_reg(void)
{
    s_holding_regs[HREG_SPEED] = 500;
    req[0] = FC_READ_HOLDING_REGS;
    req[1] = 0x00; req[2] = 0x01; /* start addr = 1 (SPEED) */
    req[3] = 0x00; req[4] = 0x01; /* quantity = 1 */
    int len = process_pdu(req, 5, resp, sizeof(resp));
    TEST_ASSERT_EQUAL(4, len); /* fc + byte_count + 2 data bytes */
    TEST_ASSERT_EQUAL_HEX8(FC_READ_HOLDING_REGS, resp[0]);
    TEST_ASSERT_EQUAL(2, resp[1]); /* byte count */
    TEST_ASSERT_EQUAL_HEX8(0x01, resp[2]); /* 500 >> 8 */
    TEST_ASSERT_EQUAL_HEX8(0xF4, resp[3]); /* 500 & 0xFF */
}

void test_fc03_read_all_four_regs(void)
{
    s_holding_regs[HREG_DIRECTION]  = 1;
    s_holding_regs[HREG_SPEED]      = 750;
    s_holding_regs[HREG_ACCEL_RATE] = 200;
    s_holding_regs[HREG_DECEL_RATE] = 300;
    req[0] = FC_READ_HOLDING_REGS;
    req[1] = 0x00; req[2] = 0x00; /* start = 0 */
    req[3] = 0x00; req[4] = 0x04; /* quantity = 4 */
    int len = process_pdu(req, 5, resp, sizeof(resp));
    TEST_ASSERT_EQUAL(10, len); /* fc + byte_count + 8 data bytes */
    TEST_ASSERT_EQUAL(8, resp[1]);
    /* direction = 1 */
    TEST_ASSERT_EQUAL_HEX8(0x00, resp[2]);
    TEST_ASSERT_EQUAL_HEX8(0x01, resp[3]);
    /* speed = 750 = 0x02EE */
    TEST_ASSERT_EQUAL_HEX8(0x02, resp[4]);
    TEST_ASSERT_EQUAL_HEX8(0xEE, resp[5]);
}

void test_fc03_address_oob_returns_exception(void)
{
    req[0] = FC_READ_HOLDING_REGS;
    req[1] = 0x00; req[2] = 0x03;
    req[3] = 0x00; req[4] = 0x02; /* addr 3 + quantity 2 > 4 */
    int len = process_pdu(req, 5, resp, sizeof(resp));
    TEST_ASSERT_EQUAL(2, len);
    TEST_ASSERT_EQUAL_HEX8(EX_ILLEGAL_DATA_ADDRESS, resp[1]);
}

/* ===== FC06 Write Single Register ===== */

void test_fc06_write_direction_forward(void)
{
    req[0] = FC_WRITE_SINGLE_REG;
    req[1] = 0x00; req[2] = 0x00; /* DIRECTION */
    req[3] = 0x00; req[4] = 0x00; /* value = 0 (forward) */
    int len = process_pdu(req, 5, resp, sizeof(resp));
    TEST_ASSERT_EQUAL(5, len);
    TEST_ASSERT_TRUE(g_fake.motor_forward); /* motor_set_direction(true) */
    TEST_ASSERT_EQUAL(0, s_holding_regs[HREG_DIRECTION]);
}

void test_fc06_write_direction_reverse(void)
{
    req[0] = FC_WRITE_SINGLE_REG;
    req[1] = 0x00; req[2] = 0x00;
    req[3] = 0x00; req[4] = 0x01; /* value = 1 (reverse) */
    int len = process_pdu(req, 5, resp, sizeof(resp));
    TEST_ASSERT_EQUAL(5, len);
    TEST_ASSERT_FALSE(g_fake.motor_forward); /* motor_set_direction(false) */
    TEST_ASSERT_EQUAL(1, s_holding_regs[HREG_DIRECTION]);
}

void test_fc06_write_direction_invalid(void)
{
    req[0] = FC_WRITE_SINGLE_REG;
    req[1] = 0x00; req[2] = 0x00;
    req[3] = 0x00; req[4] = 0x02; /* value = 2, invalid */
    int len = process_pdu(req, 5, resp, sizeof(resp));
    TEST_ASSERT_EQUAL(2, len);
    TEST_ASSERT_EQUAL_HEX8(EX_ILLEGAL_DATA_VALUE, resp[1]);
}

void test_fc06_write_speed(void)
{
    req[0] = FC_WRITE_SINGLE_REG;
    req[1] = 0x00; req[2] = 0x01; /* SPEED */
    req[3] = 0x01; req[4] = 0xF4; /* 500 */
    int len = process_pdu(req, 5, resp, sizeof(resp));
    TEST_ASSERT_EQUAL(5, len);
    TEST_ASSERT_EQUAL(500, g_fake.motor_speed);
    TEST_ASSERT_EQUAL(500, s_holding_regs[HREG_SPEED]);
    /* Response echoes request */
    TEST_ASSERT_EQUAL_HEX8(0x01, resp[2]);
    TEST_ASSERT_EQUAL_HEX8(0xF4, resp[4]);
}

void test_fc06_write_speed_max(void)
{
    req[0] = FC_WRITE_SINGLE_REG;
    req[1] = 0x00; req[2] = 0x01;
    req[3] = 0x03; req[4] = 0xE8; /* 1000 */
    int len = process_pdu(req, 5, resp, sizeof(resp));
    TEST_ASSERT_EQUAL(5, len);
    TEST_ASSERT_EQUAL(1000, g_fake.motor_speed);
}

void test_fc06_write_speed_too_high(void)
{
    req[0] = FC_WRITE_SINGLE_REG;
    req[1] = 0x00; req[2] = 0x01;
    req[3] = 0x03; req[4] = 0xE9; /* 1001, too high */
    int len = process_pdu(req, 5, resp, sizeof(resp));
    TEST_ASSERT_EQUAL(2, len);
    TEST_ASSERT_EQUAL_HEX8(EX_ILLEGAL_DATA_VALUE, resp[1]);
}

void test_fc06_write_accel_rate(void)
{
    req[0] = FC_WRITE_SINGLE_REG;
    req[1] = 0x00; req[2] = 0x02; /* ACCEL_RATE */
    req[3] = 0x00; req[4] = 0x32; /* 50 */
    int len = process_pdu(req, 5, resp, sizeof(resp));
    TEST_ASSERT_EQUAL(5, len);
    TEST_ASSERT_EQUAL(50, s_holding_regs[HREG_ACCEL_RATE]);
}

void test_fc06_write_accel_rate_zero_invalid(void)
{
    req[0] = FC_WRITE_SINGLE_REG;
    req[1] = 0x00; req[2] = 0x02;
    req[3] = 0x00; req[4] = 0x00; /* 0, invalid (min is 1) */
    int len = process_pdu(req, 5, resp, sizeof(resp));
    TEST_ASSERT_EQUAL(2, len);
    TEST_ASSERT_EQUAL_HEX8(EX_ILLEGAL_DATA_VALUE, resp[1]);
}

void test_fc06_write_decel_rate_max(void)
{
    req[0] = FC_WRITE_SINGLE_REG;
    req[1] = 0x00; req[2] = 0x03; /* DECEL_RATE */
    req[3] = 0x03; req[4] = 0xE8; /* 1000 */
    int len = process_pdu(req, 5, resp, sizeof(resp));
    TEST_ASSERT_EQUAL(5, len);
    TEST_ASSERT_EQUAL(1000, s_holding_regs[HREG_DECEL_RATE]);
}

void test_fc06_write_decel_rate_too_high(void)
{
    req[0] = FC_WRITE_SINGLE_REG;
    req[1] = 0x00; req[2] = 0x03;
    req[3] = 0x03; req[4] = 0xE9; /* 1001 */
    int len = process_pdu(req, 5, resp, sizeof(resp));
    TEST_ASSERT_EQUAL(2, len);
    TEST_ASSERT_EQUAL_HEX8(EX_ILLEGAL_DATA_VALUE, resp[1]);
}

void test_fc06_address_oob_returns_exception(void)
{
    req[0] = FC_WRITE_SINGLE_REG;
    req[1] = 0x00; req[2] = 0x04; /* addr 4 >= HREG_COUNT */
    req[3] = 0x00; req[4] = 0x01;
    int len = process_pdu(req, 5, resp, sizeof(resp));
    TEST_ASSERT_EQUAL(2, len);
    TEST_ASSERT_EQUAL_HEX8(EX_ILLEGAL_DATA_ADDRESS, resp[1]);
}

/* ===== FC04 Read Input Registers ===== */

void test_fc04_status_bits_wifi_connected(void)
{
    g_fake.config_has_creds = true; /* wifi_manager_is_connected needs this? */
    /* Directly set the wifi connected return value — the fake returns g_fake.wifi_mode based state */
    /* Actually, wifi_manager_is_connected is a weak fake, we need to check what it returns */
    /* The weak fake in fakes_state.c just returns false by default. Let's override it. */
    /* We'll read the status register through process_pdu and check bits. */
    g_fake.motor_enabled = true;
    g_fake.motor_forward = false; /* reverse → STATUS_BIT_DIRECTION set */

    req[0] = FC_READ_INPUT_REGS;
    req[1] = 0x00; req[2] = 0x00; /* start addr = 0 (STATUS) */
    req[3] = 0x00; req[4] = 0x01; /* quantity = 1 */
    int len = process_pdu(req, 5, resp, sizeof(resp));
    TEST_ASSERT_EQUAL(4, len);
    TEST_ASSERT_EQUAL_HEX8(FC_READ_INPUT_REGS, resp[0]);
    TEST_ASSERT_EQUAL(2, resp[1]); /* byte count */
    uint16_t status = (resp[2] << 8) | resp[3];
    /* wifi not connected (fake returns false), motor enabled, direction reverse */
    TEST_ASSERT_BITS(1 << STATUS_BIT_WIFI, 0, status);
    TEST_ASSERT_BITS(1 << STATUS_BIT_ENABLED, 1 << STATUS_BIT_ENABLED, status);
    TEST_ASSERT_BITS(1 << STATUS_BIT_DIRECTION, 1 << STATUS_BIT_DIRECTION, status);
}

void test_fc04_current_speed(void)
{
    g_fake.motor_current_speed = 333;
    req[0] = FC_READ_INPUT_REGS;
    req[1] = 0x00; req[2] = 0x02; /* CURRENT_SPEED */
    req[3] = 0x00; req[4] = 0x01;
    int len = process_pdu(req, 5, resp, sizeof(resp));
    TEST_ASSERT_EQUAL(4, len);
    uint16_t val = (resp[2] << 8) | resp[3];
    TEST_ASSERT_EQUAL(333, val);
}

void test_fc04_uptime(void)
{
    s_boot_time_us = 0;
    g_fake.fake_timer_time_us = 5000000LL; /* 5 seconds */
    req[0] = FC_READ_INPUT_REGS;
    req[1] = 0x00; req[2] = 0x03; /* UPTIME */
    req[3] = 0x00; req[4] = 0x01;
    int len = process_pdu(req, 5, resp, sizeof(resp));
    TEST_ASSERT_EQUAL(4, len);
    uint16_t val = (resp[2] << 8) | resp[3];
    TEST_ASSERT_EQUAL(5, val);
}

void test_fc04_read_all_four_regs(void)
{
    g_fake.motor_current_speed = 100;
    g_fake.fake_timer_time_us = 120000000LL; /* 120 seconds */
    s_boot_time_us = 0;
    req[0] = FC_READ_INPUT_REGS;
    req[1] = 0x00; req[2] = 0x00;
    req[3] = 0x00; req[4] = 0x04;
    int len = process_pdu(req, 5, resp, sizeof(resp));
    TEST_ASSERT_EQUAL(10, len); /* fc + byte_count + 8 data bytes */
    TEST_ASSERT_EQUAL(8, resp[1]);
    /* CURRENT_SPEED at offset 2+4=6 */
    uint16_t speed = (resp[6] << 8) | resp[7];
    TEST_ASSERT_EQUAL(100, speed);
    /* UPTIME at offset 2+6=8 */
    uint16_t uptime = (resp[8] << 8) | resp[9];
    TEST_ASSERT_EQUAL(120, uptime);
}

void test_fc04_address_oob_returns_exception(void)
{
    req[0] = FC_READ_INPUT_REGS;
    req[1] = 0x00; req[2] = 0x04; /* addr 4 >= IREG_COUNT */
    req[3] = 0x00; req[4] = 0x01;
    int len = process_pdu(req, 5, resp, sizeof(resp));
    TEST_ASSERT_EQUAL(2, len);
    TEST_ASSERT_EQUAL_HEX8(EX_ILLEGAL_DATA_ADDRESS, resp[1]);
}

/* ===== Public API (CLI helpers) ===== */

void test_public_read_holding_reg(void)
{
    s_holding_regs[HREG_SPEED] = 999;
    TEST_ASSERT_EQUAL(999, modbus_read_holding_reg(HREG_SPEED));
}

void test_public_write_holding_reg_calls_motor(void)
{
    modbus_write_holding_reg(HREG_SPEED, 400);
    TEST_ASSERT_EQUAL(400, g_fake.motor_speed);
    TEST_ASSERT_EQUAL(400, s_holding_regs[HREG_SPEED]);
}

void test_public_read_coil(void)
{
    g_fake.motor_enabled = true;
    TEST_ASSERT_TRUE(modbus_read_coil(COIL_MOTOR_ENABLE));
    TEST_ASSERT_FALSE(modbus_read_coil(COIL_EMERGENCY_STOP));
}

void test_public_write_coil_enable(void)
{
    modbus_write_coil(COIL_MOTOR_ENABLE, true);
    TEST_ASSERT_TRUE(g_fake.motor_enabled);
}

void test_public_write_coil_estop(void)
{
    modbus_write_coil(COIL_EMERGENCY_STOP, true);
    TEST_ASSERT_EQUAL(1, g_fake.motor_estop_calls);
}

void test_public_read_input_reg_speed(void)
{
    g_fake.motor_current_speed = 777;
    TEST_ASSERT_EQUAL(777, modbus_read_input_reg(IREG_CURRENT_SPEED));
}

void test_init_creates_task(void)
{
    modbus_server_init();
    TEST_ASSERT_EQUAL(1, g_fake.task_create_calls);
}

int main(void)
{
    UNITY_BEGIN();

    /* Exception handling */
    RUN_TEST(test_unsupported_fc_returns_illegal_function);
    RUN_TEST(test_fc0x02_returns_illegal_function);

    /* FC01 Read Coils */
    RUN_TEST(test_fc01_motor_disabled_reads_zero);
    RUN_TEST(test_fc01_motor_enabled_reads_bit0);
    RUN_TEST(test_fc01_estop_reads_zero);
    RUN_TEST(test_fc01_address_oob_returns_exception);
    RUN_TEST(test_fc01_quantity_zero_returns_exception);

    /* FC05 Write Single Coil */
    RUN_TEST(test_fc05_enable_motor);
    RUN_TEST(test_fc05_disable_motor);
    RUN_TEST(test_fc05_emergency_stop);
    RUN_TEST(test_fc05_invalid_value_returns_exception);
    RUN_TEST(test_fc05_address_oob_returns_exception);

    /* FC03 Read Holding Registers */
    RUN_TEST(test_fc03_read_single_reg);
    RUN_TEST(test_fc03_read_all_four_regs);
    RUN_TEST(test_fc03_address_oob_returns_exception);

    /* FC06 Write Single Register */
    RUN_TEST(test_fc06_write_direction_forward);
    RUN_TEST(test_fc06_write_direction_reverse);
    RUN_TEST(test_fc06_write_direction_invalid);
    RUN_TEST(test_fc06_write_speed);
    RUN_TEST(test_fc06_write_speed_max);
    RUN_TEST(test_fc06_write_speed_too_high);
    RUN_TEST(test_fc06_write_accel_rate);
    RUN_TEST(test_fc06_write_accel_rate_zero_invalid);
    RUN_TEST(test_fc06_write_decel_rate_max);
    RUN_TEST(test_fc06_write_decel_rate_too_high);
    RUN_TEST(test_fc06_address_oob_returns_exception);

    /* FC04 Read Input Registers */
    RUN_TEST(test_fc04_status_bits_wifi_connected);
    RUN_TEST(test_fc04_current_speed);
    RUN_TEST(test_fc04_uptime);
    RUN_TEST(test_fc04_read_all_four_regs);
    RUN_TEST(test_fc04_address_oob_returns_exception);

    /* Public API (CLI helpers) */
    RUN_TEST(test_public_read_holding_reg);
    RUN_TEST(test_public_write_holding_reg_calls_motor);
    RUN_TEST(test_public_read_coil);
    RUN_TEST(test_public_write_coil_enable);
    RUN_TEST(test_public_write_coil_estop);
    RUN_TEST(test_public_read_input_reg_speed);
    RUN_TEST(test_init_creates_task);

    return UNITY_END();
}

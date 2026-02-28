/*
 * Unit tests for provisioning module.
 *
 * Tests the URL-encoded form field parser (parse_form_field)
 * and CLI command registration.
 */

#include "fakes_state.h"

/* Include the production .c to access statics */
#include "provisioning.c"

#include "unity.h"

void setUp(void)
{
    fake_state_reset();
    s_httpd = NULL;
    s_ap_netif = NULL;
    memset(s_ap_ssid, 0, sizeof(s_ap_ssid));
}

void tearDown(void) { }

/* ===== parse_form_field() tests ===== */

void test_parse_simple_field(void)
{
    char out[64] = {0};
    esp_err_t ret = parse_form_field("ssid=MyNetwork&password=secret",
                                      "ssid", out, sizeof(out));
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_EQUAL_STRING("MyNetwork", out);
}

void test_parse_second_field(void)
{
    char out[64] = {0};
    esp_err_t ret = parse_form_field("ssid=MyNetwork&password=secret",
                                      "password", out, sizeof(out));
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_EQUAL_STRING("secret", out);
}

void test_parse_plus_to_space(void)
{
    char out[64] = {0};
    parse_form_field("ssid=My+Home+Network", "ssid", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("My Home Network", out);
}

void test_parse_percent_encoding(void)
{
    char out[64] = {0};
    parse_form_field("ssid=caf%C3%A9", "ssid", out, sizeof(out));
    /* %C3 = 0xC3, %A9 = 0xA9 (UTF-8 for é) */
    TEST_ASSERT_EQUAL('\xC3', out[3]);
    TEST_ASSERT_EQUAL('\xA9', out[4]);
    TEST_ASSERT_EQUAL_STRING("caf\xC3\xA9", out);
}

void test_parse_missing_field(void)
{
    char out[64] = {0};
    esp_err_t ret = parse_form_field("ssid=foo", "password", out, sizeof(out));
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, ret);
}

void test_parse_empty_value(void)
{
    char out[64] = "dirty";
    esp_err_t ret = parse_form_field("ssid=&password=x", "ssid", out, sizeof(out));
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_EQUAL_STRING("", out);
}

void test_parse_partial_name_rejected(void)
{
    /* "ssid_extra" should not match "ssid" */
    char out[64] = {0};
    esp_err_t ret = parse_form_field("ssid_extra=foo", "ssid", out, sizeof(out));
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, ret);
}

void test_parse_field_after_partial_match(void)
{
    /* Should skip "xssid" and find "ssid" */
    char out[64] = {0};
    esp_err_t ret = parse_form_field("xssid=wrong&ssid=right",
                                      "ssid", out, sizeof(out));
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_EQUAL_STRING("right", out);
}

/* ===== CLI registration test ===== */

void test_init_registers_cli_commands(void)
{
    provisioning_init();
    /* wifi_set, wifi_status, factory_reset, mr, mw, modbus_status, reset,
       speed, dir, enable, estop, status */
    TEST_ASSERT_EQUAL(12, g_fake.console_cmd_register_calls);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_parse_simple_field);
    RUN_TEST(test_parse_second_field);
    RUN_TEST(test_parse_plus_to_space);
    RUN_TEST(test_parse_percent_encoding);
    RUN_TEST(test_parse_missing_field);
    RUN_TEST(test_parse_empty_value);
    RUN_TEST(test_parse_partial_name_rejected);
    RUN_TEST(test_parse_field_after_partial_match);
    RUN_TEST(test_init_registers_cli_commands);

    return UNITY_END();
}

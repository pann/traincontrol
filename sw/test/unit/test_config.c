/*
 * Unit tests for config module.
 *
 * Uses the #include-the-.c-file pattern. NVS calls are backed by
 * the in-memory fake NVS store in fakes_state.
 */

#include "fakes_state.h"

/* Include the .c under test */
#include "config.c"

#include "unity.h"
#include <string.h>

void setUp(void)
{
    fake_state_reset();
    /* Reset module state */
    memset(s_ssid, 0, sizeof(s_ssid));
    memset(s_password, 0, sizeof(s_password));
}

void tearDown(void) { }

/* ===== config_init() tests ===== */

void test_init_with_no_stored_credentials(void)
{
    esp_err_t err = config_init();
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL_STRING("", config_get_wifi_ssid());
    TEST_ASSERT_EQUAL_STRING("", config_get_wifi_password());
    TEST_ASSERT_FALSE(config_has_wifi_credentials());
}

void test_init_loads_stored_credentials(void)
{
    fake_nvs_preset("wifi_ssid", "MyNetwork");
    fake_nvs_preset("wifi_pass", "secret123");

    esp_err_t err = config_init();
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL_STRING("MyNetwork", config_get_wifi_ssid());
    TEST_ASSERT_EQUAL_STRING("secret123", config_get_wifi_password());
    TEST_ASSERT_TRUE(config_has_wifi_credentials());
}

/* ===== config_has_wifi_credentials() tests ===== */

void test_has_credentials_false_when_empty(void)
{
    TEST_ASSERT_FALSE(config_has_wifi_credentials());
}

void test_has_credentials_true_after_set(void)
{
    config_set_wifi_credentials("net", "pass");
    TEST_ASSERT_TRUE(config_has_wifi_credentials());
}

/* ===== config_set_wifi_credentials() tests ===== */

void test_set_and_get_credentials(void)
{
    config_set_wifi_credentials("TestSSID", "TestPass");
    TEST_ASSERT_EQUAL_STRING("TestSSID", config_get_wifi_ssid());
    TEST_ASSERT_EQUAL_STRING("TestPass", config_get_wifi_password());
}

void test_set_credentials_overwrites_previous(void)
{
    config_set_wifi_credentials("First", "Pass1");
    config_set_wifi_credentials("Second", "Pass2");
    TEST_ASSERT_EQUAL_STRING("Second", config_get_wifi_ssid());
    TEST_ASSERT_EQUAL_STRING("Pass2", config_get_wifi_password());
}

void test_set_credentials_stores_in_nvs(void)
{
    config_set_wifi_credentials("NetName", "NetPass");

    /* Verify the fake NVS store has the values */
    int found = 0;
    for (int i = 0; i < g_fake.nvs_store_count; i++) {
        if (strcmp(g_fake.nvs_store[i].key, "wifi_ssid") == 0) {
            TEST_ASSERT_EQUAL_STRING("NetName", g_fake.nvs_store[i].value);
            found++;
        }
        if (strcmp(g_fake.nvs_store[i].key, "wifi_pass") == 0) {
            TEST_ASSERT_EQUAL_STRING("NetPass", g_fake.nvs_store[i].value);
            found++;
        }
    }
    TEST_ASSERT_EQUAL(2, found);
}

/* ===== config_factory_reset() tests ===== */

void test_factory_reset_clears_credentials(void)
{
    config_set_wifi_credentials("SSID", "PASS");
    TEST_ASSERT_TRUE(config_has_wifi_credentials());

    config_factory_reset();
    TEST_ASSERT_FALSE(config_has_wifi_credentials());
    TEST_ASSERT_EQUAL_STRING("", config_get_wifi_ssid());
    TEST_ASSERT_EQUAL_STRING("", config_get_wifi_password());
}

void test_factory_reset_clears_nvs(void)
{
    config_set_wifi_credentials("SSID", "PASS");
    config_factory_reset();
    /* NVS store should be empty after erase_all */
    TEST_ASSERT_EQUAL(0, g_fake.nvs_store_count);
}

/* ===== Buffer boundary tests ===== */

void test_ssid_truncated_at_32_chars(void)
{
    /* s_ssid is char[33], so max 32 chars + null */
    const char *long_ssid = "ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890";  /* 36 chars */
    config_set_wifi_credentials(long_ssid, "short");
    /* strncpy with sizeof(s_ssid)-1 = 32 should truncate */
    TEST_ASSERT_EQUAL(32, strlen(config_get_wifi_ssid()));
}

void test_password_truncated_at_64_chars(void)
{
    /* s_password is char[65], so max 64 chars + null */
    char long_pass[80];
    memset(long_pass, 'x', 79);
    long_pass[79] = '\0';
    config_set_wifi_credentials("ssid", long_pass);
    TEST_ASSERT_EQUAL(64, strlen(config_get_wifi_password()));
}

/* ===== Runner ===== */

int main(void)
{
    UNITY_BEGIN();

    /* config_init */
    RUN_TEST(test_init_with_no_stored_credentials);
    RUN_TEST(test_init_loads_stored_credentials);

    /* config_has_wifi_credentials */
    RUN_TEST(test_has_credentials_false_when_empty);
    RUN_TEST(test_has_credentials_true_after_set);

    /* config_set_wifi_credentials */
    RUN_TEST(test_set_and_get_credentials);
    RUN_TEST(test_set_credentials_overwrites_previous);
    RUN_TEST(test_set_credentials_stores_in_nvs);

    /* config_factory_reset */
    RUN_TEST(test_factory_reset_clears_credentials);
    RUN_TEST(test_factory_reset_clears_nvs);

    /* Buffer boundaries */
    RUN_TEST(test_ssid_truncated_at_32_chars);
    RUN_TEST(test_password_truncated_at_64_chars);

    return UNITY_END();
}

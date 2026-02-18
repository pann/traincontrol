/*
 * Unit tests for wifi_manager module.
 *
 * Tests hostname generation, event handler state transitions,
 * and public API initial state.
 */

#include "fakes_state.h"

/* Include the production .c to access statics */
#include "wifi_manager.c"

#include "unity.h"

void setUp(void)
{
    fake_state_reset();
    s_is_connected = false;
    memset(s_ip_str, 0, sizeof(s_ip_str));
    s_retry_count = 0;
    memset(s_hostname, 0, sizeof(s_hostname));
    s_sta_netif = NULL;
}

void tearDown(void) { }

/* ===== build_hostname() tests ===== */

void test_hostname_default_mac(void)
{
    /* Default fake MAC is all zeros */
    build_hostname();
    TEST_ASSERT_EQUAL_STRING("trainctrl-0000", s_hostname);
}

void test_hostname_uses_last_two_mac_bytes(void)
{
    fake_set_mac(0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF);
    build_hostname();
    TEST_ASSERT_EQUAL_STRING("trainctrl-eeff", s_hostname);
}

/* ===== Event handler state tests ===== */

void test_got_ip_sets_connected(void)
{
    ip_event_got_ip_t evt = {
        .ip_info.ip.addr = 0x0100A8C0,  /* 192.168.0.1 little-endian */
    };
    wifi_event_handler(NULL, IP_EVENT, IP_EVENT_STA_GOT_IP, &evt);

    TEST_ASSERT_TRUE(s_is_connected);
    TEST_ASSERT_EQUAL_STRING("192.168.0.1", s_ip_str);
    TEST_ASSERT_EQUAL(0, s_retry_count);
}

void test_got_ip_registers_mdns(void)
{
    build_hostname();
    ip_event_got_ip_t evt = {
        .ip_info.ip.addr = 0x01020304,
    };
    wifi_event_handler(NULL, IP_EVENT, IP_EVENT_STA_GOT_IP, &evt);

    TEST_ASSERT_EQUAL(1, g_fake.mdns_init_calls);
    TEST_ASSERT_EQUAL(1, g_fake.mdns_service_add_calls);
    TEST_ASSERT_EQUAL_STRING(s_hostname, g_fake.mdns_hostname);
}

void test_disconnect_clears_connected(void)
{
    s_is_connected = true;
    snprintf(s_ip_str, sizeof(s_ip_str), "1.2.3.4");

    wifi_event_sta_disconnected_t evt = { .reason = 8 };
    wifi_event_handler(NULL, WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &evt);

    TEST_ASSERT_FALSE(s_is_connected);
    TEST_ASSERT_EQUAL_STRING("", s_ip_str);
    TEST_ASSERT_EQUAL(1, s_retry_count);
}

void test_disconnect_retries_connection(void)
{
    wifi_event_sta_disconnected_t evt = { .reason = 8 };
    wifi_event_handler(NULL, WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &evt);

    TEST_ASSERT_EQUAL(1, g_fake.wifi_connect_calls);
}

void test_disconnect_stops_after_max_retries(void)
{
    s_retry_count = WIFI_MAX_RETRY;
    wifi_event_sta_disconnected_t evt = { .reason = 8 };
    wifi_event_handler(NULL, WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &evt);

    /* Should NOT increment retry count or call connect */
    TEST_ASSERT_EQUAL(WIFI_MAX_RETRY, s_retry_count);
    TEST_ASSERT_EQUAL(0, g_fake.wifi_connect_calls);
}

/* ===== Public API tests ===== */

void test_is_connected_false_initially(void)
{
    TEST_ASSERT_FALSE(wifi_manager_is_connected());
}

void test_get_ip_str_empty_initially(void)
{
    TEST_ASSERT_EQUAL_STRING("", wifi_manager_get_ip_str());
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_hostname_default_mac);
    RUN_TEST(test_hostname_uses_last_two_mac_bytes);
    RUN_TEST(test_got_ip_sets_connected);
    RUN_TEST(test_got_ip_registers_mdns);
    RUN_TEST(test_disconnect_clears_connected);
    RUN_TEST(test_disconnect_retries_connection);
    RUN_TEST(test_disconnect_stops_after_max_retries);
    RUN_TEST(test_is_connected_false_initially);
    RUN_TEST(test_get_ip_str_empty_initially);

    return UNITY_END();
}

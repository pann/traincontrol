/*
 * Unit tests for motor_control module.
 *
 * Uses the #include-the-.c-file pattern to access static functions.
 * Fake ESP-IDF headers are resolved via include path ordering (fakes/ first).
 */

/* Include fakes first — motor_control.c's #includes will find these */
#include "fakes_state.h"

/* Include the .c under test — gives access to all statics */
#include "motor_control.c"

#include "unity.h"

void setUp(void)
{
    fake_state_reset();
    /* Reset module state between tests */
    s_enabled = false;
    s_forward = true;
    s_target  = 0;
    s_current = 0;
}

void tearDown(void) { }

/* ===== speed_to_delay_us() boundary tests ===== */

void test_speed_zero_returns_zero(void)
{
    TEST_ASSERT_EQUAL_UINT32(0, speed_to_delay_us(0));
}

void test_speed_one_returns_fire_max(void)
{
    TEST_ASSERT_EQUAL_UINT32(FIRE_MAX_US, speed_to_delay_us(1));
}

void test_speed_1000_returns_fire_min(void)
{
    TEST_ASSERT_EQUAL_UINT32(FIRE_MIN_US, speed_to_delay_us(1000));
}

void test_speed_500_returns_midpoint(void)
{
    uint32_t expected = FIRE_MAX_US
        - (uint32_t)(500 - 1) * (FIRE_MAX_US - FIRE_MIN_US) / 999;
    TEST_ASSERT_EQUAL_UINT32(expected, speed_to_delay_us(500));
}

void test_speed_monotonically_decreasing(void)
{
    uint32_t prev = speed_to_delay_us(1);
    for (uint16_t s = 2; s <= 1000; s++) {
        uint32_t cur = speed_to_delay_us(s);
        TEST_ASSERT_TRUE_MESSAGE(cur <= prev,
            "Delay must decrease as speed increases");
        prev = cur;
    }
}

void test_speed_delay_always_in_valid_range(void)
{
    for (uint16_t s = 1; s <= 1000; s++) {
        uint32_t d = speed_to_delay_us(s);
        TEST_ASSERT_TRUE(d >= FIRE_MIN_US);
        TEST_ASSERT_TRUE(d <= FIRE_MAX_US);
    }
}

/* ===== motor_set_speed() tests ===== */

void test_set_speed_normal(void)
{
    motor_set_speed(500);
    TEST_ASSERT_EQUAL_UINT16(500, s_target);
    TEST_ASSERT_EQUAL_UINT16(500, s_current);
}

void test_set_speed_zero(void)
{
    motor_set_speed(0);
    TEST_ASSERT_EQUAL_UINT16(0, s_target);
    TEST_ASSERT_EQUAL_UINT16(0, s_current);
}

void test_set_speed_clamps_above_1000(void)
{
    motor_set_speed(1500);
    TEST_ASSERT_EQUAL_UINT16(1000, s_target);
    TEST_ASSERT_EQUAL_UINT16(1000, s_current);
}

void test_set_speed_boundary_1000(void)
{
    motor_set_speed(1000);
    TEST_ASSERT_EQUAL_UINT16(1000, s_target);
    TEST_ASSERT_EQUAL_UINT16(1000, s_current);
}

void test_get_current_speed_matches(void)
{
    motor_set_speed(750);
    TEST_ASSERT_EQUAL_UINT16(750, motor_get_current_speed());
}

/* ===== motor_set_direction() tests ===== */

void test_set_direction_forward(void)
{
    s_forward = false;
    motor_set_direction(true);
    TEST_ASSERT_TRUE(s_forward);
    TEST_ASSERT_TRUE(motor_get_direction_forward());
}

void test_set_direction_reverse(void)
{
    s_forward = true;
    motor_set_direction(false);
    TEST_ASSERT_FALSE(s_forward);
    TEST_ASSERT_FALSE(motor_get_direction_forward());
}

void test_set_direction_deasserts_both_gpios(void)
{
    motor_set_direction(false);
    /* Expect at least 2 gpio_set_level calls, both setting level 0 */
    TEST_ASSERT_TRUE(g_fake.gpio_history_count >= 2);
    TEST_ASSERT_EQUAL(GPIO_MOTOR_FWD, g_fake.gpio_history[0].pin);
    TEST_ASSERT_EQUAL_UINT32(0, g_fake.gpio_history[0].level);
    TEST_ASSERT_EQUAL(GPIO_MOTOR_REV, g_fake.gpio_history[1].pin);
    TEST_ASSERT_EQUAL_UINT32(0, g_fake.gpio_history[1].level);
}

/* ===== motor_set_enable() tests ===== */

void test_enable_sets_flag(void)
{
    motor_set_enable(true);
    TEST_ASSERT_TRUE(s_enabled);
    TEST_ASSERT_TRUE(motor_get_enable());
}

void test_disable_clears_flag_and_speed(void)
{
    s_enabled = true;
    s_current = 500;
    motor_set_enable(false);
    TEST_ASSERT_FALSE(s_enabled);
    TEST_ASSERT_EQUAL_UINT16(0, s_current);
}

void test_disable_deasserts_gpios(void)
{
    s_enabled = true;
    motor_set_enable(false);
    TEST_ASSERT_TRUE(g_fake.gpio_history_count >= 2);
    TEST_ASSERT_EQUAL_UINT32(0, g_fake.gpio_history[0].level);
    TEST_ASSERT_EQUAL_UINT32(0, g_fake.gpio_history[1].level);
}

void test_enable_does_not_change_speed(void)
{
    s_current = 0;
    s_target  = 500;
    motor_set_enable(true);
    TEST_ASSERT_EQUAL_UINT16(0, s_current);
}

/* ===== motor_emergency_stop() tests ===== */

void test_emergency_stop_resets_all_state(void)
{
    s_enabled = true;
    s_forward = false;
    s_target  = 800;
    s_current = 800;

    motor_emergency_stop();

    TEST_ASSERT_FALSE(s_enabled);
    TEST_ASSERT_EQUAL_UINT16(0, s_target);
    TEST_ASSERT_EQUAL_UINT16(0, s_current);
}

void test_emergency_stop_deasserts_gpios(void)
{
    s_enabled = true;
    s_current = 500;
    motor_emergency_stop();
    TEST_ASSERT_TRUE(g_fake.gpio_history_count >= 2);
    TEST_ASSERT_EQUAL(GPIO_MOTOR_FWD, g_fake.gpio_history[0].pin);
    TEST_ASSERT_EQUAL_UINT32(0, g_fake.gpio_history[0].level);
    TEST_ASSERT_EQUAL(GPIO_MOTOR_REV, g_fake.gpio_history[1].pin);
    TEST_ASSERT_EQUAL_UINT32(0, g_fake.gpio_history[1].level);
}

/* ===== ISR logic tests (logic only, not real timing) ===== */

void test_isr_does_nothing_when_disabled(void)
{
    s_enabled = false;
    s_current = 500;
    zero_cross_isr(NULL);
    TEST_ASSERT_EQUAL(0, g_fake.timer_start_once_calls);
}

void test_isr_does_nothing_when_speed_zero(void)
{
    s_enabled = true;
    s_current = 0;
    zero_cross_isr(NULL);
    TEST_ASSERT_EQUAL(0, g_fake.timer_start_once_calls);
}

void test_isr_starts_timer_when_enabled_and_speed_nonzero(void)
{
    /* Need to initialise s_fire_timer via motor_control_init first */
    motor_control_init();
    fake_state_reset();  /* clear init gpio calls */

    s_enabled = true;
    s_current = 500;
    zero_cross_isr(NULL);
    TEST_ASSERT_EQUAL(1, g_fake.timer_start_once_calls);
    TEST_ASSERT_EQUAL_UINT64(speed_to_delay_us(500),
                             g_fake.last_timer_timeout_us);
}

/* ===== fire_timer_cb() tests ===== */

void test_fire_cb_asserts_fwd_gpio_when_forward(void)
{
    motor_control_init();
    fake_state_reset();

    s_forward = true;
    fire_timer_cb(NULL);

    TEST_ASSERT_EQUAL(GPIO_MOTOR_FWD, g_fake.gpio_history[0].pin);
    TEST_ASSERT_EQUAL_UINT32(1, g_fake.gpio_history[0].level);
    /* Also starts pulse-end timer */
    TEST_ASSERT_EQUAL(1, g_fake.timer_start_once_calls);
    TEST_ASSERT_EQUAL_UINT64(GATE_PULSE_US, g_fake.last_timer_timeout_us);
}

void test_fire_cb_asserts_rev_gpio_when_reverse(void)
{
    motor_control_init();
    fake_state_reset();

    s_forward = false;
    fire_timer_cb(NULL);

    TEST_ASSERT_EQUAL(GPIO_MOTOR_REV, g_fake.gpio_history[0].pin);
    TEST_ASSERT_EQUAL_UINT32(1, g_fake.gpio_history[0].level);
}

/* ===== pulse_end_cb() tests ===== */

void test_pulse_end_deasserts_both_gpios(void)
{
    pulse_end_cb(NULL);

    TEST_ASSERT_EQUAL(2, g_fake.gpio_history_count);
    TEST_ASSERT_EQUAL(GPIO_MOTOR_FWD, g_fake.gpio_history[0].pin);
    TEST_ASSERT_EQUAL_UINT32(0, g_fake.gpio_history[0].level);
    TEST_ASSERT_EQUAL(GPIO_MOTOR_REV, g_fake.gpio_history[1].pin);
    TEST_ASSERT_EQUAL_UINT32(0, g_fake.gpio_history[1].level);
}

/* ===== Runner ===== */

int main(void)
{
    UNITY_BEGIN();

    /* speed_to_delay_us */
    RUN_TEST(test_speed_zero_returns_zero);
    RUN_TEST(test_speed_one_returns_fire_max);
    RUN_TEST(test_speed_1000_returns_fire_min);
    RUN_TEST(test_speed_500_returns_midpoint);
    RUN_TEST(test_speed_monotonically_decreasing);
    RUN_TEST(test_speed_delay_always_in_valid_range);

    /* motor_set_speed */
    RUN_TEST(test_set_speed_normal);
    RUN_TEST(test_set_speed_zero);
    RUN_TEST(test_set_speed_clamps_above_1000);
    RUN_TEST(test_set_speed_boundary_1000);
    RUN_TEST(test_get_current_speed_matches);

    /* motor_set_direction */
    RUN_TEST(test_set_direction_forward);
    RUN_TEST(test_set_direction_reverse);
    RUN_TEST(test_set_direction_deasserts_both_gpios);

    /* motor_set_enable */
    RUN_TEST(test_enable_sets_flag);
    RUN_TEST(test_disable_clears_flag_and_speed);
    RUN_TEST(test_disable_deasserts_gpios);
    RUN_TEST(test_enable_does_not_change_speed);

    /* motor_emergency_stop */
    RUN_TEST(test_emergency_stop_resets_all_state);
    RUN_TEST(test_emergency_stop_deasserts_gpios);

    /* ISR logic */
    RUN_TEST(test_isr_does_nothing_when_disabled);
    RUN_TEST(test_isr_does_nothing_when_speed_zero);
    RUN_TEST(test_isr_starts_timer_when_enabled_and_speed_nonzero);

    /* fire_timer_cb */
    RUN_TEST(test_fire_cb_asserts_fwd_gpio_when_forward);
    RUN_TEST(test_fire_cb_asserts_rev_gpio_when_reverse);

    /* pulse_end_cb */
    RUN_TEST(test_pulse_end_deasserts_both_gpios);

    return UNITY_END();
}

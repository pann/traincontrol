# Traincontrol Test Infrastructure

## Overview

Two test levels support test-driven development:

| Level | Runs on | Framework | Purpose |
|-------|---------|-----------|---------|
| **Unit tests** | Host machine (native gcc) | Unity | Fast logic validation, no hardware needed |
| **System tests** | ESP32-S3-DevKitC | esp_console REPL + pytest-embedded | Hardware-in-the-loop with function generator and oscilloscope |

## Quick Start

All commands run from `sw/test/`:

```bash
# Unit tests (host, no hardware)
./run_unit_tests.sh

# System tests (requires devkit connected)
./build_system_test.sh
./flash_system_test.sh              # defaults to /dev/ttyACM0
./flash_system_test.sh /dev/ttyUSB0 # or specify port
```

## Unit Tests

### How it works

Unit tests compile production `.c` files with **native gcc** against fake ESP-IDF headers. This avoids the ESP-IDF toolchain entirely and gives sub-second build-and-run cycles.

Static functions are tested using the `#include "module.c"` pattern — the test file includes the production `.c` directly, gaining access to all static functions and variables.

### Directory layout

```
test/unit/
├── CMakeLists.txt           # Standalone CMake (pulls Unity via FetchContent)
├── fakes/
│   ├── driver/gpio.h        # Stub gpio types and functions
│   ├── esp_err.h            # ESP_OK, esp_err_t, ESP_ERROR_CHECK
│   ├── esp_log.h            # ESP_LOGI/W/E → no-ops
│   ├── esp_timer.h          # Timer types and stub functions
│   ├── nvs.h                # NVS fakes with in-memory backing
│   ├── nvs_flash.h          # nvs_flash_init stub
│   ├── fakes_state.h        # Call-tracking state struct
│   └── fakes_state.c        # Fake implementations + call tracking
├── test_motor_control.c     # 26 tests for motor_control module
└── test_config.c            # 11 tests for config module
```

### Writing a new unit test

#### 1. Create the test file

```c
/* test_my_module.c */

/* Fakes must be included before the production .c file so that
   #include directives in the .c resolve to fakes, not real ESP-IDF. */
#include "fakes_state.h"

/* Include the production .c to access statics */
#include "my_module.c"

#include "unity.h"

void setUp(void)
{
    fake_state_reset();  /* Clear call history between tests */
}

void tearDown(void) { }

void test_something(void)
{
    my_function(42);
    TEST_ASSERT_EQUAL(42, some_expected_value);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_something);
    return UNITY_END();
}
```

#### 2. Register in CMakeLists.txt

Add a new executable and test:

```cmake
add_executable(test_my_module
    test_my_module.c
    fakes/fakes_state.c
)
target_include_directories(test_my_module PRIVATE
    ${FAKES_DIR}
    ${PROD_DIR}
)
target_link_libraries(test_my_module PRIVATE unity)

add_test(NAME my_module COMMAND test_my_module)
```

#### 3. Add fakes if needed

If your module calls ESP-IDF functions not yet faked, add them to
the appropriate header in `fakes/` and implement them in `fakes_state.c`.

### Using the fake call tracker

The `g_fake` struct records GPIO calls, timer calls, and NVS operations
for assertions in tests:

```c
/* Verify gpio_set_level was called with specific pin and level */
TEST_ASSERT_EQUAL(GPIO_NUM_4, g_fake.gpio_history[0].pin);
TEST_ASSERT_EQUAL_UINT32(1, g_fake.gpio_history[0].level);
TEST_ASSERT_EQUAL(2, g_fake.gpio_history_count);

/* Verify esp_timer_start_once was called with expected delay */
TEST_ASSERT_EQUAL(1, g_fake.timer_start_once_calls);
TEST_ASSERT_EQUAL_UINT64(5000, g_fake.last_timer_timeout_us);

/* Pre-populate NVS before calling config_init() */
fake_nvs_preset("wifi_ssid", "MyNetwork");
fake_nvs_preset("wifi_pass", "secret");
```

### Common Unity assertions

```c
TEST_ASSERT_TRUE(condition);
TEST_ASSERT_FALSE(condition);
TEST_ASSERT_EQUAL(expected, actual);            /* int */
TEST_ASSERT_EQUAL_UINT16(expected, actual);
TEST_ASSERT_EQUAL_UINT32(expected, actual);
TEST_ASSERT_EQUAL_UINT64(expected, actual);
TEST_ASSERT_EQUAL_STRING("expected", actual);
TEST_ASSERT_TRUE_MESSAGE(condition, "message");
```

### Expected output

```
$ ./run_unit_tests.sh
...
test_motor_control.c:281:test_speed_zero_returns_zero:PASS
test_motor_control.c:282:test_speed_one_returns_fire_max:PASS
...
-----------------------
26 Tests 0 Failures 0 Ignored
OK
...
11 Tests 0 Failures 0 Ignored
OK

100% tests passed, 0 tests failed out of 2
```

## System Tests

### Hardware setup

```
 Function generator            ESP32-S3-DevKitC           Oscilloscope
 ┌──────────────┐             ┌──────────────┐          ┌──────────────┐
 │ 100 Hz, 3.3V │─── out ───►│ GPIO 6 (ZC)  │          │              │
 │ square wave   │            │              │          │              │
 └──────────────┘             │ GPIO 4 (FWD) ├─── CH1 ─►│              │
                              │ GPIO 5 (REV) ├─── CH2 ─►│              │
                              │              │          │              │
                   USB ◄──────┤ USB-JTAG     │          └──────────────┘
                              └──────────────┘
```

- **Function generator**: 100 Hz square wave, 3.3 V amplitude, simulates zero-crossing detector output
- **Oscilloscope**: trigger on function generator, observe gate pulse timing on CH1/CH2

### Serial REPL commands

Flash the test firmware, then interact via the serial monitor:

```
test> enable 1
OK enable=1

test> speed 500
OK speed=500

test> dir rev
OK dir=rev

test> status
enabled=1 dir=rev speed=500

test> estop
OK emergency_stop

test> status
enabled=0 dir=rev speed=0
```

### Manual oscilloscope test procedure

| Test | Commands | Expected on oscilloscope |
|------|----------|--------------------------|
| Minimum power | `enable 1`, `speed 1` | Gate pulse ~9.0 ms after ZC falling edge |
| Maximum power | `speed 1000` | Gate pulse ~1.0 ms after ZC falling edge |
| Mid power | `speed 500` | Gate pulse ~5.0 ms after ZC falling edge |
| Direction switch | `dir fwd`, then `dir rev` | Pulses move from CH1 to CH2 |
| Emergency stop | `estop` | All pulses cease immediately |
| Gate pulse width | Any speed | Pulse width ~50 us |
| No fire at speed 0 | `speed 0` | No pulses on either channel |

### Automated system tests (pytest-embedded)

After manual verification, automated tests check serial command responses:

```bash
# Install once
pip install pytest-embedded pytest-embedded-serial-esp pytest-embedded-idf

# Run
./run_system_tests.sh                      # default /dev/ttyACM0
./run_system_tests.sh /dev/ttyUSB0         # specific port
```

These tests verify state readback (speed, direction, enable) but do **not** verify oscilloscope timing — that remains manual.

## TDD Workflow

1. Write a failing unit test in `test/unit/test_<module>.c`
2. Run `./run_unit_tests.sh` — see it fail
3. Implement the feature in `main/<module>.c`
4. Run `./run_unit_tests.sh` — see it pass
5. When ready for hardware validation, flash system test firmware and verify with oscilloscope

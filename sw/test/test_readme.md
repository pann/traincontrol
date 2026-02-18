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
│   ├── esp_wifi.h           # WiFi types (mode, config, events)
│   ├── esp_event.h          # Event loop types, WIFI_EVENT/IP_EVENT
│   ├── esp_netif.h          # Network interface stubs
│   ├── esp_mac.h            # MAC address reading stub
│   ├── esp_http_server.h    # HTTP server types and stubs
│   ├── esp_console.h        # Console REPL types and stubs
│   ├── mdns.h               # mDNS stubs
│   ├── freertos/FreeRTOS.h  # pdMS_TO_TICKS macro
│   ├── freertos/task.h      # vTaskDelay, xTaskCreate stubs
│   ├── lwip/sockets.h       # Socket types and stubs for modbus_server
│   ├── esp_system.h         # esp_restart stub
│   ├── nvs.h                # NVS fakes with in-memory backing
│   ├── nvs_flash.h          # nvs_flash_init stub
│   ├── event_log.h          # Event log fakes
│   ├── fakes_state.h        # Call-tracking state struct
│   └── fakes_state.c        # Fake implementations + call tracking
├── test_motor_control.c     # 26 tests for motor_control module
├── test_config.c            # 11 tests for config module
├── test_wifi_manager.c      # 9 tests for wifi_manager module
├── test_provisioning.c      # 9 tests for provisioning module
└── test_modbus_server.c     # 38 tests for modbus_server module
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

/* Set a specific MAC address for hostname tests */
fake_set_mac(0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF);

/* Verify WiFi connect was called */
TEST_ASSERT_EQUAL(1, g_fake.wifi_connect_calls);

/* Verify mDNS was registered */
TEST_ASSERT_EQUAL(1, g_fake.mdns_init_calls);
TEST_ASSERT_EQUAL_STRING("trainctrl-eeff", g_fake.mdns_hostname);
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
26 Tests 0 Failures 0 Ignored   (motor_control)
11 Tests 0 Failures 0 Ignored   (config)
9 Tests 0 Failures 0 Ignored    (wifi_manager)
9 Tests 0 Failures 0 Ignored    (provisioning)
38 Tests 0 Failures 0 Ignored   (modbus_server)

100% tests passed, 0 tests failed out of 5
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

#### WiFi commands

```
test> wifi_status
wifi: disconnected

test> wifi_set MyNetwork MyPassword
Credentials saved. Connecting...

test> wifi_status
wifi: connected ip=192.168.1.42 ssid=MyNetwork

test> factory_reset
Factory reset: clearing credentials...
SoftAP started. Connect to the AP to reconfigure.
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

## Building from VS Code

VS Code tasks are configured in `.vscode/tasks.json`. Open the command palette
and select the task to run:

| How to run | What it does |
|------------|-------------|
| `Ctrl+Shift+B` → **Build production firmware** | Builds the main application in `sw/` |
| `Ctrl+Shift+B` → **Build system test firmware** | Builds the test firmware in `sw/test/system/` |
| `Ctrl+Shift+P` → Tasks: Run Task → **Flash system test firmware** | Flashes test firmware to devkit and opens serial monitor |
| `Ctrl+Shift+P` → Tasks: Run Task → **Run unit tests** | Runs host-side unit tests (no hardware needed) |

The ESP-IDF extension's built-in build button (`Ctrl+E B`) always builds the
**production firmware**. Use the tasks above to build/flash the system test
firmware instead.

To exit the serial monitor (flash task), press `Ctrl+]`.

## WiFi Provisioning

### Initial setup (production)

When the device boots for the first time (or after a factory reset), it has no
WiFi credentials stored. The provisioning module automatically starts:

1. **SoftAP captive portal** — the device creates an open WiFi access point
   named `TrainCtrl-XXXX` (where XXXX is derived from the device MAC address).
2. **USB serial CLI** — always available for credential entry.

#### Method 1: Captive portal (phone/laptop)

1. Flash the production firmware: `idf.py -p /dev/ttyACM0 flash`
2. On your phone or laptop, scan for WiFi networks
3. Connect to `TrainCtrl-XXXX` (open, no password)
4. A captive portal page should appear automatically. If it doesn't, open a browser and navigate to `http://192.168.4.1/`
5. Enter your home WiFi SSID and password, then tap **Connect**
6. The device saves the credentials to NVS, stops the SoftAP, and connects to your WiFi
7. The device's IP address is logged to serial output

#### Method 2: USB serial CLI

1. Connect the DevKitC via USB and open a serial monitor (`idf.py monitor` or `minicom -D /dev/ttyACM0`)
2. Type: `wifi_set <ssid> <password>`
3. The device saves the credentials and connects

### After provisioning

- Credentials persist across reboots (stored in NVS flash)
- The device auto-connects on each boot
- On disconnect, it retries up to 10 times with 5-second intervals
- mDNS service is registered as `trainctrl-XXXX._modbus._tcp` on port 502

### Factory reset

To clear stored credentials and restart provisioning:

- **USB CLI**: type `factory_reset`
- **Programmatically**: call `config_factory_reset()` followed by `provisioning_start_softap()`

### Verifying WiFi connection

- **USB CLI**: type `wifi_status` — shows connected/disconnected, IP, and SSID
- **mDNS**: from another device on the same network, `ping trainctrl-XXXX.local`
- **Serial monitor**: connection events are logged via `event_log`

## Modbus TCP Testing

### Unit tests (host-side)

The modbus_server unit tests exercise the PDU processing layer directly — all
38 tests call `process_pdu()` with crafted byte arrays and verify responses.
No sockets are involved; the lwip fakes are just stubs for compilation.

Tests cover all 5 function codes (FC01, FC03, FC04, FC05, FC06), exception
handling (illegal function, address, data value), register validation ranges,
and the public CLI helper API (`modbus_read_*`, `modbus_write_*`).

### On-device testing with mbpoll

After flashing the production or system test firmware and connecting to WiFi,
use [mbpoll](https://github.com/epsilonrt/mbpoll) to test Modbus TCP from a PC:

```bash
# Read coils (FC01)
mbpoll -m tcp -t0 -a1 -r1 -c2 trainctrl-XXXX.local -p502

# Read holding registers (FC03)
mbpoll -m tcp -t4:int -a1 -r1 -c4 trainctrl-XXXX.local -p502

# Read input registers (FC04)
mbpoll -m tcp -t3:int -a1 -r1 -c4 trainctrl-XXXX.local -p502

# Write single coil: enable motor (FC05)
mbpoll -m tcp -t0 -a1 -r1 trainctrl-XXXX.local -p502 1

# Write single register: set speed to 500 (FC06)
mbpoll -m tcp -t4:int -a1 -r2 trainctrl-XXXX.local -p502 500
```

### On-device testing with CLI

The serial CLI provides register access without needing a Modbus client:

```
test> mr all
Coils:
  [0] MOTOR_ENABLE     = 0
  [1] EMERGENCY_STOP   = 0
Holding registers:
  [0] DIRECTION        = 0
  [1] SPEED            = 0
  [2] ACCEL_RATE       = 100
  [3] DECEL_RATE       = 100
Input registers:
  [0] STATUS           = 0
  [1] RAIL_VOLTAGE     = 0
  [2] CURRENT_SPEED    = 0
  [3] UPTIME           = 42

test> mw holding 1 500
Holding [1] SPEED = 500

test> mw coil 0 1
Coil [0] MOTOR_ENABLE = 1

test> modbus_status
Client: not connected
Transactions: 0
```

## TDD Workflow

1. Write a failing unit test in `test/unit/test_<module>.c`
2. Run `./run_unit_tests.sh` — see it fail
3. Implement the feature in `main/<module>.c`
4. Run `./run_unit_tests.sh` — see it pass
5. When ready for hardware validation, flash system test firmware and verify with oscilloscope

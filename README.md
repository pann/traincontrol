# Traincontrol

A WiFi-enabled controller for classic Marklin AC model trains. The module fits inside a locomotive, is powered directly from the 12 VAC track voltage, and exposes motor control via Modbus TCP over WiFi.

## Features

- **Phase-angle speed control** — variable speed via TRIAC firing delay, supporting smooth acceleration and deceleration ramps
- **Bidirectional motor control** — forward/reverse via dual field windings with hardware XOR interlock preventing simultaneous activation
- **WiFi connectivity** — connects to your home/layout network as a standard WiFi station
- **Modbus TCP interface** — industry-standard protocol on port 502 for integration with any Modbus client
- **mDNS discovery** — automatically advertises as `trainctrl-XXXX._modbus._tcp` on the local network
- **Zero-crossing detection** — AC optocoupler provides precise timing reference for phase-angle control
- **USB serial console** — command-line interface over USB for configuration, diagnostics, and testing
- **SoftAP provisioning** — phone-friendly WiFi setup via captive portal on first boot

## Hardware

The traincontrol shield is a custom PCB designed in KiCad, built around an **ESP32-C3-MINI-1** module. Key subsystems:

| Subsystem | Components | Function |
|---|---|---|
| Power supply | DB107 bridge rectifier, MP2359 buck converter | 12 VAC rail to 3.3 VDC |
| Motor driver | 2x BTA204 TRIACs, 2x MOC3021 opto-TRIACs | AC phase-angle switching |
| Safety interlock | 74LVC1G86 XOR + 74LVC2G08 dual AND | Hardware anti-shoot-through |
| Zero-crossing | H11AA1 AC optocoupler | AC timing reference |
| MCU | ESP32-C3-MINI-1 | WiFi, USB, motor control logic |

Schematic and PCB files are in `hw/traincontrol-shield/`.

## Repository Structure

```
traincontrol/
├── docs/
│   ├── high-level-design.md      System architecture and design rationale
│   └── development-setup.md      IDE and toolchain setup guide
├── hw/
│   └── traincontrol-shield/      KiCad schematic and PCB project
└── sw/                           ESP-IDF firmware
    ├── main/                     Application source code
    │   ├── main.c                Entry point
    │   ├── motor_control.c/h     Phase-angle TRIAC control
    │   ├── modbus_server.c/h     Modbus TCP server
    │   ├── wifi_manager.c/h      WiFi station management
    │   ├── provisioning.c/h      SoftAP portal + USB CLI
    │   ├── config.c/h            NVS configuration storage
    │   └── status_led.c/h        LED pattern driver
    ├── components/
    │   └── console_commands/     Shared CLI command handlers
    └── test/
        ├── unit/                 Host-based unit tests
        └── system/               On-target system tests (pytest)
```

## Getting Started

### Prerequisites

- ESP-IDF v5.x ([installation guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/get-started/))
- ESP32-C3 devkit (for development) or traincontrol shield PCB
- USB-C cable

### Build and Flash

```bash
cd sw
idf.py set-target esp32c3
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

See `docs/development-setup.md` for VS Code integration and detailed setup instructions.

### WiFi Provisioning

On first boot, the device creates a WiFi access point named **TrainCtrl-XXXX** (where XXXX is derived from the MAC address). Connect to it with a phone or laptop and a captive portal will prompt for your WiFi network credentials.

Alternatively, connect via USB and use the serial console:

```
> wifi_set MyNetwork MyPassword
wifi: credentials saved, connecting...

> wifi_status
wifi: connected ip=192.168.1.42 ssid=MyNetwork
```

### Controlling a Train

Once connected to WiFi, the controller listens for Modbus TCP connections on port 502. Use any Modbus client — for example, with Python:

```python
from pymodbus.client import ModbusTcpClient

client = ModbusTcpClient("192.168.1.42")
client.connect()

# Enable motor
client.write_coil(0x0000, True)

# Set forward direction
client.write_register(0x0000, 0)

# Set speed to 50%
client.write_register(0x0001, 500)

# Read status
result = client.read_input_registers(0x0000, 4)
print(f"Status: {result.registers}")

client.close()
```

### Console Commands

The USB serial console provides the following commands:

| Command | Description |
|---|---|
| `speed <0-1000>` | Set motor speed (0.1% steps) |
| `dir <fwd\|rev>` | Set motor direction |
| `enable <0\|1>` | Enable/disable motor output |
| `estop` | Emergency stop |
| `status` | Show motor state |
| `wifi_set <ssid> <pass>` | Set WiFi credentials |
| `wifi_status` | Show WiFi connection status |
| `factory_reset` | Clear all settings and reboot |
| `mr <addr\|all>` | Read Modbus register(s) |
| `mw <addr> <value>` | Write Modbus register |
| `modbus_status` | Show Modbus server status |
| `reset` | Reboot the device |

### Modbus Register Map

**Coils** (read/write, single-bit):

| Address | Name | Description |
|---|---|---|
| 0x0000 | MOTOR_ENABLE | 1 = motor output enabled |
| 0x0001 | EMERGENCY_STOP | 1 = immediate stop |

**Holding Registers** (read/write, 16-bit):

| Address | Name | Range | Description |
|---|---|---|---|
| 0x0000 | DIRECTION | 0-1 | 0 = forward, 1 = reverse |
| 0x0001 | SPEED | 0-1000 | Speed in 0.1% steps |
| 0x0002 | ACCEL_RATE | 1-1000 | Acceleration ramp (ms) |
| 0x0003 | DECEL_RATE | 1-1000 | Deceleration ramp (ms) |

**Input Registers** (read-only, 16-bit):

| Address | Name | Description |
|---|---|---|
| 0x0000 | STATUS | Bit field: WiFi, enabled, direction, fault |
| 0x0001 | RAIL_VOLTAGE | Measured rail voltage (mV) |
| 0x0002 | CURRENT_SPEED | Actual speed during ramp |
| 0x0003 | UPTIME | Seconds since boot |

## Testing

### Unit Tests

```bash
cd sw
./test/run_unit_tests.sh
```

### System Tests

System tests run on-target using pytest. The test firmware exposes the same console interface as the production build.

```bash
cd sw
./test/build_system_test.sh
./test/flash_system_test.sh
./test/run_system_tests.sh
```

## License

MIT License. See [LICENSE](LICENSE) for details.

# Traincontrol User Guide

## What is Traincontrol?

Traincontrol is a WiFi-enabled controller for classic Marklin AC model trains.
It replaces the locomotive's existing electronics with a small module that
lets you control the motor speed and direction over your home WiFi network
using the Modbus TCP protocol.

Key features:

- **Wireless control** via any Modbus TCP client (phone, PC, or automation system)
- **Smooth speed control** with phase-angle motor driving and configurable acceleration/deceleration ramps
- **Automatic network discovery** via mDNS (no need to remember IP addresses)
- **Simple setup** with a captive portal or USB serial CLI
- **Hardware safety interlock** prevents simultaneous forward+reverse activation

## Getting Started

### 1. Install the module

Mount the Traincontrol PCB inside the locomotive. Connect the two field
windings and the motor armature to the designated terminals. The module is
powered directly from the 12 VAC rail voltage through the pickup shoe.

### 2. First boot and WiFi provisioning

When the module powers up for the first time, it has no WiFi credentials
stored. It will automatically start a provisioning mode so you can tell it
which WiFi network to connect to.

There are two ways to provide credentials:

#### Option A: Captive portal (recommended)

1. Power on the locomotive by placing it on the track
2. On your phone or laptop, open WiFi settings and look for a network named
   **TrainCtrl-XXXX** (where XXXX is unique to your module)
3. Connect to it (no password required)
4. A setup page should appear automatically. If it doesn't, open a browser
   and go to `http://192.168.4.1/`
5. Enter your home WiFi network name (SSID) and password, then tap **Connect**
6. The module saves the credentials, disconnects the access point, and joins
   your WiFi network

#### Option B: USB serial CLI

1. Connect the module to a computer via the USB-C port
2. Open a serial terminal (115200 baud). On Linux: `minicom -D /dev/ttyACM0`.
   With ESP-IDF installed: `idf.py monitor`
3. Type the following command:
   ```
   wifi_set YourNetworkName YourPassword
   ```
4. The module saves the credentials and connects

### 3. Find the module on your network

After successful provisioning, the module connects to your WiFi network and
registers itself via mDNS. You can find it by:

- **mDNS name**: `ping trainctrl-XXXX.local` (from any device on the same network)
- **Service discovery**: the module advertises `trainctrl-XXXX._modbus._tcp` on port 502
- **Serial output**: the assigned IP address is printed to the USB serial console

The module remembers your WiFi credentials across power cycles. It will
automatically reconnect each time the locomotive is powered on.

## Controlling the Train

### Modbus TCP connection

Connect to the module on **TCP port 502** using any Modbus TCP client. Recommended clients:

| Platform | Client |
|----------|--------|
| Windows | QModMaster (free) |
| Linux/Mac | `pymodbus` (Python library) |
| Mobile | Modbus Master (iOS/Android) |
| Automation | Node-RED, Home Assistant |

### Modbus register map

#### Coils (read/write, single-bit)

| Address | Name | Description |
|---------|------|-------------|
| 0x0000 | MOTOR_ENABLE | 1 = motor output enabled, 0 = disabled |
| 0x0001 | EMERGENCY_STOP | Write 1 = immediate stop (disables motor, sets speed to 0) |

#### Holding Registers (read/write, 16-bit)

| Address | Name | Range | Description |
|---------|------|-------|-------------|
| 0x0000 | DIRECTION | 0 or 1 | 0 = forward, 1 = reverse |
| 0x0001 | SPEED | 0 - 1000 | Motor speed (0 = stop, 1000 = full power) |
| 0x0002 | ACCEL_RATE | 1 - 1000 | Time to accelerate from 0 to full speed (ms) |
| 0x0003 | DECEL_RATE | 1 - 1000 | Time to decelerate from full speed to 0 (ms) |

#### Input Registers (read-only, 16-bit)

| Address | Name | Description |
|---------|------|-------------|
| 0x0000 | STATUS | Status bit field (see below) |
| 0x0001 | RAIL_VOLTAGE | Measured rail voltage in millivolts |
| 0x0002 | CURRENT_SPEED | Actual speed during acceleration/deceleration ramp |
| 0x0003 | UPTIME | Seconds since boot |

**STATUS bit field:**

| Bit | Meaning |
|-----|---------|
| 0 | WiFi connected |
| 1 | Motor enabled |
| 2 | Direction (0 = forward, 1 = reverse) |
| 3 | Fault (over-voltage or under-voltage) |

### Quick start example (Python)

```python
from pymodbus.client import ModbusTcpClient

# Connect to the train controller
client = ModbusTcpClient('trainctrl-eeff.local', port=502)
client.connect()

# Enable motor and set forward direction
client.write_coil(0x0000, True)         # MOTOR_ENABLE = on
client.write_register(0x0000, 0)        # DIRECTION = forward

# Set speed to 50%
client.write_register(0x0001, 500)      # SPEED = 500/1000

# Read current status
result = client.read_input_registers(0x0000, 4)
print(f"Status: {result.registers[0]:#06x}")
print(f"Rail voltage: {result.registers[1]} mV")
print(f"Current speed: {result.registers[2]}")
print(f"Uptime: {result.registers[3]} s")

# Emergency stop
client.write_coil(0x0001, True)         # EMERGENCY_STOP

client.close()
```

### Driving tips

- **Start slow**: increase speed gradually (0 -> 200 -> 400 -> ...) rather than
  jumping to full speed, especially with older Marklin motors
- **Use ramp rates**: set ACCEL_RATE and DECEL_RATE to smooth out speed changes
  and prevent derailments on curves
- **Direction changes**: always reduce speed to 0 before changing direction
- **Emergency stop**: writing to the EMERGENCY_STOP coil immediately disables
  the motor output regardless of current speed

## USB Serial Commands

The USB-C port provides a command-line interface that is always available,
regardless of WiFi status. Connect a serial terminal at 115200 baud.

| Command | Description |
|---------|-------------|
| `wifi_status` | Show WiFi connection state, IP address, and SSID |
| `wifi_set <ssid> <password>` | Store new WiFi credentials and connect |
| `factory_reset` | Clear all stored settings and restart provisioning |
| `help` | List all available commands |

## Troubleshooting

### Module doesn't create the TrainCtrl-XXXX access point

- Verify the module is receiving power (place locomotive on powered track)
- If WiFi credentials are already stored from a previous setup, the module
  connects directly to your network instead of starting the access point.
  Use `factory_reset` via USB serial to clear stored credentials

### Cannot connect to the module after provisioning

- Verify the module is on the same network as your client device
- Try `ping trainctrl-XXXX.local`. If mDNS doesn't work on your network,
  check the serial output for the assigned IP address
- The module retries the WiFi connection up to 10 times (5 seconds apart)
  after a disconnect. Check the serial monitor for connection status messages

### Motor doesn't respond

- Verify MOTOR_ENABLE coil (0x0000) is set to 1
- Check that SPEED is greater than 0
- Read the STATUS input register — bit 3 indicates a fault condition
- Verify the zero-crossing detector is working (the motor requires the AC
  waveform reference signal to operate)

### Module keeps disconnecting from WiFi

- Check signal strength — the module is inside a metal locomotive body which
  attenuates WiFi signals. Consider positioning the layout closer to a WiFi
  access point
- After 10 failed reconnection attempts, the module stops retrying. Power-cycle
  the locomotive to restart the connection process

### How to change WiFi network

Use either method:

- **USB serial**: `wifi_set NewNetwork NewPassword`
- **Factory reset + captive portal**: `factory_reset` via USB serial, then
  connect to the TrainCtrl-XXXX access point and enter new credentials

## Safety

- The hardware XOR interlock prevents the forward and reverse motor windings
  from being energised simultaneously, regardless of software state
- If no Modbus command is received within the watchdog timeout (default 5
  seconds), the motor is automatically stopped
- The module monitors rail voltage and disables outputs if an over-voltage
  or under-voltage condition is detected
- Emergency stop (coil 0x0001) takes effect immediately without deceleration

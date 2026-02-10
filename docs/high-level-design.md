# Traincontrol - High Level Design

## 1. Overview

A WiFi-enabled controller for classic Marklin AC model trains. The module
sits inside (or replaces) the locomotive's existing electronics, is powered
directly from the 12 VAC rail voltage, and exposes motor control via
Modbus TCP over WiFi.

```
 ┌──────────────────────────────────────────────────────────┐
 │  Marklin Track (12 VAC, 50 Hz)                           │
 └──────────┬──────────────────────────────────┬────────────┘
            │                                  │
  ┌─────────▼──────────┐             12 VAC to motor
  │  Power Supply       │                      │
  │  12 VAC → 3.3 VDC   │          ┌──────────▼──────────┐
  └─────────┬──────────┘          │   Motor Armature     │
            │ 3.3 V               └──┬───────────────┬──┘
  ┌─────────▼──────────┐            │               │
  │  ESP32-C3 / S3      │◄──┐  ┌────▼────┐    ┌────▼────┐
  │  Microcontroller    │   │  │Winding A│    │Winding B│
  └──┬──────┬──────┬───┘   │  │ (FWD)   │    │ (REV)   │
     │      │      │       │  └────┬────┘    └────┬────┘
     │GPIO  │GPIO  │ZC_IN  │       │              │
     │_FWD  │_REV  │       │  ┌────▼────┐    ┌────▼────┐
  ┌──▼──────▼──────▼──┐    │  │ TRIAC 1 │    │ TRIAC 2 │
  │  HW XOR Interlock  │   │  └────┬────┘    └────┬────┘
  │  + Opto-TRIAC gate │   │       │              │
  │  drivers           ├───┘       ├──────────────┤
  └────────────────────┘           │              │
           ▲                      GND            GND
           │                   (wheels)        (wheels)
     Zero-crossing
     detector
```

## 2. Hardware Design

### 2.1 Power Supply

The module is powered entirely from the 12 VAC rail pickup.

- **Bridge rectifier** (e.g. DB107) converts 12 VAC to pulsating DC
  (~15.5 V peak after diode drops).
- **Bulk capacitor** (220-470 uF / 25 V) smooths the rectified voltage.
- **Switching regulator** (e.g. AP2112K-3.3 LDO, or MP2359 buck converter)
  steps down to 3.3 VDC for the microcontroller. A buck converter is
  preferred due to the large voltage drop (15 V → 3.3 V) and better thermal
  performance in the confined locomotive body.
- **Decoupling capacitors** (100 nF + 10 uF) at the MCU supply pins.

The MCU supply is the only load on the rectified/regulated path. The motor
is powered directly from the 12 VAC rail through TRIACs (see 2.3).

### 2.2 Microcontroller

**ESP32-C3-MINI-1** (or ESP32-S3-MINI-1 if more GPIO is needed).

Rationale:
- Built-in WiFi 802.11 b/g/n.
- Native USB CDC/ACM (no external FTDI/CP2102 needed).
- Very small module footprint suitable for fitting inside a locomotive.
- Excellent ESP-IDF and Arduino framework support.
- Low cost (~$1.50 in small qty).
- Ultra-low-power modes for idle periods.

Key connections:
| Function       | GPIO (C3)  | Notes                          |
|----------------|------------|--------------------------------|
| Motor FWD      | GPIO 4     | Output to XOR interlock        |
| Motor REV      | GPIO 5     | Output to XOR interlock        |
| Zero-crossing  | GPIO 6     | Input, interrupt on edge       |
| USB D-         | GPIO 18    | Native USB for provisioning    |
| USB D+         | GPIO 19    | Native USB for provisioning    |
| Status LED     | GPIO 8     | On-board or external           |
| Rail voltage   | GPIO 2     | ADC - monitor supply voltage   |

### 2.3 Motor Driver with Hardware XOR Interlock

The classic Marklin AC motor is powered directly from the 12 VAC rail.
The current path through the locomotive is:

```
  Rail (12 VAC) ──► Pickup shoe ──► Motor armature ──┬──► Winding A (fwd) ──► Wheels (GND)
                                                     └──► Winding B (rev) ──► Wheels (GND)
```

The motor has two field windings — energising one drives forward, the other
drives reverse. **Both must never be active simultaneously** as this would
create a short through the field coils and potentially damage them.

Speed is controlled by **phase-angle control**: delaying the TRIAC firing
point within each AC half-cycle reduces the effective voltage delivered to
the motor.

#### 2.3.1 Driver Stage — TRIACs

Two TRIACs (e.g. BTA204-600E, 4A / 600V in SOT-223 — significantly
overrated for the ~0.5 A motor load, providing good margin) act as AC
switches, one per winding path. Each TRIAC is triggered through an
opto-TRIAC coupler (MOC3021) which provides galvanic isolation between
the 3.3 V logic side and the AC motor side.

```
  12 VAC rail
      │
  ┌───▼───┐
  │ Motor  │
  │Armature│
  └───┬───┘
      │
      ├────────────────────────────┐
      │                            │
 ┌────▼────┐                 ┌────▼────┐
 │Winding A│                 │Winding B│
 │ (FWD)   │                 │ (REV)   │
 └────┬────┘                 └────┬────┘
      │                            │
  MT1┌┴┐MT2                   MT1┌┴┐MT2
     │T1│ ◄── BTA204             │T2│ ◄── BTA204
     └┬─┘                        └┬─┘
      │                            │
   ┌──┴──┐                      ┌──┴──┐
   │Snub │ RC snubber            │Snub │ RC snubber
   └──┬──┘                      └──┬──┘
      │                            │
      ├────────────────────────────┤
      │                            │
     GND (wheels)                GND (wheels)
```

Each TRIAC gate is driven by a MOC3021 opto-TRIAC:

```
             3.3V logic side   │  AC motor side
                               │
  GATE_FWD ──►┌─────────┐     │    ┌─────────┐
              │ R 330Ω  ├──►──┤──►─┤ MOC3021 ├──► T1 gate
              └─────────┘     │    │(opto)   │
                               │    └────┬────┘
                               │         │ 360Ω gate resistor
                               │         ▼
                               │      T1 MT1
```

The MOC3021 is a non-zero-crossing type, which is required for
phase-angle control (a zero-crossing type like MOC3041 would only
allow on/off control, not variable phase angle).

An RC snubber network (100 Ω + 100 nF) across each TRIAC suppresses
voltage transients from the inductive motor load and prevents false
triggering.

#### 2.3.2 Hardware XOR Interlock

A discrete logic interlock ensures that T1 and T2 can never conduct at
the same time, regardless of software state. This is implemented using a
single 74LVC1G86 (single XOR gate) and a 74LVC2G08 (dual AND gate):

```
  GPIO_FWD ──┬──────────────────────────┐
             │                          │
             ├──►┌───────────┐          │
             │   │ 74LVC1G86 │          │
  GPIO_REV ──┼──►│   XOR     ├──► VALID │
             │   └───────────┘          │
             │                          │
             │   ┌───────────┐          │
             ├──►│ 74LVC2G08 │          │
             │   │  AND #1   ├──► GATE_FWD (to MOC3021 #1)
     VALID ──┼──►│           │          │
             │   ├───────────┤          │
             │   │  AND #2   ├──► GATE_REV (to MOC3021 #2)
     VALID ──┴──►│           │
                 └───────────┘
```

Logic truth table:

| GPIO_FWD | GPIO_REV | VALID (XOR) | GATE_FWD | GATE_REV | State       |
|----------|----------|-------------|----------|----------|-------------|
| 0        | 0        | 0           | 0        | 0        | Coast       |
| 0        | 1        | 1           | 0        | 1        | Reverse     |
| 1        | 0        | 1           | 1        | 0        | Forward     |
| 1        | 1        | 0           | 0        | 0        | **Blocked** |

When software erroneously asserts both GPIOs, the XOR output goes low,
forcing both AND gates low — neither opto-TRIAC is driven, so neither
TRIAC fires. This is a fail-safe hardware protection independent of
firmware.

Note: The GATE_FWD / GATE_REV signals from the AND gates drive the
opto-TRIAC LED inputs. The MCU pulses these signals at the correct
phase angle to achieve speed control (see 2.3.4).

#### 2.3.3 Zero-Crossing Detection

To perform phase-angle control, the MCU must know when the AC waveform
crosses zero. A zero-crossing detector feeds a logic-level pulse to a
GPIO interrupt input.

```
  12 VAC ──┬──►┌────────┐
           │   │ 220 kΩ │
           │   └────┬───┘          3.3V
           │        │               │
           │   ┌────▼───┐      ┌───┴───┐
           │   │H11AA1  │      │ 10 kΩ │ pull-up
           │   │(AC opto│      └───┬───┘
           │   │coupler)├──────────┼──────► ZC_OUT to GPIO 6
           │   └────┬───┘          │
           │        │              │
  GND ─────┴──►┌────┘         (open collector output,
               │                pulses low at each
              GND               zero crossing)
```

The H11AA1 contains two anti-parallel LEDs, producing an output pulse
on every zero crossing (both positive-to-negative and negative-to-
positive). The 220 kΩ resistor limits LED current to ~55 uA peak,
which is sufficient for the H11AA1 and provides safe isolation.

At 50 Hz, zero crossings occur every 10 ms (100 Hz rate). The MCU
timestamps each crossing via a GPIO interrupt.

#### 2.3.4 Phase-Angle Speed Control

Speed is controlled by varying the firing delay after each zero crossing.
The MCU uses a hardware timer started at each zero-crossing interrupt:

```
  AC voltage
   ┊         ╱╲              ╱╲
   ┊        ╱  ╲            ╱  ╲
   ┊       ╱    ╲          ╱    ╲
  ─┼──────╱──────╲────────╱──────╲────── t
   ┊     ╱  ▲    ╲╱     ╱  ▲    ╲╱
   ┊    ╱   │              │
   ┊   ╱    │              │
   ┊  ╱     │              │
   ┊        │              │
   ZC    fire delay     fire delay
         (1-9 ms)       (1-9 ms)
```

- **Delay ~ 1 ms** after ZC → TRIAC fires early → nearly full power.
- **Delay ~ 9 ms** after ZC → TRIAC fires late → minimal power.
- **No fire pulse** → TRIAC stays off → motor coast / stop.

The TRIAC latches on once triggered and turns off automatically when
the AC current crosses zero at the end of the half-cycle. The process
repeats every half-cycle (every 10 ms at 50 Hz).

The gate pulse duration only needs to be ~10-50 us — long enough to
reliably trigger the TRIAC but short enough that the MCU is not
holding the opto-LED on continuously.

### 2.4 Optional: Rail Voltage Sensing

A resistor divider (e.g. 100k / 33k) feeds the rectified rail voltage
to an ADC input, allowing the firmware to monitor supply voltage and
detect loss-of-track-contact.

### 2.5 PCB Form Factor

Target: Single-sided PCB, approximately 30 x 15 mm, to fit inside a
Marklin H0 locomotive body. USB-C connector at one edge for provisioning.

## 3. Software Design

### 3.1 Platform and Framework

- **ESP-IDF** (Espressif IoT Development Framework), v5.x.
- Language: **C** (ESP-IDF native).
- Build system: **CMake / idf.py**.

Rationale: ESP-IDF gives full control over WiFi, TCP stack, and
hardware timers needed for phase-angle control. Arduino would add
unnecessary abstraction for this embedded application.

### 3.2 Firmware Architecture

```
  ┌─────────────────────────────────────────────────┐
  │                Application Layer                │
  │  ┌────────────┐  ┌────────────┐  ┌───────────┐ │
  │  │  Modbus    │  │  Motor     │  │  Config   │ │
  │  │  TCP Server│  │  Control   │  │  Manager  │ │
  │  └─────┬──────┘  └─────┬──────┘  └─────┬─────┘ │
  │        │               │               │       │
  │  ┌─────▼───────────────▼───────────────▼─────┐ │
  │  │            Hardware Abstraction            │ │
  │  │  GPIO / Timer / ADC / NVS / WiFi / USB    │ │
  │  └───────────────────────────────────────────┘ │
  │                                                 │
  │                ESP-IDF / FreeRTOS               │
  └─────────────────────────────────────────────────┘
```

#### 3.2.1 Main Components

| Component         | Responsibility                                  |
|-------------------|-------------------------------------------------|
| **Motor Control** | Handles zero-crossing ISR, phase-angle timer,   |
|                   | and direction GPIOs. Enforces software-level     |
|                   | safety (in addition to HW XOR). Provides         |
|                   | ramp-up/ramp-down for smooth acceleration.       |
| **Modbus TCP**    | Listens on TCP port 502. Maps Modbus registers   |
|                   | to motor control and status. Uses the            |
|                   | `esp-modbus` component (included in ESP-IDF).   |
| **Config Manager**| Stores WiFi credentials and device parameters in |
|                   | NVS (non-volatile storage). Handles first-boot   |
|                   | provisioning flow.                               |
| **Status LED**    | Visual feedback: blinking = no WiFi, solid = OK, |
|                   | fast blink = error / fault.                      |

#### 3.2.2 Task Structure (FreeRTOS)

| Task              | Priority | Core | Notes                        |
|-------------------|----------|------|------------------------------|
| modbus_tcp_task   | 5        | 0    | Modbus TCP server loop       |
| motor_ctrl_task   | 10       | 1    | Phase-angle / ramp control   |
| wifi_task         | 7        | 0    | WiFi connection management   |
| status_led_task   | 2        | 0    | LED pattern driver           |

### 3.3 Modbus Register Map

All registers are 16-bit as per Modbus convention.

#### Coils (read/write, single-bit)

| Address | Name           | Description                        |
|---------|----------------|------------------------------------|
| 0x0000  | MOTOR_ENABLE   | 1 = motor output enabled           |
| 0x0001  | EMERGENCY_STOP | 1 = immediate stop (clears enable) |

#### Holding Registers (read/write, 16-bit)

| Address | Name           | Range     | Description                   |
|---------|----------------|-----------|-------------------------------|
| 0x0000  | DIRECTION      | 0 / 1     | 0 = forward, 1 = reverse     |
| 0x0001  | SPEED          | 0 - 1000  | Speed in 0.1% steps (0-100%) |
| 0x0002  | ACCEL_RATE     | 1 - 1000  | Acceleration ramp (ms to full)|
| 0x0003  | DECEL_RATE     | 1 - 1000  | Deceleration ramp (ms to 0)  |

#### Input Registers (read-only, 16-bit)

| Address | Name           | Description                        |
|---------|----------------|------------------------------------|
| 0x0000  | STATUS         | Bit field (see below)              |
| 0x0001  | RAIL_VOLTAGE   | Measured rail voltage in mV        |
| 0x0002  | CURRENT_SPEED  | Actual current speed (during ramp) |
| 0x0003  | UPTIME         | Seconds since boot                 |

STATUS bit field:
- Bit 0: WiFi connected
- Bit 1: Motor enabled
- Bit 2: Direction (0=fwd, 1=rev)
- Bit 3: Fault (over-voltage / under-voltage)

### 3.4 WiFi Provisioning

**Primary method: SoftAP + Captive Portal** (recommended over USB ACM).

On first boot (no stored credentials), the ESP32 creates a WiFi access
point named `TrainCtrl-XXXX` (where XXXX = last 4 hex digits of MAC).
When a user connects, a captive portal presents a simple web page to
enter the SSID and password of the home/layout WiFi network. Credentials
are stored in NVS.

Advantages over USB ACM:
- No cable needed — works with a phone.
- No drivers or terminal software required.
- Standard pattern, well-supported by ESP-IDF (`wifi_provisioning`
  component).

**Fallback: USB CDC/ACM serial console.**

If the SoftAP method fails or the user prefers a wired approach, the
USB port exposes a CDC/ACM serial interface with a simple command-line
interface:

```
> wifi_set SSID password
OK, credentials stored. Rebooting...

> wifi_status
Connected to "LayoutNet", IP 192.168.1.42, RSSI -53 dBm

> factory_reset
OK, clearing NVS. Rebooting...
```

This also serves as a debug/diagnostic interface.

### 3.5 mDNS / Discovery

The module registers itself via mDNS as `trainctrl-XXXX._modbus._tcp`
so that a control application can automatically discover all train
controllers on the network without needing to know IP addresses.

## 4. Project Structure

```
traincontrol/
├── docs/
│   ├── high-level-design.md      ← this document
│   └── modbus-register-map.md    ← detailed register documentation
├── hw/
│   ├── kicad/                    ← KiCad project files
│   │   ├── traincontrol.kicad_pro
│   │   ├── traincontrol.kicad_sch
│   │   └── traincontrol.kicad_pcb
│   └── bom/                     ← bill of materials
├── sw/
│   ├── traincontrol.code-workspace  ← VS Code workspace
│   ├── CMakeLists.txt
│   ├── sdkconfig.defaults
│   ├── main/
│   │   ├── CMakeLists.txt
│   │   ├── main.c               ← entry point, task init
│   │   ├── motor_control.c/.h   ← phase-angle control, ramp logic
│   │   ├── modbus_server.c/.h   ← Modbus TCP register handling
│   │   ├── wifi_manager.c/.h    ← WiFi connect, reconnect
│   │   ├── provisioning.c/.h    ← SoftAP portal + USB CLI
│   │   ├── config.c/.h          ← NVS read/write
│   │   └── status_led.c/.h      ← LED pattern driver
│   └── components/              ← any local ESP-IDF components
└── .gitignore
```

## 5. Bill of Materials (Key Components)

| Component              | Part Number       | Qty | Notes                  |
|------------------------|-------------------|-----|------------------------|
| MCU Module             | ESP32-C3-MINI-1   | 1   | WiFi + USB CDC/ACM     |
| Bridge Rectifier       | DB107             | 1   | 1A, 1000V (MCU PSU)   |
| Buck Regulator         | MP2359DJ          | 1   | 4.5-24V in, 3.3V out  |
| TRIAC                  | BTA204-600E       | 2   | AC motor switch, SOT223|
| Opto-TRIAC             | MOC3021           | 2   | Non-zero-cross, DIP-6  |
| AC Optocoupler         | H11AA1            | 1   | Zero-crossing detect   |
| XOR Gate               | 74LVC1G86         | 1   | HW interlock           |
| Dual AND Gate          | 74LVC2G08         | 1   | HW interlock           |
| Inductor (buck)        | 4.7 uH            | 1   | For buck regulator     |
| Bulk cap               | 220 uF / 25 V     | 1   | Input filter           |
| USB-C connector        | USB4110-GF-A      | 1   | Provisioning + debug   |
| Resistors, caps        | various           | ~20 | Passives + RC snubbers |

## 6. Development Phases

1. **Phase 1 — Prototype on devkit**: Use an ESP32-C3 devkit on a
   breadboard with TRIACs, opto-couplers, and the XOR interlock circuit.
   Validate phase-angle motor control and Modbus TCP.

2. **Phase 2 — Schematic & PCB**: Design the full schematic in KiCad,
   lay out the PCB to fit inside a Marklin locomotive.

3. **Phase 3 — Firmware complete**: Implement provisioning, mDNS,
   and robust error handling. Add OTA firmware update capability.

4. **Phase 4 — Integration test**: Install in a locomotive, test on
   track with a Modbus client (e.g. QModMaster or a custom Python
   script using `pymodbus`).

## 7. Safety Considerations

- **Hardware XOR interlock** prevents simultaneous winding activation
  independent of software state.
- **Software watchdog**: If no Modbus command is received within a
  configurable timeout (default 5 s), the motor is automatically
  stopped (fail-safe).
- **Ramp control** prevents abrupt speed changes that could derail
  the train or damage gears.
- **Voltage monitoring** detects loss of rail contact and disables
  outputs cleanly.
- **Galvanic separation**: The USB port should include ESD protection
  (e.g. USBLC6-2SC6) to protect both the module and the connected
  computer.

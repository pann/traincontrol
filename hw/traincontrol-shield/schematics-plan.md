# Traincontrol Shield — Schematics Implementation Plan

## 1. Design Rules (from PLAN.md)

1. KiCad standard libraries for symbols/footprints wherever available.
2. easyeda2kicad skill for missing components → downloaded into `libs/`.
3. 0603 passives throughout (resistors, capacitors, inductors).
4. 50V max rating — no component needs to be rated higher.
5. Hierarchical schematic — one sub-sheet per logical block.
6. Short wire stubs only — use net labels for all inter-component connections.
7. **All component symbols must exist in libraries before any schematic file is created.**

---

## 2. Ground Architecture

The DB107 bridge rectifier has **four distinct roles** for its pins:

| Pin | Net | Description |
|-----|-----|-------------|
| AC1 | `AC_IN` | 12 VAC hot (from track pickup shoe) |
| AC2 | `AC_RTN` | 12 VAC return (through motor → wheels) |
| +DC | `+15VDC` | Rectified unregulated DC |
| −DC | `GND` | Logic/PSU ground (DC negative rail) |

`AC_RTN` and `GND` are **different nets** — separated by the bridge rectifier diodes.

Isolation boundaries (optocouplers cross the AC/logic divide):

| Device | Logic side (GND, +3V3) | AC side (AC_IN, AC_RTN) |
|--------|------------------------|--------------------------|
| U2 H11AA1 | Transistor output → ZC_OUT | LED input: AC_IN via R1, return: AC_RTN |
| U5 MOC3021 | LED: GATE_FWD/GND | TRIAC: gate resistor → Q1; ref: AC_RTN |
| U6 MOC3021 | LED: GATE_REV/GND | TRIAC: gate resistor → Q2; ref: AC_RTN |

---

## 3. Net Naming Convention

| Net | Description |
|-----|-------------|
| `AC_IN` | 12 VAC hot — feeds D1 bridge + motor armature |
| `AC_RTN` | 12 VAC return — feeds D1 bridge + TRIAC MT1s |
| `+15VDC` | Rectified DC from D1 +DC output |
| `GND` | Logic/DC ground (D1 −DC output) |
| `+3V3` | Regulated 3.3 V from MP2359DJ |
| `SW_NODE` | MP2359DJ switching node (U1 SW pin / L1 input) |
| `ZC_OUT` | Zero-crossing pulse from H11AA1, active-low |
| `GPIO_FWD` | ESP32 GPIO 4 — motor forward direction output |
| `GPIO_REV` | ESP32 GPIO 5 — motor reverse direction output |
| `VALID` | XOR output — high when exactly one direction asserted |
| `GATE_FWD` | AND gate #1 output → U5 LED anode |
| `GATE_REV` | AND gate #2 output → U6 LED anode |
| `WINDING_A` | Forward motor winding → Q1 MT2 |
| `WINDING_B` | Reverse motor winding → Q2 MT2 |
| `RAIL_SENSE` | ADC input from +15VDC resistor divider |
| `LED_STATUS` | ESP32 GPIO 8 — status LED drive |

---

## 4. KiCad Library Status

### Standard KiCad libraries (`/usr/share/kicad/symbols/`) — confirmed present

| Symbol | Library | Status |
|--------|---------|--------|
| `Isolator:H11AA1` | Isolator.kicad_sym | ✓ Found |
| `74xGxx:74LVC1G86` | 74xGxx.kicad_sym | ✓ Found |
| `74xGxx:74LVC2G08` | 74xGxx.kicad_sym | ✓ Found |
| `Relay_SolidState:MOC3021M` | Relay_SolidState.kicad_sym | ✓ Found (M = DIP-6 package) |
| `Device:R`, `Device:C`, `Device:L`, `Device:CP`, `Device:LED` | Device.kicad_sym | ✓ Found |
| `Connector_Generic:Conn_01x02/03/22` | Connector_Generic.kicad_sym | ✓ Found |

### Local library (`libs/traincontrol-shield.kicad_sym`) — already downloaded

| Symbol | Status |
|--------|--------|
| `traincontrol-shield:MP2359DJ-LF-Z` | ✓ Present |

### Local library (`libs/traincontrol-shield.kicad_sym`) — downloaded via easyeda2kicad

| Symbol | LCSC | Footprint | Status |
|--------|------|-----------|--------|
| `traincontrol-shield:DB107` | C2492 | `DIO-BG-TH_DF` (DIP-4 THT) | ✓ Downloaded |
| `traincontrol-shield:BT134W-600D` | C253549 | `SOT-223-3_L6.5-W3.4-P2.30-LS7.0-BR` | ✓ Downloaded |

*Note: Original design specified BTA204-600E (4A TRIAC). That part is TO-220AB through-hole and not available on LCSC. Replaced with BT134W-600D (1A, SOT-223, WeEn), which provides adequate 2× margin over the ~0.5A motor load and fits the SMD-first design.*

---

## 5. Files to Create

```
hw/traincontrol-shield/
├── traincontrol-shield.kicad_pro        # KiCad project (registers local lib)
├── traincontrol-shield.kicad_sch        # Top-level sheet
├── power-supply.kicad_sch               # Sheet 1
├── zero-crossing.kicad_sch              # Sheet 2
├── xor-interlock.kicad_sch              # Sheet 3
├── opto-triac-drive.kicad_sch           # Sheet 4
├── triacs-snubbers.kicad_sch            # Sheet 5
├── rail-voltage-sense.kicad_sch         # Sheet 6
└── mcu.kicad_sch                        # Sheet 7
```

---

## 6. Top-Level Sheet

The top-level sheet (`traincontrol-shield.kicad_sch`) contains:
- Seven hierarchical sheet instances (one per sub-sheet file)
- Top-level external connectors
- Global power net symbols (+15VDC, +3V3, GND, AC_IN, AC_RTN)
- Hierarchical labels routing inter-sheet signals

### Top-Level Connectors

| Ref | Symbol | Value | Footprint | Nets |
|-----|--------|-------|-----------|------|
| J4 | `Connector_Generic:Conn_01x02` | Track Input | `TerminalBlock_Phoenix:TerminalBlock_Phoenix_MKDS-1,5-2-5.08_1x02_P5.08mm_Horizontal` | AC_IN, AC_RTN |
| J5 | `Connector_Generic:Conn_01x02` | Motor Windings | `TerminalBlock_Phoenix:TerminalBlock_Phoenix_MKDS-1,5-2-5.08_1x02_P5.08mm_Horizontal` | WINDING_A, WINDING_B |
| J6 | `Connector_Generic:Conn_01x03` | Motor + AC | `TerminalBlock_Phoenix:TerminalBlock_Phoenix_MKDS-1,5-3-5.08_1x03_P5.08mm_Horizontal` | AC_IN, WINDING_A, WINDING_B |

*Note: J6 allows a single 3-pin connector for wiring the motor and pickup to the PCB. J4 and J5 are kept for configurations where these are separate.*

---

## 7. Sub-Sheet Details

### Sheet 1: Power Supply (`power-supply.kicad_sch`)

**Purpose:** Convert 12 VAC rail to regulated +3.3 V for the MCU.

#### Components

| Ref | Symbol | Value | Footprint | Note |
|-----|--------|-------|-----------|------|
| D1 | `traincontrol-shield:DB107` | DB107 | *(downloaded)* | Bridge rectifier |
| C1 | `Device:CP` | 220 µF / 25 V | `Capacitor_THT:CP_Radial_D8.0mm_P3.50mm` | Bulk capacitor |
| U1 | `traincontrol-shield:MP2359DJ-LF-Z` | MP2359DJ | `traincontrol-shield:SOT-23-6_L2.9-W1.6-P0.95-LS2.8-BR` | Buck converter |
| L1 | `Device:L` | 4.7 µH | `Inductor_SMD:L_0805_2012Metric` | Buck inductor |
| C2 | `Device:C` | 22 µF | `Capacitor_SMD:C_0603_1608Metric` | Buck output cap |
| C3 | `Device:C` | 100 nF | `Capacitor_SMD:C_0603_1608Metric` | U1 IN decoupling |
| C9 | `Device:C` | 100 nF | `Capacitor_SMD:C_0603_1608Metric` | U1 BST bootstrap |
| R12 | `Device:R` | 150 kΩ | `Resistor_SMD:R_0603_1608Metric` | FB top (sets 3.3 V) |
| R13 | `Device:R` | 47 kΩ | `Resistor_SMD:R_0603_1608Metric` | FB bottom |

*VOUT = 0.792 V × (1 + 150k/47k) = 3.31 V*

#### Connections

```
AC_IN  ──stub──► D1 AC1
AC_RTN ──stub──► D1 AC2
D1 +DC ──stub──► +15VDC ──stub──► C1(+) ──stub──► GND
                 +15VDC ──stub──► U1 IN
                 +15VDC ──stub──► U1 EN  (always enabled)
                 +15VDC ──stub──► C3(+)  ──stub──► GND

D1 -DC ──stub──► GND

U1 SW  ──stub──► SW_NODE ──stub──► L1 pin1
U1 BST ──stub──► BST_NODE ──stub──► C9(+) (other end to SW_NODE)
L1 pin2 ──stub──► +3V3
U1 FB  ──stub──► FB_NODE
+3V3   ──stub──► R12 ──stub──► FB_NODE ──stub──► R13 ──stub──► GND
+3V3   ──stub──► C2(+) ──stub──► GND
GND    ──stub──► C1(-), C2(-), C3(-), C9(-)
```

#### Hierarchical Ports Exported
`AC_IN` (in), `AC_RTN` (in), `+15VDC` (out), `+3V3` (out), `GND` (pwr)

---

### Sheet 2: Zero-Crossing Detector (`zero-crossing.kicad_sch`)

**Purpose:** Produce a logic-level pulse at every AC zero crossing (both positive→negative and negative→positive), giving 100 Hz pulse rate at 50 Hz AC.

#### Components

| Ref | Symbol | Value | Footprint |
|-----|--------|-------|-----------|
| U2 | `Isolator:H11AA1` | H11AA1 | `Package_DIP:DIP-6_W7.62mm` |
| R1 | `Device:R` | 220 kΩ | `Resistor_SMD:R_0603_1608Metric` |
| R2 | `Device:R` | 10 kΩ | `Resistor_SMD:R_0603_1608Metric` |

#### Connections

```
AC_IN  ──stub──► R1 ──stub──► U2 pin1 (Anode 1)   ← AC side of optocoupler
AC_RTN ──stub──►            U2 pin2 (Anode 2)   ← AC side
GND    ──stub──►            U2 pin3 (Cathode)   ← AC side common

+3V3   ──stub──► R2 ──stub──► U2 pin5 (Collector)
                              U2 pin5 ──stub──► ZC_OUT
                 U2 pin4 (Emitter) ──stub──► GND
```

*H11AA1 provides galvanic isolation. LED side is on AC domain; transistor side is on logic domain. ZC_OUT is active-low: pulled high by R2, pulled low each zero crossing.*

#### Hierarchical Ports Exported
`AC_IN` (in), `AC_RTN` (in), `+3V3` (in), `GND` (pwr), `ZC_OUT` (out)

---

### Sheet 3: XOR Interlock (`xor-interlock.kicad_sch`)

**Purpose:** Hardware interlock ensuring both TRIACs cannot fire simultaneously, regardless of firmware state.

#### Components

| Ref | Symbol | Value | Footprint |
|-----|--------|-------|-----------|
| U3 | `74xGxx:74LVC1G86` | 74LVC1G86 | `Package_TO_SOT_SMD:SOT-23-5` |
| U4 | `74xGxx:74LVC2G08` | 74LVC2G08 | `Package_TO_SOT_SMD:SOT-363_SC-70-6` |
| C10 | `Device:C` | 100 nF | `Capacitor_SMD:C_0603_1608Metric` |
| C11 | `Device:C` | 100 nF | `Capacitor_SMD:C_0603_1608Metric` |

#### Logic Truth Table

| GPIO_FWD | GPIO_REV | VALID (XOR) | GATE_FWD | GATE_REV | State |
|----------|----------|-------------|----------|----------|-------|
| 0 | 0 | 0 | 0 | 0 | Coast |
| 0 | 1 | 1 | 0 | 1 | Reverse |
| 1 | 0 | 1 | 1 | 0 | Forward |
| 1 | 1 | 0 | 0 | 0 | **Blocked** |

#### Connections

```
GPIO_FWD ──stub──► U3 A
GPIO_REV ──stub──► U3 B
U3 Y     ──stub──► VALID

GPIO_FWD ──stub──► U4 AND1 A1
VALID    ──stub──► U4 AND1 B1
U4 AND1 Y1 ──stub──► GATE_FWD

GPIO_REV ──stub──► U4 AND2 A2
VALID    ──stub──► U4 AND2 B2
U4 AND2 Y2 ──stub──► GATE_REV

+3V3 ──stub──► U3 VCC; C10 across U3 VCC/GND
+3V3 ──stub──► U4 VCC; C11 across U4 VCC/GND
GND  ──stub──► U3 GND, U4 GND
```

#### Hierarchical Ports
`GPIO_FWD` (in), `GPIO_REV` (in), `+3V3` (in), `GND` (pwr), `GATE_FWD` (out), `GATE_REV` (out)

---

### Sheet 4: Opto-TRIAC Drive (`opto-triac-drive.kicad_sch`)

**Purpose:** Galvanic isolation of phase-angle gate pulses from logic domain to AC motor domain via MOC3021 non-zero-crossing opto-TRIACs.

#### Components

| Ref | Symbol | Value | Footprint |
|-----|--------|-------|-----------|
| U5 | `Relay_SolidState:MOC3021M` | MOC3021 | `Package_DIP:DIP-6_W7.62mm` |
| U6 | `Relay_SolidState:MOC3021M` | MOC3021 | `Package_DIP:DIP-6_W7.62mm` |
| R3 | `Device:R` | 330 Ω | `Resistor_SMD:R_0603_1608Metric` |
| R4 | `Device:R` | 330 Ω | `Resistor_SMD:R_0603_1608Metric` |
| R5 | `Device:R` | 360 Ω | `Resistor_SMD:R_0603_1608Metric` |
| R6 | `Device:R` | 360 Ω | `Resistor_SMD:R_0603_1608Metric` |

*MOC3021 is a **non-zero-crossing** type — required for phase-angle speed control. A zero-crossing type (e.g. MOC3041) would only allow on/off control.*

#### Connections

```
— U5 (forward) —
GATE_FWD ──stub──► R3 ──stub──► U5 pin1 (LED Anode)
GND      ──stub──► U5 pin2 (LED Cathode)       ← logic domain
                   U5 pin4 (TRIAC out) ──stub──► R5 ──stub──► TRIAC_FWD_G
                   U5 pin6 (TRIAC ref) ──stub──► AC_RTN     ← AC domain

— U6 (reverse) —
GATE_REV ──stub──► R4 ──stub──► U6 pin1 (LED Anode)
GND      ──stub──► U6 pin2 (LED Cathode)
                   U6 pin4 (TRIAC out) ──stub──► R6 ──stub──► TRIAC_REV_G
                   U6 pin6 (TRIAC ref) ──stub──► AC_RTN
```

#### Hierarchical Ports
`GATE_FWD` (in), `GATE_REV` (in), `GND` (pwr), `AC_RTN` (in), `TRIAC_FWD_G` (out), `TRIAC_REV_G` (out)

---

### Sheet 5: TRIACs + Snubbers (`triacs-snubbers.kicad_sch`)

**Purpose:** AC motor switching. BT134W-600D TRIACs (1 A / 600 V — 2× margin over ~0.5 A motor load, SOT-223 SMD) with RC snubbers for inductive load protection.

#### Components

| Ref | Symbol | Value | Footprint |
|-----|--------|-------|-----------|
| Q1 | `traincontrol-shield:BT134W-600D` | BT134W-600D | `traincontrol-shield:SOT-223-3_L6.5-W3.4-P2.30-LS7.0-BR` |
| Q2 | `traincontrol-shield:BT134W-600D` | BT134W-600D | `traincontrol-shield:SOT-223-3_L6.5-W3.4-P2.30-LS7.0-BR` |
| R7 | `Device:R` | 39 Ω | `Resistor_SMD:R_0603_1608Metric` |
| R8 | `Device:R` | 39 Ω | `Resistor_SMD:R_0603_1608Metric` |
| C7 | `Device:C` | 10 nF | `Capacitor_SMD:C_0603_1608Metric` |
| C8 | `Device:C` | 10 nF | `Capacitor_SMD:C_0603_1608Metric` |

#### Connections

```
— Q1 (Forward TRIAC) —
WINDING_A    ──stub──► Q1 MT2
AC_RTN       ──stub──► Q1 MT1
TRIAC_FWD_G  ──stub──► Q1 Gate

Snubber Q1:  WINDING_A ──stub──► R7 ──stub──► C7 ──stub──► AC_RTN

— Q2 (Reverse TRIAC) —
WINDING_B    ──stub──► Q2 MT2
AC_RTN       ──stub──► Q2 MT1
TRIAC_REV_G  ──stub──► Q2 Gate

Snubber Q2:  WINDING_B ──stub──► R8 ──stub──► C8 ──stub──► AC_RTN
```

*AC_IN connects to motor armature (both WINDING_A and WINDING_B source) — this junction is on the top-level sheet via J6.*

#### Hierarchical Ports
`WINDING_A` (in), `WINDING_B` (in), `AC_RTN` (in), `TRIAC_FWD_G` (in), `TRIAC_REV_G` (in)

---

### Sheet 6: Rail Voltage Sense (`rail-voltage-sense.kicad_sch`)

**Purpose:** Resistor divider monitoring +15VDC to allow firmware to detect over/under-voltage and loss of track contact.

#### Components

| Ref | Symbol | Value | Footprint |
|-----|--------|-------|-----------|
| R9 | `Device:R` | 100 kΩ | `Resistor_SMD:R_0603_1608Metric` |
| R10 | `Device:R` | **27 kΩ** | `Resistor_SMD:R_0603_1608Metric` |

*⚠ R10 changed from BOM 33 kΩ to 27 kΩ: at 15.5 V peak, 33 kΩ gives 3.84 V (above ESP32-S3 ADC max 3.3 V). 27 kΩ gives 3.30 V — within range. BOM.md updated accordingly.*

#### Connections

```
+15VDC ──stub──► R9 ──stub──► RAIL_SENSE ──stub──► R10 ──stub──► GND
```

#### Hierarchical Ports
`+15VDC` (in), `GND` (pwr), `RAIL_SENSE` (out)

---

### Sheet 7: MCU (`mcu.kicad_sch`)

**Purpose:** ESP32-S3-DevKitC-1 module via 2×22-pin headers, supply decoupling, and status LED.

#### Components

| Ref | Symbol | Value | Footprint |
|-----|--------|-------|-----------|
| J1 | `Connector_Generic:Conn_01x22` | ESP32-S3 Header L | `Connector_PinHeader_2.54mm:PinHeader_1x22_P2.54mm_Vertical` |
| J2 | `Connector_Generic:Conn_01x22` | ESP32-S3 Header R | `Connector_PinHeader_2.54mm:PinHeader_1x22_P2.54mm_Vertical` |
| C4 | `Device:C` | 100 nF | `Capacitor_SMD:C_0603_1608Metric` |
| C5 | `Device:C` | 100 nF | `Capacitor_SMD:C_0603_1608Metric` |
| C6 | `Device:C` | 100 nF | `Capacitor_SMD:C_0603_1608Metric` |
| D2 | `Device:LED` | Green LED | `LED_SMD:LED_0603_1608Metric` |
| R11 | `Device:R` | 1 kΩ | `Resistor_SMD:R_0603_1608Metric` |

#### ESP32-S3-DevKitC-1 Pin Assignments

*Verify exact pin positions against the physical board before placing. The DevKitC-1 has 15+16 or 19+19 pins per side depending on the sub-variant — confirm against the actual board.*

| Function | GPIO | Net | Header pin |
|----------|------|-----|------------|
| Motor FWD | GPIO 4 | `GPIO_FWD` | J1 or J2 |
| Motor REV | GPIO 5 | `GPIO_REV` | J1 or J2 |
| Zero-crossing | GPIO 6 | `ZC_OUT` | J1 or J2 |
| Rail ADC | GPIO 2 | `RAIL_SENSE` | J1 or J2 |
| Status LED | GPIO 8 | `LED_STATUS` | J1 or J2 |
| Supply | 3V3 | `+3V3` | J1 or J2 |
| Ground | GND | `GND` | J1 and J2 |

#### Connections

```
J1 / J2 3V3 pins ──stub──► +3V3; C4, C5, C6 across +3V3/GND (near VCC pins)
J1 / J2 GND pins ──stub──► GND
J1 / J2 GPIO4   ──stub──► GPIO_FWD
J1 / J2 GPIO5   ──stub──► GPIO_REV
J1 / J2 GPIO6   ──stub──► ZC_OUT
J1 / J2 GPIO2   ──stub──► RAIL_SENSE
J1 / J2 GPIO8   ──stub──► LED_STATUS ──stub──► R11 ──stub──► D2 anode
D2 cathode ──stub──► GND
```

#### Hierarchical Ports
`+3V3` (in), `GND` (pwr), `ZC_OUT` (in), `GPIO_FWD` (out), `GPIO_REV` (out), `RAIL_SENSE` (in), `LED_STATUS` (in)

---

## 8. Complete BOM (Updated)

| Ref | Component | Value | KiCad Symbol | Footprint | LCSC | Download? |
|-----|-----------|-------|-------------|-----------|------|-----------|
| D1 | DB107 bridge rectifier | DB107 | `traincontrol-shield:DB107` | *(from easyeda2kicad)* | Search LCSC | **Yes** |
| U1 | MP2359DJ buck converter | MP2359DJ | `traincontrol-shield:MP2359DJ-LF-Z` | `traincontrol-shield:SOT-23-6_L2.9-W1.6-P0.95-LS2.8-BR` | C14259 | Already done |
| L1 | 4.7 µH inductor | 4.7 µH | `Device:L` | `Inductor_SMD:L_0805_2012Metric` | — | No |
| C1 | Bulk cap | 220 µF / 25 V | `Device:CP` | `Capacitor_THT:CP_Radial_D8.0mm_P3.50mm` | — | No |
| C2 | Output decoupling | 22 µF | `Device:C` | `Capacitor_SMD:C_0603_1608Metric` | — | No |
| C3–C6 | Decoupling caps | 100 nF | `Device:C` | `Capacitor_SMD:C_0603_1608Metric` | — | No |
| C7, C8 | Snubber caps | 10 nF | `Device:C` | `Capacitor_SMD:C_0603_1608Metric` | — | No |
| C9 | Bootstrap cap (U1 BST) | 100 nF | `Device:C` | `Capacitor_SMD:C_0603_1608Metric` | — | No |
| C10, C11 | Logic IC decoupling | 100 nF | `Device:C` | `Capacitor_SMD:C_0603_1608Metric` | — | No |
| U2 | H11AA1 AC optocoupler | H11AA1 | `Isolator:H11AA1` | `Package_DIP:DIP-6_W7.62mm` | — | No |
| R1 | ZC series resistor | 220 kΩ | `Device:R` | `Resistor_SMD:R_0603_1608Metric` | — | No |
| R2 | ZC pull-up | 10 kΩ | `Device:R` | `Resistor_SMD:R_0603_1608Metric` | — | No |
| U3 | 74LVC1G86 XOR gate | 74LVC1G86 | `74xGxx:74LVC1G86` | `Package_TO_SOT_SMD:SOT-23-5` | — | No |
| U4 | 74LVC2G08 dual AND gate | 74LVC2G08 | `74xGxx:74LVC2G08` | `Package_TO_SOT_SMD:SOT-363_SC-70-6` | — | No |
| U5, U6 | MOC3021 opto-TRIAC | MOC3021 | `Relay_SolidState:MOC3021M` | `Package_DIP:DIP-6_W7.62mm` | — | No |
| R3, R4 | Opto LED resistors | 330 Ω | `Device:R` | `Resistor_SMD:R_0603_1608Metric` | — | No |
| R5, R6 | TRIAC gate resistors | 360 Ω | `Device:R` | `Resistor_SMD:R_0603_1608Metric` | — | No |
| Q1, Q2 | BT134W-600D TRIAC | BT134W-600D | `traincontrol-shield:BT134W-600D` | `traincontrol-shield:SOT-223-3_L6.5-W3.4-P2.30-LS7.0-BR` | C253549 | Already done |
| R7, R8 | Snubber resistors | 39 Ω | `Device:R` | `Resistor_SMD:R_0603_1608Metric` | — | No |
| R9 | Voltage divider high | 100 kΩ | `Device:R` | `Resistor_SMD:R_0603_1608Metric` | — | No |
| R10 | Voltage divider low | **27 kΩ** *(changed from 33 kΩ)* | `Device:R` | `Resistor_SMD:R_0603_1608Metric` | — | No |
| R11 | LED resistor | 1 kΩ | `Device:R` | `Resistor_SMD:R_0603_1608Metric` | — | No |
| R12 | Buck FB top | 150 kΩ | `Device:R` | `Resistor_SMD:R_0603_1608Metric` | — | No |
| R13 | Buck FB bottom | 47 kΩ | `Device:R` | `Resistor_SMD:R_0603_1608Metric` | — | No |
| J1, J2 | ESP32-S3 headers | 1×22 pin | `Connector_Generic:Conn_01x22` | `Connector_PinHeader_2.54mm:PinHeader_1x22_P2.54mm_Vertical` | — | No |
| J4, J5 | Screw terminal 2-pin | — | `Connector_Generic:Conn_01x02` | `TerminalBlock_Phoenix:TerminalBlock_Phoenix_MKDS-1,5-2-5.08_1x02_P5.08mm_Horizontal` | — | No |
| J6 | Screw terminal 3-pin | — | `Connector_Generic:Conn_01x03` | `TerminalBlock_Phoenix:TerminalBlock_Phoenix_MKDS-1,5-3-5.08_1x03_P5.08mm_Horizontal` | — | No |
| D2 | Status LED | Green | `Device:LED` | `LED_SMD:LED_0603_1608Metric` | — | No |

**Total passives: R×13, C×11, L×1, LED×1**

---

## 9. Implementation Order

> All phases pause for user review and git commit before proceeding.

### Phase 0 — Download Missing Components ✓ COMPLETE
1. ✓ DB107 (LCSC C2492) — symbol + footprint downloaded and integrated
2. ✓ BT134W-600D (LCSC C253549) — replaces BTA204-600E (see BOM.md notes); symbol + footprint downloaded and integrated
3. ✓ All symbols verified in `libs/traincontrol-shield.kicad_sym`: MP2359DJ-LF-Z, DB107, BT134W-600D
4. ✓ All footprints verified in `libs/traincontrol-shield.pretty/`
5. **→ Review + commit**

### Phase 1 — Create KiCad Project File + Verify Libraries
1. Create `traincontrol-shield.kicad_pro` with project-local library table entries:
   - `sym_lib_table`: `traincontrol-shield` → `${KIPRJMOD}/libs/traincontrol-shield.kicad_sym`
   - `fp_lib_table`: `traincontrol-shield` → `${KIPRJMOD}/libs/traincontrol-shield.pretty`
2. **Verify** local library contents — confirm DB107, BTA204-600E, and MP2359DJ symbols are all present in `traincontrol-shield.kicad_sym`
3. **Verify** footprint files exist in `traincontrol-shield.pretty/` for all three downloaded components
4. Only proceed if all checks pass
5. **→ Review + commit**

### Phase 2 — Create Schematic Skeletons
Create 8 empty `.kicad_sch` files with title blocks, paper size A4, and hierarchical sheet declarations. No components placed yet.
- **→ Review + commit**

### Phase 3 — Implement Sub-sheets (one at a time, in order)
1. `power-supply.kicad_sch`
2. `zero-crossing.kicad_sch`
3. `xor-interlock.kicad_sch`
4. `opto-triac-drive.kicad_sch`
5. `triacs-snubbers.kicad_sch`
6. `rail-voltage-sense.kicad_sch`
7. `mcu.kicad_sch`
- **→ Review all 7 sub-sheets + commit**

### Phase 4 — Implement Top-Level Sheet
Add sub-sheet instances, external connectors (J4, J5, J6), global power symbols, and hierarchical net labels.
- **→ Review + commit**

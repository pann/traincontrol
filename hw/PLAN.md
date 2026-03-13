# Traincontrol Rev.1 — ESP32-S3-DevKitC Addon Board

## Design Overview

Rev.1 is an **integrated board** with the ESP32-C3-MINI-1 module soldered
directly on-board, along with all analog, power, and motor drive circuitry.
Connection to external wiring (AC input, motor windings) is via solder pads.
USB Micro-B provides programming and debug access.

### What goes on the board

| Block | Components |
|-------|-----------|
| Power supply | BAT54S + SS34 rectification, MP2359DJ buck (3.3 V), bulk caps, decoupling |
| Zero-crossing detector | EL357N optocoupler + BAT54S protection + 1 kΩ / 10 kΩ |
| HW XOR interlock | 74LVC1G86 (XOR) + 74LVC2G08 (dual AND) |
| Opto-TRIAC drivers | 2× MOC3021 + 330 Ω LED resistors |
| TRIACs | 2× BT134W-600E (SOT-223) + 360 Ω gate resistors + RC snubbers |
| Rail voltage divider | 100 kΩ / 27 kΩ to ADC (GPIO 2) |
| MCU | ESP32-C3-MINI-1-N4 module, WS2812B RGB LED, push buttons, USB Micro-B |
| Connectors | USB Micro-B (J1), 7× connection pads (CON1–CON7) |

### GPIO mapping (ESP32-C3-MINI-1)

| GPIO | Function | Notes |
|------|----------|-------|
| 4 | MOTOR_FWD (to XOR interlock) | Output |
| 5 | MOTOR_REV (to XOR interlock) | Output |
| 6 | ZERO_CROSS input | Interrupt on edge |
| 2 | RAIL_VOLTAGE ADC | Analog input |
| 8 | WS2812B status LED | Addressable RGB |
| 18 | USB D- | Native USB CDC/ACM |
| 19 | USB D+ | Native USB CDC/ACM |

---

## Implementation Plan

### Step 1 — LCSC part selection and library download

Look up LCSC part numbers for every component. Use the `easyeda2kicad` skill
to download KiCad symbols + footprints for each.

Key parts to source:
- ESP32-C3-MINI-1-N4 WiFi module
- MP2359DJ buck regulator
- 4.7 µH inductor (for buck)
- BT134W-600E TRIAC × 2 (SOT-223, LCSC C5380692)
- MOC3021S-TA1 opto-TRIAC × 2 (SMD)
- EL357N optocoupler (zero-crossing)
- 74LVC1G86 single XOR gate
- 74LVC2G08 dual AND gate (VSSOP-8)
- BAT54S dual Schottky × 5, SS34 Schottky × 1
- WS2812B-2020 RGB LED
- USB Micro-B connector (Molex 105017-0001)
- TL3342 push buttons × 2
- SMD electrolytic caps 220 µF × 2
- SMD passives (resistors, ceramic caps) in 0603

### Step 2 — Create KiCad project

Use the `kicad-pcb` skill to create the project at
`/mnt/1510/home/pa/work/traincontrol/hw/kicad/`.

Project name: `traincontrol-shield`

### Step 3 — Schematic capture

Draw the schematic in logical blocks:

1. **Power input & supply** — 12 VAC screw terminal → DB107 → bulk cap →
   MP2359DJ buck → 3.3 V rail. 3.3 V feeds into DevKitC 3V3 pin through the
   header and also powers the logic ICs.

2. **Zero-crossing detector** — 12 VAC → 220 kΩ → H11AA1 → 10 kΩ pull-up to
   3.3 V → GPIO 6 via header.

3. **HW XOR interlock** — GPIO 4 + GPIO 5 from header → 74LVC1G86 XOR →
   VALID signal → 74LVC2G08 AND gates → GATE_FWD, GATE_REV outputs.

4. **Opto-TRIAC drivers** — GATE_FWD/REV → 330 Ω → MOC3021 LED input.
   MOC3021 output → 360 Ω → BTA204 gate. RC snubber (100 Ω + 100 nF) across
   each TRIAC.

5. **Motor output** — Screw terminal: 12 VAC in, Winding A out, Winding B
   out. TRIACs switch the winding paths to GND (wheels/rail return).

6. **Rail voltage sensing** — Rectified rail → 100 kΩ / 33 kΩ divider →
   GPIO 2 via header.

7. **Header connections** — Two 22-pin female headers matching the
   ESP32-S3-DevKitC-1 pinout. Only the needed signals are routed; the rest
   are pass-through or unconnected.

### Step 4 — PCB layout

- **Board shape**: Rectangular, sized to match the DevKitC footprint
  (~70 × 28 mm). The two female headers sit on the long edges.
- **Layer count**: 2 layers (top + bottom).
- **High-voltage separation**: Keep 12 VAC / TRIAC traces physically
  separated from the 3.3 V logic side. Use a clear creepage gap (≥2 mm)
  between AC and DC domains.
- **Component placement**:
  - AC input terminal and TRIACs on one end of the board
  - Logic ICs and opto-couplers in the middle (they bridge AC and DC)
  - Buck regulator and caps near the 3V3 header pin
  - Motor output terminal adjacent to TRIACs
- **Copper weight**: 1 oz (standard), sufficient for the <1 A motor current.
- **Trace widths**: ≥0.5 mm for AC power paths, 0.25 mm for logic signals.

### Step 5 — Design verification

- Run ERC (electrical rules check) on schematic
- Run DRC (design rules check) on PCB
- Generate schematic PDF and PCB preview images for review
- Review BOM for completeness

### Step 6 — Manufacturing export

- Export Gerber files + drill files
- Export BOM in JLCPCB format
- Export pick-and-place (CPL) file
- Package into ZIP for ordering

---

## Bill of Materials (current)

| Ref | Component | Package | Qty | Notes |
|-----|-----------|---------|-----|-------|
| U7 | ESP32-C3-MINI-1-N4 | Module | 1 | WiFi MCU |
| U1 | MP2359DJ buck reg | SOT-23-6 | 1 | 3.3 V output |
| D6 | SS34 Schottky | SMA | 1 | Buck catch diode |
| D7,D8 | BAT54S dual Schottky | SOT-23 | 2 | AC rectification |
| L1 | 4.7 µH inductor | 0805 | 1 | For buck |
| C1,C4 | 220 µF electrolytic | 6.3x7.7 SMD | 2 | Bulk caps |
| C2 | 22 µF | 0603 | 1 | Buck output |
| C3,C5,C9–C12 | 100 nF | 0603 | 6 | Decoupling |
| C6 | 10 µF | 0603 | 1 | MCU decoupling |
| C13 | 1 µF | 0603 | 1 | |
| C7,C8 | 10 nF | 0603 | 2 | Snubber caps |
| Q1,Q2 | BT134W-600E | SOT-223 | 2 | Motor TRIACs (LCSC C5380692) |
| U5,U6 | MOC3021S-TA1 | SMD-6 | 2 | Opto-TRIAC (SMD) |
| U8 | EL357N optocoupler | SMD-4 | 1 | ZC detector |
| U3 | 74LVC1G86 | SOT-23-5 | 1 | XOR gate |
| U4 | 74LVC2G08 | VSSOP-8 | 1 | Dual AND |
| D2,D3,D5 | BAT54S dual Schottky | SOT-23 | 3 | Protection |
| D4 | WS2812B-2020 | PLCC4 2x2mm | 1 | RGB status LED |
| R3,R4 | 330 Ω | 0603 | 2 | Opto LED |
| R5,R6 | 360 Ω | 0603 | 2 | TRIAC gate |
| R7,R8 | 39 Ω | 0603 | 2 | Snubber R |
| R9 | 100 kΩ | 0603 | 1 | Voltage div |
| R10 | 27 kΩ | 0603 | 1 | Voltage div |
| R11,R18–R20 | 1 kΩ | 0603 | 4 | Current limiters |
| R12 | 150 kΩ | 0603 | 1 | FB top |
| R13 | 47 kΩ | 0603 | 1 | FB bottom |
| R14–R17,R23 | 10 kΩ | 0603 | 5 | Pull-ups |
| R21 | 1 kΩ | 0603 | 1 | ZC opto current |
| J1 | USB Micro-B | Molex 105017 | 1 | Programming/debug |
| SW1,SW2 | Push button | TL3342 | 2 | Reset/boot |
| CON1–CON7 | Connection pads | D2.0mm | 7 | AC/motor/debug |
| H1 | Mounting hole | M2 pad | 1 | |

---

## Key Design Decisions

1. **Power strategy**: The MP2359DJ buck converter provides 3.3 V from the
   rectified AC rail. BAT54S dual Schottky diodes and SS34 handle
   rectification (replacing the original DB107 bridge rectifier for a
   more compact SMD design). BAT54S on USB (D3) provides ESD protection.

2. **AC safety**: Opto-couplers (MOC3021S-TA1, EL357N) provide galvanic
   isolation between the 12 VAC motor side and the 3.3 V logic. The PCB
   layout must maintain adequate creepage between AC and DC zones.

3. **Snubber values**: 39 Ω / 10 nF per TRIAC — sized for the BT134W-600E
   and inductive motor load.

4. **Integrated design**: ESP32-C3-MINI-1 module on-board with USB Micro-B,
   WS2812B status LED, and push buttons. No longer requires a separate
   DevKitC — this board is designed to fit inside a locomotive.

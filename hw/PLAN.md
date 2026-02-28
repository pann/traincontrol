# Traincontrol Rev.1 — ESP32-S3-DevKitC Addon Board

## Design Overview

Rev.1 is a **shield/addon board** that plugs onto an ESP32-S3-DevKitC-1 via
its two 22-pin headers (J1 and J3, 2.54 mm pitch). The addon carries all the
analog and power circuitry; the DevKitC provides the MCU, WiFi, USB, and the
WS2812 status LED (GPIO 48).

This board is for **prototyping and validation** — a future Rev.2 will
integrate the ESP32 module directly onto a miniaturised PCB.

### What goes on the addon board

| Block | Components |
|-------|-----------|
| Power supply | DB107 bridge rectifier, MP2359DJ buck (3.3 V), bulk cap, decoupling |
| Zero-crossing detector | H11AA1 + 220 kΩ + 10 kΩ pull-up |
| HW XOR interlock | 74LVC1G86 (XOR) + 74LVC2G08 (dual AND) |
| Opto-TRIAC drivers | 2× MOC3021 + 330 Ω LED resistors |
| TRIACs | 2× BTA204-600E (SOT-223) + 360 Ω gate resistors + RC snubbers |
| Rail voltage divider | 100 kΩ / 33 kΩ to ADC (GPIO 2) |
| Connectors | 2× 22-pin female headers (mate with DevKitC), screw terminal for 12 VAC + motor |

### What stays on the DevKitC

- ESP32-S3-WROOM-1 module (MCU + WiFi + flash)
- USB-C (provisioning/debug)
- WS2812 RGB LED on GPIO 48
- 3.3 V LDO (DevKitC on-board regulator — **not** used to power the addon;
  the addon's own buck supplies 3.3 V to the DevKitC through the 3V3 pin)

### GPIO mapping (unchanged from firmware)

| GPIO | Function | Header pin |
|------|----------|-----------|
| 4 | MOTOR_FWD (to XOR interlock) | J1 |
| 5 | MOTOR_REV (to XOR interlock) | J1 |
| 6 | ZERO_CROSS input | J1 |
| 2 | RAIL_VOLTAGE ADC | J3 |
| 48 | WS2812 LED (on DevKitC) | — |

---

## Implementation Plan

### Step 1 — LCSC part selection and library download

Look up LCSC part numbers for every component. Use the `easyeda2kicad` skill
to download KiCad symbols + footprints for each.

Key parts to source:
- DB107 bridge rectifier
- MP2359DJ buck regulator
- 4.7 µH inductor (for buck)
- BTA204-600E TRIAC × 2
- MOC3021 opto-TRIAC × 2
- H11AA1 AC optocoupler
- 74LVC1G86 single XOR gate
- 74LVC2G08 dual AND gate
- Screw terminal (2-pos and 3-pos, 5.08 mm pitch)
- 2× 22-pin 2.54 mm female pin headers
- Bulk electrolytic cap 220 µF / 25 V
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

## Bill of Materials (preliminary)

| Ref | Component | Package | Qty | Notes |
|-----|-----------|---------|-----|-------|
| D1 | DB107 bridge rectifier | DIP-4 | 1 | |
| U1 | MP2359DJ buck reg | SOT-23-6 | 1 | 3.3 V output |
| L1 | 4.7 µH inductor | SMD | 1 | For buck |
| C1 | 220 µF / 25 V | Radial | 1 | Bulk input |
| C2 | 22 µF / 10 V | 0603 | 1 | Buck output |
| C3,C4 | 100 nF | 0603 | 2 | Decoupling |
| C5 | 10 µF | 0603 | 1 | Buck input |
| T1,T2 | BTA204-600E | SOT-223 | 2 | Motor TRIACs |
| U2,U3 | MOC3021 | DIP-6 | 2 | Opto-TRIAC |
| U4 | H11AA1 | DIP-6 | 1 | ZC detector |
| U5 | 74LVC1G86 | SOT-23-5 | 1 | XOR gate |
| U6 | 74LVC2G08 | SOT-23-6 | 1 | Dual AND |
| R1 | 220 kΩ | 0603 | 1 | ZC input |
| R2 | 10 kΩ | 0603 | 1 | ZC pull-up |
| R3,R4 | 330 Ω | 0603 | 2 | Opto LED |
| R5,R6 | 360 Ω | 0603 | 2 | TRIAC gate |
| R7,R8 | 100 Ω | 0603 | 2 | Snubber R |
| C6,C7 | 100 nF / 50 V | 0603 | 2 | Snubber C (ceramic) |
| R9 | 100 kΩ | 0603 | 1 | Voltage div |
| R10 | 33 kΩ | 0603 | 1 | Voltage div |
| J1,J2 | 22-pin female header | 2.54 mm | 2 | DevKitC mate |
| J3 | Screw terminal 2-pos | 5.08 mm | 1 | 12 VAC in |
| J4 | Screw terminal 3-pos | 5.08 mm | 1 | Motor out |

---

## Key Design Decisions

1. **Power strategy**: The addon's MP2359DJ buck provides 3.3 V to the
   DevKitC via its 3V3 header pin, bypassing the DevKitC's on-board LDO.
   This is safe because the DevKitC LDO input (5V from USB) will be absent
   when powered from track. When USB is also connected (debug), the higher
   voltage source wins — need a Schottky diode (or rely on DevKitC's
   existing power-path design) to prevent backfeed.

2. **AC safety**: Opto-couplers (MOC3021, H11AA1) provide galvanic isolation
   between the 12 VAC motor side and the 3.3 V logic. The PCB layout must
   maintain adequate creepage between AC and DC zones.

3. **Snubber capacitors**: 50 V ceramic MLCCs (0603) are adequate — the
   rectified 12 VAC peak is ~17 V, well within a 50 V rating.

4. **Form factor**: This Rev.1 board will be larger than the final product —
   it's optimised for testability, not for fitting inside a locomotive.

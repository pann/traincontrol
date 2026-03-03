# Traincontrol Shield — HW Implementation Plan (hw-take2)

## Design Rules

1. **Use KiCad standard libraries** for symbols, footprints, and 3D models whenever available.
2. **Use easyeda2kicad** (skill) to download missing components (symbols, footprints, 3D models) into a project-local library and register it in the KiCad project.
3. **0603 passive components** for all resistors, capacitors, and inductors where applicable.
4. **50V max rating** — no components need to be rated above 50V.
5. **Hierarchical schematic** — one sub-sheet per logical block, plus a top-level sheet that instantiates them all.
6. **Short wire stubs + net labels** — no long wires across the schematic. Connect components using net labels.
7. **All component symbols must exist in libraries before any schematic file is created.** No symbol substitutions.

## Ground Architecture

The DB107 bridge rectifier creates two separate AC rails and a DC rail:

| Pin | Net | Description |
|-----|-----|-------------|
| AC1 | `AC_IN` | 12 VAC hot (track pickup shoe) |
| AC2 | `AC_RTN` | 12 VAC return (wheels / motor GND) |
| +DC | `+15VDC` | Rectified unregulated DC |
| −DC | `GND` | Logic/PSU ground |

`AC_RTN` ≠ `GND`. The H11AA1 and MOC3021 optocouplers are the isolation boundary between AC and logic domains.

## Schematic Hierarchy

| Sheet | Contents |
|-------|----------|
| **Top level** | Sub-sheet instances, inter-block net labels, connectors J4/J5/J6 |
| 1. Power Supply | DB107 bridge rectifier, MP2359DJ buck, bulk/decoupling caps, FB resistors R12/R13, bootstrap cap C9 |
| 2. Zero-Crossing | H11AA1 AC optocoupler (LED side on AC_IN/AC_RTN, transistor side on GND/+3V3), R1 series, R2 pull-up |
| 3. XOR Interlock | 74LVC1G86 (XOR) + 74LVC2G08 (dual AND) + decoupling C10/C11 |
| 4. Opto-TRIAC Drive | 2× MOC3021 (LED side on logic, TRIAC side on AC_RTN), gate resistors R3–R6 |
| 5. TRIACs + Snubbers | 2× BT134W-600D (MT1→AC_RTN), 39Ω/10nF RC snubber networks |
| 6. Rail Voltage Sense | Resistor divider 100kΩ/27kΩ (changed from 33kΩ — see note) to ADC |
| 7. MCU (ESP32-S3-DevKitC) | 2× 22-pin headers, decoupling C4/C5/C6, status LED D2+R11 |

## Component Sourcing Strategy

Components are sourced in this priority order:

1. **KiCad standard library** — checked first
2. **Project-local library** (`hw/traincontrol-shield/libs/`) — easyeda2kicad downloads go here
3. **easyeda2kicad download** — for any part not found in steps 1–2

### Library Verification Status

| Component | Library | Status |
|-----------|---------|--------|
| DB107 | `traincontrol-shield` (local) | ✓ Downloaded via easyeda2kicad (LCSC C2492) |
| MP2359DJ | `traincontrol-shield` (local) | ✓ Downloaded via easyeda2kicad (LCSC C14259) |
| H11AA1 | `Isolator:H11AA1` | ✓ In KiCad standard lib |
| 74LVC1G86 | `74xGxx:74LVC1G86` | ✓ In KiCad standard lib |
| 74LVC2G08 | `74xGxx:74LVC2G08` | ✓ In KiCad standard lib |
| MOC3021 | `Relay_SolidState:MOC3021M` | ✓ In KiCad standard lib (M = DIP-6 package) |
| BT134W-600D | `traincontrol-shield` (local) | ✓ Downloaded via easyeda2kicad (LCSC C253549) — replaces BTA204-600E |
| All passives | `Device:R/C/L/CP/LED` | ✓ In KiCad standard lib |
| Connectors | `Connector_Generic:Conn_01x*` | ✓ In KiCad standard lib |

## Implementation Phases

Each phase ends with user review and a git commit before the next phase begins.

| Phase | Deliverable |
|-------|-------------|
| 0 | ✓ Download DB107 (C2492) + BT134W-600D (C253549) via easyeda2kicad into local lib |
| 1 | ✓ Create `traincontrol-shield.kicad_pro` + lib tables; all symbols + footprints verified |
| 2 | ✓ Create 8 empty `.kicad_sch` skeletons (title blocks only) |
| 3 | ✓ Implement sub-sheets 1–7 with components and net labels |
| 4 | ✓ Implement top-level sheet (sub-sheet instances + connectors) |

## Design Notes

- **ADC divider:** R10 is 27 kΩ (not 33 kΩ). The 33 kΩ value gives 3.84 V at 15.5 V peak, which exceeds the ESP32-S3 ADC maximum of 3.3 V. At 27 kΩ the output is 3.30 V.
- **MP2359DJ feedback:** R12 = 150 kΩ (top), R13 = 47 kΩ (bottom). Sets VOUT = 0.792 × (1 + 150k/47k) = 3.31 V. C9 = 100 nF bootstrap cap (BST → SW node).
- **Snubbers:** 39 Ω / 10 nF per TRIAC (values from BOM.md).
- **MOC3021 type:** Non-zero-crossing — required for phase-angle speed control.
- **USB-C connector:** Omitted from shield PCB. The ESP32-S3-DevKitC has its own onboard USB-C.

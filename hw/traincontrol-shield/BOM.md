# Traincontrol Shield — Bill of Materials

## Component Mapping

| Ref | Component | Value | KiCad Symbol | KiCad Footprint | LCSC | Download? |
|-----|-----------|-------|-------------|-----------------|------|-----------|
| D1 | DB107 bridge rectifier | DB107 | `traincontrol-shield:DB107` | *(from easyeda2kicad)* | Search LCSC | **Yes** |
| U1 | MP2359DJ buck converter | MP2359DJ | `traincontrol-shield:MP2359DJ-LF-Z` | `traincontrol-shield:SOT-23-6_L2.9-W1.6-P0.95-LS2.8-BR` | C14259 | Already done |
| L1 | 4.7µH inductor | 4.7µH | `Device:L` | `Inductor_SMD:L_0805_2012Metric` | — | No |
| C1 | Input bulk cap | 220µF/25V | `Device:CP` | `Capacitor_THT:CP_Radial_D8.0mm_P3.50mm` | — | No |
| C2 | Output decoupling | 22µF | `Device:C` | `Capacitor_SMD:C_0603_1608Metric` | — | No |
| C3 | U1 IN decoupling | 100nF | `Device:C` | `Capacitor_SMD:C_0603_1608Metric` | — | No |
| C4, C5, C6 | MCU VCC decoupling | 100nF | `Device:C` | `Capacitor_SMD:C_0603_1608Metric` | — | No |
| C7, C8 | Snubber caps | 10nF | `Device:C` | `Capacitor_SMD:C_0603_1608Metric` | — | No |
| C9 | U1 BST bootstrap cap | 100nF | `Device:C` | `Capacitor_SMD:C_0603_1608Metric` | — | No |
| C10 | U3 VCC decoupling | 100nF | `Device:C` | `Capacitor_SMD:C_0603_1608Metric` | — | No |
| C11 | U4 VCC decoupling | 100nF | `Device:C` | `Capacitor_SMD:C_0603_1608Metric` | — | No |
| U2 | H11AA1 AC optocoupler | H11AA1 | `Isolator:H11AA1` | `Package_DIP:DIP-6_W7.62mm` | — | No |
| R1 | ZC series resistor | 220kΩ | `Device:R` | `Resistor_SMD:R_0603_1608Metric` | — | No |
| R2 | ZC pull-up | 10kΩ | `Device:R` | `Resistor_SMD:R_0603_1608Metric` | — | No |
| U3 | 74LVC1G86 XOR gate | 74LVC1G86 | `74xGxx:74LVC1G86` | `Package_TO_SOT_SMD:SOT-23-5` | — | No |
| U4 | 74LVC2G08 dual AND | 74LVC2G08 | `74xGxx:74LVC2G08` | `Package_TO_SOT_SMD:SOT-363_SC-70-6` | — | No |
| U5, U6 | MOC3021 opto-TRIAC | MOC3021 | `Relay_SolidState:MOC3021M` | `Package_DIP:DIP-6_W7.62mm` | — | No |
| R3, R4 | Opto LED resistors | 330Ω | `Device:R` | `Resistor_SMD:R_0603_1608Metric` | — | No |
| R5, R6 | TRIAC gate resistors | 360Ω | `Device:R` | `Resistor_SMD:R_0603_1608Metric` | — | No |
| Q1, Q2 | BT134W-600D TRIAC | BT134W-600D | `traincontrol-shield:BT134W-600D` | `traincontrol-shield:SOT-223-3_L6.5-W3.4-P2.30-LS7.0-BR` | C253549 | Already done |
| R7, R8 | Snubber resistors | 39Ω | `Device:R` | `Resistor_SMD:R_0603_1608Metric` | — | No |
| R9 | Voltage divider high | 100kΩ | `Device:R` | `Resistor_SMD:R_0603_1608Metric` | — | No |
| R10 | Voltage divider low | **27kΩ** | `Device:R` | `Resistor_SMD:R_0603_1608Metric` | — | No |
| R11 | LED resistor | 1kΩ | `Device:R` | `Resistor_SMD:R_0603_1608Metric` | — | No |
| R12 | Buck FB top resistor | 150kΩ | `Device:R` | `Resistor_SMD:R_0603_1608Metric` | — | No |
| R13 | Buck FB bottom resistor | 47kΩ | `Device:R` | `Resistor_SMD:R_0603_1608Metric` | — | No |
| J1, J2 | ESP32-S3 headers | 1×22 pin | `Connector_Generic:Conn_01x22` | `Connector_PinHeader_2.54mm:PinHeader_1x22_P2.54mm_Vertical` | — | No |
| J4, J5 | Screw terminal 2-pin | 2-pin 5.08mm | `Connector_Generic:Conn_01x02` | `TerminalBlock_Phoenix:TerminalBlock_Phoenix_MKDS-1,5-2-5.08_1x02_P5.08mm_Horizontal` | — | No |
| J6 | Screw terminal 3-pin | 3-pin 5.08mm | `Connector_Generic:Conn_01x03` | `TerminalBlock_Phoenix:TerminalBlock_Phoenix_MKDS-1,5-3-5.08_1x03_P5.08mm_Horizontal` | — | No |
| D2 | Status LED | Green | `Device:LED` | `LED_SMD:LED_0603_1608Metric` | — | No |

## Notes

- All passive components use 0603 (1608 Metric) SMD package; exception: C1 (electrolytic, through-hole)
- **DB107** not in KiCad standard Diode_Bridge library — downloaded via easyeda2kicad (LCSC C2492) into `traincontrol-shield` local lib. Footprint: DIO-BG-TH_DF (through-hole DIP-4).
- **BT134W-600D** replaces BTA204-600E (original design). BTA204-600E is TO-220AB through-hole and not available on LCSC. BT134W-600D is SOT-223 SMD, 1A/600V, WeEn (LCSC C253549). Downloaded via easyeda2kicad. Footprint: SOT-223-3_L6.5-W3.4-P2.30-LS7.0-BR.
- **MP2359DJ** already downloaded via easyeda2kicad (LCSC C14259)
- **MOC3021M** is the KiCad library name for the MOC3021 in DIP-6 package (M = package variant, same electrical part)
- **R10 changed to 27kΩ** (was 33kΩ): 33kΩ gives 3.84V at 15.5V peak rail — above ESP32-S3 ADC max 3.3V. 27kΩ gives 3.30V.
- **R12, R13 added**: feedback resistors for MP2359DJ to set VOUT = 3.3V. VOUT = 0.792V × (1 + 150k/47k) = 3.31V.
- **C9 added**: 100nF bootstrap capacitor required on MP2359DJ BST pin (BST → SW node).
- **C10, C11 added**: 100nF VCC decoupling for U3 (74LVC1G86) and U4 (74LVC2G08).
- **Connector assignments**: J4 = track input (AC_IN, AC_RTN); J5 = motor windings (WINDING_A, WINDING_B); J6 = combined motor+AC (AC_IN, WINDING_A, WINDING_B)

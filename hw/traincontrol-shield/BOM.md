# Traincontrol Shield — Bill of Materials

## Component Mapping

| Ref | Component | Value | KiCad Symbol | KiCad Footprint | LCSC | Download? |
|-----|-----------|-------|-------------|-----------------|------|-----------|
| D1 | DB107 bridge rectifier | DB107 | `Diode_Bridge:B40C800DM` (value=DB107) | `Diode_THT:Diode_Bridge_DIP-4_W7.62mm_P5.08mm` | — | No |
| U1 | MP2359DJ buck converter | MP2359DJ | *project lib* | SOT-23-6 (from LCSC) | C14289 | **Yes** |
| L1 | 4.7µH inductor | 4.7µH | `Device:L` | `Inductor_SMD:L_0805_2012Metric` | — | No |
| C1 | Input bulk cap | 220µF/25V | `Device:CP` | `Capacitor_THT:CP_Radial_D8.0mm_P3.50mm` | — | No |
| C2 | Output decoupling | 22µF | `Device:C` | `Capacitor_SMD:C_0603_1608Metric` | — | No |
| C3-C6 | Decoupling caps | 100nF | `Device:C` | `Capacitor_SMD:C_0603_1608Metric` | — | No |
| U2 | H11AA1 AC optocoupler | H11AA1 | `Isolator:H11AA1` | `Package_DIP:DIP-6_W7.62mm` | — | No |
| R1 | ZC series resistor | 220kΩ | `Device:R` | `Resistor_SMD:R_0603_1608Metric` | — | No |
| R2 | ZC pull-up | 10kΩ | `Device:R` | `Resistor_SMD:R_0603_1608Metric` | — | No |
| U3 | 74LVC1G86 XOR gate | 74LVC1G86 | `74xGxx:74LVC1G86` | `Package_TO_SOT_SMD:SOT-23-5` | — | No |
| U4 | 74LVC2G08 dual AND | 74LVC2G08 | `74xGxx:74LVC2G08` | `Package_TO_SOT_SMD:SOT-363_SC-70-6` | — | No |
| U5, U6 | MOC3021 opto-TRIAC | MOC3021 | `Relay_SolidState:MOC3021M` | `Package_DIP:DIP-6_W7.62mm` | — | No |
| R3, R4 | Opto LED resistors | 330Ω | `Device:R` | `Resistor_SMD:R_0603_1608Metric` | — | No |
| R5, R6 | TRIAC gate resistors | 360Ω | `Device:R` | `Resistor_SMD:R_0603_1608Metric` | — | No |
| Q1, Q2 | BTA204-600E TRIAC | BTA204-600E | `Triac_Thyristor:Z0103MN` (value=BTA204-600E) | `Package_TO_SOT_SMD:SOT-223-3_TabPin2` | — | No |
| R7, R8 | Snubber resistors | 39Ω | `Device:R` | `Resistor_SMD:R_0603_1608Metric` | — | No |
| C7, C8 | Snubber caps | 10nF | `Device:C` | `Capacitor_SMD:C_0603_1608Metric` | — | No |
| R9 | Voltage divider high | 100kΩ | `Device:R` | `Resistor_SMD:R_0603_1608Metric` | — | No |
| R10 | Voltage divider low | 33kΩ | `Device:R` | `Resistor_SMD:R_0603_1608Metric` | — | No |
| J1, J2 | ESP32-S3 headers | 1x22 | `Connector_Generic:Conn_01x22` | `Connector_PinHeader_2.54mm:PinHeader_1x22_P2.54mm_Vertical` | — | No |
| J3 | USB-C connector | USB4110-GF-A | *project lib* | (from LCSC) | C2765186 | **Yes** |
| J4, J5 | Screw terminal 2-pin | 2-pin 5.08mm | `Connector_Generic:Conn_01x02` | `TerminalBlock_Phoenix:TerminalBlock_Phoenix_MKDS-1,5-2-5.08_1x02_P5.08mm_Horizontal` | — | No |
| J6 | Screw terminal 3-pin | 3-pin 5.08mm | `Connector_Generic:Conn_01x03` | `TerminalBlock_Phoenix:TerminalBlock_Phoenix_MKDS-1,5-3-5.08_1x03_P5.08mm_Horizontal` | — | No |
| D2 | Status LED | Green | `Device:LED` | `LED_SMD:LED_0603_1608Metric` | — | No |
| R11 | LED resistor | 1kΩ | `Device:R` | `Resistor_SMD:R_0603_1608Metric` | — | No |

## Notes

- All passive components use 0603 (1608 Metric) package
- MP2359DJ and USB4110-GF-A need download via easyeda2kicad into project-local library
- BTA204-600E comes in SOT-223 package; KiCad has the symbol in `Triac_Thyristor` library
- DB107 bridge rectifier is in KiCad standard library `Diode_Bridge`
- Electrolytic cap C1 is through-hole (radial); all other caps are 0603 SMD

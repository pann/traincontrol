# Traincontrol Shield — Bill of Materials

## JLCPCB Part Numbers

| Ref | Component | Value | Footprint | LCSC | Type | Notes |
|-----|-----------|-------|-----------|------|------|-------|
| **Power Supply** |
| U1 | MP2359DJ buck converter | MP2359DJ | `traincontrol-shield:SOT-23-6` | C2959845 | Ext | TECH PUBLIC, 33k stock, $0.30 |
| D6 | SS34 Schottky diode | SS34 | `Diode_SMD:D_SMA` | C115205 | Basic | Jingdao, 23.9k stock, $0.02 |
| D7 | BAT54S dual Schottky | BAT54S | `Package_TO_SOT_SMD:SOT-23` | C8592 | Basic | JSCJ, 125k stock, $0.02 |
| D8 | BAT54S dual Schottky | BAT54S | `Package_TO_SOT_SMD:SOT-23` | C8592 | Basic | (same as D7) |
| L1 | Power inductor | 4.7µH | `Inductor_SMD:L_0805_2012Metric` | C436789 | Ext | PROD Tech PWCWAQ2012F-4R7KTT, 840mA, 0805, 5.6k stock, $0.09 |
| C1 | Bulk cap (input) | 220µF/25V | `Capacitor_SMD:CP_Elec_6.3x7.7` | C3445241 | Ext | KNSCHA, 6.3x7.7mm, 7.4k stock, $0.12 |
| C4 | Bulk cap | 220µF/25V | `Capacitor_SMD:CP_Elec_6.3x7.7` | C3445241 | Ext | (same as C1) |
| C2 | Output cap | 22µF | `Capacitor_SMD:C_0603_1608Metric` | C159801 | Basic | Samsung, 5.9M stock, $0.007 |
| C3 | Decoupling | 100nF | `Capacitor_SMD:C_0603_1608Metric` | C14663 | Basic | Samsung, $0.002 |
| C9 | Bootstrap cap | 100nF | `Capacitor_SMD:C_0603_1608Metric` | C14663 | Basic | (same as C3) |
| C13 | Cap | 1µF | `Capacitor_SMD:C_0603_1608Metric` | C15849 | Basic | Samsung, $0.002 |
| R12 | FB top resistor | 150kΩ | `Resistor_SMD:R_0603_1608Metric` | C22807 | Basic | UNI-ROYAL, $0.001 |
| R13 | FB bottom resistor | 47kΩ | `Resistor_SMD:R_0603_1608Metric` | C25819 | Basic | UNI-ROYAL, $0.001 |
| **Zero-Crossing Detector** |
| U8 | EL357N optocoupler | EL357N | `traincontrol-shield:OPTO-SMD-4` | C42379244 | Basic | EL357N-C-TA-G, 522k stock, $0.05 |
| D2 | BAT54S dual Schottky | BAT54S | `Package_TO_SOT_SMD:SOT-23` | C8592 | Basic | (same as D7) |
| D5 | BAT54S dual Schottky | BAT54S | `Package_TO_SOT_SMD:SOT-23` | C8592 | Basic | (same as D7) |
| R21 | ZC input resistor | 1kΩ | `Resistor_SMD:R_0603_1608Metric` | C21190 | Basic | UNI-ROYAL, $0.001 |
| R23 | ZC pull-up | 10kΩ | `Resistor_SMD:R_0603_1608Metric` | C25804 | Basic | UNI-ROYAL, $0.001 |
| **XOR Interlock** |
| U3 | 74LVC1G86 XOR gate | 74LVC1G86 | `Package_TO_SOT_SMD:SOT-23-5` | C176932 | Ext | Diodes Inc, 4.6k stock, $0.05 |
| U4 | 74LVC2G08 dual AND | 74LVC2G08 | `Package_SO:VSSOP-8_2.3x2mm` | C91875 | Ext | TI SN74LVC2G08DCUR, 7.3k stock, $0.25 |
| C10 | U3 VCC decoupling | 100nF | `Capacitor_SMD:C_0603_1608Metric` | C14663 | Basic | (same as C3) |
| C11 | U4 VCC decoupling | 100nF | `Capacitor_SMD:C_0603_1608Metric` | C14663 | Basic | (same as C3) |
| **Opto-TRIAC Drive** |
| U5 | MOC3021S opto-TRIAC | MOC3021S-TA1 | `traincontrol-shield:SMD-6` | C115465 | Ext | Lite-On, 48k stock, $0.17 |
| U6 | MOC3021S opto-TRIAC | MOC3021S-TA1 | `traincontrol-shield:SMD-6` | C115465 | Ext | (same as U5) |
| R3 | Opto LED resistor | 330Ω | `Resistor_SMD:R_0603_1608Metric` | C23138 | Basic | UNI-ROYAL, 427k stock, $0.001 |
| R4 | Opto LED resistor | 330Ω | `Resistor_SMD:R_0603_1608Metric` | C23138 | Basic | (same as R3) |
| R5 | TRIAC gate resistor | 360Ω | `Resistor_SMD:R_0603_1608Metric` | C25194 | Basic | UNI-ROYAL, 606k stock, $0.001 |
| R6 | TRIAC gate resistor | 360Ω | `Resistor_SMD:R_0603_1608Metric` | C25194 | Basic | (same as R5) |
| **TRIACs + Snubbers** |
| Q1 | BT134W-600E TRIAC | BT134W-600E | `traincontrol-shield:SOT-223-3` | C5380692 | Ext | FUXINSEMI, 1.4k stock, $0.05 |
| Q2 | BT134W-600E TRIAC | BT134W-600E | `traincontrol-shield:SOT-223-3` | C5380692 | Ext | (same as Q1) |
| R7 | Snubber resistor | 39Ω | `Resistor_SMD:R_0603_1608Metric` | C25236 | Basic | UNI-ROYAL, 5k stock, $0.001 |
| R8 | Snubber resistor | 39Ω | `Resistor_SMD:R_0603_1608Metric` | C25236 | Basic | (same as R7) |
| C7 | Snubber cap | 10nF | `Capacitor_SMD:C_0603_1608Metric` | C100042 | Basic | YAGEO 50V X7R, 4.4M stock, $0.002 |
| C8 | Snubber cap | 10nF | `Capacitor_SMD:C_0603_1608Metric` | C100042 | Basic | (same as C7) |
| **Rail Voltage Sense** |
| R9 | Divider high | 100kΩ | `Resistor_SMD:R_0603_1608Metric` | C25803 | Basic | UNI-ROYAL, $0.001 |
| R10 | Divider low | 27kΩ | `Resistor_SMD:R_0603_1608Metric` | C22967 | Basic | UNI-ROYAL, $0.001 |
| **MCU** |
| U7 | ESP32-C3-MINI-1-N4 | ESP32-C3-MINI-1-N4 | `traincontrol-shield:WIFIM-SMD_ESP32-C3-MINI-1` | C2838502 | Basic | Espressif, 10.7k stock, $2.06 |
| D4 | WS2812B-2020 RGB LED | WS2812B-2020 | `LED_SMD:LED_WS2812B-2020_PLCC4_2.0x2.0mm` | C965555 | Ext | Worldsemi, 562k stock, $0.09 |
| SW1 | Push button | SW_Push | `Button_Switch_SMD:SW_SPST_TL3342` | C2886894 | Ext | E-Switch TL3342F260QG, 164k stock, $0.78 |
| SW2 | Push button | SW_Push | `Button_Switch_SMD:SW_SPST_TL3342` | C2886894 | Ext | (same as SW1) |
| R11 | LED resistor | 1kΩ | `Resistor_SMD:R_0603_1608Metric` | C21190 | Basic | UNI-ROYAL, $0.001 |
| R14 | Pull-up | 10kΩ | `Resistor_SMD:R_0603_1608Metric` | C25804 | Basic | UNI-ROYAL, $0.001 |
| R15 | Pull-up | 10kΩ | `Resistor_SMD:R_0603_1608Metric` | C25804 | Basic | (same as R14) |
| R16 | Pull-up | 10kΩ | `Resistor_SMD:R_0603_1608Metric` | C25804 | Basic | (same as R14) |
| R17 | Pull-up | 10kΩ | `Resistor_SMD:R_0603_1608Metric` | C25804 | Basic | (same as R14) |
| R18 | Current limiter | 1kΩ | `Resistor_SMD:R_0603_1608Metric` | C21190 | Basic | (same as R11) |
| R19 | Current limiter | 1kΩ | `Resistor_SMD:R_0603_1608Metric` | C21190 | Basic | (same as R11) |
| R20 | Current limiter | 1kΩ | `Resistor_SMD:R_0603_1608Metric` | C21190 | Basic | (same as R11) |
| C5 | MCU decoupling | 100nF | `Capacitor_SMD:C_0603_1608Metric` | C14663 | Basic | (same as C3) |
| C6 | MCU decoupling | 10µF | `Capacitor_SMD:C_0603_1608Metric` | C85713 | Basic | Samsung, 10.4M stock, $0.003 |
| C12 | MCU decoupling | 100nF | `Capacitor_SMD:C_0603_1608Metric` | C14663 | Basic | (same as C3) |
| CON5 | Connection pad | — | `TestPoint:TestPoint_Pad_D2.0mm` | — | — | No part needed (bare pad) |
| CON6 | Connection pad | — | `TestPoint:TestPoint_Pad_D2.0mm` | — | — | No part needed (bare pad) |
| CON7 | Connection pad | — | `TestPoint:TestPoint_Pad_D2.0mm` | — | — | No part needed (bare pad) |
| **Top-Level** |
| J1 | USB Micro-B connector | USB_B_Micro | `Connector_USB:USB_Micro-B_Molex-105017-0001` | C136000 | Ext | Molex 1050170001, 38.7k stock, $0.37 |
| D3 | BAT54S dual Schottky | BAT54S | `Package_TO_SOT_SMD:SOT-23` | C8592 | Basic | (same as D7) |
| CON1 | Connection pad | — | `TestPoint:TestPoint_Pad_D2.0mm` | — | — | No part needed (bare pad) |
| CON2 | Connection pad | — | `TestPoint:TestPoint_Pad_D2.0mm` | — | — | No part needed (bare pad) |
| CON3 | Connection pad | — | `TestPoint:TestPoint_Pad_D2.0mm` | — | — | No part needed (bare pad) |
| CON4 | Connection pad | — | `TestPoint:TestPoint_Pad_D2.0mm` | — | — | No part needed (bare pad) |
| H1 | Mounting hole | — | `MountingHole:MountingHole_2.2mm_M2_Pad` | — | — | No part needed |

## Unique Parts for JLCPCB Order

| LCSC | Component | Value | Qty | Type | Unit Price |
|------|-----------|-------|-----|------|------------|
| C2838502 | ESP32-C3-MINI-1-N4 | — | 1 | Basic | $2.06 |
| C2959845 | MP2359DJ (TECH PUBLIC) | — | 1 | Ext | $0.30 |
| C115205 | SS34 Schottky | — | 1 | Basic | $0.02 |
| C8592 | BAT54S dual Schottky | — | 5 | Basic | $0.02 |
| C436789 | PWCWAQ2012F-4R7KTT inductor | 4.7µH 840mA 0805 | 1 | Ext | $0.09 |
| C3445241 | 220µF/25V electrolytic | 6.3x7.7mm | 2 | Ext | $0.12 |
| C42379244 | EL357N-C-TA-G optocoupler | — | 1 | Basic | $0.05 |
| C176932 | 74LVC1G86W5-7 XOR | — | 1 | Ext | $0.05 |
| C91875 | SN74LVC2G08DCUR AND | — | 1 | Ext | $0.25 |
| C115465 | MOC3021S-TA1 opto-TRIAC | — | 2 | Ext | $0.17 |
| C5380692 | BT134W-600E TRIAC (FUXINSEMI) | SOT-223 | 2 | Ext | $0.05 |
| C965555 | WS2812B-2020 RGB LED | — | 1 | Ext | $0.09 |
| C2886894 | TL3342F260QG button | — | 2 | Ext | $0.78 |
| C136000 | Molex USB Micro-B | — | 1 | Ext | $0.37 |
| C159801 | 22µF 0603 MLCC | — | 1 | Basic | $0.007 |
| C14663 | 100nF 0603 MLCC | — | 6 | Basic | $0.002 |
| C85713 | 10µF 0603 MLCC | — | 1 | Basic | $0.003 |
| C15849 | 1µF 0603 MLCC | — | 1 | Basic | $0.002 |
| C100042 | 10nF 50V 0603 MLCC | — | 2 | Basic | $0.002 |
| C21190 | 1kΩ 0603 resistor | — | 5 | Basic | $0.001 |
| C25804 | 10kΩ 0603 resistor | — | 5 | Basic | $0.001 |
| C23138 | 330Ω 0603 resistor | — | 2 | Basic | $0.001 |
| C25194 | 360Ω 0603 resistor | — | 2 | Basic | $0.001 |
| C25236 | 39Ω 0603 resistor | — | 2 | Basic | $0.001 |
| C25803 | 100kΩ 0603 resistor | — | 1 | Basic | $0.001 |
| C22967 | 27kΩ 0603 resistor | — | 1 | Basic | $0.001 |
| C22807 | 150kΩ 0603 resistor | — | 1 | Basic | $0.001 |
| C25819 | 47kΩ 0603 resistor | — | 1 | Basic | $0.001 |

**Total unique LCSC parts: 28** (16 Basic + 12 Extended)

## Summary

| Type | Count |
|------|-------|
| ICs | 7 (U1, U3–U8) |
| TRIACs | 2 (Q1, Q2) |
| Diodes | 6 (D2, D3, D5–D8) |
| LED | 1 (D4 WS2812B) |
| Resistors | 17 (R3–R21, R23) |
| Capacitors | 13 (C1–C13) |
| Inductor | 1 (L1) |
| Connectors | 1 (J1 USB) |
| Connection pads | 7 (CON1–CON7) |
| Switches | 2 (SW1, SW2) |
| Mounting | 1 (H1) |
| **Total** | **58** |

## Notes

### Sourcing Notes
- **L1 (C436789)**: PROD Tech PWCWAQ2012F-4R7KTT wire-wound 4.7µH 0805 inductor, 840mA saturation, 559mΩ DCR. Fits the schematic 0805 footprint. MP2359DJ max switch current is 1.2A peak but typical load current for this design is ~200mA (ESP32-C3 + logic), well within 840mA rating.
- **Q1/Q2 (C5380692)**: FUXINSEMI BT134W-600E replaces original WeEn BT134W-600D (C253549, pre-order only). Pin-compatible SOT-223, same 600V rating. Alternative: C256438 (WeEn BT134W-600E, 1.7k stock, $0.31).
- **U1 (C2959845)**: TECH PUBLIC MP2359DJ replaces MPS original (C14259, $2.29). Same pinout and specs at $0.30. Alternative if quality concern: C14259 (MPS original, Basic part).
- **Connection pads (CON1–CON7) and H1**: These are bare PCB features — no assembled component needed.

### Design Notes
- All passive components use 0603 (1608 Metric) SMD package; exceptions: C1, C4 (SMD electrolytic 6.3x7.7mm)
- **BAT54S (D2/D3/D5/D7/D8)**: Dual Schottky diodes used for AC rectification and input protection throughout
- **SS34 (D6)**: SMA Schottky catch/flyback diode for the MP2359DJ buck converter
- **EL357N (U8)**: SMD optocoupler for zero-crossing detection (replaces original DIP-6 H11AA1)
- **MOC3021S-TA1 (U5/U6)**: SMD variant of MOC3021 opto-TRIAC (was DIP-6)
- **74LVC2G08 (U4)**: Now in VSSOP-8 package (was SC-70-6)
- **ESP32-C3-MINI-1-N4 (U7)**: MCU integrated on board (no longer a DevKitC shield)
- **WS2812B-2020 (D4)**: Addressable RGB LED for status indication
- **R10 = 27kΩ** (was 33kΩ): At 15.5V peak, 27kΩ gives 3.30V (within ESP32 ADC range)
- **R12/R13**: FB resistors for MP2359DJ. VOUT = 0.792V × (1 + 150k/47k) = 3.31V
- **BT134W-600E (Q1/Q2)**: 1A/600V SOT-223 TRIACs (FUXINSEMI C5380692, replaces original BTA204-600E and WeEn BT134W-600D)

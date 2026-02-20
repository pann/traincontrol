#!/usr/bin/env python3
"""Generate traincontrol-shield.kicad_sch from BOM + circuit description."""

import uuid, math, re
from pathlib import Path

KICAD_SYM = Path("/usr/share/kicad/symbols")
OUT = Path("/mnt/1510/home/pa/work/traincontrol/hw/traincontrol-shield/"
           "traincontrol-shield.kicad_sch")

# ── Helpers ──────────────────────────────────────────────────────────────────

def uid():
    return str(uuid.uuid4())

def extract_sym(lib: str, name: str) -> str:
    """Extract a top-level symbol block from a .kicad_sym file."""
    text = (KICAD_SYM / f"{lib}.kicad_sym").read_text()
    target = f'\t(symbol "{name}"'
    start = text.find(target)
    if start < 0:
        raise ValueError(f"{lib}:{name} not found")
    depth, i = 0, start
    while i < len(text):
        c = text[i]
        if c == '(':
            depth += 1
        elif c == ')':
            depth -= 1
            if depth == 0:
                return text[start:i+1]
        i += 1
    raise ValueError(f"Unbalanced parens in {lib}:{name}")

def pin_tip(cx, cy, rot_deg, px_sym, py_sym):
    """Pin tip in schematic coords (Y-down) given component position,
    rotation (deg CCW), and pin position in symbol space (Y-up)."""
    r = math.radians(rot_deg)
    rx = px_sym * math.cos(r) - py_sym * math.sin(r)
    ry = px_sym * math.sin(r) + py_sym * math.cos(r)
    return round(cx + rx, 3), round(cy - ry, 3)

def label_angle(px_sym, py_sym, rot=0):
    """Reasonable label angle for a pin at (px,py) in symbol space."""
    r = math.radians(rot)
    rx = px_sym * math.cos(r) - py_sym * math.sin(r)
    ry = px_sym * math.sin(r) + py_sym * math.cos(r)
    # In schematic (Y-down): tip is at (cx+rx, cy-ry)
    # Label should extend AWAY from component
    if abs(rx) >= abs(ry):
        return 0 if rx > 0 else 180
    else:
        return 270 if ry > 0 else 90  # ry>0 → tip below center → label goes down

# ── Pin positions (symbol space, Y-up) ───────────────────────────────────────
# Format: lib_id → {pin_number: (x, y)}

PINS = {
    "Device:R":                  {"1": (0, 3.81),  "2": (0, -3.81)},
    "Device:C":                  {"1": (0, 3.81),  "2": (0, -3.81)},
    "Device:C_Polarized":        {"1": (0, 3.81),  "2": (0, -3.81)},
    "Device:L":                  {"1": (0, 3.81),  "2": (0, -3.81)},
    "Device:D_Bridge_+A-A":      {"1": (7.62, 0),  "2": (0, -7.62),
                                   "3": (-7.62, 0), "4": (0, 7.62)},
    "Isolator:H11AA1":           {"1": (-7.62,  2.54), "2": (-7.62, -2.54),
                                   "3": (-5.08,  0),   "4": (7.62,  -2.54),
                                   "5": (7.62,   0),   "6": (7.62,   2.54)},
    "Relay_SolidState:MOC3010M": {"1": (-7.62,  2.54), "2": (-7.62, -2.54),
                                   "3": (-5.08,  0),   "4": (7.62,  -2.54),
                                   "5": (5.08,   0),   "6": (7.62,   2.54)},
    "74xGxx:74LVC1G86":          {"1": (-15.24, 2.54), "2": (-15.24, -2.54),
                                   "3": (0, -10.16),   "4": (12.7,    0),
                                   "5": (0,  10.16)},
    "74xGxx:74LVC2G08":          # Units 1&2 share same shape; unit 3 = power
                                 {"1": (-15.24, 2.54), "2": (-15.24, -2.54),
                                   "7": (12.7,   0),
                                   "5": (-15.24, 2.54), "6": (-15.24, -2.54),
                                   "3": (12.7,   0),
                                   "8": (0,  10.16),   "4": (0, -10.16)},
    "Triac_Thyristor:Generic_Triac_A1A2G":
                                 {"1": (0,  -3.81), "2": (0,  3.81),
                                  "3": (-3.81, -2.54)},
    "Connector_Generic:Conn_01x02":
                                 {"1": (-5.08, 0),  "2": (-5.08, -2.54)},
    "Connector_Generic:Conn_01x03":
                                 {"1": (-5.08, 2.54), "2": (-5.08, 0),
                                  "3": (-5.08, -2.54)},
    "Connector_Generic:Conn_01x22":
                                 {str(n): (-5.08, 25.4 - (n-1)*2.54)
                                  for n in range(1, 23)},
    "power:GND":                 {"1": (0, 0)},
    "power:+3V3":                {"1": (0, 0)},
    "power:PWR_FLAG":            {"1": (0, 0)},
    "Custom:MP2359DJ":           {"1": (-10.16,  2.54), "3": (-10.16, 0),
                                   "4": (0,  -10.16),   "5": (10.16,  0),
                                   "6": (10.16, -2.54), "2": (10.16,  2.54)},
}

# ── Custom MP2359DJ symbol ────────────────────────────────────────────────────

MP2359_SYM = r"""	(symbol "MP2359DJ"
		(exclude_from_sim no)
		(in_bom yes)
		(on_board yes)
		(property "Reference" "U"
			(at 6.35 2.54 0)
			(effects (font (size 1.27 1.27)))
		)
		(property "Value" "MP2359DJ"
			(at 6.35 0 0)
			(effects (font (size 1.27 1.27)))
		)
		(property "Footprint" ""
			(at 0 0 0)
			(effects (font (size 1.27 1.27)) (hide yes))
		)
		(property "Datasheet" "~"
			(at 0 0 0)
			(effects (font (size 1.27 1.27)) (hide yes))
		)
		(property "Description" "MPS buck regulator SOT-23-6"
			(at 0 0 0)
			(effects (font (size 1.27 1.27)) (hide yes))
		)
		(symbol "MP2359DJ_0_1"
			(rectangle (start -5.08 -7.62) (end 5.08 7.62)
				(stroke (width 0.254) (type default))
				(fill (type background))
			)
		)
		(symbol "MP2359DJ_1_1"
			(pin input line (at -10.16 2.54 0) (length 5.08)
				(name "EN" (effects (font (size 1.27 1.27))))
				(number "1" (effects (font (size 1.27 1.27))))
			)
			(pin input line (at -10.16 0 0) (length 5.08)
				(name "VIN" (effects (font (size 1.27 1.27))))
				(number "3" (effects (font (size 1.27 1.27))))
			)
			(pin power_in line (at 0 -10.16 90) (length 2.54)
				(name "GND" (effects (font (size 1.27 1.27))))
				(number "4" (effects (font (size 1.27 1.27))))
			)
			(pin output line (at 10.16 0 180) (length 5.08)
				(name "SW" (effects (font (size 1.27 1.27))))
				(number "5" (effects (font (size 1.27 1.27))))
			)
			(pin input line (at 10.16 -2.54 180) (length 5.08)
				(name "FB" (effects (font (size 1.27 1.27))))
				(number "6" (effects (font (size 1.27 1.27))))
			)
			(pin input line (at 10.16 2.54 180) (length 5.08)
				(name "BST" (effects (font (size 1.27 1.27))))
				(number "2" (effects (font (size 1.27 1.27))))
			)
		)
	)"""

# ── Component list ────────────────────────────────────────────────────────────
# (ref, lib_id, value, footprint, x, y, rot, unit, {pin: net|None})

R_FP   = "Resistor_SMD:R_0603_1608Metric"
C_FP   = "Capacitor_SMD:C_0603_1608Metric"
CP_FP  = "Capacitor_THT:CP_Radial_D8.0mm_P3.50mm"
L_FP   = "Inductor_SMD:L_4.0x4.0mm_Pad1.8x1.8mm"
DIP4   = "Diode_THT:Diode_Bridge_DIP-4_W7.62mm_P5.08mm"
DIP6   = "Package_DIP:DIP-6_W7.62mm"
SOT235 = "Package_TO_SOT_SMD:SOT-23-5"
SOT236 = "Package_TO_SOT_SMD:SOT-23-6"
SOT363 = "Package_TO_SOT_SMD:SOT-363_SC-88_MO-203"
SOT223 = "Package_TO_SOT_SMD:SOT-223-3_TabPin2"
TB2    = "TerminalBlock_Phoenix:PhoenixContact_MC_1,5_2-G-3.5_1x02_P3.50mm_Horizontal"
TB3    = "TerminalBlock_Phoenix:PhoenixContact_MC_1,5_3-G-3.5_1x03_P3.50mm_Horizontal"
HDR22  = "Connector_PinSocket_2.54mm:PinSocket_1x22_P2.54mm_Vertical"

COMPONENTS = [
    # ── Block 1: Power supply ─────────────────────────────────────────────
    # J3: 12 VAC screw terminal
    ("J3", "Connector_Generic:Conn_01x02", "12VAC_IN", TB2, 18, 30, 270, 1,
     {"1": "VAC_A", "2": "VAC_B"}),
    # D1: DB107 bridge rectifier (using D_Bridge_+A-A symbol)
    ("D1", "Device:D_Bridge_+A-A", "DB107", DIP4, 48, 30, 0, 1,
     {"1": "+RAIL", "2": "VAC_A", "3": "GND", "4": "VAC_B"}),
    # C1: 220µF/25V bulk cap
    ("C1", "Device:C_Polarized", "220uF/25V", CP_FP, 70, 30, 0, 1,
     {"1": "+RAIL", "2": "GND"}),
    # C5: 10µF buck input
    ("C5", "Device:C", "10uF", C_FP, 90, 50, 0, 1,
     {"1": "+RAIL", "2": "GND"}),
    # U1: MP2359DJ buck (EN tied to +RAIL via 0R — modelled as net label)
    ("U1", "Custom:MP2359DJ", "MP2359DJ", SOT236, 115, 30, 0, 1,
     {"1": "+RAIL",      # EN — tie high
      "3": "+RAIL",      # VIN
      "4": "GND",
      "5": "SW_3V3",
      "6": "FB_3V3",
      "2": "BST_3V3"}),
    # L1: 4.7µH
    ("L1", "Device:L", "4.7uH", L_FP, 145, 30, 0, 1,
     {"1": "+3V3", "2": "SW_3V3"}),
    # C2: 22µF/10V output
    ("C2", "Device:C", "22uF", C_FP, 158, 30, 0, 1,
     {"1": "+3V3", "2": "GND"}),
    # Feedback divider modelled as net label on FB pin (value sets 3.3V)
    # C3, C4: 100nF decoupling
    ("C3", "Device:C", "100nF", C_FP, 93, 65, 0, 1,
     {"1": "+3V3", "2": "GND"}),
    ("C4", "Device:C", "100nF", C_FP, 106, 65, 0, 1,
     {"1": "+3V3", "2": "GND"}),

    # ── Block 2: Zero-crossing detector ─────────────────────────────────
    ("R1", "Device:R", "220k", R_FP, 20, 115, 0, 1,
     {"1": "VAC_A", "2": "ZC_LED_A"}),
    ("U4", "Isolator:H11AA1", "H11AA1", DIP6, 48, 118, 0, 1,
     {"1": "ZC_LED_A", "2": "ZC_LED_K",
      "3": None,
      "4": "GND", "5": "ZERO_CROSS", "6": None}),
    ("R2", "Device:R", "10k", R_FP, 78, 118, 0, 1,
     {"1": "+3V3", "2": "ZERO_CROSS"}),

    # ── Block 3: HW XOR interlock ────────────────────────────────────────
    ("U5", "74xGxx:74LVC1G86", "74LVC1G86", SOT235, 185, 30, 0, 1,
     {"1": "MOTOR_FWD", "2": "MOTOR_REV",
      "5": "+3V3", "3": "GND", "4": "VALID"}),
    # U6 gate A: MOTOR_FWD AND VALID → GATE_FWD
    ("U6", "74xGxx:74LVC2G08", "74LVC2G08", SOT363, 223, 22, 0, 1,
     {"1": "MOTOR_FWD", "2": "VALID", "7": "GATE_FWD"}),
    # U6 gate B: MOTOR_REV AND VALID → GATE_REV
    ("U6", "74xGxx:74LVC2G08", "74LVC2G08", SOT363, 223, 52, 0, 2,
     {"5": "MOTOR_REV", "6": "VALID", "3": "GATE_REV"}),
    # U6 power unit
    ("U6", "74xGxx:74LVC2G08", "74LVC2G08", SOT363, 253, 37, 0, 3,
     {"8": "+3V3", "4": "GND"}),

    # ── Block 4: Opto-TRIAC drivers ──────────────────────────────────────
    # FWD path
    ("R3", "Device:R", "330", R_FP, 253, 80, 0, 1,
     {"1": "GATE_FWD", "2": "OPTO_FWD_A"}),
    ("U2", "Relay_SolidState:MOC3010M", "MOC3021", DIP6, 280, 80, 0, 1,
     {"1": "OPTO_FWD_A", "2": "GND",
      "3": None, "5": None,
      "6": "MT2_FWD", "4": "MT1_FWD"}),
    ("R5", "Device:R", "360", R_FP, 310, 80, 0, 1,
     {"1": "MT1_FWD", "2": "TRIAC_G1"}),
    # REV path
    ("R4", "Device:R", "330", R_FP, 253, 115, 0, 1,
     {"1": "GATE_REV", "2": "OPTO_REV_A"}),
    ("U3", "Relay_SolidState:MOC3010M", "MOC3021", DIP6, 280, 115, 0, 1,
     {"1": "OPTO_REV_A", "2": "GND",
      "3": None, "5": None,
      "6": "MT2_REV", "4": "MT1_REV"}),
    ("R6", "Device:R", "360", R_FP, 310, 115, 0, 1,
     {"1": "MT1_REV", "2": "TRIAC_G2"}),

    # ── Block 5: TRIACs + snubbers ───────────────────────────────────────
    # T1 (FWD): A1=MOTOR_A, A2=VAC_B, G=TRIAC_G1
    ("T1", "Triac_Thyristor:Generic_Triac_A1A2G", "BTA204-600E", SOT223,
     340, 78, 0, 1,
     {"1": "MOTOR_A", "2": "VAC_B", "3": "TRIAC_G1"}),
    # Snubber across T1
    ("R7", "Device:R", "100", R_FP, 340, 94, 90, 1,
     {"1": "VAC_B", "2": "SNUB1"}),
    ("C6", "Device:C", "100nF/50V", C_FP, 354, 94, 0, 1,
     {"1": "SNUB1", "2": "MOTOR_A"}),
    # T2 (REV): A1=MOTOR_B, A2=VAC_B, G=TRIAC_G2
    ("T2", "Triac_Thyristor:Generic_Triac_A1A2G", "BTA204-600E", SOT223,
     340, 115, 0, 1,
     {"1": "MOTOR_B", "2": "VAC_B", "3": "TRIAC_G2"}),
    # Snubber across T2
    ("R8", "Device:R", "100", R_FP, 340, 131, 90, 1,
     {"1": "VAC_B", "2": "SNUB2"}),
    ("C7", "Device:C", "100nF/50V", C_FP, 354, 131, 0, 1,
     {"1": "SNUB2", "2": "MOTOR_B"}),
    # J4: motor output (VAC_A, MOTOR_A, MOTOR_B)
    ("J4", "Connector_Generic:Conn_01x03", "MOTOR_OUT", TB3, 380, 95, 270, 1,
     {"1": "VAC_A", "2": "MOTOR_A", "3": "MOTOR_B"}),

    # ── Block 6: Rail voltage divider ────────────────────────────────────
    ("R9",  "Device:R", "100k", R_FP, 330, 148, 0, 1,
     {"1": "+RAIL", "2": "RAIL_SENSE"}),
    ("R10", "Device:R", "33k",  R_FP, 330, 168, 0, 1,
     {"1": "RAIL_SENSE", "2": "GND"}),

    # ── Block 7: DevKitC headers ─────────────────────────────────────────
    # J1 – left header (J1 on DevKitC). Key pins:
    #   1=GND, 2=3.3V, GPIO4=MOTOR_FWD, GPIO5=MOTOR_REV, GPIO6=ZERO_CROSS
    #   Approximate mapping: pin3=GPIO4, pin4=GPIO5, pin5=GPIO6
    ("J1", "Connector_Generic:Conn_01x22", "ESP32_J1", HDR22, 82, 225, 0, 1,
     {"1": "GND", "2": "+3V3",
      "3": "MOTOR_FWD", "4": "MOTOR_REV", "5": "ZERO_CROSS",
      **{str(n): None for n in range(6, 23)}}),
    # J2 – right header (J3 on DevKitC). Key pins:
    #   1=GND, 2=3.3V, GPIO2=RAIL_SENSE
    ("J2", "Connector_Generic:Conn_01x22", "ESP32_J2", HDR22, 160, 225, 0, 1,
     {"1": "GND", "2": "+3V3",
      "3": "RAIL_SENSE",
      **{str(n): None for n in range(4, 23)}}),
]

# ── Schematic generation ──────────────────────────────────────────────────────

def make_prop(name, value, x, y, angle=0, hide=False):
    hide_str = "\n      (effects (font (size 1.27 1.27)) (hide yes))" if hide \
               else "\n      (effects (font (size 1.27 1.27)))"
    return (f'  (property "{name}" "{value}" (at {x:.3f} {y:.3f} {angle})'
            f'{hide_str}\n  )')

def make_component(ref, lib_id, value, fp, x, y, rot, unit, pins_dict):
    """Generate a symbol instance S-expression."""
    pins_sym = PINS.get(lib_id, {})
    lines = [f'  (symbol (lib_id "{lib_id}") (at {x:.3f} {y:.3f} {rot})'
             f' (unit {unit})\n'
             f'    (exclude_from_sim no) (in_bom yes) (on_board yes)\n'
             f'    (uuid "{uid()}")']
    # Properties
    rx, ry = pin_tip(x, y, rot, 2.54, 2.54)  # offset for ref label
    lines.append(f'    (property "Reference" "{ref}" (at {rx:.3f} {ry:.3f} {rot})'
                 f'\n      (effects (font (size 1.27 1.27)))\n    )')
    lines.append(f'    (property "Value" "{value}" (at {x:.3f} {y:.3f} {rot})'
                 f'\n      (effects (font (size 1.27 1.27)))\n    )')
    lines.append(f'    (property "Footprint" "{fp}" (at 0 0 0)'
                 f'\n      (effects (font (size 1.27 1.27)) (hide yes))\n    )')
    lines.append(f'    (property "Datasheet" "~" (at 0 0 0)'
                 f'\n      (effects (font (size 1.27 1.27)) (hide yes))\n    )')
    # Pin UUIDs
    for pnum in pins_dict:
        lines.append(f'    (pin "{pnum}" (uuid "{uid()}"))')
    lines.append('  )')
    return '\n'.join(lines)

def make_label(net, x, y, angle=0):
    return (f'  (label "{net}" (at {x:.3f} {y:.3f} {angle})\n'
            f'    (effects (font (size 1.27 1.27)) (justify left))\n'
            f'    (uuid "{uid()}")\n  )')

def make_power(sym_name, x, y, ref_num):
    """Power symbol instance."""
    return (f'  (symbol (lib_id "power:{sym_name}") (at {x:.3f} {y:.3f} 0)'
            f' (unit 1)\n'
            f'    (exclude_from_sim no) (in_bom yes) (on_board yes)\n'
            f'    (uuid "{uid()}")\n'
            f'    (property "Reference" "#PWR{ref_num:03d}"'
            f' (at {x:.3f} {y:.3f} 0)\n'
            f'      (effects (font (size 1.27 1.27)) (hide yes))\n    )\n'
            f'    (property "Value" "{sym_name}" (at {x:.3f} {y:.3f} 0)\n'
            f'      (effects (font (size 1.27 1.27)))\n    )\n'
            f'    (property "Footprint" "" (at 0 0 0)\n'
            f'      (effects (font (size 1.27 1.27)) (hide yes))\n    )\n'
            f'    (pin "1" (uuid "{uid()}"))\n'
            f'  )')

def make_noconnect(x, y):
    return f'  (no_connect (at {x:.3f} {y:.3f}) (uuid "{uid()}"))'

def main():
    # ── 1. Collect all needed lib_id's ───────────────────────────────────
    needed_libs = set()
    for comp in COMPONENTS:
        needed_libs.add(comp[1])  # lib_id is index 1

    lib_map = {
        "Device:R":                  ("Device", "R"),
        "Device:C":                  ("Device", "C"),
        "Device:C_Polarized":        ("Device", "C_Polarized"),
        "Device:L":                  ("Device", "L"),
        "Device:D_Bridge_+A-A":      ("Device", "D_Bridge_+A-A"),
        "Isolator:H11AA1":           ("Isolator", "H11AA1"),
        "Relay_SolidState:MOC3010M": ("Relay_SolidState", "MOC3010M"),
        "74xGxx:74LVC1G86":          ("74xGxx", "74LVC1G86"),
        "74xGxx:74LVC2G08":          ("74xGxx", "74LVC2G08"),
        "Triac_Thyristor:Generic_Triac_A1A2G":
                                     ("Triac_Thyristor", "Generic_Triac_A1A2G"),
        "Connector_Generic:Conn_01x02": ("Connector_Generic", "Conn_01x02"),
        "Connector_Generic:Conn_01x03": ("Connector_Generic", "Conn_01x03"),
        "Connector_Generic:Conn_01x22": ("Connector_Generic", "Conn_01x22"),
        "power:GND":                 ("power", "GND"),
        "power:+3V3":                ("power", "+3V3"),
        "power:PWR_FLAG":            ("power", "PWR_FLAG"),
    }

    # ── 2. Build lib_symbols section ─────────────────────────────────────
    lib_syms_parts = [MP2359_SYM]
    seen = set()
    for lib_id in needed_libs:
        if lib_id == "Custom:MP2359DJ":
            continue
        if lib_id not in seen and lib_id in lib_map:
            lib, name = lib_map[lib_id]
            lib_syms_parts.append(extract_sym(lib, name))
            seen.add(lib_id)
    # Always include power symbols needed
    for lib_id in ["power:GND", "power:+3V3", "power:PWR_FLAG"]:
        if lib_id not in seen:
            lib, name = lib_map[lib_id]
            lib_syms_parts.append(extract_sym(lib, name))
            seen.add(lib_id)

    lib_syms = "(lib_symbols\n" + "\n".join(lib_syms_parts) + "\n)"

    # ── 3. Place components, labels, power, no-connects ──────────────────
    comp_blocks = []
    label_blocks = []
    nc_blocks = []
    pwr_ctr = [1]

    POWER_NETS = {"+RAIL", "+3V3", "GND"}  # handled by power symbols

    for comp in COMPONENTS:
        ref, lib_id, value, fp, cx, cy, rot, unit, pin_nets = comp
        comp_blocks.append(make_component(ref, lib_id, value, fp,
                                          cx, cy, rot, unit, pin_nets))
        pins_sym = PINS.get(lib_id, {})

        for pnum, net in pin_nets.items():
            px, py = pins_sym.get(pnum, (0, 0))
            tx, ty = pin_tip(cx, cy, rot, px, py)

            if net is None:
                nc_blocks.append(make_noconnect(tx, ty))
            elif net == "GND":
                label_blocks.append(make_power("GND", tx, ty, pwr_ctr[0]))
                pwr_ctr[0] += 1
            elif net == "+3V3":
                label_blocks.append(make_power("+3V3", tx, ty, pwr_ctr[0]))
                pwr_ctr[0] += 1
            elif net == "+RAIL":
                # DC rail: use a net label (not a standard power symbol)
                ang = label_angle(px, py, rot)
                label_blocks.append(make_label(net, tx, ty, ang))
            else:
                ang = label_angle(px, py, rot)
                label_blocks.append(make_label(net, tx, ty, ang))

    # PWR_FLAG on main power rails (ERC requirement)
    label_blocks.append(make_power("PWR_FLAG", 20, 145, pwr_ctr[0]))
    pwr_ctr[0] += 1
    label_blocks.append(make_power("PWR_FLAG", 33, 145, pwr_ctr[0]))

    # ── 4. Assemble the schematic ─────────────────────────────────────────
    parts = [
        f'(kicad_sch (version 20230121) (generator "eeschema")',
        f'  (uuid "{uid()}")',
        '  (paper "A2")',
        '',
        '  ' + lib_syms,
        '',
    ]
    parts.extend(comp_blocks)
    parts.extend(label_blocks)
    parts.extend(nc_blocks)
    parts.append('  (sheet_instances')
    parts.append('    (path "/" (page "1"))')
    parts.append('  )')
    parts.append(')')

    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text('\n'.join(parts) + '\n')
    print(f"✅ Schematic written to {OUT}")
    print(f"   Components : {len(COMPONENTS)}")
    print(f"   Labels     : {len(label_blocks)}")
    print(f"   No-connects: {len(nc_blocks)}")

if __name__ == "__main__":
    main()

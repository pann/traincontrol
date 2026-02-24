#!/usr/bin/env python3
"""Generator for power-supply.kicad_sch (Phase 3, Sheet 1)"""
import uuid, re

PROJ = "traincontrol-shield"
ROOT_UUID = "12cc2a67-9b1c-41e4-828f-9c2a8df6bb35"
SHEET_UUID = "26688641-94a2-4c54-b660-2bb712a21975"   # power-supply.kicad_sch file UUID
INST_UUID  = "a7305cff-bbaf-4846-983d-83b0ea81fbce"   # sheet instance UUID in top-level

LOCAL_LIB  = "../libs/traincontrol-shield.kicad_sym"
DEVICE_LIB = "/usr/share/kicad/symbols/Device.kicad_sym"
POWER_LIB  = "/usr/share/kicad/symbols/power.kicad_sym"
OUTFILE    = "../power-supply.kicad_sch"

def u():
    return str(uuid.uuid4())

# ---- Symbol extraction helpers ----

def extract_sym(lib_path, sym_name):
    with open(lib_path) as f:
        content = f.read()
    target = f'(symbol "{sym_name}"'
    start = content.find(target)
    if start == -1:
        raise ValueError(f"Symbol '{sym_name}' not found in {lib_path}")
    depth, i = 0, start
    while i < len(content):
        if content[i] == '(': depth += 1
        elif content[i] == ')':
            depth -= 1
            if depth == 0: break
        i += 1
    return content[start:i+1]

def upgrade_for_embed(block):
    """Convert KiCad 6 symbol format to KiCad 9 for embedding in a schematic.

    Handles multi-line properties with (id N), stroke colors, and bare 'hide'.
    Safe to call on KiCad 9 format blocks (transformations are no-ops).
    """
    # 1. Remove (id N) from multi-line KiCad 6 properties
    output, i = [], 0
    while i < len(block):
        if block[i:].startswith('(property\n'):
            depth, j = 0, i
            while j < len(block):
                if block[j] == '(':   depth += 1
                elif block[j] == ')':
                    depth -= 1
                    if depth == 0: break
                j += 1
            prop = block[i:j+1]
            inner = prop[len('(property'):prop.rfind(')')].strip()
            inner = re.sub(r'\(id \d+\)\s*', '', inner)
            qm = re.match(r'"((?:[^"\\]|\\.)*)"\s+"((?:[^"\\]|\\.)*)"(.*)',
                          inner, re.DOTALL)
            if qm:
                name, val, attrs = qm.group(1), qm.group(2), qm.group(3).strip()
                output.append(f'(property "{name}" "{val}"\n      {attrs}\n    )')
            else:
                output.append(re.sub(r'\s*\(id \d+\)', '', prop))
            i = j + 1
        else:
            output.append(block[i])
            i += 1
    block = ''.join(output)
    # 2. Remove (color R G B A) from strokes
    block = re.sub(
        r'\(stroke\s+\(width\s+([^)]+)\)\s+\(type\s+([^)]+)\)\s+\(color\s+[\d.\s]+\)\s*\)',
        r'(stroke (width \1) (type \2))', block)
    # 3. Fix effects: trailing space + bare 'hide' token
    block = re.sub(
        r'\(effects\s+\(font\s+\(size\s+([\d.]+\s+[\d.]+)\)\s*\)\s+hide\)',
        r'(effects (font (size \1)) (hide yes))', block)
    block = re.sub(
        r'\(effects\s+\(font\s+\(size\s+([\d.]+\s+[\d.]+)\)\s*\)\s*\)',
        r'(effects (font (size \1)))', block)
    return block


def extract_subsymbols(block, sym_name):
    """Extract _N_M sub-symbol blocks from a symbol block."""
    subsyms, i = [], 0
    target_prefix = f'(symbol "{sym_name}_'
    while i < len(block):
        if block[i:].startswith(target_prefix):
            depth, j = 0, i
            while j < len(block):
                if block[j] == '(':   depth += 1
                elif block[j] == ')':
                    depth -= 1
                    if depth == 0: break
                j += 1
            subsyms.append(block[i:j+1])
            i = j + 1
        else:
            i += 1
    return subsyms


def lib_sym_entry(lib_path, sym_name, lib_prefix):
    """Get lib_symbols entry with fully-qualified name (lib_prefix:sym_name).

    Sub-symbol unit names are kept WITHOUT the library prefix, as required by
    KiCad 9's schematic parser (m_symbolName check in parseLibSymbol).
    Derived symbols (extends) are flattened by copying graphics from the base.
    """
    block = extract_sym(lib_path, sym_name)
    fqn = f"{lib_prefix}:{sym_name}"
    # Flatten derived symbols: copy graphics from base, remove (extends ...)
    extends_match = re.search(r'\(extends "([^"]+)"\)', block)
    if extends_match:
        base_name = extends_match.group(1)
        base_block = extract_sym(lib_path, base_name)
        # Extract and rename sub-symbols (base_name_N_M → sym_name_N_M)
        base_subsyms = extract_subsymbols(base_block, base_name)
        renamed = [ss.replace(f'(symbol "{base_name}_', f'(symbol "{sym_name}_', 1)
                   for ss in base_subsyms]
        block = re.sub(r'\s*\(extends "[^"]+"\)', '', block)
        block = block.rstrip()
        block = block[:-1].rstrip() + '\n' + '\n'.join(renamed) + '\n)'
    # Rename outer symbol only; sub-symbol names stay bare (e.g. "DB107_0_1")
    block = block.replace(f'(symbol "{sym_name}"', f'(symbol "{fqn}"', 1)
    # Convert KiCad 6 → KiCad 9 format inline (no-op for KiCad 9 libraries)
    block = upgrade_for_embed(block)
    # Add (exclude_from_sim no) before (in_bom yes) if missing
    if '(exclude_from_sim' not in block:
        block = block.replace('(in_bom yes)', '(exclude_from_sim no)\n  (in_bom yes)', 1)
    # Add (embedded_fonts no) before final closing paren if missing
    if '(embedded_fonts no)' not in block:
        block = block.rstrip()
        block = block[:-1].rstrip() + '\n  (embedded_fonts no)\n)'
    return block

# ---- Schematic element generators ----

STUB = 7.62  # wire stub length (mm)

def placed_sym(lib_id, x, y, angle, ref, value, footprint, pin_nums, unit=1,
               in_bom="yes", exclude_sim="no"):
    path = f"/{ROOT_UUID}/{INST_UUID}"
    prop_ref   = f'\t\t(property "Reference" "{ref}"\n\t\t\t(at {x} {y-4} 0)\n\t\t\t(effects (font (size 1.27 1.27)))\n\t\t)'
    prop_val   = f'\t\t(property "Value" "{value}"\n\t\t\t(at {x} {y+4} 0)\n\t\t\t(effects (font (size 1.27 1.27)))\n\t\t)'
    prop_fp    = f'\t\t(property "Footprint" "{footprint}"\n\t\t\t(at {x} {y} 0)\n\t\t\t(effects (font (size 1.27 1.27)) (hide yes))\n\t\t)'
    prop_ds    = f'\t\t(property "Datasheet" "~"\n\t\t\t(at {x} {y} 0)\n\t\t\t(effects (font (size 1.27 1.27)) (hide yes))\n\t\t)'
    pins_s = "\n".join(f'\t\t(pin "{p}" (uuid "{u()}"))' for p in pin_nums)
    inst = (f'\t\t(instances\n\t\t\t(project "{PROJ}"\n'
            f'\t\t\t\t(path "{path}"\n'
            f'\t\t\t\t\t(reference "{ref}")\n'
            f'\t\t\t\t\t(unit {unit})\n'
            f'\t\t\t\t)\n\t\t\t)\n\t\t)')
    return (f'\t(symbol\n'
            f'\t\t(lib_id "{lib_id}")\n'
            f'\t\t(at {x} {y} {angle})\n'
            f'\t\t(unit {unit})\n'
            f'\t\t(exclude_from_sim {exclude_sim})\n'
            f'\t\t(in_bom {in_bom})\n'
            f'\t\t(on_board yes)\n'
            f'\t\t(dnp no)\n'
            f'\t\t(uuid "{u()}")\n'
            f'{prop_ref}\n{prop_val}\n{prop_fp}\n{prop_ds}\n'
            f'{pins_s}\n{inst}\n\t)')

def power_sym(net_name, x, y, pwr_ref, angle=0):
    path = f"/{ROOT_UUID}/{INST_UUID}"
    lib_id = f"power:{net_name}"
    if net_name == "PWR_FLAG":
        sx, sy = x, y
    elif "GND" in net_name:
        sx, sy = x, y + STUB
    else:
        sx, sy = x, y - STUB
    sym = (f'\t(symbol\n'
           f'\t\t(lib_id "{lib_id}")\n'
           f'\t\t(at {sx} {sy} {angle})\n'
           f'\t\t(unit 1)\n'
           f'\t\t(exclude_from_sim no)\n'
           f'\t\t(in_bom no)\n'
           f'\t\t(on_board yes)\n'
           f'\t\t(dnp no)\n'
           f'\t\t(uuid "{u()}")\n'
           f'\t\t(property "Reference" "{pwr_ref}"\n'
           f'\t\t\t(at {sx} {sy-2} 0)\n'
           f'\t\t\t(effects (font (size 1.27 1.27)) (hide yes))\n'
           f'\t\t)\n'
           f'\t\t(property "Value" "{net_name}"\n'
           f'\t\t\t(at {sx} {sy+2} 0)\n'
           f'\t\t\t(effects (font (size 1.27 1.27)))\n'
           f'\t\t)\n'
           f'\t\t(property "Footprint" ""\n'
           f'\t\t\t(at {sx} {sy} 0)\n'
           f'\t\t\t(effects (font (size 1.27 1.27)) (hide yes))\n'
           f'\t\t)\n'
           f'\t\t(property "Datasheet" ""\n'
           f'\t\t\t(at {sx} {sy} 0)\n'
           f'\t\t\t(effects (font (size 1.27 1.27)) (hide yes))\n'
           f'\t\t)\n'
           f'\t\t(pin "1" (uuid "{u()}"))\n'
           f'\t\t(instances\n'
           f'\t\t\t(project "{PROJ}"\n'
           f'\t\t\t\t(path "{path}"\n'
           f'\t\t\t\t\t(reference "{pwr_ref}")\n'
           f'\t\t\t\t\t(unit 1)\n'
           f'\t\t\t\t)\n\t\t\t)\n\t\t)\n\t)')
    if net_name == "PWR_FLAG":
        return sym
    return wire(x, y, sx, sy) + '\n' + sym

def wire(x1, y1, x2, y2):
    return (f'\t(wire\n'
            f'\t\t(pts (xy {x1} {y1}) (xy {x2} {y2}))\n'
            f'\t\t(stroke (width 0) (type solid))\n'
            f'\t\t(uuid "{u()}")\n\t)')

def net_label(name, x, y, angle=0):
    just = "right" if angle == 180 else "left"
    return (f'\t(label "{name}"\n'
            f'\t\t(at {x} {y} {angle})\n'
            f'\t\t(fields_autoplaced yes)\n'
            f'\t\t(effects (font (size 1.27 1.27)) (justify {just} bottom))\n'
            f'\t\t(uuid "{u()}")\n\t)')

def global_label(name, x, y, angle, shape="input"):
    just = "right" if angle == 180 else "left"
    dx = STUB if angle == 180 else -STUB
    lx = x + dx
    label = (f'\t(global_label "{name}"\n'
             f'\t\t(shape {shape})\n'
             f'\t\t(at {lx} {y} {angle})\n'
             f'\t\t(fields_autoplaced yes)\n'
             f'\t\t(effects (font (size 1.27 1.27)) (justify {just} bottom))\n'
             f'\t\t(uuid "{u()}")\n'
             f'\t\t(property "Intersheetrefs" "${{INTERSHEET_REFS}}"\n'
             f'\t\t\t(at {lx} {y} 0)\n'
             f'\t\t\t(effects (font (size 1.27 1.27)) (hide yes))\n'
             f'\t\t)\n\t)')
    return wire(min(x, lx), y, max(x, lx), y) + '\n' + label

def hlabel(name, x, y, angle, shape="input"):
    """Hierarchical label — creates a port on the parent sheet's sub-sheet symbol.

    x   = IC/component pin tip (inner wire end).
    lx  = label connection point (outer stub end), offset by ±STUB from x.
    angle=180 → lx is LEFT  of x (left-side inputs);  label body extends further left.
    angle=0   → lx is RIGHT of x (right-side outputs); label body extends further right.
    Wire runs from x (pin) to lx (label), label sits at lx.
    """
    just = "right" if angle == 180 else "left"
    dx = -STUB if angle == 180 else STUB
    lx = x + dx
    label = (f'\t(hierarchical_label "{name}"\n'
             f'\t\t(shape {shape})\n'
             f'\t\t(at {lx} {y} {angle})\n'
             f'\t\t(fields_autoplaced yes)\n'
             f'\t\t(effects (font (size 1.27 1.27)) (justify {just} bottom))\n'
             f'\t\t(uuid "{u()}")\n'
             f'\t\t(property "Intersheetrefs" "${{INTERSHEET_REFS}}"\n'
             f'\t\t\t(at {lx} {y} 0)\n'
             f'\t\t\t(effects (font (size 1.27 1.27)) (hide yes))\n'
             f'\t\t)\n\t)')
    return wire(min(x, lx), y, max(x, lx), y) + '\n' + label

def junction(x, y):
    return (f'\t(junction\n'
            f'\t\t(at {x} {y})\n'
            f'\t\t(diameter 0)\n'
            f'\t\t(color 0 0 0 0)\n'
            f'\t\t(uuid "{u()}")\n\t)')

# ---- lib_symbols section ----
lib_entries = []
lib_entries.append(lib_sym_entry(LOCAL_LIB,  "DB107",        "traincontrol-shield"))
lib_entries.append(lib_sym_entry(LOCAL_LIB,  "MP2359DJ-LF-Z","traincontrol-shield"))
lib_entries.append(lib_sym_entry(DEVICE_LIB, "C",            "Device"))
lib_entries.append(lib_sym_entry(DEVICE_LIB, "C_Polarized",  "Device"))
lib_entries.append(lib_sym_entry(DEVICE_LIB, "L",            "Device"))
lib_entries.append(lib_sym_entry(DEVICE_LIB, "R",            "Device"))
lib_entries.append(lib_sym_entry(POWER_LIB,  "GND",          "power"))
lib_entries.append(lib_sym_entry(POWER_LIB,  "+15V",         "power"))
lib_entries.append(lib_sym_entry(POWER_LIB,  "+3V3",         "power"))
lib_entries.append(lib_sym_entry(POWER_LIB,  "PWR_FLAG",     "power"))

lib_section_body = "\n".join(
    "\n".join("\t\t" + line for line in e.split("\n")) for e in lib_entries
)
lib_section = f"\t(lib_symbols\n{lib_section_body}\n\t)"

# ---- Coordinate layout ----
# All coordinates in mm.  Horizontal spacing ~2x vs original.
#
# Power supply signal flow (left → right):
#   AC_IN/AC_RTN  →  D1(DB107)  →  +15V bus  →  C1,C3,U1  →  +3V3 bus  →  L1,C2
#   FB divider: R12,R13 above U1 (same x as U1 FB pin)
#   BST cap: C9 above U1 (left of U1 BST pin)
#
# Component centres:
#   D1  ( 50.80, 68.58)   DB107 bridge rectifier  (anchor)
#   C1  ( 96.52, 68.58)   220µF bulk cap  (angle=180 → + at top)
#   C3  (121.92, 68.58)   100nF U1 IN bypass  (angle=180)
#   U1  (167.64, 68.58)   MP2359DJ buck IC
#   C9  (149.86, 58.42)   100nF BST cap  (angle=0)
#   L1  (190.50, 68.58)   4.7µH inductor  (angle=0)
#   C2  (215.90, 68.58)   22µF output cap  (angle=180)
#   R12 (157.48, 43.18)   150kΩ FB top  (angle=0)
#   R13 (157.48, 50.80)   47kΩ  FB bottom  (angle=0)
#
# Passive pin offsets (angle=0): Pin2 at y-3.81 (top), Pin1 at y+3.81 (bottom)
# With angle=180 rotation:       Pin1 at y-3.81 (top), Pin2 at y+3.81 (bottom)
#
# DB107 pin offsets: Pin1(+) at (+10.16,-2.54), Pin2(-) at (+10.16,+2.54)
#                    Pin3(AC_IN) at (-10.16,+2.54), Pin4(AC_RTN) at (-10.16,-2.54)
# MP2359DJ offsets:  BST(1) at (-10.16,+2.54), GND(2) at (-10.16,0)
#                    FB(3)  at (-10.16,-2.54),  EN(4)  at (+10.16,-2.54)
#                    IN(5)  at (+10.16,0),       SW(6)  at (+10.16,+2.54)

elements = []

# ── Component instances ──
elements.append(placed_sym(
    "traincontrol-shield:DB107", 50.80, 68.58, 0, "D1", "DB107",
    "traincontrol-shield:DIO-BG-TH_DF", ["1","2","3","4"]
))
elements.append(placed_sym(
    "Device:C_Polarized", 96.52, 68.58, 180, "C1", "220µF",
    "Capacitor_THT:CP_Radial_D8.0mm_P3.50mm", ["1","2"]
))
elements.append(placed_sym(
    "Device:C", 121.92, 68.58, 180, "C3", "100nF",
    "Capacitor_SMD:C_0603_1608Metric", ["1","2"]
))
elements.append(placed_sym(
    "traincontrol-shield:MP2359DJ-LF-Z", 167.64, 68.58, 0, "U1", "MP2359DJ",
    "traincontrol-shield:SOT-23-6_L2.9-W1.6-P0.95-LS2.8-BR", ["1","2","3","4","5","6"]
))
elements.append(placed_sym(
    "Device:C", 149.86, 58.42, 0, "C9", "100nF",
    "Capacitor_SMD:C_0603_1608Metric", ["1","2"]
))
elements.append(placed_sym(
    "Device:L", 190.50, 68.58, 0, "L1", "4.7µH",
    "Inductor_SMD:L_0805_2012Metric", ["1","2"]
))
elements.append(placed_sym(
    "Device:C", 215.90, 68.58, 180, "C2", "22µF",
    "Capacitor_SMD:C_0603_1608Metric", ["1","2"]
))
elements.append(placed_sym(
    "Device:R", 157.48, 43.18, 0, "R12", "150kΩ",
    "Resistor_SMD:R_0603_1608Metric", ["1","2"]
))
elements.append(placed_sym(
    "Device:R", 157.48, 50.80, 0, "R13", "47kΩ",
    "Resistor_SMD:R_0603_1608Metric", ["1","2"]
))

# ── Power symbols ──
pwr_n = [201]
def P(net, x, y, angle=0):
    ref = f"#PWR{pwr_n[0]:04d}"
    pwr_n[0] += 1
    elements.append(power_sym(net, x, y, ref, angle))

# D1: pin1(+DC)@(60.96,66.04), pin2(-DC)@(60.96,71.12)
P("+15V", 60.96, 66.04)
P("GND",  60.96, 71.12)
# C1 angle=180: pin1(+,top)@(96.52,64.77), pin2(-,btm)@(96.52,72.39)
P("+15V", 96.52, 64.77)
P("GND",  96.52, 72.39)
# C3 angle=180: pin1(+,top)@(121.92,64.77), pin2(-,btm)@(121.92,72.39)
P("+15V", 121.92, 64.77)
P("GND",  121.92, 72.39)
# U1 GND pin2@(157.48,68.58): stub left to avoid BST pin
elements.append(wire(157.48, 68.58, 154.94, 68.58))
P("GND",  154.94, 68.58)
# U1 EN pin4@(177.80,66.04): tie to VIN (+15V)
P("+15V", 177.80, 66.04)
# U1 IN pin5@(177.80,68.58): stub right to avoid EN pin above
elements.append(wire(177.80, 68.58, 180.34, 68.58))
P("+15V", 180.34, 68.58)
# L1 angle=0: pin2(top)@(190.50,64.77) → +3V3
P("+3V3", 190.50, 64.77)
# C2 angle=180: pin1(+,top)@(215.90,64.77), pin2(-,btm)@(215.90,72.39)
P("+3V3", 215.90, 64.77)
P("GND",  215.90, 72.39)
# R12 pin2(top)@(157.48,39.37) → +3V3
P("+3V3", 157.48, 39.37)
# R13 pin1(btm)@(157.48,54.61) → GND
P("GND",  157.48, 54.61)
# PWR_FLAG on +15V and +3V3 rails
P("PWR_FLAG", 63.50, 66.04)
P("PWR_FLAG", 218.44, 64.77)

# ── Wires ──
elements.append(wire(60.96, 66.04, 63.50, 66.04))     # +15V PWR_FLAG stub
elements.append(wire(215.90, 64.77, 218.44, 64.77))   # +3V3 PWR_FLAG stub

# ── Local net labels ──
# VBST: U1 BST pin1@(157.48,71.12) ↔ C9 pin1(btm)@(149.86,62.23)
elements.append(net_label("VBST", 157.48, 71.12))
elements.append(net_label("VBST", 149.86, 62.23))
# VSW: U1 SW pin6@(177.80,71.12) ↔ C9 pin2(top)@(149.86,54.61) ↔ L1 pin1(btm)@(190.50,72.39)
elements.append(net_label("VSW", 177.80, 71.12))
elements.append(net_label("VSW", 149.86, 54.61))
elements.append(net_label("VSW", 190.50, 72.39))
# VFB: U1 FB pin3@(157.48,66.04) ↔ R12/R13 node@(157.48,46.99)
# R12 pin1(btm)=43.18+3.81=46.99, R13 pin2(top)=50.80-3.81=46.99 → shared
elements.append(net_label("VFB", 157.48, 66.04))
elements.append(net_label("VFB", 157.48, 46.99))

# ── Hierarchical labels ──
# D1 pin3(AC_IN)@(40.64,71.12), pin4(AC_RTN)@(40.64,66.04)
elements.append(hlabel("AC_IN",  40.64, 71.12, 180, "input"))   # D1 pin3
elements.append(hlabel("AC_RTN", 40.64, 66.04, 180, "input"))   # D1 pin4

# ── Junctions ──
# R12 pin1 and R13 pin2 both at (157.48, 46.99)
elements.append(junction(157.48, 46.99))

# ── Sheet instances ──
sheet_inst = (f'\t(sheet_instances\n'
              f'\t\t(path "/{ROOT_UUID}/{INST_UUID}"\n'
              f'\t\t\t(page "2")\n'
              f'\t\t)\n\t)')

# ── Assemble ──
header = f"""(kicad_sch
\t(version 20250114)
\t(generator "eeschema")
\t(generator_version "9.0")
\t(uuid "{SHEET_UUID}")
\t(paper "A4")
\t(title_block
\t\t(title "Power Supply")
\t\t(date "2026-02-21")
\t\t(rev "1")
\t\t(company "")
\t)
{lib_section}"""

body = "\n".join(elements)
result = header + "\n" + body + "\n" + sheet_inst + "\n)"

import os
script_dir = os.path.dirname(os.path.abspath(__file__))
outpath = os.path.join(script_dir, OUTFILE)
with open(outpath, "w") as f:
    f.write(result)
print(f"Written {len(result)} bytes → {outpath}")

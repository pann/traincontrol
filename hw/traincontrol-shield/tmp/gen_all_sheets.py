#!/usr/bin/env python3
"""Generator for power-supply.kicad_sch sub-sheets 2–7 (Phase 3)

Coordinate conventions (all in mm):
  - schematic_y = component_y + library_pin_y  (no Y-flip, both Y-down)
  - For angle=90 rotation: (x,y) -> (y, -x) in schematic pin offset
  - For angle=180 rotation: (x,y) -> (-x, -y) in schematic pin offset

Sheet instance UUIDs (pre-generated, must match top-level sheet):
  power-supply   a7305cff-bbaf-4846-983d-83b0ea81fbce
  zero-crossing  94283aa8-ba3a-413b-ba4f-3b3274da13e8
  xor-interlock  e0f8704d-1e30-470b-9f75-e6a1f31638d7
  opto-triac-drive 1155aa41-bfef-4df9-96ba-c8e7c9de0aa7
  triacs-snubbers  d4fe8f86-314f-434b-af4a-2b112cc8638c
  rail-voltage-sense a8cf3e0f-6d5f-4a60-9682-8cd8e6228106
  mcu            8c6fba78-5fd4-419a-8456-de6c668aaabc
"""
import uuid, re, os

ROOT_UUID = "12cc2a67-9b1c-41e4-828f-9c2a8df6bb35"
PROJ      = "traincontrol-shield"
BASE      = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")

LOCAL_LIB  = os.path.join(BASE, "libs/traincontrol-shield.kicad_sym")
DEVICE_LIB = "/usr/share/kicad/symbols/Device.kicad_sym"
POWER_LIB  = "/usr/share/kicad/symbols/power.kicad_sym"
ISOLAT_LIB = "/usr/share/kicad/symbols/Isolator.kicad_sym"
LOGIC_LIB  = "/usr/share/kicad/symbols/74xGxx.kicad_sym"
RELAY_LIB  = "/usr/share/kicad/symbols/Relay_SolidState.kicad_sym"
CONN_LIB   = "/usr/share/kicad/symbols/Connector_Generic.kicad_sym"

# ─────────────────────── helpers ───────────────────────────────────────

def u():
    return str(uuid.uuid4())

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

# ─────────────────────── schematic elements ────────────────────────────

STUB = 7.62  # wire stub length (mm)

def placed_sym(inst_uuid, lib_id, x, y, angle, ref, value, footprint, pin_nums,
               unit=1, in_bom="yes", exclude_sim="no", mirror=None):
    path = f"/{ROOT_UUID}/{inst_uuid}"
    p_ref  = f'\t\t(property "Reference" "{ref}"\n\t\t\t(at {x} {y-4} 0)\n\t\t\t(effects (font (size 1.27 1.27)))\n\t\t)'
    p_val  = f'\t\t(property "Value" "{value}"\n\t\t\t(at {x} {y+4} 0)\n\t\t\t(effects (font (size 1.27 1.27)))\n\t\t)'
    p_fp   = f'\t\t(property "Footprint" "{footprint}"\n\t\t\t(at {x} {y} 0)\n\t\t\t(effects (font (size 1.27 1.27)) (hide yes))\n\t\t)'
    p_ds   = f'\t\t(property "Datasheet" "~"\n\t\t\t(at {x} {y} 0)\n\t\t\t(effects (font (size 1.27 1.27)) (hide yes))\n\t\t)'
    pins_s = "\n".join(f'\t\t(pin "{p}" (uuid "{u()}"))' for p in pin_nums)
    inst   = (f'\t\t(instances\n\t\t\t(project "{PROJ}"\n'
              f'\t\t\t\t(path "{path}"\n'
              f'\t\t\t\t\t(reference "{ref}")\n'
              f'\t\t\t\t\t(unit {unit})\n'
              f'\t\t\t\t)\n\t\t\t)\n\t\t)')
    mirror_s = f'\t\t(mirror {mirror})\n' if mirror else ''
    return (f'\t(symbol\n\t\t(lib_id "{lib_id}")\n\t\t(at {x} {y} {angle})\n'
            f'{mirror_s}'
            f'\t\t(unit {unit})\n\t\t(exclude_from_sim {exclude_sim})\n'
            f'\t\t(in_bom {in_bom})\n\t\t(on_board yes)\n\t\t(dnp no)\n'
            f'\t\t(uuid "{u()}")\n{p_ref}\n{p_val}\n{p_fp}\n{p_ds}\n{pins_s}\n{inst}\n\t)')

def power_sym(inst_uuid, net, x, y, ref, angle=0):
    """Place a power symbol with a wire stub from (x,y) to symbol pin.

    angle=0   (default): VCC stub goes UP (y-STUB), GND stub goes DOWN (y+STUB).
                         Use for pins at the top (VCC) or bottom (GND) of an IC.
    angle=180 (flipped):  VCC stub goes DOWN (y+STUB), GND stub goes UP (y-STUB).
                         Use when the IC power unit has VCC at the bottom and GND
                         at the top (common for SOT-23/VSSOP power units).
    """
    path = f"/{ROOT_UUID}/{inst_uuid}"
    if net == "PWR_FLAG":
        sx, sy = x, y
    elif "GND" in net:
        sy = y - STUB if angle == 180 else y + STUB
        sx = x
    else:
        sy = y + STUB if angle == 180 else y - STUB
        sx = x
    sym = (f'\t(symbol\n\t\t(lib_id "power:{net}")\n\t\t(at {sx} {sy} {angle})\n'
           f'\t\t(unit 1)\n\t\t(exclude_from_sim no)\n\t\t(in_bom no)\n'
           f'\t\t(on_board yes)\n\t\t(dnp no)\n\t\t(uuid "{u()}")\n'
           f'\t\t(property "Reference" "{ref}"\n\t\t\t(at {sx} {sy-2} 0)\n'
           f'\t\t\t(effects (font (size 1.27 1.27)) (hide yes))\n\t\t)\n'
           f'\t\t(property "Value" "{net}"\n\t\t\t(at {sx} {sy+2} 0)\n'
           f'\t\t\t(effects (font (size 1.27 1.27)))\n\t\t)\n'
           f'\t\t(property "Footprint" ""\n\t\t\t(at {sx} {sy} 0)\n'
           f'\t\t\t(effects (font (size 1.27 1.27)) (hide yes))\n\t\t)\n'
           f'\t\t(property "Datasheet" ""\n\t\t\t(at {sx} {sy} 0)\n'
           f'\t\t\t(effects (font (size 1.27 1.27)) (hide yes))\n\t\t)\n'
           f'\t\t(pin "1" (uuid "{u()}"))\n'
           f'\t\t(instances\n\t\t\t(project "{PROJ}"\n'
           f'\t\t\t\t(path "{path}"\n\t\t\t\t\t(reference "{ref}")\n'
           f'\t\t\t\t\t(unit 1)\n\t\t\t\t)\n\t\t\t)\n\t\t)\n\t)')
    if net == "PWR_FLAG":
        return sym
    return wire(x, y, sx, sy) + '\n' + sym

def wire(x1,y1,x2,y2):
    return (f'\t(wire\n\t\t(pts (xy {x1} {y1}) (xy {x2} {y2}))\n'
            f'\t\t(stroke (width 0) (type solid))\n\t\t(uuid "{u()}")\n\t)')

def net_label(name, x, y, angle=0):
    """Net label with wire stub — mirrors hlabel convention.

    x = component pin tip (inner wire end).
    Label sits at lx (outer stub end):
      angle=180 → lx = x - STUB (left-side); angle=0 → lx = x + STUB (right-side).
    """
    just = "right" if angle == 180 else "left"
    dx = -STUB if angle == 180 else STUB
    lx = x + dx
    label = (f'\t(label "{name}"\n\t\t(at {lx} {y} {angle})\n'
             f'\t\t(fields_autoplaced yes)\n'
             f'\t\t(effects (font (size 1.27 1.27)) (justify {just} bottom))\n'
             f'\t\t(uuid "{u()}")\n\t)')
    return wire(min(x, lx), y, max(x, lx), y) + '\n' + label

def glabel(name, x, y, angle, shape="input"):
    just = "right" if angle == 180 else "left"
    dx = STUB if angle == 180 else -STUB
    lx = x + dx
    label = (f'\t(global_label "{name}"\n\t\t(shape {shape})\n'
             f'\t\t(at {lx} {y} {angle})\n\t\t(fields_autoplaced yes)\n'
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
    label = (f'\t(hierarchical_label "{name}"\n\t\t(shape {shape})\n'
             f'\t\t(at {lx} {y} {angle})\n\t\t(fields_autoplaced yes)\n'
             f'\t\t(effects (font (size 1.27 1.27)) (justify {just} bottom))\n'
             f'\t\t(uuid "{u()}")\n'
             f'\t\t(property "Intersheetrefs" "${{INTERSHEET_REFS}}"\n'
             f'\t\t\t(at {lx} {y} 0)\n'
             f'\t\t\t(effects (font (size 1.27 1.27)) (hide yes))\n'
             f'\t\t)\n\t)')
    return wire(min(x, lx), y, max(x, lx), y) + '\n' + label

def junction(x, y):
    return (f'\t(junction\n\t\t(at {x} {y})\n\t\t(diameter 0)\n'
            f'\t\t(color 0 0 0 0)\n\t\t(uuid "{u()}")\n\t)')

def no_connect(x, y):
    return f'\t(no_connect\n\t\t(at {x} {y})\n\t\t(uuid "{u()}")\n\t)'

def sheet_inst(root_uuid, inst_uuid, page):
    return (f'\t(sheet_instances\n'
            f'\t\t(path "/{root_uuid}/{inst_uuid}"\n'
            f'\t\t\t(page "{page}")\n'
            f'\t\t)\n\t)')

def make_lib_section(entries):
    body = "\n".join(
        "\n".join(("\t\t" + line) for line in e.split("\n")) for e in entries
    )
    return f"\t(lib_symbols\n{body}\n\t)"

def assemble(sheet_uuid, inst_uuid, title, page, lib_entries, elements):
    lib_sec = make_lib_section(lib_entries)
    body    = "\n".join(elements)
    sinst   = sheet_inst(ROOT_UUID, inst_uuid, page)
    return (f'(kicad_sch\n\t(version 20250114)\n\t(generator "eeschema")\n'
            f'\t(generator_version "9.0")\n\t(uuid "{sheet_uuid}")\n'
            f'\t(paper "A4")\n\t(title_block\n\t\t(title "{title}")\n'
            f'\t\t(date "2026-02-21")\n\t\t(rev "1")\n\t\t(company "")\n\t)\n'
            f'{lib_sec}\n{body}\n{sinst}\n)')

def write_sheet(filename, content):
    path = os.path.join(BASE, filename)
    with open(path, "w") as f:
        f.write(content)
    print(f"Written {len(content):6d} bytes → {path}")


# ═══════════════════════════════════════════════════════════════════════
# SHEET 2 — Zero-Crossing Detector
# ═══════════════════════════════════════════════════════════════════════
def gen_zero_crossing():
    SINST = "94283aa8-ba3a-413b-ba4f-3b3274da13e8"
    UUID  = "ff836894-14a4-40e0-bf39-4a5b3dc36c9d"

    libs = [
        lib_sym_entry(ISOLAT_LIB, "H11AA1",  "Isolator"),
        lib_sym_entry(DEVICE_LIB, "R",        "Device"),
        lib_sym_entry(POWER_LIB,  "GND",      "power"),
        lib_sym_entry(POWER_LIB,  "+3V3",     "power"),
    ]
    E = []
    PWR_BASE = 300
    def P(net, x, y, n, angle=0):
        E.append(power_sym(SINST, net, x, y, f"#PWR{PWR_BASE+n:04d}", angle))

    # U2 (H11AA1) at (96.52, 90.17)
    # Symbol y increases upward, so schematic positions are cy ∓ sym_y:
    #   1@(88.90,87.63)[upper-L]  2@(88.90,92.71)[lower-L]  3NC@(91.44,90.17)
    #   4(E)@(104.14,92.71)[lower-R]  5(C)@(104.14,90.17)[centre-R]  6(B)@(104.14,87.63)[upper-R]
    cx, cy = 96.52, 90.17
    E.append(placed_sym(SINST, "Isolator:H11AA1", cx, cy, 0,
             "U2", "H11AA1", "Package_DIP:DIP-6_W7.62mm", ["1","2","3","4","5","6"]))

    # R1 (220kΩ) horizontal: angle=90 → pin1 at (+3.81,0), pin2 at (-3.81,0)
    # Centre at (77.47,92.71): pin1@(81.28,92.71), pin2@(73.66,92.71)→AC_IN
    # Wire bridges R1 pin1 to U2 pin2(Anode B)@(88.90,92.71)
    E.append(placed_sym(SINST, "Device:R", 77.47, 92.71, 90,
             "R1", "220kΩ", "Resistor_SMD:R_0603_1608Metric", ["1","2"]))
    E.append(wire(81.28, 92.71, 88.90, 92.71))

    # R2 (10kΩ) vertical: angle=0, spread further right for label room
    # Centre at (116.84,82.55): pin2(top)@(116.84,78.74)=+3V3, pin1(btm)@(116.84,86.36)→node
    E.append(placed_sym(SINST, "Device:R", 116.84, 82.55, 0,
             "R2", "10kΩ",  "Resistor_SMD:R_0603_1608Metric", ["1","2"]))

    # Power
    # GND: U2 pin4(E)@(104.14,92.71) — cy+2.54 is the emitter (lower-right)
    E.append(wire(cx+7.62, cy+2.54, cx+7.62+STUB, cy+2.54))
    P("GND",  cx+7.62+STUB, cy+2.54, 1)   # U2 pin4 (emitter) → GND
    P("+3V3", 116.84, 78.74, 2)            # R2 pin2 (top) → +3V3

    # Wires
    E.append(wire(116.84, 86.36, 116.84, 90.17))  # R2 pin1 down to junction
    E.append(wire(116.84, 90.17, cx+7.62, cy))    # junction to U2 pin5(C)@(104.14,90.17)
    E.append(wire(116.84, 90.17, 131.04, 90.17))  # junction to ZC_OUT label
    # U2 pin1(Anode A)@(88.90,87.63) — cy-2.54 is upper-left → AC_RTN
    E.append(wire(cx-7.62, cy-2.54, 71.12, cy-2.54))

    # Hierarchical labels (inter-sheet ports)
    # Left-side inputs: angle=180 → body extends LEFT (outside); x = circuit endpoint.
    # Right-side output: angle=0  → body extends RIGHT (outside); x = circuit endpoint.
    E.append(hlabel("AC_IN",  73.66,  92.71, 180, "input"))   # left-side; R1 pin2
    E.append(hlabel("AC_RTN", 71.12,  87.63, 180, "input"))   # left-side; U2 pin1
    E.append(hlabel("ZC_OUT", 131.04, 90.17, 0,   "output"))  # right-side

    # Junctions
    E.append(junction(116.84, 90.17))   # R2↔U2pin5↔ZC_OUT

    # No-connects
    E.append(no_connect(cx-5.08, cy))       # U2 pin3 NC at (91.44,90.17)
    E.append(no_connect(cx+7.62, cy-2.54))  # U2 pin6(B) NC at (104.14,87.63)

    return assemble(UUID, SINST, "Zero-Crossing Detector", "3", libs, E)


# ═══════════════════════════════════════════════════════════════════════
# SHEET 3 — XOR Interlock
# ═══════════════════════════════════════════════════════════════════════
def gen_xor_interlock():
    SINST = "e0f8704d-1e30-470b-9f75-e6a1f31638d7"
    UUID  = "a2bf21a6-3675-4941-886f-2fda6222e24f"

    libs = [
        lib_sym_entry(LOGIC_LIB,  "74LVC1G86", "74xGxx"),
        lib_sym_entry(LOGIC_LIB,  "74LVC2G08", "74xGxx"),
        lib_sym_entry(DEVICE_LIB, "C",          "Device"),
        lib_sym_entry(POWER_LIB,  "GND",        "power"),
        lib_sym_entry(POWER_LIB,  "+3V3",       "power"),
    ]
    E = []
    PWR_BASE = 400
    def P(net, x, y, n, angle=0):
        E.append(power_sym(SINST, net, x, y, f"#PWR{PWR_BASE+n:04d}", angle))

    # U3 (74LVC1G86 XOR) at (63.5, 90.17) — anchor
    # Pins: 1@(48.26,92.71), 2@(48.26,87.63), 4(Y)@(76.20,90.17)
    #       5(VCC)@(63.5,100.33), 3(GND)@(63.5,80.01)
    E.append(placed_sym(SINST, "74xGxx:74LVC1G86", 63.5, 90.17, 0,
             "U3", "74LVC1G86", "Package_TO_SOT_SMD:SOT-23-5", ["1","2","3","4","5"]))
    P("+3V3", 63.5, 100.33, 1, 180)   # U3 VCC — pin at BOTTOM of SOT-23 body → stub DOWN
    P("GND",  63.5,  80.01, 2, 180)   # U3 GND — pin at TOP  of SOT-23 body → stub UP

    # C10 (100nF decoupling for U3) at (91.44, 90.17) angle=180
    # Moved right of U3 output wire; pin1@(91.44,86.36)→+3V3, pin2@(91.44,93.98)→GND
    E.append(placed_sym(SINST, "Device:C", 91.44, 90.17, 180,
             "C10", "100nF", "Capacitor_SMD:C_0603_1608Metric", ["1","2"]))
    P("+3V3", 91.44, 86.36, 3)
    P("GND",  91.44, 93.98, 4)

    # U4 unit 1 (74LVC2G08 AND gate A) at (127.00, 90.17) — shifted right vs U3
    # Pins unit1: 1@(111.76,92.71), 2@(111.76,87.63), 7(Y)@(139.70,90.17)
    E.append(placed_sym(SINST, "74xGxx:74LVC2G08", 127.00, 90.17, 0,
             "U4", "74LVC2G08", "Package_SO:VSSOP-8_2.3x2mm_P0.5mm", ["1","2","7"],
             unit=1))
    # U4 unit 2 (AND gate B) at (127.00, 109.22)
    # Pins unit2: 5@(111.76,111.76), 6@(111.76,106.68), 3(Y)@(139.70,109.22)
    E.append(placed_sym(SINST, "74xGxx:74LVC2G08", 127.00, 109.22, 0,
             "U4", "74LVC2G08", "Package_SO:VSSOP-8_2.3x2mm_P0.5mm", ["5","6","3"],
             unit=2))
    # U4 unit 3 (power) at (152.40, 109.22)
    # Pins: 8(VCC)@(152.40,119.38), 4(GND)@(152.40,99.06)
    E.append(placed_sym(SINST, "74xGxx:74LVC2G08", 152.40, 109.22, 0,
             "U4", "74LVC2G08", "Package_SO:VSSOP-8_2.3x2mm_P0.5mm", ["4","8"],
             unit=3))
    P("+3V3", 152.40, 119.38, 5, 180)  # U4 VCC — pin at BOTTOM of VSSOP body → stub DOWN
    P("GND",  152.40,  99.06, 6, 180)  # U4 GND — pin at TOP  of VSSOP body → stub UP

    # C11 (100nF decoupling for U4) at (177.80, 109.22) angle=180
    # Moved further right of U4 power unit; pin1@(177.80,105.41)→+3V3, pin2@(177.80,113.03)→GND
    E.append(placed_sym(SINST, "Device:C", 177.80, 109.22, 180,
             "C11", "100nF", "Capacitor_SMD:C_0603_1608Metric", ["1","2"]))
    P("+3V3", 177.80, 105.41, 7)
    P("GND",  177.80, 113.03, 8)

    # Hierarchical labels — inputs to U3 (left-side: angle=180, x = circuit pin)
    E.append(hlabel("CTRL_A",  48.26, 92.71, 180, "input"))   # U3 pin1
    E.append(hlabel("CTRL_B",  48.26, 87.63, 180, "input"))   # U3 pin2
    # Local labels — XOR_OUT (sheet-local) connects U3 Y → U4 AND inputs
    E.append(net_label("XOR_OUT", 76.20,  90.17))             # U3 pin4 (Y)    — right-side output
    E.append(net_label("XOR_OUT", 111.76, 92.71,  180))      # U4 unit1 pin1  — left-side input
    E.append(net_label("XOR_OUT", 111.76, 111.76, 180))      # U4 unit2 pin5  — left-side input
    # CTRL_A/B also fed to U4 AND gates (left-side: angle=180, x = circuit pin)
    E.append(hlabel("CTRL_A", 111.76, 87.63,  180, "input"))  # U4 unit1 pin2
    E.append(hlabel("CTRL_B", 111.76, 106.68, 180, "input"))  # U4 unit2 pin6
    # Outputs (right-side: angle=0, x = circuit pin)
    E.append(hlabel("PHASE_A", 139.70,  90.17, 0, "output"))  # U4 unit1 pin7 (Y)
    E.append(hlabel("PHASE_B", 139.70, 109.22, 0, "output"))  # U4 unit2 pin3 (Y)

    return assemble(UUID, SINST, "XOR Interlock", "4", libs, E)


# ═══════════════════════════════════════════════════════════════════════
# SHEET 4 — Opto-TRIAC Drive
# ═══════════════════════════════════════════════════════════════════════
def gen_opto_triac():
    SINST = "1155aa41-bfef-4df9-96ba-c8e7c9de0aa7"
    UUID  = "09b38e2f-bc9c-4a87-9439-1aae11b985c4"

    libs = [
        lib_sym_entry(RELAY_LIB,  "MOC3021M", "Relay_SolidState"),
        lib_sym_entry(DEVICE_LIB, "R",         "Device"),
        lib_sym_entry(POWER_LIB,  "GND",       "power"),
    ]
    E = []
    PWR_BASE = 500
    def P(net, x, y, n, angle=0):
        E.append(power_sym(SINST, net, x, y, f"#PWR{PWR_BASE+n:04d}", angle))

    # MOC3021M pin offsets from centre (angle=0):
    # Pin1(LED+)@(-7.62,+2.54) Pin2(LED-)@(-7.62,-2.54) Pin3NC@(-5.08,0)
    # Pin4@(+7.62,-2.54) Pin5NC@(+5.08,0) Pin6@(+7.62,+2.54)

    def moc_sheet(ref, cy, phase, gate, pwr_n, no_cnt_offset):
        cx = 80.01
        COMP_GAP = 7.62   # spacing gap between component pin and adjacent resistor pin
        # Component
        E.append(placed_sym(SINST, "Relay_SolidState:MOC3021M", cx, cy, 0,
                 ref, "MOC3021M", "Package_DIP:DIP-6_W7.62mm",
                 ["1","2","3","4","5","6"]))
        # LED- (pin2) → GND
        P("GND", cx-7.62, cy-2.54, pwr_n)
        # AC_RTN at pin6 (MT1, lower-right); right-side: angle=0, x = circuit pin
        E.append(hlabel("AC_RTN", cx+7.62, cy+2.54, 0, "bidirectional"))
        # LED series resistor R3/R4 horizontal, spaced COMP_GAP left of MOC pin1
        # Centre at (cx-7.62-COMP_GAP-3.81): pin1@(cx-7.62-COMP_GAP), pin2→PHASE label
        rval = "330Ω"
        rfp  = "Resistor_SMD:R_0603_1608Metric"
        E.append(placed_sym(SINST, "Device:R", cx-7.62-COMP_GAP-3.81, cy+2.54, 90,
                 f"R{pwr_n}", rval, rfp, ["1","2"]))
        # Wire from R pin1 to MOC pin1(LED+)
        E.append(wire(cx-7.62-COMP_GAP, cy+2.54, cx-7.62, cy+2.54))
        E.append(hlabel(phase, cx-7.62-COMP_GAP-3.81-3.81, cy+2.54, 180, "input"))  # R pin2
        # Gate resistor R5/R6 horizontal, spaced COMP_GAP right of MOC pin4
        # Centre at gx=cx+7.62+COMP_GAP+3.81: pin2(left)@(gx-3.81) bridges via wire to MOC pin4
        gx = cx + 7.62 + COMP_GAP + 3.81
        g_pwr_n = pwr_n + 2
        E.append(placed_sym(SINST, "Device:R", gx, cy-2.54, 90,
                 f"R{g_pwr_n}", "360Ω", rfp, ["1","2"]))
        # Wire from MOC pin4 (cx+7.62) to R gate pin2 (gx-3.81)
        E.append(wire(cx+7.62, cy-2.54, gx-3.81, cy-2.54))
        E.append(hlabel(gate, gx+3.81, cy-2.54, 0, "output"))  # R gate pin1
        # No-connects on hidden NC pins
        E.append(no_connect(cx-5.08, cy))         # pin3
        E.append(no_connect(cx+5.08, cy))          # pin5

    # U5 (phase A) at cy=90.17
    moc_sheet("U5",  90.17, "PHASE_A", "TRIAC_A_GATE", 3, 0)
    # U6 (phase B) at cy=154.94 — doubled vertical gap vs original
    moc_sheet("U6", 154.94, "PHASE_B", "TRIAC_B_GATE", 4, 2)

    return assemble(UUID, SINST, "Opto-TRIAC Drive", "5", libs, E)


# ═══════════════════════════════════════════════════════════════════════
# SHEET 5 — TRIACs & Snubbers
# ═══════════════════════════════════════════════════════════════════════
def gen_triacs_snubbers():
    SINST = "d4fe8f86-314f-434b-af4a-2b112cc8638c"
    UUID  = "ab7d41de-f42d-490a-9017-d92213e7ec79"

    libs = [
        lib_sym_entry(LOCAL_LIB,  "BT134W-600D", "traincontrol-shield"),
        lib_sym_entry(DEVICE_LIB, "R",             "Device"),
        lib_sym_entry(DEVICE_LIB, "C",             "Device"),
    ]
    E = []

    # BT134W-600D pin offsets (angle=0):
    # T1(1)@(-2.54,+2.54)  T2(2)@(-2.54,0)  G(3)@(-2.54,-2.54)  T2tab(4)@(+12.70,0)

    pwr_n = [601]
    def P(net, x, y, angle=0):
        E.append(power_sym(SINST, net, x, y, f"#PWR{pwr_n[0]:04d}", angle))
        pwr_n[0] += 1

    def triac_set(ref_q, ref_r_sn, ref_c_sn, cy, winding, gate):
        cx = 80.01
        # TRIAC
        E.append(placed_sym(SINST, "traincontrol-shield:BT134W-600D", cx, cy, 0,
                 ref_q, "BT134W-600D",
                 "traincontrol-shield:SOT-223-3_L6.5-W3.4-P2.30-LS7.0-BR",
                 ["1","2","3","4"]))
        # T1 (AC_RTN) at (cx-2.54, cy+2.54)
        E.append(hlabel("AC_RTN", cx-2.54, cy+2.54, 180, "bidirectional"))
        # T2 (winding) at (cx-2.54, cy) and T2tab (cx+12.70, cy) both = winding
        E.append(hlabel(winding, cx-2.54,   cy, 180, "bidirectional"))
        E.append(hlabel(winding, cx+12.70,  cy,   0, "bidirectional"))
        # Gate at (cx-2.54, cy-2.54) from opto-triac
        E.append(hlabel(gate,    cx-2.54, cy-2.54, 180, "input"))

        # Snubber: R_sn series + C_sn in series from winding to AC_RTN
        # R_sn vertical: pin2(top)=winding level, pin1(btm)=SN_x node
        sn_cx = 109.22   # moved right for clearance from TRIAC
        # R centre: pin2 at cy-2.54 (winding), pin1 at cy+5.08
        r_cy = cy - 2.54 + 3.81  # = cy+1.27
        E.append(placed_sym(SINST, "Device:R", sn_cx, r_cy, 0,
                 ref_r_sn, "39Ω", "Resistor_SMD:R_0603_1608Metric", ["1","2"]))
        # R pin2 at (sn_cx, r_cy-3.81=cy-2.54): label winding net
        E.append(hlabel(winding, sn_cx, r_cy-3.81, 0, "bidirectional"))
        # R pin1 at (sn_cx, r_cy+3.81=cy+5.08): junction + SN_x label
        sn_label = f"SN_{winding[-1]}"  # SN_A or SN_B
        E.append(net_label(sn_label, sn_cx, r_cy+3.81))
        # C_sn vertical: gap of 7.62mm between R pin1 and C pin2 — bridged by wire
        c_cy = r_cy + 3.81 + 7.62 + 3.81  # centre: pin2 at r_cy+3.81+7.62
        E.append(wire(sn_cx, r_cy+3.81, sn_cx, c_cy-3.81))   # R pin1 → C pin2
        E.append(placed_sym(SINST, "Device:C", sn_cx, c_cy, 0,
                 ref_c_sn, "10nF", "Capacitor_SMD:C_0603_1608Metric", ["1","2"]))
        E.append(hlabel("AC_RTN", sn_cx, c_cy+3.81, 0, "bidirectional"))  # C pin1
        E.append(junction(sn_cx, r_cy+3.81))                   # R pin1 / wire / SN label

    triac_set("Q1", "R7", "C7",  90.17, "WINDING_A", "TRIAC_A_GATE")
    triac_set("Q2", "R8", "C8", 177.80, "WINDING_B", "TRIAC_B_GATE")

    return assemble(UUID, SINST, "TRIACs & Snubbers", "6", libs, E)


# ═══════════════════════════════════════════════════════════════════════
# SHEET 6 — Rail Voltage Sense
# ═══════════════════════════════════════════════════════════════════════
def gen_rail_voltage_sense():
    SINST = "a8cf3e0f-6d5f-4a60-9682-8cd8e6228106"
    UUID  = "e779d9a9-4c11-42ff-bec5-a02605591825"

    libs = [
        lib_sym_entry(DEVICE_LIB, "R",    "Device"),
        lib_sym_entry(POWER_LIB,  "GND",  "power"),
        lib_sym_entry(POWER_LIB,  "+15V", "power"),
    ]
    E = []
    PWR_BASE = 700
    def P(net, x, y, n, angle=0):
        E.append(power_sym(SINST, net, x, y, f"#PWR{PWR_BASE+n:04d}", angle))

    # R9 (100kΩ) at (80.01, 76.20): pin2(top)@(80.01,72.39)=+15V, pin1(btm)@(80.01,80.01)
    # R10 (27kΩ)  at (80.01, 91.44): pin2(top)@(80.01,87.63), pin1(btm)@(80.01,95.25)=GND
    # Vertical wire (80.01,80.01)→(80.01,87.63) bridges R9 pin1 to R10 pin2.
    # VSENSE taps the midpoint at (80.01, 83.82) via a horizontal branch.
    E.append(placed_sym(SINST, "Device:R", 80.01, 76.20, 0,
             "R9",  "100kΩ", "Resistor_SMD:R_0603_1608Metric", ["1","2"]))
    E.append(placed_sym(SINST, "Device:R", 80.01, 91.44, 0,
             "R10",  "27kΩ", "Resistor_SMD:R_0603_1608Metric", ["1","2"]))
    P("+15V", 80.01, 72.39, 1)   # R9 pin2 (top)
    P("GND",  80.01, 95.25, 2)   # R10 pin1 (bottom)

    # Vertical wire bridging R9 pin1 to R10 pin2
    E.append(wire(80.01, 80.01, 80.01, 87.63))
    # VSENSE tap at midpoint of bridge wire
    E.append(junction(80.01, 83.82))
    E.append(wire(80.01, 83.82, 109.22, 83.82))
    E.append(hlabel("VSENSE", 109.22, 83.82, 0, "output"))  # right-side: angle=0

    return assemble(UUID, SINST, "Rail Voltage Sense", "7", libs, E)


# ═══════════════════════════════════════════════════════════════════════
# SHEET 7 — MCU (ESP32-S3-DevKitC Headers)
# ═══════════════════════════════════════════════════════════════════════
def gen_mcu():
    SINST = "8c6fba78-5fd4-419a-8456-de6c668aaabc"
    UUID  = "b17ddd41-b910-4f09-a62f-46566a666def"

    libs = [
        lib_sym_entry(CONN_LIB,   "Conn_01x22", "Connector_Generic"),
        lib_sym_entry(DEVICE_LIB, "C",           "Device"),
        lib_sym_entry(DEVICE_LIB, "R",           "Device"),
        lib_sym_entry(DEVICE_LIB, "LED",         "Device"),
        lib_sym_entry(POWER_LIB,  "GND",         "power"),
        lib_sym_entry(POWER_LIB,  "+3V3",        "power"),
    ]
    E = []
    pwr_n = [801]
    def P(net, x, y, angle=0):
        E.append(power_sym(SINST, net, x, y, f"#PWR{pwr_n[0]:04d}", angle))
        pwr_n[0] += 1

    # Conn_01x22 pin offset formula (angle=0):
    # Pin n at library (-5.08, 25.4-(n-1)*2.54) → schematic (cx-5.08, cy+25.4-(n-1)*2.54)
    # All pins exit to the LEFT (angle=0° in library → connection at -5.08 from center)

    # J1 at (80.01, 120.65): pins 1..22
    # J2 at (160.02, 120.65): pins 1..22
    # J1 pin positions: x=74.93, y=146.05 (pin1) to y=92.71 (pin22), step=-2.54
    # J2 pin positions: x=154.94, y=146.05 to 92.71

    E.append(placed_sym(SINST, "Connector_Generic:Conn_01x22", 80.01, 120.65, 0,
             "J1", "ESP32-S3 Header L",
             "Connector_PinHeader_2.54mm:PinHeader_1x22_P2.54mm_Vertical",
             [str(i) for i in range(1,23)]))
    E.append(placed_sym(SINST, "Connector_Generic:Conn_01x22", 200.02, 120.65, 0,
             "J2", "ESP32-S3 Header R",
             "Connector_PinHeader_2.54mm:PinHeader_1x22_P2.54mm_Vertical",
             [str(i) for i in range(1,23)]))

    # Pin labels for J1 (ESP32-S3-DevKitC left header, typical assignment)
    # J1 left side connection x = 74.93, y from pin1 to pin22
    j1x = 74.93
    j2x = 194.94

    def pin_y(cy, n):
        return round(cy + 25.4 - (n-1)*2.54, 3)

    # Key signal assignments for J1 (left header):
    j1_pins = {
        1:  ("GND",      "power"),
        2:  ("+3V3",     "power"),
        3:  ("CTRL_A",   "output"),   # GPIO to XOR interlock
        4:  ("CTRL_B",   "output"),   # GPIO to XOR interlock
        5:  ("ZC_OUT",   "input"),    # Zero-crossing interrupt input
        6:  ("LED_CTRL", None),        # local net
        7:  ("GPIO7",    None),
        8:  ("GPIO8",    None),
        9:  ("GPIO9",    None),
        10: ("GPIO10",   None),
        11: ("GPIO11",   None),
        12: ("GPIO12",   None),
        13: ("GPIO13",   None),
        14: ("GPIO14",   None),
        15: ("GPIO15",   None),
        16: ("GPIO16",   None),
        17: ("GPIO17",   None),
        18: ("GPIO18",   None),
        19: ("GPIO19",   None),
        20: ("GPIO20",   None),
        21: ("GND",      "power"),
        22: ("GND",      "power"),
    }
    # Key signal assignments for J2 (right header):
    j2_pins = {
        1:  ("GND",     "power"),
        2:  ("+3V3",    "power"),
        3:  ("VSENSE",  "input"),    # ADC input
        4:  ("GPIO37",  None),
        5:  ("GPIO36",  None),
        6:  ("GPIO35",  None),
        7:  ("GPIO34",  None),
        8:  ("GPIO33",  None),
        9:  ("GPIO26",  None),
        10: ("GPIO21",  None),
        11: ("GPIO47",  None),
        12: ("GPIO48",  None),
        13: ("GPIO45",  None),
        14: ("GPIO0",   None),
        15: ("GPIO3",   None),
        16: ("GPIO46",  None),
        17: ("GPIO9",   None),
        18: ("GPIO10",  None),
        19: ("GPIO11",  None),
        20: ("GPIO12",  None),
        21: ("GND",     "power"),
        22: ("GND",     "power"),
    }

    cy = 120.65
    for n, (sig, kind) in j1_pins.items():
        y = pin_y(cy, n)
        x = j1x
        if kind == "power":
            # Route left by STUB so vertical power stubs don't hit adjacent signal pins
            E.append(wire(x, y, x - STUB, y))
            P(sig, x - STUB, y)
        elif kind in ("output", "input", "bidirectional"):
            shape = kind
            E.append(hlabel(sig, x, y, 180, shape))
        else:
            E.append(net_label(sig, x, y, 180))

    for n, (sig, kind) in j2_pins.items():
        y = pin_y(cy, n)
        x = j2x
        if kind == "power":
            # Route left by STUB so vertical power stubs don't hit adjacent signal pins
            E.append(wire(x, y, x - STUB, y))
            P(sig, x - STUB, y)
        elif kind in ("output", "input", "bidirectional"):
            E.append(hlabel(sig, x, y, 180, kind))
        else:
            E.append(net_label(sig, x, y, 180))

    # C4, C5, C6 (100nF decoupling) placed to the right of J2 (at 200.02)
    # angle=180: pin1(top=+3V3), pin2(bottom=GND)
    for i, dx in enumerate([215.90, 226.06, 236.22]):
        cy_c = 107.95
        E.append(placed_sym(SINST, "Device:C", dx, cy_c, 180,
                 f"C{4+i}", "100nF",
                 "Capacitor_SMD:C_0603_1608Metric", ["1","2"]))
        P("+3V3", dx, cy_c-3.81)   # pin1 top
        P("GND",  dx, cy_c+3.81)   # pin2 bottom

    # Status LED: MCU→R11→D2→GND
    # D2 (LED) horizontal at (120.65, 162.56): K@(116.84,162.56) A@(124.46,162.56)
    E.append(placed_sym(SINST, "Device:LED", 120.65, 162.56, 0,
             "D2", "Green", "LED_SMD:LED_0603_1608Metric", ["K","A"]))
    P("GND", 116.84, 162.56)   # D2 cathode

    # R11 (1kΩ) horizontal, pin2@(128.27,162.56) connects to D2 anode@(124.46,162.56)
    # angle=90: pin1@(cx+3.81,cy), pin2@(cx-3.81,cy) → centre at (131.76,162.56)
    E.append(placed_sym(SINST, "Device:R", 131.76, 162.56, 90,
             "R11", "1kΩ", "Resistor_SMD:R_0603_1608Metric", ["1","2"]))
    E.append(net_label("LED_CTRL", 135.57, 162.56))   # R11 pin1 → MCU GPIO
    # Wire D2 anode to R11 pin2
    E.append(wire(124.46, 162.56, 128.27, 162.56))  # D2 A to R11 pin2 (131.76-3.81)

    return assemble(UUID, SINST, "MCU (ESP32-S3-DevKitC)", "8", libs, E)


# ─────────────────────── main ──────────────────────────────────────────
if __name__ == "__main__":
    write_sheet("zero-crossing.kicad_sch",    gen_zero_crossing())
    write_sheet("xor-interlock.kicad_sch",    gen_xor_interlock())
    write_sheet("opto-triac-drive.kicad_sch", gen_opto_triac())
    write_sheet("triacs-snubbers.kicad_sch",  gen_triacs_snubbers())
    write_sheet("rail-voltage-sense.kicad_sch", gen_rail_voltage_sense())
    write_sheet("mcu.kicad_sch",              gen_mcu())
    print("All sheets generated.")

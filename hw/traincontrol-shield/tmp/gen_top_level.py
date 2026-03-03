#!/usr/bin/env python3
"""Generator for top-level traincontrol-shield.kicad_sch (Phase 4)

Top-level contains:
  - 7 sub-sheet frame instances
  - J4: Conn_01x02 Track Input (AC_IN, AC_RTN)
  - J5: Conn_01x02 Motor Windings (WINDING_A, WINDING_B)
  - J6: Conn_01x03 Motor+AC (AC_IN, WINDING_A, WINDING_B)
  - Global labels routing inter-sheet signals
"""
import uuid, re, os

ROOT_UUID = "12cc2a67-9b1c-41e4-828f-9c2a8df6bb35"
PROJ      = "traincontrol-shield"
BASE      = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")

DEVICE_LIB = "/usr/share/kicad/symbols/Device.kicad_sym"
POWER_LIB  = "/usr/share/kicad/symbols/power.kicad_sym"
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
        if content[i] == '(':
            depth += 1
        elif content[i] == ')':
            depth -= 1
            if depth == 0:
                break
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
    # Rename outer symbol only; sub-symbol names stay bare (e.g. "Conn_01x02_1_1")
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

def make_lib_section(entries):
    body = "\n".join(
        "\n".join(("\t\t" + line) for line in e.split("\n")) for e in entries
    )
    return f"\t(lib_symbols\n{body}\n\t)"

def wire(x1, y1, x2, y2):
    return (f'\t(wire\n\t\t(pts (xy {x1} {y1}) (xy {x2} {y2}))\n'
            f'\t\t(stroke (width 0) (type solid))\n\t\t(uuid "{u()}")\n\t)')

STUB = 7.62  # wire stub length (mm)

def glabel(name, x, y, angle, shape="input"):
    just = "right" if angle == 180 else "left"
    dx = STUB if angle == 180 else -STUB
    lx = x + dx
    label = (f'\t(global_label "{name}"\n\t\t(shape {shape})\n'
             f'\t\t(at {x} {y} {angle})\n\t\t(fields_autoplaced yes)\n'
             f'\t\t(effects (font (size 1.27 1.27)) (justify {just} bottom))\n'
             f'\t\t(uuid "{u()}")\n'
             f'\t\t(property "Intersheetrefs" "${{INTERSHEET_REFS}}"\n'
             f'\t\t\t(at {x} {y} 0)\n'
             f'\t\t\t(effects (font (size 1.27 1.27)) (hide yes))\n'
             f'\t\t)\n\t)')
    return wire(min(x, lx), y, max(x, lx), y) + '\n' + label

def junction(x, y):
    return (f'\t(junction\n\t\t(at {x} {y})\n\t\t(diameter 0)\n'
            f'\t\t(color 0 0 0 0)\n\t\t(uuid "{u()}")\n\t)')


def placed_conn(inst_uuid, lib_id, x, y, angle, ref, value, footprint, pin_nums):
    """Place a connector symbol on the top-level sheet."""
    path = f"/{ROOT_UUID}"
    p_ref = (f'\t\t(property "Reference" "{ref}"\n'
             f'\t\t\t(at {x} {y-4} 0)\n'
             f'\t\t\t(effects (font (size 1.27 1.27)))\n\t\t)')
    p_val = (f'\t\t(property "Value" "{value}"\n'
             f'\t\t\t(at {x} {y+4} 0)\n'
             f'\t\t\t(effects (font (size 1.27 1.27)))\n\t\t)')
    p_fp  = (f'\t\t(property "Footprint" "{footprint}"\n'
             f'\t\t\t(at {x} {y} 0)\n'
             f'\t\t\t(effects (font (size 1.27 1.27)) (hide yes))\n\t\t)')
    p_ds  = (f'\t\t(property "Datasheet" "~"\n'
             f'\t\t\t(at {x} {y} 0)\n'
             f'\t\t\t(effects (font (size 1.27 1.27)) (hide yes))\n\t\t)')
    pins_s = "\n".join(f'\t\t(pin "{p}" (uuid "{u()}"))' for p in pin_nums)
    inst = (f'\t\t(instances\n\t\t\t(project "{PROJ}"\n'
            f'\t\t\t\t(path "{path}"\n'
            f'\t\t\t\t\t(reference "{ref}")\n'
            f'\t\t\t\t\t(unit 1)\n\t\t\t\t)\n\t\t\t)\n\t\t)')
    return (f'\t(symbol\n\t\t(lib_id "{lib_id}")\n\t\t(at {x} {y} {angle})\n'
            f'\t\t(unit 1)\n\t\t(exclude_from_sim no)\n'
            f'\t\t(in_bom yes)\n\t\t(on_board yes)\n\t\t(dnp no)\n'
            f'\t\t(uuid "{u()}")\n{p_ref}\n{p_val}\n{p_fp}\n{p_ds}\n{pins_s}\n{inst}\n\t)')

def sheet_frame(sinst_uuid, filename, sheetname, page, x, y, w, h, pins=None):
    """Generate a hierarchical sheet frame entry for the top-level.

    pins: list of (name, shape, bx, by, angle) tuples where (bx, by) is the
          absolute position on the sheet border and angle is 0 (exits right)
          or 180 (exits left).
    """
    pins_str = ""
    if pins:
        for (pname, pshape, bx, by, pangle) in pins:
            just = "right" if pangle == 0 else "left"
            pins_str += (
                f'\t\t(pin "{pname}" {pshape}\n'
                f'\t\t\t(at {bx} {by} {pangle})\n'
                f'\t\t\t(effects (font (size 1.27 1.27)) (justify {just}))\n'
                f'\t\t\t(uuid "{u()}")\n'
                f'\t\t)\n'
            )
    return (f'\t(sheet\n'
            f'\t\t(at {x} {y})\n'
            f'\t\t(size {w} {h})\n'
            f'\t\t(fields_autoplaced yes)\n'
            f'\t\t(stroke (width 0) (type solid))\n'
            f'\t\t(fill (color 255 255 255 255))\n'
            f'\t\t(uuid "{sinst_uuid}")\n'
            f'\t\t(property "Sheetname" "{sheetname}"\n'
            f'\t\t\t(at {x} {y-1.27} 0)\n'
            f'\t\t\t(effects (font (size 1.27 1.27)) (justify left bottom))\n'
            f'\t\t)\n'
            f'\t\t(property "Sheetfile" "{filename}"\n'
            f'\t\t\t(at {x} {y+h+1.27} 0)\n'
            f'\t\t\t(effects (font (size 1.27 1.27)) (justify left top))\n'
            f'\t\t)\n'
            f'{pins_str}'
            f'\t\t(instances\n'
            f'\t\t\t(project "{PROJ}"\n'
            f'\t\t\t\t(path "/"\n'
            f'\t\t\t\t\t(page "{page}")\n'
            f'\t\t\t\t)\n'
            f'\t\t\t)\n'
            f'\t\t)\n'
            f'\t)')


# ═══════════════════════════════════════════════════════════════════════
# TOP-LEVEL SHEET
# ═══════════════════════════════════════════════════════════════════════
def gen_top_level():
    # Sheet instance UUIDs (match sub-sheet path entries)
    SINST_POWER   = "a7305cff-bbaf-4846-983d-83b0ea81fbce"
    SINST_ZC      = "94283aa8-ba3a-413b-ba4f-3b3274da13e8"
    SINST_XOR     = "e0f8704d-1e30-470b-9f75-e6a1f31638d7"
    SINST_OPTO    = "1155aa41-bfef-4df9-96ba-c8e7c9de0aa7"
    SINST_TRIACS  = "d4fe8f86-314f-434b-af4a-2b112cc8638c"
    SINST_RAIL    = "a8cf3e0f-6d5f-4a60-9682-8cd8e6228106"
    SINST_MCU     = "8c6fba78-5fd4-419a-8456-de6c668aaabc"

    libs = [
        lib_sym_entry(CONN_LIB, "Conn_01x02", "Connector_Generic"),
        lib_sym_entry(CONN_LIB, "Conn_01x03", "Connector_Generic"),
    ]

    E = []

    # ── Layout: sub-sheet boxes on left/centre, connectors on far right ──
    # Left column  (x=15,  w=65): 5 sheets stacked vertically → right border x=80
    # Right column (x=110, w=65): 2 sheets                    → left border x=110
    # Connectors column: x=190
    # Inter-column gap (x=80..110): holds global labels (CL=87.62, CR=102.38)

    BOX_W = 65
    RX = 15 + BOX_W   # = 80: right border of left-column sheets
    LX = RX + 30      # = 110: left border of right-column sheets (30 mm gap)

    # ── Hierarchical sheet pin definitions ──
    # Each tuple: (name, shape, bx, by, angle) — bx,by = absolute position on border
    # Pins spaced 5 mm apart, centred within each frame.
    PINS_POWER = [
        ("AC_IN",  "input",  RX, 27, 0),
        ("AC_RTN", "input",  RX, 33, 0),
    ]
    PINS_ZC = [
        ("AC_IN",  "input",  RX, 57, 0),
        ("AC_RTN", "input",  RX, 62, 0),
        ("ZC_OUT", "output", RX, 67, 0),
    ]
    PINS_XOR = [
        ("CTRL_A",  "input",  RX,  87, 0),
        ("CTRL_B",  "input",  RX,  92, 0),
        ("PHASE_A", "output", RX,  97, 0),
        ("PHASE_B", "output", RX, 102, 0),
    ]
    PINS_OPTO = [
        ("PHASE_A",      "input",         RX, 127, 0),
        ("PHASE_B",      "input",         RX, 132, 0),
        ("AC_RTN",       "bidirectional", RX, 137, 0),
        ("TRIAC_A_GATE", "output",        RX, 142, 0),
        ("TRIAC_B_GATE", "output",        RX, 147, 0),
    ]
    PINS_TRIACS = [
        ("AC_RTN",       "bidirectional", RX, 172, 0),
        ("WINDING_A",    "bidirectional", RX, 177, 0),
        ("WINDING_B",    "bidirectional", RX, 182, 0),
        ("TRIAC_A_GATE", "input",         RX, 187, 0),
        ("TRIAC_B_GATE", "input",         RX, 192, 0),
    ]
    PINS_RAIL = [
        ("VSENSE", "output", LX, 27, 180),
    ]
    PINS_MCU = [
        ("CTRL_A", "output", LX, 57, 180),
        ("CTRL_B", "output", LX, 62, 180),
        ("ZC_OUT", "input",  LX, 67, 180),
        ("VSENSE", "input",  LX, 72, 180),
    ]

    # Left column — frame heights sized to pin count (5 mm/pin + margins)
    frames = [
        (SINST_POWER,  "power-supply.kicad_sch",       "Power Supply",             "2",  15,  20, BOX_W, 20, PINS_POWER),
        (SINST_ZC,     "zero-crossing.kicad_sch",       "Zero-Crossing Detector",   "3",  15,  50, BOX_W, 20, PINS_ZC),
        (SINST_XOR,    "xor-interlock.kicad_sch",       "XOR Interlock",            "4",  15,  80, BOX_W, 30, PINS_XOR),
        (SINST_OPTO,   "opto-triac-drive.kicad_sch",    "Opto-TRIAC Drive",         "5",  15, 120, BOX_W, 35, PINS_OPTO),
        (SINST_TRIACS, "triacs-snubbers.kicad_sch",     "TRIACs & Snubbers",        "6",  15, 165, BOX_W, 35, PINS_TRIACS),
        # Right column
        (SINST_RAIL,   "rail-voltage-sense.kicad_sch",  "Rail Voltage Sense",       "7", LX,  20, BOX_W, 20, PINS_RAIL),
        (SINST_MCU,    "mcu.kicad_sch",                 "MCU (ESP32-S3-DevKitC)",   "8", LX,  50, BOX_W, 30, PINS_MCU),
    ]

    for sinst, fn, name, pg, x, y, w, h, pins in frames:
        E.append(sheet_frame(sinst, fn, name, pg, x, y, w, h, pins))

    # ── Connect sheet pins to top-level global labels ──
    # Left-column right-border (RX=105, angle=0):
    #   glabel("NAME", CL, y, 0) → wire(RX,y,CL,y), label at RX
    # Right-column left-border (LX=130, angle=180):
    #   glabel("NAME", CR, y, 180) → wire(CR,y,LX,y), label at LX
    CL = RX + STUB   # = 112.62
    CR = LX - STUB   # = 122.38  (gap 112.62 → 122.38 = 9.76 mm between labels)

    for (pname, _pshape, _bx, by, _ang) in (PINS_POWER + PINS_ZC + PINS_XOR +
                                              PINS_OPTO + PINS_TRIACS):
        E.append(glabel(pname, CL, by, 0, "bidirectional"))

    for (pname, _pshape, _bx, by, _ang) in (PINS_RAIL + PINS_MCU):
        E.append(glabel(pname, CR, by, 180, "bidirectional"))

    # ── Connectors J4, J5, J6 ──────────────────────────────────────────
    # Placed at x=190 (right of right column at x=110+65=175).
    # Conn_01x02 pin positions (sym→sch with Y-flip, angle=0):
    #   Pin 1: sym(-5.08, 0)     → sch(cx-5.08, cy)
    #   Pin 2: sym(-5.08, -2.54) → sch(cx-5.08, cy+2.54)
    # Conn_01x03:
    #   Pin 1: sym(-5.08, 2.54)  → sch(cx-5.08, cy-2.54)
    #   Pin 2: sym(-5.08, 0)     → sch(cx-5.08, cy)
    #   Pin 3: sym(-5.08, -2.54) → sch(cx-5.08, cy+2.54)

    # J4: Track Input (2-pin): AC_IN, AC_RTN
    # glabel() x = label position; for angle=180, wire extends right (+STUB) to pin
    j4x, j4y = 190, 110
    j4px = j4x - 5.08          # pin x-coordinate
    E.append(placed_conn(u(), "Connector_Generic:Conn_01x02",
                         j4x, j4y, 0, "J4", "Track Input",
                         "TerminalBlock_Phoenix:TerminalBlock_Phoenix_MKDS-1,5-2-5.08_1x02_P5.08mm_Horizontal",
                         ["1", "2"]))
    E.append(glabel("AC_IN",  j4px - STUB, j4y,        180, "bidirectional"))
    E.append(glabel("AC_RTN", j4px - STUB, j4y + 2.54, 180, "bidirectional"))

    # J5: Motor Windings (2-pin): WINDING_A, WINDING_B
    j5x, j5y = 190, 135
    j5px = j5x - 5.08
    E.append(placed_conn(u(), "Connector_Generic:Conn_01x02",
                         j5x, j5y, 0, "J5", "Motor Windings",
                         "TerminalBlock_Phoenix:TerminalBlock_Phoenix_MKDS-1,5-2-5.08_1x02_P5.08mm_Horizontal",
                         ["1", "2"]))
    E.append(glabel("WINDING_A", j5px - STUB, j5y,        180, "bidirectional"))
    E.append(glabel("WINDING_B", j5px - STUB, j5y + 2.54, 180, "bidirectional"))

    # J6: Motor+AC (3-pin): AC_IN, WINDING_A, WINDING_B
    j6x, j6y = 190, 165
    j6px = j6x - 5.08
    E.append(placed_conn(u(), "Connector_Generic:Conn_01x03",
                         j6x, j6y, 0, "J6", "Motor+AC",
                         "TerminalBlock_Phoenix:TerminalBlock_Phoenix_MKDS-1,5-3-5.08_1x03_P5.08mm_Horizontal",
                         ["1", "2", "3"]))
    E.append(glabel("AC_IN",     j6px - STUB, j6y - 2.54, 180, "bidirectional"))
    E.append(glabel("WINDING_A", j6px - STUB, j6y,        180, "bidirectional"))
    E.append(glabel("WINDING_B", j6px - STUB, j6y + 2.54, 180, "bidirectional"))

    # NOTE: PWR_FLAG symbols for +15V and +3V3 are on the power-supply
    # sub-sheet (gen_power_supply.py).  GND is driven by the bridge
    # rectifier on that same sheet.  No standalone flags needed here.

    # ── Assemble ───────────────────────────────────────────────────────
    lib_sec = make_lib_section(libs)
    body    = "\n".join(E)
    sinst   = ('\t(sheet_instances\n'
               '\t\t(path "/"\n'
               '\t\t\t(page "1")\n'
               '\t\t)\n'
               '\t)')

    return (f'(kicad_sch\n'
            f'\t(version 20250114)\n'
            f'\t(generator "eeschema")\n'
            f'\t(generator_version "9.0")\n'
            f'\t(uuid "{ROOT_UUID}")\n'
            f'\t(paper "A4")\n'
            f'\t(title_block\n'
            f'\t\t(title "Traincontrol Shield — Top Level")\n'
            f'\t\t(date "2026-02-21")\n'
            f'\t\t(rev "1")\n'
            f'\t\t(company "")\n'
            f'\t)\n'
            f'{lib_sec}\n'
            f'{body}\n'
            f'{sinst}\n'
            f')')


def write_sheet(filename, content):
    path = os.path.join(BASE, filename)
    with open(path, "w") as f:
        f.write(content)
    print(f"Written {len(content):6d} bytes → {path}")


if __name__ == "__main__":
    write_sheet("traincontrol-shield.kicad_sch", gen_top_level())
    print("Top-level sheet generated.")

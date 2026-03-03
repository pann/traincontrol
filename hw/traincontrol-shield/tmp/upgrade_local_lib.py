#!/usr/bin/env python3
"""Convert easyeda2kicad KiCad 6 format library to KiCad 9 format.

KiCad 6 (version 20211014) differences from KiCad 9 (version 20250114):
  1. Properties: multi-line with (id N) token
     Old: (property\n  "Name"\n  "Value"\n  (id N)\n  (at ...)\n  (effects ...)\n)
     New: (property "Name" "Value"\n      (at ...)\n      (effects ...)\n    )
  2. Strokes: include (color R G B A)
     Old: (stroke (width 0) (type default) (color 0 0 0 0))
     New: (stroke (width 0) (type default))
  3. Effects hide: bare token 'hide' at end
     Old: (effects (font (size 1.27 1.27) ) hide)
     New: (effects (font (size 1.27 1.27)) (hide yes))
  4. Effects trailing space: (font (size S S) ) → (font (size S S))
"""
import re
import os

BASE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
SRC  = os.path.join(BASE, "libs/traincontrol-shield.kicad_sym")
DST  = os.path.join(BASE, "libs/traincontrol-shield.kicad_sym")


def upgrade_sym_content(content):
    """Convert a KiCad 6 symbol library content to KiCad 9 format."""

    # ── Step 1: Update library version ────────────────────────────────
    content = content.replace("(version 20211014)", "(version 20250114)")
    content = content.replace(
        "(generator https://github.com/uPesy/easyeda2kicad.py)",
        '(generator "eeschema")\n  (generator_version "9.0")'
    )

    # ── Step 2: Convert multi-line properties with (id N) ─────────────
    # Pattern (multiline):
    #   (property
    #     "Name"
    #     "Value"
    #     (id N)
    #     (at X Y R)
    #     (effects ...)
    #   )
    # Target:
    #   (property "Name" "Value"
    #     (at X Y R)
    #     (effects ...)
    #   )
    #
    # Strategy: use a state machine to parse (property ...) blocks
    output = []
    i = 0
    while i < len(content):
        # Look for "(property\n" pattern (multi-line old-format property)
        m = re.match(r'\(property\n', content[i:])
        if m:
            # Find the closing paren of this property block
            depth = 0
            j = i
            while j < len(content):
                if content[j] == '(':
                    depth += 1
                elif content[j] == ')':
                    depth -= 1
                    if depth == 0:
                        break
                j += 1
            # Extract the property block (including outer parens)
            prop_block = content[i:j+1]

            # Extract contents between outer parens
            inner = prop_block[len('(property'):prop_block.rfind(')')].strip()
            # Remove (id N) tokens
            inner = re.sub(r'\(id \d+\)\s*', '', inner)
            # Parse: first token = name (quoted), second = value (quoted), rest = attrs
            # Use a simple parser for the two leading quoted strings
            qm = re.match(
                r'"((?:[^"\\]|\\.)*)"\s+"((?:[^"\\]|\\.)*)"(.*)',
                inner, re.DOTALL
            )
            if qm:
                name = qm.group(1)
                value = qm.group(2)
                attrs = qm.group(3).strip()
                # Rebuild as single-line start with attrs on next line(s)
                prop_new = f'(property "{name}" "{value}"\n      {attrs}\n    )'
                output.append(prop_new)
            else:
                # Fallback: just remove (id N)
                prop_new = re.sub(r'\s*\(id \d+\)', '', prop_block)
                output.append(prop_new)

            i = j + 1
        else:
            output.append(content[i])
            i += 1

    content = ''.join(output)

    # ── Step 3: Remove (color R G B A) from strokes ───────────────────
    # Pattern: (stroke (width W) (type T) (color R G B A))
    content = re.sub(
        r'\(stroke\s+\(width\s+([^)]+)\)\s+\(type\s+([^)]+)\)\s+\(color\s+[\d.\s]+\)\s*\)',
        r'(stroke (width \1) (type \2))',
        content
    )

    # ── Step 4: Fix effects trailing space ────────────────────────────
    # (effects (font (size S S) ) hide) → (effects (font (size S S)) (hide yes))
    content = re.sub(
        r'\(effects\s+\(font\s+\(size\s+([\d.]+\s+[\d.]+)\)\s*\)\s+hide\)',
        r'(effects (font (size \1)) (hide yes))',
        content
    )
    # (effects (font (size S S) ) ) → (effects (font (size S S)))
    content = re.sub(
        r'\(effects\s+\(font\s+\(size\s+([\d.]+\s+[\d.]+)\)\s*\)\s*\)',
        r'(effects (font (size \1)))',
        content
    )

    # ── Step 5: Add missing KiCad 9 symbol-level attributes ───────────
    # After (on_board yes) or (in_bom yes), add (exclude_from_sim no) if missing
    # We do this only for top-level symbols (not sub-symbols)
    def add_exclude_from_sim(m):
        block = m.group(0)
        if '(exclude_from_sim' not in block:
            block = block.replace('(in_bom yes)', '(exclude_from_sim no)\n    (in_bom yes)')
        return block

    # ── Step 6: Add (embedded_fonts no) before closing paren of each symbol ─
    # Handled below

    return content


def process_library(src, dst):
    with open(src) as f:
        content = f.read()

    print(f"Input:  {len(content)} bytes, {content.count(chr(10))} lines")
    content = upgrade_sym_content(content)
    print(f"Output: {len(content)} bytes, {content.count(chr(10))} lines")

    with open(dst, 'w') as f:
        f.write(content)
    print(f"Written → {dst}")

    # Verify: check for remaining (id N) tokens
    remaining_ids = re.findall(r'\(id \d+\)', content)
    if remaining_ids:
        print(f"WARNING: {len(remaining_ids)} remaining (id N) tokens!")
    else:
        print("OK: No remaining (id N) tokens")

    # Check for (color ...) in strokes
    remaining_colors = re.findall(r'\(stroke[^)]*\(color', content)
    if remaining_colors:
        print(f"WARNING: {len(remaining_colors)} remaining stroke colors!")
    else:
        print("OK: No remaining stroke color tokens")

    # Check for bare 'hide' in effects
    remaining_hide = re.findall(r'\) hide\)', content)
    if remaining_hide:
        print(f"WARNING: {len(remaining_hide)} remaining bare 'hide' tokens!")
    else:
        print("OK: No remaining bare 'hide' tokens")


if __name__ == "__main__":
    process_library(SRC, DST)

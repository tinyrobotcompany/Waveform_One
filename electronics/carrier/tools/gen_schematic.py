"""Writes carrier.kicad_sch from netlist.py.

Each part is drawn from its KiCad library symbol. Every connected pin gets a
short wire and a net label; unused pins get a no-connect flag. Parts are laid
out in titled groups. Run with any Python 3.9+:
    python3 electronics/carrier/tools/gen_schematic.py
then check with: kicad-cli sch erc electronics/carrier/carrier.kicad_sch
"""
import math
import uuid
from pathlib import Path

import kicad_lib as lib
from kicad_lib import String as S
from netlist import GROUP_TITLES, PARTS, POWER_FLAGS

ROOT = Path(__file__).resolve().parents[1]
NAMESPACE = uuid.UUID("6f1e2d3c-4b5a-4987-a6b5-c4d3e2f1a0b9")
SHEET_UUID = str(uuid.uuid5(NAMESPACE, "sheet"))
GRID = 2.54
STUB = 2.54
LABEL_ROOM = 22.86     # horizontal space for labels either side of a symbol
GROUP_WIDTH = 270.0
GROUPS = {             # top-left corner of each group on an A1 sheet (841 x 594)
    "power": (25.4, 40.64),
    "supply": (25.4, 170.18),
    "usbc_power": (25.4, 279.4),
    "s3": (325.12, 40.64),
    "hub75": (325.12, 309.88),
    "connectors": (604.52, 40.64),
}


def uid(*parts):
    return S(str(uuid.uuid5(NAMESPACE, "/".join(parts))))


def num(value):
    """Exact decimal text for a coordinate (KiCad compares wire ends exactly)."""
    text = f"{value:.4f}".rstrip("0").rstrip(".")
    return "0" if text in ("-0", "") else text


def snap(value):
    return round(round(value / 1.27) * 1.27, 4)


def font(hidden=False, justify=None):
    effects = ["effects", ["font", ["size", "1.27", "1.27"]]]
    if justify:
        effects.append(["justify"] + justify)
    if hidden:
        effects.append(["hide", "yes"])
    return effects


def prop(name, value, x, y, hidden=False):
    return ["property", S(name), S(value), ["at", num(x), num(y), "0"], font(hidden)]


def resolve_pins(part, pins):
    """netlist pin keys (number or name) to {number: net}."""
    by_name = {}
    for number, name, *_ in pins:
        by_name.setdefault(name, []).append(number)
    numbers = {number for number, *_ in pins}
    resolved = {}
    for key, net in part.pins.items():
        if key in numbers:
            resolved[key] = net
        elif key in by_name:
            for number in by_name[key]:
                resolved[number] = net
        else:
            raise KeyError(f"{part.ref}: no pin {key!r} in {part.symbol}")
    return resolved


def bbox(pins):
    xs = [x for *_, x, _, _ in pins] or [0]
    ys = [y for *_, _, y, _ in pins] or [0]
    return min(xs), max(xs), min(ys), max(ys)


def place_groups():
    """(part, x, y) for every part, row-packed inside its group."""
    placed = []
    by_group = {}
    for part in PARTS:
        by_group.setdefault(part.group, []).append(part)
    for group, parts in by_group.items():
        x0, y0 = GROUPS[group]
        x, y, row_height = x0, y0 + 10.16, 0.0
        for part in parts:
            pins = lib.pins(lib.symbol(part.symbol))
            left, right, bottom, top = bbox(pins)
            width = (right - left) + 2 * LABEL_ROOM
            height = (top - bottom) + 15.24
            if x + width > x0 + GROUP_WIDTH and x > x0:
                x, y, row_height = x0, y + row_height, 0.0
            sx, sy = snap(x + LABEL_ROOM - left), snap(y + top + 5.08)
            placed.append((part, sx, sy))
            x += width
            row_height = max(row_height, height)
    return placed


def wire(a, b, key):
    return ["wire", ["pts", ["xy", num(a[0]), num(a[1])], ["xy", num(b[0]), num(b[1])]],
            ["stroke", ["width", "0"], ["type", "default"]], ["uuid", uid("wire", key)]]


def label(net, at, outward, key):
    dx, dy = outward
    angle, justify = (0, ["left", "bottom"]) if dx > 0.5 else (180, ["right", "bottom"]) if dx < -0.5 \
        else (90, ["left", "bottom"]) if dy < 0 else (270, ["right", "bottom"])
    return ["label", S(net), ["at", num(at[0]), num(at[1]), str(angle)], font(justify=justify), ["uuid", uid("label", key)]]


def symbol_instance(lib_id, ref, value, footprint, x, y, pin_numbers, dnp=False, note="", in_bom=True):
    node = ["symbol", ["lib_id", S(lib_id)], ["at", num(x), num(y), "0"], ["unit", "1"],
            ["exclude_from_sim", "no"], ["in_bom", "yes" if in_bom else "no"], ["on_board", "yes"],
            ["dnp", "yes" if dnp else "no"], ["uuid", uid("symbol", ref)],
            prop("Reference", ref, x + 2.54, y - 6.35, hidden=ref.startswith("#")),
            prop("Value", value, x + 2.54, y + 6.35, hidden=ref.startswith("#")),
            prop("Footprint", footprint, x, y, hidden=True),
            prop("Datasheet", "~", x, y, hidden=True)]
    if note:
        node.append(prop("Note", note, x, y, hidden=True))
    for number in sorted(set(pin_numbers)):
        node.append(["pin", S(number), ["uuid", uid("pin", ref, number)]])
    node.append(["instances", ["project", S("carrier"),
                               ["path", S("/" + SHEET_UUID), ["reference", S(ref)], ["unit", "1"]]]])
    return node


def connect(ref, pins, nets, x, y, items):
    """Stub wires, labels and no-connects for one placed symbol."""
    at_point = {}
    for number, _, _, px, py, angle in pins:
        at_point.setdefault((round(px, 3), round(py, 3), angle), []).append(number)
    for (px, py, angle), numbers in at_point.items():
        connected = {nets[n] for n in numbers if nets.get(n)}
        if len(connected) > 1:
            raise ValueError(f"{ref}: stacked pins {numbers} on different nets {connected}")
        end = (round(x + px, 4), round(y - py, 4))
        if not connected:
            items.append(["no_connect", ["at", num(end[0]), num(end[1])], ["uuid", uid("nc", ref, numbers[0])]])
            continue
        net = connected.pop()
        a = math.radians(angle)
        outward = (-round(math.cos(a)), round(math.sin(a)))
        tip = (round(end[0] + outward[0] * STUB, 4), round(end[1] + outward[1] * STUB, 4))
        items.append(wire(end, tip, f"{ref}.{numbers[0]}"))
        items.append(label(net, tip, outward, f"{ref}.{numbers[0]}"))


def main():
    items, symbols_used = [], {}
    for part, x, y in place_groups():
        definition = lib.symbol(part.symbol)
        symbols_used[part.symbol] = definition
        pins = lib.pins(definition)
        nets = resolve_pins(part, pins)
        unknown = set(part.pins) - set(nets) - {name for _, name, *_ in pins}
        if unknown:
            raise KeyError(f"{part.ref}: {unknown}")
        # Holes and solder jumpers are features of the bare board, not parts to buy.
        bought = not part.ref.startswith(("H", "JP"))
        items.append(symbol_instance(part.symbol, part.ref, part.value, part.footprint, x, y,
                                     [number for number, *_ in pins], part.dnp, part.note, in_bom=bought))
        connect(part.ref, pins, nets, x, y, items)
    # Power flags: nets fed only through passive parts still count as driven.
    flag = lib.symbol("power:PWR_FLAG")
    symbols_used["power:PWR_FLAG"] = flag
    fx, fy = 25.4, 25.4
    for index, net in enumerate(POWER_FLAGS):
        x, y = snap(fx + 30 * index + 10), snap(fy)
        ref = f"#FLG{index + 1:02d}"
        items.append(symbol_instance("power:PWR_FLAG", ref, "PWR_FLAG", "", x, y, ["1"], in_bom=False))
        connect(ref, lib.pins(flag), {"1": net}, x, y, items)
    # Group titles.
    for group, (x, y) in GROUPS.items():
        items.append(["text", S(GROUP_TITLES[group]), ["exclude_from_sim", "no"],
                      ["at", num(x), num(y), "0"],
                      ["effects", ["font", ["size", "2.54", "2.54"], ["bold", "yes"]], ["justify", "left", "bottom"]],
                      ["uuid", uid("title", group)]])
    sheet = ["kicad_sch", ["version", "20250114"], ["generator", S("waveform_one_gen_schematic")],
             ["generator_version", S("10.0")], ["uuid", S(SHEET_UUID)], ["paper", S("A1")],
             ["title_block", ["title", S("Waveform One carrier board")], ["rev", S("1")],
              ["company", S("Waveform One")],
              ["comment", "1", S("Generated by electronics/carrier/tools/gen_schematic.py from netlist.py; do not edit by hand.")]],
             ["lib_symbols"] + list(symbols_used.values())]
    sheet += items
    sheet.append(["sheet_instances", ["path", S("/"), ["page", S("1")]]])
    sheet.append(["embedded_fonts", "no"])
    output = ROOT / "carrier.kicad_sch"
    output.write_text(lib.dump(sheet) + "\n")
    print(f"Wrote {output.name}: {len(PARTS)} parts")


if __name__ == "__main__":
    main()

"""Minimal reader for KiCad symbol libraries (.kicad_sym S-expressions).

Used by the schematic generator to copy library symbols into the schematic and
to look up pins by name or number. Handles `extends` inheritance.
"""
import os
from pathlib import Path

SYMBOLS = Path(os.environ.get("KICAD_SYMBOLS",
    "/Applications/KiCad/KiCad.app/Contents/SharedSupport/symbols"))


def parse(text):
    """S-expression text to nested lists; strings keep their quotes off."""
    stack, current, i, n = [], [], 0, len(text)
    while i < n:
        c = text[i]
        if c == "(":
            stack.append(current); current = []; i += 1
        elif c == ")":
            done = current; current = stack.pop(); current.append(done); i += 1
        elif c == '"':
            j, out = i + 1, []
            while text[j] != '"':
                if text[j] == "\\":
                    out.append(text[j + 1]); j += 2
                else:
                    out.append(text[j]); j += 1
            current.append(String("".join(out))); i = j + 1
        elif c.isspace():
            i += 1
        else:
            j = i
            while j < n and not text[j].isspace() and text[j] not in "()":
                j += 1
            current.append(text[i:j]); i = j
    return current[0]


class String(str):
    """A quoted string, so it is written back with quotes."""


def dump(node, indent=0):
    if isinstance(node, list):
        if all(not isinstance(item, list) for item in node):
            return "(" + " ".join(dump(item) for item in node) + ")"
        head = [dump(item) for item in node if not isinstance(item, list)]
        body = [dump(item, indent + 1) for item in node if isinstance(item, list)]
        pad = "\n" + "\t" * (indent + 1)
        return "(" + " ".join(head) + "".join(pad + b for b in body) + "\n" + "\t" * indent + ")"
    if isinstance(node, String):
        return '"' + node.replace("\\", "\\\\").replace('"', '\\"') + '"'
    return str(node)


def find(node, head):
    return [item for item in node if isinstance(item, list) and item and item[0] == head]


_cache = {}


def library(name):
    if name not in _cache:
        _cache[name] = parse((SYMBOLS / f"{name}.kicad_sym").read_text())
    return _cache[name]


def symbol(lib_id):
    """The symbol definition for 'Library:Name', with `extends` resolved and
    renamed to the full lib_id as a schematic's lib_symbols expects."""
    lib, name = lib_id.split(":", 1)
    tree = library(lib)
    definitions = {item[1]: item for item in find(tree, "symbol")}
    if name not in definitions:
        raise KeyError(f"{lib_id} not in library")
    node = definitions[name]
    extends = find(node, "extends")
    if extends:
        base = definitions[extends[0][1]]
        merged = [item for item in base if not (isinstance(item, list) and item[0] == "property")]
        merged = [item for item in merged if not (isinstance(item, list) and item[0] == "symbol")]
        merged[1] = name
        # Own properties first, then the base's units renamed to this symbol.
        merged += [item for item in node if isinstance(item, list) and item[0] == "property"]
        for unit in find(base, "symbol"):
            renamed = list(unit)
            renamed[1] = String(unit[1].replace(base[1], name, 1))
            merged.append(renamed)
        node = merged
    out = [item for item in node if not (isinstance(item, list) and item[0] == "extends")]
    out = list(out)
    out[1] = String(lib_id)
    return out


def pins(definition):
    """[(number, name, electrical_type, x, y, angle)] across all units."""
    found = []
    for unit in find(definition, "symbol"):
        for pin in find(unit, "pin"):
            at = find(pin, "at")[0]
            name = find(pin, "name")[0][1]
            number = find(pin, "number")[0][1]
            found.append((str(number), str(name), pin[1], float(at[1]), float(at[2]), float(at[3]) if len(at) > 3 else 0.0))
    return found

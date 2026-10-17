"""Writes led-adapter.kicad_sch from netlist.py, with the carrier's generator.
Run with any Python 3.9+:
    python3 electronics/led-adapter/tools/gen_schematic.py
"""
import uuid

from reuse import NAMESPACE, carrier, netlist

base = carrier("gen_schematic")
base.SHEET_UUID = str(uuid.uuid5(NAMESPACE, "sheet"))
base.GROUPS = netlist.GROUPS
base.TITLE = "Waveform One LED adapter board"

if __name__ == "__main__":
    base.main()

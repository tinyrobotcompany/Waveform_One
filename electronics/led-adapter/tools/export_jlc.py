"""Writes the JLCPCB order files into electronics/led-adapter/fab/, with the
carrier's exporter. Run with any Python 3.9+ after route_board.py:
    python3 electronics/led-adapter/tools/export_jlc.py
"""
from reuse import ROOT, carrier

base = carrier("export_jlc")
base.OUT = ROOT / "fab"

if __name__ == "__main__":
    base.main()

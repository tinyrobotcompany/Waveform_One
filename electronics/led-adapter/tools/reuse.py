"""Loads the carrier board's generators and points them at this board.

The carrier's tools take their board folder, file names and title from module
globals; this sets those for the LED adapter. Import netlist (this board's)
before calling carrier(), so the generators' `import netlist` finds it.
"""
import importlib.util
import sys
import uuid
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

import netlist  # noqa: E402  (this board's; also puts the carrier's tools on the path)

# Distinct from the carrier's, so the two projects' symbol IDs never coincide.
NAMESPACE = uuid.UUID("3b8f0a52-6c1d-4e7a-9f20-5d4c3b2a1e0f")


def carrier(name):
    spec = importlib.util.spec_from_file_location(f"carrier_{name}", netlist.CARRIER_TOOLS / f"{name}.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    module.ROOT = ROOT
    if hasattr(module, "NAMESPACE"):
        module.NAMESPACE = NAMESPACE
    if hasattr(module, "BOARD"):
        module.BOARD = ROOT / f"{ROOT.name}.kicad_pcb"
    return module

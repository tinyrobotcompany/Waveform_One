"""Builds led-adapter.kicad_pcb's outline, holes, placement and pad nets from
board.json and netlist.py, with the carrier's builder, then adds the
silkscreen labels from board.json. Run with KiCad's Python:
  /Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3 \
      electronics/led-adapter/tools/build_board.py
"""
import json

import pcbnew

from reuse import ROOT, carrier

base = carrier("build_board")


def labels():
    """Silkscreen text: which cable goes where."""
    board = pcbnew.LoadBoard(str(ROOT / f"{ROOT.name}.kicad_pcb"))
    for text, x, y, size in json.loads((ROOT / "board.json").read_text())["labels"]:
        item = pcbnew.PCB_TEXT(board)
        item.SetText(text)
        item.SetPosition(base.mm(x, y))
        item.SetLayer(pcbnew.F_SilkS)
        item.SetTextSize(pcbnew.VECTOR2I_MM(size, size))
        item.SetTextThickness(pcbnew.FromMM(size * 0.15))
        board.Add(item)
    board.Save(str(ROOT / f"{ROOT.name}.kicad_pcb"))


if __name__ == "__main__":
    base.main()
    labels()

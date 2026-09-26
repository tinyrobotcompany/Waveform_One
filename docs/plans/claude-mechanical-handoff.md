# Claude mechanical development handoff

Use Claude Code CLI (or Desktop's local Code tab), with Autodesk Fusion available
for executing and visually inspecting the generated Fusion Python script.

## Paste this instruction into Claude

You are developing the Waveform One enclosure in parallel with another agent
working on Pi software and ESP firmware. You are not alone in the repository.
Create your own branch `feature/mechanical-m0` and worktree
`.worktrees/mechanical-m0` from current origin/main. Do not switch or modify the
other worktrees, revert another agent's changes, change Git core.bare, or edit
Pi software, firmware, protocols, CI, repository-wide agent instructions or secrets.

Read these files from your own worktree:
1. docs/prd/WAVEFORM_ONE_MECHANICAL_CAD_PRD_v2.md
2. docs/prd/waveform_one_precise_purchasing_bom_v1.md
3. docs/prd/waveform_one_v1_architecture_engineering_and_social_strategy_v2.md
4. firmware/esp32/README.md
5. mechanical/ (inspect actual implementation; the current Python files are empty scaffolding)

Hardware corrections override older PRD/BOM assumptions for this prototype:
- Actual SBC is Raspberry Pi 4 Model B, NOT Pi 5.
- Actual screen is a 7-inch Pi display, NOT the PRD's 5-inch display.
  Exact model, board outline, mounting holes, thickness and connector dimensions
  need measurement; do not infer them from the diagonal size.
- ESP32-S3 drives a Waveshare 64x32 HUB75 panel directly; the two buffer chips
  from the old prototype are no longer in the working signal path. This does
  not freeze production electrical design.
- INMP441 microphone connects to ESP GPIO5/6/7, 3V3 and GND as documented.
- Pi powers via USB-C; Pi-to-ESP data uses USB-A; LED panel has its own 5V supply.
  Reserve real connector and cable-bend space and accessible service connections.
- Touchscreen and phone browser controls are planned. Whether physical encoders
  remain in the enclosure is unresolved: retain a parametric option, ask before
  committing front-panel holes. Do not silently remove LP sleeve support.

Own only mechanical/**, mechanical-specific tests, and mechanical documentation.
Preserve Fusion as authoritative parametric CAD with Python generation. Centralize
all dimensions, units, coordinate conventions and PROVISIONAL/MEASURED status.
Do not invent board dimensions or treat old BOM values as measured.

Implement mechanical milestone M0 ONLY, following the PRD: a rerunnable Fusion
script creating the WaveformOne root and a reference envelope 330mm wide x
145mm deep x 245mm high; X left/right, Y front/rear, Z up, origin at horizontal
centre/front/bottom. The envelope is reference geometry, NOT a printable part.
No full enclosure, holes, electronics placeholders or exports represented as
print-ready yet. Document the real-hardware measurement checklist for later stages.

Respect the specified Elegoo Centauri Carbon engineering print envelope of
245mm per axis for printable parts. Start with calibration coupons in the
appropriate later milestone. Keep dimensional/code validation separate from
actual Fusion execution, visual inspection, slicer validation and physical fit.
Do not claim Fusion execution or print validation unless actually performed.

Deliver source, run instructions, validation results, screenshots if Fusion was
run, and unresolved measurements. Stop at the M0 milestone for review as the
mechanical PRD requires. Commit within your own branch; do not merge main.

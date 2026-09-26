---
"waveform-one": patch
---
Use noise-subtracted spectral RMS to gate the visualizer, so quieter music in
clear frequency bands is not hidden by the total startup room-noise threshold.
Keep silence suppression, short-dip hold and fixed display scaling. Add explicit
GATE_RMS diagnostics and a sustained quiet-music regression test. The standalone
microphone diagnostic retains its reference raw-RMS gate.

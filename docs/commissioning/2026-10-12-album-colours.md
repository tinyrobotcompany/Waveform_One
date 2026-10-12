# Album-colour prototype — 12 October 2026

## Scope

Implemented in `feature/p4-recognition-stability`, worktree `.worktrees/p4-recognition-stability`.
The P4 extracts a shared palette from its existing decoded RGB565 cover. A fixed 32×32
sampling grid and twelve hue buckets limit work to 1024 pixels with no additional heap
allocation. Chromatic coverage, area and brightness weighting select dark, muted backgrounds
and readable primary/secondary accents. Missing or sufficiently neutral covers use the
standard near-black, green/lavender palette. This is our own heuristic, not Apple's algorithm.

P4 and phone use the same background, panel, border/status and search colour values.
The native screen uses a matte tint: the gradient preview showed visible RGB565 bands.
Changes fade over 700 ms. Rechecks and song changes sharing an artwork URL retain the palette.
The search waveform accepts palette colours while retaining its original colours by default.

An Album colours / Classic black choice is available in both settings interfaces. The device
persists the choice in NVS. The phone receives the palette and preference in its authenticated
state response; it does not sample third-party images or require CORS permissions. The new
appearance endpoint uses the existing pairing, origin and CSRF checks, rejects other values,
and reports persistence failures. No credentials are erased.

The S3, capture, fingerprinting, recognition service, retries and session policy are unchanged.

## Offline validation

- Full firmware host tests passed with ASan/UBSan, including bounded sampling, neutral/tiny
  colour fallbacks, deterministic extraction, text/accent contrast and remote state lifecycle.
- Chrome checks passed for phone layouts, artwork fallback, search/check retention, both
  appearance choices, CSRF-bearing requests, settings and safe text/session loss.
- The actual LVGL helper/panel code passed native font/layout checks. Rendered the four
  supplied album covers and verified the native appearance buttons fit without overlap.
- Offline phone/MacBook previews rendered all four references and Classic black without
  browser errors. Supplied screenshots were cropped only into local temporary preview fixtures;
  no album images are committed to the repository.
- ESP-IDF 6.1 P4 build passed. `git diff --check` passed.

Local preview: `/Users/simonholmes/Projects/Applications/Waveform_One/build/album-colours/renders/index.html`.
Native comparison: `/Users/simonholmes/Projects/Applications/Waveform_One/build/album-colours/renders/p4-comparison.png`.
Individual phone/P4 software renders are in `renders/`; the ready-to-flash binary and
checksum are in the adjacent `firmware/` folder. Logs, fixtures and preview tools are grouped
under the same project-local `build/album-colours/` folder, excluded from Git.
These are generated local artifacts, not screenshots from physical hardware.

P4 application SHA-256: `7e686019bbdd9e9a2af86e3dd3142ae9a16cbe067335bf7e5611f1961eff7080`.

## Hardware status and next check

No hardware was flashed or tested. The user is at work and explicitly requested waiting for
further instructions this evening. The connected-device baseline remains the previously
flashed complete pre-filter rollback. The user approved the software render; physical-device acceptance remains pending.

After the user instructs us to continue: flash only the P4, verify preference synchronization
and persistence on the device/phone, check album/search transitions and render stability,
and confirm that the additional background fade does not disturb normal operation. Actual
panel colour, touch use and animation cost remain unverified. Announce any audio test before
starting and wait for the user's answer if asking a question.

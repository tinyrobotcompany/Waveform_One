# 0002 — Prototype hardware corrections and envelope

Date: 2026-09-26
Status: Accepted for prototype; envelope frozen only after M3 review

## Hardware (overrides older PRD/BOM assumptions)

| Item | PRD/BOM said | Actual prototype |
|---|---|---|
| SBC | Raspberry Pi 5 | Raspberry Pi 4 Model B; no cooler yet, space reserved for a fan-heatsink |
| Screen | Waveshare 5-inch | Official Raspberry Pi 7-inch Touchscreen (original, DSI) |
| LED panel | Waveshare **P4** 64×32 (256×128 mm) | Waveshare **P3** 64×32 (192×96 mm); owner measured ~195×100×12 mm |
| Matrix drive | via buffer board | ESP32-S3-DevKitC-1 drives the panel directly |
| Controls | 3 rotary encoders | Touchscreen + phone; **encoders deferred** (parameters kept, no front holes) |
| LED diffuser | — | Owner already has one; must be measured |
| LED 5 V supply | — | External regulated supply (bench setup); rear inlet in M5 |

Power: Pi via USB-C; Pi→ESP32 over USB-A; LED panel on its own 5 V supply.

Open with Codex: whether the ESP32 and wiring move onto a custom PCB. The ESP32
zone behind the matrix is where such a board would go.

## Envelope

With the P3 panel, the display (~110 mm) stacked above the matrix (96 mm)
needs ~232 mm including bezel bands, so the PRD's **330 × 145 × 245 mm**
envelope holds (13 mm above the display). An earlier revision raised the height
to 265 mm on the assumption of a P4 panel; that is withdrawn.

Width exceeds the 245 mm print envelope, so the front must be split (M3).

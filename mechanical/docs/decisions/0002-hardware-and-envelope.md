# 0002 — Prototype hardware corrections and envelope height

Date: 2026-09-26
Status: Accepted for prototype; envelope frozen only after M3 review

## Hardware (overrides older PRD/BOM assumptions)

| Item | PRD/BOM said | Actual prototype |
|---|---|---|
| SBC | Raspberry Pi 5 | Raspberry Pi 4 Model B |
| Screen | Waveshare 5-inch | Official Raspberry Pi 7-inch Touchscreen (original, DSI) |
| Matrix drive | via buffer board | ESP32-S3 drives Waveshare 64×32 HUB75 directly |
| Controls | 3 rotary encoders | Touchscreen + phone browser; **encoders deferred** (parametric option kept, no front holes) |
| LED diffuser | — | Owner already has one; must be measured |

Power: Pi via USB-C; Pi→ESP32 over USB-A; LED panel on its own 5 V supply.

## Envelope height

The official 7-inch display's published outline is about 194 × 110 mm and the
P4 64×32 matrix is 256 × 128 mm (both DATASHEET, unmeasured). Stacked
vertically, as in the PRD front layout, they need ~238 mm before walls, bezel
lips, separation and clearance, which the PRD's 245 mm height cannot hold.

`CASE_HEIGHT` is raised to **265 mm (PROVISIONAL)**. Width (330 mm) and depth
(145 mm) are unchanged. The M3 printed front-layout mock-up decides the final
value.

Consequence: both width and height exceed the 245 mm print envelope, so the
front must split in both directions (or into panels printed lying flat, each
within 245 × 245 mm). This is resolved in M3.

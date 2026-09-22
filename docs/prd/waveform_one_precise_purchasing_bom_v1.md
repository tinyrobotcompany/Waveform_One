# WAVEFORM ONE — Precise Purchasing BOM (V1 Prototype)

**Purpose:** Orderable V1 prototype BOM for the Waveform One hardware build.

**Important:** This BOM is optimized for a first working prototype in France/EU. It deliberately separates **prototype power** from the later single-supply integrated enclosure. It also corrects one component choice from the earlier PRD: **do not buy the INMP441 microphone**. TDK lists INMP441 as NRND and DigiKey lists the underlying part as obsolete. For the prototype, use a current, supported I²S microphone breakout instead.

---

## 1. Recommended purchasing strategy

Use four suppliers rather than forcing everything through one store:

1. **Mouser France / DigiKey France** — exact semiconductor, encoder and development-board part numbers.
2. **Waveshare direct or an official EU/French Waveshare distributor** — exact LCD and HUB75 matrix.
3. **Kubii / authorised Raspberry Pi reseller** — Raspberry Pi 5, official cooler and official Pi PSU.
4. **Reichelt / TME** — external regulated 5 V supply and general power hardware.

**Seeed Studio is a good supplier**, especially for maker hardware, Grove/XIAO modules and later PCB/PCBA manufacturing. It is not the best single-source supplier for this specific BOM because Waveform One depends on exact parts from Raspberry Pi, Espressif, Texas Instruments, Bourns and Waveshare.

---

# 2. Core compute and display

| Ref | Component | Exact part / specification | Qty to buy | Primary source | Notes |
|---|---|---|---:|---|---|
| C01 | Main SBC | **Raspberry Pi 5, 4 GB RAM** | 1 | Kubii / authorised Pi reseller | Do not substitute Pi 4. |
| C02 | Pi active cooling | **Raspberry Pi Active Cooler, MPN SC1148** | 1 | Kubii / RS / authorised Pi reseller | Required for sustained appliance use. |
| C03 | Pi prototype PSU | **Official Raspberry Pi 27 W USB-C Power Supply, EU plug** | 1 | Kubii / authorised Pi reseller | Use this during development instead of back-powering GPIO. |
| C04 | Storage | **64 GB high-endurance microSD, U3/A2**, e.g. SanDisk High Endurance or Samsung PRO Endurance | 2 | Amazon / electronics retailer | One active + one spare/recovery card. |
| C05 | Real-time controller | **Espressif ESP32-S3-DevKitC-1-N8R8** | 2 | Mouser | One active + one spare. Exact 8 MB flash / 8 MB PSRAM version. |
| C06 | Metadata display | **Waveshare 5DP-CAPLCD-H**, 5", 1024×600, IPS, capacitive touch, narrow bezel | 1 | Waveshare direct / OpenELAB / official distributor | Must be **H** version, not B or G. |
| C07 | Visualizer panel | **Waveshare RGB-Matrix-P4-64x32**, Part No. **22101**, HUB75, 256×128 mm, 5 V / 4 A | 1 | Waveshare / official distributor | Must be **P4 64×32**. Mechanical design depends on 256×128 mm size. |

---

# 3. Audio input

## Recommended V1 prototype choice

| Ref | Component | Exact part / specification | Qty to buy | Primary source | Notes |
|---|---|---|---:|---|---|
| A01 | I²S MEMS microphone breakout | **Adafruit I²S MEMS Microphone Breakout — SPH0645LM4H, Product ID 3421** | 2 | Adafruit / DigiKey / BerryBase | Buy two. One active + one spare. 3.3 V logic, I²S digital audio. |
| A02 | 0.1" breakaway header | 2.54 mm male header, single row | 1 strip | Mouser / DigiKey / maker supplier | For microphone breakout if not pre-fitted. |

### Why this replaces INMP441

The earlier PRD selected INMP441 because it is common in hobby modules. For a new design, that is no longer the best purchasing choice. TDK marks INMP441 **NRND** and DigiKey marks the underlying part **obsolete**. For V1, use a supported breakout with known documentation and a stable board layout.

For a later production PCB, the microphone should be re-evaluated again and a current-production MEMS part selected before PCB freeze.

---

# 4. HUB75 level shifting and digital interface

For the breadboard/prototype stage, use through-hole logic so it can be hand-wired and debugged easily.

| Ref | Component | Exact MPN | Qty to buy | Primary source | Notes |
|---|---|---|---:|---|---|
| L01 | 8-bit AHCT bus transceiver | **Texas Instruments SN74AHCT245N** | 4 | Mouser | 2 used, 2 spare. PDIP-20, 5 V supply, TTL-compatible inputs suitable for ESP32 3.3 V → HUB75 5 V logic. |
| L02 | DIP socket | 20-pin, 7.62 mm, turned-pin or quality dual-wipe | 4 | Mouser / DigiKey | Do not solder ICs directly in prototype if avoidable. |
| L03 | CLK series resistor | **47 Ω, 1%, 0.25 W metal film** | 10 | Mouser / DigiKey | Use one initially; spares included. |
| L04 | Logic decoupling capacitor | **100 nF, 50 V, X7R ceramic** | 20 | Mouser / DigiKey | One close to each IC plus spares. |
| L05 | Matrix bulk capacitor | **1000 µF, 10 V or 16 V, low-ESR, 105°C** | 3 | Mouser / DigiKey | One at HUB75 power input; buy spares. |
| L06 | Additional bulk capacitor | **470 µF, 10 V or 16 V, low-ESR** | 3 | Mouser / DigiKey | Useful on 5 V branch/control board. |
| L07 | Pull-up resistors | **10 kΩ, 1%, 0.25 W** | 20 | Mouser / DigiKey | Encoders, OE defaults, misc. |
| L08 | Misc logic resistors | **1 kΩ, 1%, 0.25 W** | 20 | Mouser / DigiKey | Debug/status/interface use. |

---

# 5. Physical controls

| Ref | Component | Exact MPN | Qty to buy | Primary source | Notes |
|---|---|---|---:|---|---|
| U01 | Rotary encoder with push switch | **Bourns PEC11R-4215F-S0024** | 5 | Mouser | 3 used + 2 spare. 24 PPR, 24 detents, 6 mm D/flatted shaft, integrated push switch. |
| U02 | Aluminium knob | **22 mm diameter, 6 mm D-shaft compatible**, ~15 mm deep | 5 | Conrad / Amazon / AliExpress / hi-fi parts supplier | Buy after verifying exact shaft fit. 3 used + spares. |
| U03 | Optional encoder debounce capacitor | **10 nF, 50 V ceramic** | 20 | Mouser / DigiKey | Populate only if hardware debounce is required. |

---

# 6. Prototype power

## 6.1 Raspberry Pi power

Use the official Pi PSU for V1 development:

- **Raspberry Pi 27 W USB-C Power Supply, EU**
- powers only the Raspberry Pi 5

Do **not** back-power the Pi through the GPIO header during the breadboard phase.

## 6.2 Matrix + LCD + ESP32 prototype supply

| Ref | Component | Exact MPN / specification | Qty | Primary source | Notes |
|---|---|---|---:|---|---|
| P01 | 5 V desktop PSU | **MEAN WELL GST60A05-P1J**, 5 V, 6 A, 30 W, 5.5/2.1 mm barrel, centre-positive | 1 | Reichelt / TME | Enough for controlled prototype operation of matrix + ESP32; do not run every load at unrealistic maximum brightness simultaneously. |
| P02 | IEC mains lead | **CEE 7/7 EU plug → IEC C13**, 1.5–2 m | 1 | Reichelt / Amazon | Required because GST60A05 uses IEC C14 inlet. |
| P03 | Barrel jack breakout | **5.5 mm OD / 2.1 mm ID female barrel to screw terminal**, rated ≥6 A | 2 | Reichelt / maker supplier | One used + spare. |
| P04 | Inline blade fuse holder | automotive mini/ATO style, wire-mounted | 3 | Reichelt / Mouser | Separate matrix/control branches. |
| P05 | 5 A blade fuse | automotive, 5 A | 5 | Reichelt | Matrix branch. |
| P06 | 2 A blade fuse | automotive, 2 A | 5 | Reichelt | LCD/control branch. |
| P07 | TVS diode for 5 V rail | **Bourns SMAJ5.0A** | 5 | Mouser | Mainly for later PCB/power-board prototype. SMD. |

### Prototype power warning

A 5.5/2.1 mm barrel connector is not a good choice for a future 12–15 A integrated power architecture. It is acceptable here because the GST60A05 supply is limited to 6 A.

For the final enclosure, Waveform One should move to either:

- a higher-voltage external certified brick plus an internal high-current 5 V DC/DC stage, or
- another appropriately rated low-voltage locking connector and PSU architecture.

Do not freeze the production power connector from the breadboard BOM.

---

# 7. Wiring and connectors

| Ref | Component | Specification | Qty | Notes |
|---|---|---|---:|---|
| W01 | HUB75 ribbon cable | **2×8 IDC, 16-conductor, 2.54 mm, female-female** | 2 | Usually one is supplied with the Waveshare panel; buy one spare if not. |
| W02 | Matrix power lead | **VH4-compatible / panel-supplied 5 V cable** | 1 | Prefer supplied Waveshare lead. |
| W03 | Pi ↔ ESP32 USB cable | **USB-A to USB-C, USB 2.0 data-capable**, 0.5–1 m | 2 | Must be a data cable, not charge-only. |
| W04 | 22 AWG stranded hookup wire | red/black | 5 m each | General 5 V prototype power. |
| W05 | 18 AWG stranded hookup wire | red/black | 3 m each | Matrix/high-current branch. |
| W06 | 26 AWG stranded hookup wire | assorted colours | 10 m assortment | Logic, encoders, I²S. |
| W07 | Dupont jumper set | male-male / male-female / female-female | 1 kit | Development only. |
| W08 | Heat-shrink tubing | mixed sizes | 1 kit | Internal wiring. |
| W09 | Ferrules | 0.5–1.0 mm² assortment | 1 kit | For screw-terminal power connections. |
| W10 | M3 heat-set inserts | brass, suitable for chosen print wall | 50 | Mechanical prototype. |
| W11 | M3 screws | M3×6, M3×8, M3×10, M3×12 assortment | 1 kit | Mechanical prototype. |
| W12 | M2.5 standoffs | nylon/brass assortment | 1 kit | Pi / development boards. |

---

# 8. Prototype construction hardware

| Ref | Component | Specification | Qty | Notes |
|---|---|---|---:|---|
| B01 | Solderless breadboard | full-size, quality 830-point board | 2 | Low-speed/control prototyping. |
| B02 | Solderable prototype board | 2.54 mm plated-through protoboard, ≥90×70 mm | 3 | Prefer for AHCT/HUB75 interface once pinout is proven. |
| B03 | 2.54 mm female headers | breakaway | 2 strips | DevKit/prototyping. |
| B04 | 2.54 mm male headers | breakaway | 2 strips | General use. |
| B05 | Screw-terminal distribution block | 5 V/GND, ≥10 A | 2 | Prototype low-voltage star distribution. |

---

# 9. Recommended test equipment

Not part of the finished product, but strongly recommended before integrating high-current LEDs.

| Ref | Item | Minimum recommendation | Qty |
|---|---|---|---:|
| T01 | Digital multimeter | trustworthy CAT-rated meter | 1 |
| T02 | USB logic analyser | 8-channel, ≥24 MHz; Saleae-compatible clone acceptable for early prototype | 1 |
| T03 | Bench PSU | 0–30 V, 0–5 A minimum, current limiting | 1 |
| T04 | Soldering station | temperature controlled | 1 |
| T05 | Crimp tool | suitable for JST/VH/XH terminals later selected | 1 |
| T06 | Ferrule crimper | for low-voltage power wiring | 1 |

An oscilloscope is useful but not required for the first order if you already have access to one elsewhere.

---

# 10. Purchase quantities summary

## Must buy now

- 1 × Raspberry Pi 5 4 GB
- 1 × Raspberry Pi Active Cooler SC1148
- 1 × official Raspberry Pi 27 W EU PSU
- 2 × 64 GB high-endurance microSD
- 2 × ESP32-S3-DevKitC-1-N8R8
- 1 × Waveshare 5DP-CAPLCD-H
- 1 × Waveshare RGB-Matrix-P4-64x32 / Part 22101
- 2 × Adafruit SPH0645LM4H I²S microphone breakout / Product 3421
- 4 × TI SN74AHCT245N
- 4 × 20-pin DIP sockets
- 5 × Bourns PEC11R-4215F-S0024
- 1 × Mean Well GST60A05-P1J
- 1 × EU C13 mains cable
- 2 × 5.5/2.1 mm barrel-to-screw-terminal adapters
- passives listed above
- hookup wire
- protoboard/breadboard
- fuses and holders
- USB data cables
- mechanical M3 hardware

---

# 11. Recommended supplier split

## Order A — Raspberry Pi / maker supplier

Buy:

- Raspberry Pi 5 4 GB
- Active Cooler SC1148
- official 27 W USB-C EU PSU
- microSD cards if competitively priced

Preferred:

- Kubii or another authorised Raspberry Pi reseller

## Order B — Mouser France

Buy:

- ESP32-S3-DevKitC-1-N8R8 ×2
- SN74AHCT245N ×4
- 20-pin DIP sockets ×4
- Bourns PEC11R-4215F-S0024 ×5
- 47 Ω resistors
- 10 kΩ resistors
- 1 kΩ resistors
- 100 nF capacitors
- 10 nF capacitors
- 1000 µF capacitors
- 470 µF capacitors
- SMAJ5.0A TVS diodes
- headers / connectors as convenient

## Order C — Waveshare / official distributor

Buy:

- 5DP-CAPLCD-H ×1
- RGB-Matrix-P4-64x32, Part 22101 ×1

The Waveshare LCD package already includes HDMI/adapter/touch cables and a screw pack. Do not duplicate those until the package arrives.

## Order D — DigiKey / Adafruit / BerryBase

Buy:

- Adafruit SPH0645LM4H I²S MEMS microphone breakout, Product 3421 ×2

If DigiKey has the exact breakout in stock, combining this with other electronic parts is preferable to creating another international order.

## Order E — Reichelt / TME

Buy:

- Mean Well GST60A05-P1J ×1
- C13 EU mains lead
- fuse holders / fuses if not purchased elsewhere
- power wire / general electromechanical parts as convenient

---

# 12. Seeed Studio assessment

Seeed Studio is a legitimate and useful supplier for Waveform One.

Strengths:

- long-standing open-source hardware company
- strong maker/embedded ecosystem
- good ESP32/XIAO, sensors, audio modules and edge hardware
- German EU warehouse for many products
- current Seeed policy lists delivery from the German warehouse to France in roughly 3–7 days for eligible stock
- official store provides warranty/returns
- particularly interesting later for **Fusion PCB/PCBA manufacturing**

Where I would use Seeed for Waveform One:

- prototype sensors and development accessories
- Grove/Qwiic-style modules if needed
- test fixtures
- possible future reSpeaker experiments
- PCB fabrication
- PCB assembly
- low-volume hardware manufacturing/DFM work

Where I would **not** substitute just to buy from Seeed:

- ESP32-S3-DevKitC-1-N8R8 → keep the exact Espressif board
- Bourns encoders → keep exact Bourns parts
- TI AHCT buffers → keep exact TI logic
- Waveshare display/matrix → keep exact Waveshare mechanical parts
- Raspberry Pi → use authorised Pi distribution

The objective is not to minimize the number of suppliers. It is to make V1 electrically and mechanically reproducible.

---

# 13. Components deliberately not ordered yet

Do **not** order these until the breadboard prototype validates the architecture:

- custom control PCB
- custom power PCB
- final locking DC input connector
- final wiring harness connectors
- final JST series/terminal choices
- production MEMS microphone
- final single external PSU
- walnut side panels
- final aluminium knobs in bulk
- optional line-level ADC hardware
- ambient light sensor
- custom PCB ESP32-S3 module
- production-grade enclosure fasteners in quantity

These should be frozen only after the breadboard and first 3D enclosure iteration.

---

# 14. Key part verification links

- Espressif ESP32-S3-DevKitC-1-N8R8:
  https://www.mouser.fr/fr/ProductDetail/Espressif-Systems/ESP32-S3-DevKitC-1-N8R8

- TI SN74AHCT245N:
  https://www.mouser.fr/ProductDetail/Texas-Instruments/SN74AHCT245N

- Bourns PEC11R-4215F-S0024:
  https://www.mouser.fr/fr/ProductDetail/Bourns/PEC11R-4215F-S0024

- Waveshare 5DP-CAPLCD-H:
  https://www.waveshare.com/5dp-caplcd.htm

- Waveshare RGB-Matrix-P4-64x32:
  https://www.waveshare.com/rgb-matrix-p4-64x32.htm

- Adafruit I²S MEMS microphone breakout SPH0645LM4H:
  https://www.adafruit.com/product/3421

- Mean Well GST60A05-P1J:
  https://www.reichelt.com/fr/fr/shop/produit/power_supply_30_0_w_2_1_x_5_5_mm_barrel_plug-171051

- Seeed Studio EU warehouse:
  https://www.seeedstudio.com/eu_warehouse

---

# 15. BOM status

**Status: READY FOR V1 PROTOTYPE PURCHASING**

The only intentional architectural correction relative to the earlier PRD is the microphone choice. The obsolete/NRND INMP441 should not be purchased for a new project. The rest of the core architecture remains:

```text
ESP32-S3       → C++ / ESP-IDF
Raspberry Pi 5 → Rust core daemon
UI             → C++ / Qt 6 / QML
LCD            → Waveshare 5DP-CAPLCD-H
Visualizer     → Waveshare 64×32 P4 HUB75
Audio          → I²S MEMS microphone
Database       → SQLite
```

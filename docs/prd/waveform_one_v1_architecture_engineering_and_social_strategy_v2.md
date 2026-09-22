Yes. I would make V1 a **hybrid two-display device**: a proper IPS screen for album/track information and a real RGB matrix underneath for the music-reactive visualizer. The Raspberry Pi handles the application/UI/network side; an ESP32-S3 handles deterministic audio processing and LED driving.

## 1. V1 design

The finished device would look roughly like this:

```text id="s2n4x5"
                    12" RECORD SLEEVE
             ┌──────────────────────────┐
             │                          │
             │                          │
             │                          │
             └──────────────────────────┘
                       ╲      ╱
                        ╲    ╱

     ┌──────────────────────────────────────┐
     │ ┌───────────────────┐    BRIGHTNESS │
     │ │                   │        ◉       │
     │ │   ALBUM ART       │    VISUALIZER │
     │ │                   │        ◉       │
     │ │ Pink Floyd        │       MODE     │
     │ │ Time              │        ◉       │
     │ └───────────────────┘                │
     │                                      │
     │  ▂▄▆█▇▅▃▂▄▇██▆▃▂▃▅▇███▅▃▂        │
     │  ▂▄▆█▇▅▃▂▄▇██▆▃▂▃▅▇███▅▃▂        │
     │                                      │
     └──────────────────────────────────────┘
              walnut / printed enclosure
```

The LCD is deliberately not the whole front. I want the LED matrix to remain visibly a separate piece of hardware, because that gives it the aesthetic of the devices in your examples rather than simply looking like another tablet.

---

# 2. Exact processing architecture

### Main computer

**Raspberry Pi 5, 4 GB**

This runs:

- Raspberry Pi OS Lite 64-bit
- graphical compositor
- Now Playing UI
- album artwork
- recognition API client
- metadata enrichment
- local SQLite cache
- configuration
- Wi-Fi
- system management
- communication with the ESP32

The Pi 5 gives us far more compute than V1 actually requires, but that is intentional. It means we can later add richer graphics, local audio fingerprinting, Spotify/Apple Music integration, web configuration and perhaps AI-generated visualizers without replacing the computer.

The current Pi 5 uses a 2.4 GHz quad-core Cortex-A76 and supports dual HDMI, Wi-Fi and Bluetooth; Raspberry Pi specifies a 5 V/5 A supply for full operation. ([raspberrypi.com](https://www.raspberrypi.com/products/raspberry-pi-5/?pubDate=20260608&utm_source=chatgpt.com))

### Real-time controller

**Espressif ESP32-S3-DevKitC-1 N8R8**

I would use:

- ESP32-S3
- 8 MB PSRAM
- 8 MB flash
- USB directly to Raspberry Pi
- I²S microphone
- HUB75 matrix
- three rotary encoders

The ESP32 has one job:

> Everything that needs deterministic, low-latency interaction with the music or physical controls stays here.

So Linux scheduling, networking or fetching artwork can never make the spectrum display stutter.

---

# 3. Display choice

For the metadata screen, I would actually upgrade my previous suggestion slightly.

### Waveshare 5DP-CAPLCD-H

**5-inch, 1024 × 600, IPS, HDMI**

Important characteristics:

- 1024 × 600
- IPS
- 178° viewing angle
- 800 cd/m²
- HDMI
- narrow bezel
- tempered front glass
- capacitive touch

Waveshare lists compatibility with Raspberry Pi and specifically offers it as a 5-inch high-brightness IPS panel. ([waveshare.com](https://www.waveshare.com/5dp-caplcd.htm?utm_source=chatgpt.com))

We won't initially use the touchscreen. But the glass front and narrow bezel make it worth having.

A 5-inch screen is intentional. A 7-inch screen begins to dominate the physical design.

### LCD layout

At 1024×600 I would use approximately:

```text id="95pejf"
┌───────────────────────────────────┐
│                                   │
│ ┌────────────┐                    │
│ │            │  MASSIVE ATTACK    │
│ │            │                    │
│ │ album art  │  Teardrop          │
│ │            │  Mezzanine         │
│ │            │  1998              │
│ └────────────┘                    │
│                                   │
│  ● VINYL             05:30        │
└───────────────────────────────────┘
```

Different UI modes could later show full-screen artwork, larger typography, a clock, detailed album information, etc.

---

# 4. RGB visualizer

### Exact V1 panel

**64 × 32 RGB HUB75 LED matrix, 4 mm pitch**

Nominal illuminated area:

**256 × 128 mm**

That's almost perfect for a piece of hi-fi equipment of this size.

A 64×32/4 mm panel contains 2,048 RGB LEDs. Adafruit rates this style of panel at up to around **4 A at 5 V per panel** under worst-case conditions. ([adafruit.com](https://www.adafruit.com/product/2278?utm_source=chatgpt.com))

I would **not** drive it from the Raspberry Pi.

The ESP32-S3 drives it directly using DMA.

### Interface circuit

HUB75 expects 5 V-friendly logic, whereas ESP32 is 3.3 V.

Our prototype interface board therefore contains:

```text id="ste6ty"
ESP32-S3
   │
   ├── R1 ─────┐
   ├── G1      │
   ├── B1      │
   ├── R2      │
   ├── G2      │
   ├── B2      ├──► 74AHCT245 ──► HUB75
   ├── A       │
   ├── B       │
   ├── C       │
   ├── D       │
   ├── CLK     │
   ├── LAT     │
   └── OE ─────┘
```

I'd use **two 74AHCT245 octal buffers**.

Also:

- 33–68 Ω series resistance on CLK
- 1,000 µF electrolytic across matrix 5 V/GND
- 100 nF ceramic decoupling at each logic IC
- 5 A dedicated matrix fuse
- common ground between ESP32 and matrix

---

# 5. Audio capture

This part deserves a deliberate design.

For V1, I would **listen acoustically to the record**, rather than physically connect the turntable.

That means it works with:

- vinyl
- Spotify
- Apple Music
- radio
- CD
- Bluetooth
- television
- anything playing in the room

### V1 microphone

**INMP441 I²S MEMS microphone module**

No analogue audio conversion is needed.

```text id="ls02xi"
                INMP441
              ┌─────────┐
3.3V ─────────┤ VDD     │
GND ──────────┤ GND     │
GPIO 4 ◄──────┤ SCK     │
GPIO 5 ◄──────┤ WS      │
GPIO 6 ◄──────┤ SD      │
GND ──────────┤ L/R     │
              └─────────┘
                   │
                   ▼
               ESP32-S3
```

I'd mount the microphone on a little removable daughterboard directly behind an acoustic grille on the rear/top of the enclosure.

Not deep inside the box.

---

# 6. Audio processing

Capture:

**48 kHz / mono / 24-bit I²S**

The ESP32 maintains a circular audio buffer.

Two completely independent pipelines consume it:

```text id="mn41ju"
                        INMP441
                           │
                      48 kHz PCM
                           │
              ┌────────────┴─────────────┐
              │                          │
              ▼                          ▼
         VISUALIZER                 RECOGNITION
              │                          │
       1024-point FFT             downsample
              │                    to 16 kHz
       frequency bands                   │
              │                     USB stream
       beat detector                     │
              │                          ▼
              ▼                     Raspberry Pi
          RGB matrix                     │
                                         ▼
                                  recognition API
```

### FFT

I'd start with:

- 1024-sample FFT
- Hann window
- 48 kHz sampling
- approximately 21 ms analysis window
- 24 logarithmic frequency bands
- automatic gain control
- attack/decay smoothing

Band structure roughly:

```text id="nefy0p"
  50
  70
  90
 120
 160
 220
 300
 400
 550
 700
 900
1.2k
1.5k
2.0k
2.5k
3.2k
4.0k
5.0k
6.3k
8.0k
10k
12k
15k
18k Hz
```

The response should be logarithmic rather than linear because that corresponds much better to how music looks and feels spectrally.

---

# 7. Visualizer modes

V1 should ship with at least six.

### 1. Classic spectrum

```text id="djavcn"
          ██
          ██    ██
      ██  ██ ██ ██
   ██ ██ ██████ ██
████████████████████
```

### 2. Mirrored spectrum

```text id="9hda94"
        █    █
      ███    ███
████████████████████
████████████████████
      ███    ███
        █    █
```

### 3. Peak spectrum

Bars fall immediately but peak markers decay slowly.

### 4. Waveform

Actual PCM waveform scrolling across the display.

### 5. VU meter

```text id="pnlxw5"
L  ███████████████░░░
R  ████████████░░░░░░
```

### 6. Generative

Bass controls large structures, mids control motion and vocals/treble control particles/details.

Later, this can become an entire visualizer SDK.

---

# 8. Beat detection

The ESP32 should also calculate spectral flux.

Essentially:

```text id="l406xi"
FFT(n)
  │
  ▼
energy change compared with FFT(n-1)
  │
  ▼
spectral flux
  │
threshold
  ▼
BEAT
```

The resulting beat event can affect:

- animation
- brightness pulse
- direction
- colour palette
- particles
- transitions

So the visuals aren't merely frequency bars.

---

# 9. Raspberry Pi ↔ ESP32 protocol

USB serial.

Not Wi-Fi.

Something like:

```text id="6mj1p9"
Pi → ESP32

SET_BRIGHTNESS 72
SET_MODE 3
SET_PALETTE 7
DISPLAY_ON
DISPLAY_OFF
START_AUDIO_STREAM


ESP32 → Pi

BUTTON MODE
ROTARY BRIGHTNESS +1
ROTARY VISUALIZER -1

AUDIO_BEGIN
<PCM data>
AUDIO_END

STATUS TEMP=38 FPS=58
```

For production I'd replace the text commands with a framed binary protocol:

```text id="sykjg5"
MAGIC
VERSION
MESSAGE_TYPE
PAYLOAD_LENGTH
PAYLOAD
CRC32
```

The audio frames would also use this protocol.

---

# 10. Recognition architecture

For V1 I recommend **AudD**.

Not an unofficial Shazam endpoint.

AudD's standard recognition endpoint accepts audio and returns artist, title, album, release information and other metadata. Their standard recognition workflow is explicitly targeted at song identification; the current service supports short clips and quotes roughly 0.1–1.5 s processing times after upload. ([docs.audd.io](https://docs.audd.io/?utm_source=chatgpt.com))

Their current pricing is **$5 per 1,000 standard recognition requests**, with the first 300 requests free. ([audd.io](https://www.audd.io/?utm_source=chatgpt.com))

That is actually cheap enough for this project if we are intelligent about triggering recognition.

---

# 11. We should NOT constantly call the recognition API

The state machine should look like this:

```text id="pj8sar"
             ┌─────────────┐
             │   SILENCE   │
             └──────┬──────┘
                    │
                music detected
                    ▼
             ┌─────────────┐
             │ UNKNOWN     │
             └──────┬──────┘
                    │
               capture 12 s
                    ▼
             ┌─────────────┐
             │ RECOGNISE   │
             └──────┬──────┘
                    │
              recognised
                    ▼
             ┌─────────────┐
             │ NOW PLAYING │
             └──────┬──────┘
                    │
            monitor fingerprints
                    │
           probable track change
                    ▼
             ┌─────────────┐
             │ VERIFY      │
             └──────┬──────┘
                    │
               new track?
              ┌─────┴─────┐
             NO           YES
              │             │
              ▼             ▼
        NOW PLAYING      RECOGNISE
```

AudD documents its standard endpoint around short audio clips, including 12-second chunks. ([audd.io](https://www.audd.io/?utm_source=chatgpt.com))

---

# 12. How do we know a song changed?

This is a nice engineering problem.

I don't want to simply query every 20 seconds.

The ESP32 can calculate a cheap local audio signature from:

- spectral centroid
- RMS energy
- chroma
- spectral flux
- band distribution

The Pi maintains:

```text id="gyxbj1"
current_track_signature
```

When the signal changes dramatically and remains different for perhaps 3–5 seconds:

```text id="mmf7vv"
track_change_probability > threshold
```

the Pi runs recognition again.

For vinyl it can also identify:

```text id="dypt0d"
music
   ↓
near silence
   ↓
music
```

as a very strong track-change indication.

So API traffic drops dramatically.

---

# 13. Metadata enrichment

Recognition alone shouldn't control what gets shown.

The architecture would be:

```text id="ypw7ob"
AudD
 │
 ├── title
 ├── artist
 ├── album
 ├── release date
 └── ISRC
       │
       ▼
  Metadata Service
       │
       ├── MusicBrainz
       ├── Cover Art Archive
       └── optional Discogs
              │
              ▼
        normalized record
```

Our own internal model:

```json id="1ru2ry"
{
  "artist": "Massive Attack",
  "title": "Teardrop",
  "album": "Mezzanine",
  "year": 1998,
  "isrc": "...",
  "artwork": "...",
  "musicbrainz_release_id": "...",
  "duration_ms": 330000
}
```

MusicBrainz provides a free non-commercial API without an API key, but requires a meaningful User-Agent and requests applications stay at or below roughly one request per second. ([musicbrainz.org](https://musicbrainz.org/doc/MusicBrainz_API?utm_source=chatgpt.com))

---

# 14. Local caching

Very important for vinyl.

SQLite:

```text id="q0ad8w"
albums
tracks
recognitions
artwork
settings
```

If you regularly play the same records, recognition becomes:

```text id="yxj7u4"
fingerprint
     │
     ▼
local cache?
  │       │
 YES      NO
  │       │
  ▼       ▼
 instant  AudD
```

Artwork should also be stored locally.

That means records you play frequently appear almost immediately.

---

# 15. Controls

I would use three **Bourns PEC11R rotary encoders with integrated push switch**.

### Encoder 1 — BRIGHTNESS

Rotate:

```text id="rorpvb"
0 ─────────────────── 100%
```

Push:

```text id="qnmkg3"
DISPLAY ON/OFF
```

### Encoder 2 — VISUALIZER

Rotate:

```text id="b6xhjn"
Spectrum
Mirrored
Waveform
VU
Particles
Generative
```

Push:

```text id="pkob6s"
change palette
```

### Encoder 3 — MODE

Rotate:

```text id="ss1w8o"
NOW PLAYING
ALBUM ART
VISUALIZER
ALBUM INFO
CLOCK
```

Push:

```text id="2ja8n2"
select / favourite
```

The knobs themselves should be turned aluminium, approximately:

**22 mm diameter × 15 mm deep**

That gives it hi-fi equipment proportions rather than "Arduino project" proportions.

---

# 16. Power architecture

This is one area where I would **not cut corners**.

Worst-case rough loading:

| Device | Maximum design allocation |
|---|---:|
| Pi 5 | 5 V × 5 A |
| RGB matrix | 5 V × 4 A |
| LCD | ~5 V × 1 A |
| ESP32 + electronics | ~5 V × 0.5 A |
| Margin | ~1–2 A |

So I would design the 5 V distribution system for:

**12–15 A**

even though normal consumption will be much lower.

### V1 power topology

```text id="7dapyj"
             EXTERNAL CERTIFIED PSU
                     5 V / 15 A
                         │
                         ▼
                ┌────────────────┐
                │ POWER ENTRY PCB│
                └───────┬────────┘
                        │
          ┌─────────────┼─────────────┐
          │             │             │
        5A fuse       5A fuse       2A fuse
          │             │             │
          ▼             ▼             ▼
         Pi 5        LED matrix   LCD + ESP32
```

Every high-current branch uses separate wiring.

### Important

I would keep **230 V completely outside our device**.

The enclosure receives only low-voltage DC from a certified external brick.

Much safer and far easier to prototype.

---

# 17. Powering the Pi

For the first breadboard prototype I'd use the official **Raspberry Pi 27 W USB-C supply** independently.

For the integrated V1 enclosure, we can back-power through the 5 V header from our protected power-distribution board.

Raspberry Pi's HAT design guidance explicitly allows 5 V back-powering via the expansion header, but doing so bypasses some normal input protection, meaning our board must provide appropriate protection itself. ([github.com](https://github.com/raspberrypi/hats/blob/master/designguide.md?utm_source=chatgpt.com))

Therefore our board includes:

```text id="c2i1pd"
5V input
 │
 ├── fuse
 │
 ├── TVS
 │
 ├── reverse polarity protection
 │
 ├── bulk capacitor
 │
 └── Pi 5V rail
```

For the first prototype, though, **don't do this yet**. Use the proper Pi USB-C supply.

---

# 18. Custom electronics PCB

Once breadboarded, I'd build one custom carrier PCB.

It would contain:

```text id="1hwjvr"
┌───────────────────────────────────────┐
│              CONTROL PCB              │
│                                       │
│ ESP32-S3 module                       │
│                                       │
│ 2 × 74AHCT245                         │
│                                       │
│ HUB75 connector ──────────────────►   │
│                                       │
│ I²S MIC connector                     │
│                                       │
│ Encoder 1                             │
│ Encoder 2                             │
│ Encoder 3                             │
│                                       │
│ USB-C / USB to Raspberry Pi           │
│                                       │
│ Power distribution                    │
│ fuses                                 │
│ TVS                                   │
│ capacitors                            │
│                                       │
│ fan header                            │
└───────────────────────────────────────┘
```

That board probably ends up around:

**100 × 80 mm**

and mounts horizontally behind the display.

---

# 19. Mechanical architecture

I propose these V1 outer dimensions:

**330 mm wide × 245 mm high × 145 mm deep**

That is deliberate.

A 12-inch LP sleeve is roughly 315 mm square, so the enclosure and album sleeve visually belong together.

### Front panel

```text id="41zegb"
330 mm
◄────────────────────────────────────►

┌─────────────────────────────────────┐ ▲
│  5" LCD             ○              │ │
│                      ○              │ │ ~95
│                      ○              │ │
├─────────────────────────────────────┤
│                                     │ │
│        64 × 32 RGB MATRIX           │ │ 130
│                                     │ │
└─────────────────────────────────────┘ ▼
                 245 mm
```

Actual cut-outs come from the CAD models of the chosen display and LED panel, not these nominal figures.

---

# 20. Album sleeve support

Rear slot:

**318 × 10 mm**

with replaceable inserts so we can tune the fit.

I'd tilt the sleeve:

**7°–10° backwards**

from vertical.

Cross section:

```text id="r9tip3"
                LP sleeve
                   ╱
                  ╱
                 ╱ 8°
                ╱
        ┌──────╱──────────┐
        │     ╱ slot      │
        │                 │
        │    electronics  │
        │                 │
        └─────────────────┘
```

A removable rear support lip prevents the cardboard sleeve from sagging backwards.

---

# 21. 3D-print strategy

I would not attempt to print the whole 330 mm body as one object.

Make it modular:

```text id="ryn90r"
front bezel
    +
left shell
    +
right shell
    +
rear cover
    +
bottom electronics tray
    +
album-slot insert
```

Fastening:

**M3 heat-set brass inserts**

throughout.

Materials:

- prototype: PETG
- final structural shell: ASA or PETG-CF
- front bezel: matte black
- side panels: actual walnut veneer or CNC-cut walnut

That will look considerably better than wood-effect filament.

---

# 22. Cooling

Use the official Raspberry Pi 5 **Active Cooler**.

It is a small aluminium heatsink/blower assembly designed specifically for the Pi 5 and controlled by the Pi's fan header. ([raspberrypi.com](https://www.raspberrypi.com/products/active-cooler/?utm_source=chatgpt.com))

Air path:

```text id="gm8i8z"
side intake
     ↓
  Raspberry Pi
     ↓
 matrix electronics
     ↓
 rear/top exhaust
```

I would include ventilation as part of the aesthetic rather than drilling random holes later.

---

# 23. Software architecture

The production runtime should **not use Python on the Raspberry Pi**.

The recommended production stack is:

- **ESP32-S3 firmware: modern C++ using ESP-IDF**
- **Raspberry Pi core daemon: Rust**
- **Raspberry Pi graphical UI: C++20/23 + Qt 6 + QML**
- **Local persistence: SQLite**
- **Service management: systemd**
- **Python: development/manufacturing/test utilities only; not part of the production runtime**

The architectural split is deliberate:

```text id="x0dj61"
ESP32-S3
│
├── C++ / ESP-IDF
│
├── audio_capture.cpp
├── fft.cpp
├── beat_detector.cpp
├── led_renderer.cpp
├── visualizers/
│    ├── spectrum.cpp
│    ├── mirrored_spectrum.cpp
│    ├── peak_spectrum.cpp
│    ├── waveform.cpp
│    ├── vu.cpp
│    └── generative.cpp
├── controls.cpp
└── protocol.cpp
        │
        │ USB binary protocol
        ▼
Raspberry Pi 5
│
├── waveform-core
│       Rust
│
│       ├── recognition/
│       │      AudD client
│       ├── metadata/
│       │      MusicBrainz
│       │      Cover Art Archive
│       ├── artwork/
│       ├── cache/
│       ├── device/
│       │      ESP32 USB protocol
│       ├── state/
│       │      authoritative device state machine
│       ├── persistence/
│       │      SQLite
│       ├── config/
│       └── telemetry/
│
├── waveform-ui
│       C++ + Qt 6 + QML
│
│       ├── Now Playing UI
│       ├── album artwork
│       ├── typography
│       ├── animations
│       ├── transitions
│       ├── settings
│       └── device-status views
│
├── local IPC
│       Unix domain socket
│       versioned message protocol
│
└── systemd
        ├── waveform-core.service
        └── waveform-ui.service
```

### Why Rust for the Raspberry Pi core?

Waveform One is intended to behave like a dedicated appliance and may run continuously for weeks or months.

The Pi core daemon is responsible for:

- asynchronous networking
- recognition API calls
- metadata enrichment
- artwork retrieval
- SQLite persistence
- cache management
- USB communication with the ESP32
- reconnection logic
- authoritative product state
- offline behavior
- configuration
- structured logging
- error recovery

Rust is the best fit for this layer because it provides:

- native performance
- memory safety without garbage collection
- excellent asynchronous I/O
- strong concurrency guarantees
- strong type safety
- predictable resource usage
- single-binary deployment
- robust long-running service behavior
- no interpreter, virtual environment or runtime dependency management

The daemon should be built around an asynchronous runtime such as Tokio and use strongly typed internal state and protocol models.

### Why C++ + Qt 6/QML for the UI?

Because this is a display appliance whose visual quality is a core product feature.

Qt 6/QML is excellent for:

- smooth GPU-accelerated animation
- typography
- album artwork
- transitions
- image handling
- declarative screen composition
- embedded display applications
- full-screen kiosk behavior

C++ remains the best native integration language for Qt, while QML should define the majority of the visual composition and animation behavior.

This is preferable to embedding Chromium/Next.js simply to render a single-purpose local interface.

### Why separate the Rust daemon from the UI?

The Rust daemon owns the authoritative product state.

The Qt/QML UI is a presentation client.

They communicate locally through a **Unix domain socket using a versioned message protocol**.

```text
ESP32-S3
   │
   │ USB binary protocol
   ▼
Rust core daemon
   │
   │ Unix domain socket
   ▼
C++ / Qt 6 / QML UI
```

This separation allows:

- the UI to restart without interrupting recognition or device state
- the daemon to recover independently
- clearer testing boundaries
- future alternative UIs
- clean separation between product logic and presentation

### Role of Python

Python is **not part of the Waveform One production runtime**.

Python may still be used where it is objectively useful for:

- manufacturing scripts
- test harnesses
- fixture control
- data conversion
- development utilities
- release tooling
- one-off diagnostics

No deployed Waveform One service should require Python unless a future dependency presents a specific, documented engineering reason to change this decision.

---

# 24. Boot behaviour

The device needs to feel like an appliance.

Not a Raspberry Pi.

Power on:

```text id="81kw5w"
0 sec
 │
 ▼
Pi boot
 │
 ▼
ESP32 starts visualizer immediately
 │
 ▼
LCD splash screen
 │
 ▼
network connects
 │
 ▼
Now Playing service starts
 │
 ▼
microphone detects audio
 │
 ▼
"Listening..."
 │
 ▼
track recognised
 │
 ▼
album UI
```

Goal:

**usable visualizer within ~2 seconds from ESP32 power-up**, while Linux boots in parallel.

---

# 25. Failure behaviour

No Internet shouldn't kill the device.

Without Internet:

```text id="x3je28"
visualizer       ✓
controls         ✓
cached albums    ✓
clock            ✓
local recognition cache ✓

new recognition  ✗
```

LCD:

```text id="q9ypu7"
       UNKNOWN TRACK

  ▂▅▇██▆▄▃▅██▇▅▃

   OFFLINE
```

Still useful.

---

# 26. Initial BOM

| Component | V1 choice | Qty | Approx. budget |
|---|---|---:|---:|
| Main SBC | Raspberry Pi 5 4 GB | 1 | €70–110 |
| Cooling | Pi 5 Active Cooler | 1 | €5–10 |
| Controller | ESP32-S3 DevKitC-1 N8R8 | 1 | €15–20 |
| LCD | Waveshare 5DP-CAPLCD-H 5" 1024×600 | 1 | €50–65 |
| RGB | 64×32 HUB75 4 mm RGB matrix | 1 | €35–50 |
| Microphone | INMP441 I²S module | 1 | €3–8 |
| Level shifting | 74AHCT245 | 2 | €4 |
| Encoders | Bourns PEC11R | 3 | €10–15 |
| Knobs | 22 mm aluminium | 3 | €15–25 |
| Storage | 32/64 GB high-endurance microSD | 1 | €10–15 |
| Power | 5 V 12–15 A external PSU | 1 | €30–50 |
| Fuses/TVS/connectors | assorted | — | €15–25 |
| PCB prototype | custom | 1 | €10–30 |
| Heat-set inserts | M3 | — | €5 |
| Print material | PETG/ASA | — | €15–25 |
| Misc wiring/cables | — | — | €15–20 |

I'd budget approximately:

**€300–€400 for the complete prototype**, including mistakes, spare parts and printed iterations.

The core components currently surface around these price ranges:

### Raspberry Pi 5
Description: None
Price: €85.00
URL: 
Number of Reviews: 4000
Rating: 4.8
Featured Tag: 
Merchants: Nozzler + others

### ESP32-S3 DevKitC-1
Description: None
Price: €19.99
URL: 
Number of Reviews: 157
Rating: 4.4
Featured Tag: 
Merchants: Conrad.fr + others

### Waveshare 5-inch 1024×600 display
Description: None
Price: €64.95
URL: 
Number of Reviews: 9
Rating: 3.4
Featured Tag: 
Merchants: OpenELAB

### 64×32 RGB LED Matrix
Description: None
Price: €34.47
URL: 
Number of Reviews: 16
Rating: 4.1
Featured Tag: 
Merchants: Kiwi Electronics + others

### INMP441 I²S microphone
Description: None
Price: €4.50
URL: 
Number of Reviews: 13
Rating: 4.7
Featured Tag: 
Merchants: ElectroProto

---

# 27. Optional line-level input

I would leave this **out of the minimum V1 electronics**, but design the rear panel to accommodate it later.

If you want maximum quality, we can tap a **line-level output after the turntable's phono preamp**.

Then something such as the HiFiBerry DAC+ ADC can give the Pi a proper stereo 24-bit ADC input supporting up to 192 kHz. ([hifiberry.com](https://www.hifiberry.com/docs/data-sheets/datasheet-dac-adc/?utm_source=chatgpt.com))

Architecture:

```text id="b0ow9s"
Turntable
   │
phono preamp
   │
   ├──────────────► amplifier
   │
   └──────────────► Now Playing line-in
```

This would give spectacularly clean FFT results.

But I **would not require this** because the elegance of the device is that you can put it in a room and it simply listens.

---

# 28. One change I would make to the original concept

I would not put the actual LP record on the device.

I would put the **LP sleeve** on it.

There are several reasons:

- less risk of damaging records
- no dust contamination
- sleeve gives us artwork physically
- sleeve is square and visually matches the enclosure
- device can identify what's playing automatically
- the physical sleeve + digital artwork/track information makes a particularly nice juxtaposition

The record itself stays where it belongs: on the turntable.

---

# 29. V1 system architecture

Putting all of this together:

```text id="e4vqpp"
                          ┌─────────────────┐
                          │   MUSIC / ROOM  │
                          └────────┬────────┘
                                   │
                                   ▼
                              INMP441 MIC
                                   │ I²S
                                   ▼
                    ┌─────────────────────────┐
                    │       ESP32-S3          │
                    │                         │
                    │ audio ring buffer       │
                    │ FFT                     │
                    │ beat detection          │
                    │ visualizers             │
                    │ encoder handling        │
                    └──────┬──────────┬───────┘
                           │          │
                       HUB75       USB serial
                           │          │
                           ▼          ▼
                    ┌──────────┐ ┌───────────────────┐
                    │ 64×32    │ │ Raspberry Pi 5    │
                    │ RGB LED  │ │                   │
                    │ MATRIX   │ │ recognition       │
                    └──────────┘ │ metadata          │
                                 │ artwork           │
                                 │ SQLite            │
                                 │ UI                │
                                 └────────┬──────────┘
                                          │ HDMI
                                          ▼
                                   ┌─────────────┐
                                   │ 5" IPS LCD  │
                                   │ 1024 × 600  │
                                   └─────────────┘

                                        Wi-Fi
                                          │
                    ┌─────────────────────┼─────────────┐
                    ▼                     ▼             ▼
                  AudD              MusicBrainz     Artwork
              recognition            metadata        sources
```

That is the architecture I would build.

The most important design choices are therefore now fixed: **Pi 5 + ESP32-S3; 5-inch 1024×600 IPS information display; 64×32/4 mm HUB75 visualizer; acoustic I²S microphone; three physical rotary encoders; external low-voltage PSU; AudD recognition with MusicBrainz enrichment; modular ~330×245×145 mm enclosure sized around a 12-inch LP sleeve.**

The logical next step is **not yet writing the software**. It is to produce the engineering package for the prototype: exact pin assignment, complete wiring schematic, power-distribution schematic, ESP32/HUB75 circuit, connector specification, front/rear mechanical drawings with dimensions, and then a repository/project structure and implementation plan. That gets us to the point where you can order the parts and start physically assembling V1.

---

# 30. Engineering package for the prototype

## Exact ESP32-S3 pin assignment

For the ESP32-S3-DevKitC-1 N8R8 prototype, use the following pin allocation.

### I²S microphone

| Function | ESP32-S3 GPIO | INMP441 pin |
|---|---:|---|
| I²S BCLK / SCK | GPIO 4 | SCK |
| I²S WS / LRCLK | GPIO 5 | WS |
| I²S SD input | GPIO 6 | SD |
| Microphone channel select | GND | L/R |
| Power | 3.3 V | VDD |
| Ground | GND | GND |

### HUB75 matrix

| HUB75 signal | ESP32-S3 GPIO |
|---|---:|
| R1 | GPIO 7 |
| G1 | GPIO 8 |
| B1 | GPIO 9 |
| R2 | GPIO 10 |
| G2 | GPIO 11 |
| B2 | GPIO 12 |
| A | GPIO 13 |
| B | GPIO 14 |
| C | GPIO 15 |
| D | GPIO 16 |
| CLK | GPIO 17 |
| LAT / STB | GPIO 18 |
| OE | GPIO 21 |

GPIO 19 and GPIO 20 are intentionally left free because they are associated with native USB on the ESP32-S3 and should not be consumed by the matrix interface.

### Rotary encoders

| Encoder | Function | GPIO |
|---|---|---:|
| Encoder 1 | Brightness A | GPIO 38 |
| Encoder 1 | Brightness B | GPIO 39 |
| Encoder 1 | Push switch | GPIO 40 |
| Encoder 2 | Visualizer A | GPIO 41 |
| Encoder 2 | Visualizer B | GPIO 42 |
| Encoder 2 | Push switch | GPIO 47 |
| Encoder 3 | Mode A | GPIO 1 |
| Encoder 3 | Mode B | GPIO 2 |
| Encoder 3 | Push switch | GPIO 3 |

All encoder signal inputs use pull-ups and software debounce. For the final PCB, add optional RC debounce footprints so hardware filtering can be populated if required.

### Reserved / future expansion

| GPIO / interface | Reservation |
|---|---|
| GPIO 19/20 | Native USB |
| I²C bus | Ambient-light sensor / future sensors |
| UART | Debug/service interface |
| Additional GPIO | Status LED, fan control, service button |

The final production PCB should freeze the pinout only after checking the exact ESP32-S3 module and board revision against Espressif's current strapping-pin and flash/PSRAM restrictions.

---

# 31. Complete wiring schematic

```text
                                      ┌──────────────────────────────┐
                                      │       5 V / 15 A PSU        │
                                      │      certified external      │
                                      └──────────────┬───────────────┘
                                                     │
                                                     ▼
                                      ┌──────────────────────────────┐
                                      │       POWER ENTRY PCB        │
                                      │ fuse / TVS / reverse protect │
                                      └───────┬────────┬─────────────┘
                                              │        │
                          ┌───────────────────┘        └────────────────────┐
                          │                                                │
                          ▼                                                ▼
                   Raspberry Pi 5                                   64×32 HUB75
                   protected 5 V                                      5 V power
                          │
                          │ USB
                          ▼
                 ┌──────────────────┐
                 │    ESP32-S3      │
                 └──────┬───────────┘
                        │
             ┌──────────┼───────────────────────────────┐
             │          │                               │
             ▼          ▼                               ▼
         INMP441    74AHCT245 pair               3 × PEC11R
           I²S            │                     rotary encoders
                          │
                          ▼
                     HUB75 data

Raspberry Pi 5
│
├── HDMI ─────────────────────────────► 5" Waveshare IPS
│
├── USB ──────────────────────────────► ESP32-S3
│
├── Wi-Fi ────────────────────────────► Internet
│
└── GPIO fan connector ───────────────► Pi Active Cooler

All grounds are common at the low-voltage power-distribution system.
High-current matrix wiring does not share thin signal-return wiring.
```

---

# 32. Power-distribution schematic

```text
5 V / 15 A EXTERNAL PSU
        │
        ▼
   DC INPUT
        │
        ├── input fuse
        │
        ├── TVS diode
        │
        ├── reverse-polarity MOSFET
        │
        ├── bulk capacitance
        │
        ▼
     5 V BUS
        │
        ├────────► Branch A
        │            5 A fuse
        │            │
        │            └────────► Raspberry Pi 5
        │
        ├────────► Branch B
        │            5 A fuse
        │            │
        │            └────────► HUB75 matrix
        │
        └────────► Branch C
                     2 A fuse
                     │
                     ├────────► LCD
                     └────────► ESP32/control electronics
```

Recommended design principles:

- star-style high-current distribution from the power PCB
- minimum appropriately sized conductors for the Pi and matrix feeds
- separate pluggable connectors for each power branch
- local bulk capacitance close to the HUB75 input
- 1,000 µF or larger low-ESR bulk capacitor at the matrix
- 100 nF decoupling at every digital IC
- no mains voltage inside the enclosure
- chassis power switch only switches the low-voltage DC input
- optional soft-power controller can be added later for clean Raspberry Pi shutdown

---

# 33. ESP32/HUB75 circuit

```text
ESP32-S3 GPIO
    │
    │ 3.3 V logic
    ▼
┌──────────────────────┐
│ 74AHCT245 #1         │
│                      │
│ R1                   ├────────► HUB75 R1
│ G1                   ├────────► HUB75 G1
│ B1                   ├────────► HUB75 B1
│ R2                   ├────────► HUB75 R2
│ G2                   ├────────► HUB75 G2
│ B2                   ├────────► HUB75 B2
│ A                    ├────────► HUB75 A
│ B                    ├────────► HUB75 B
└──────────────────────┘

┌──────────────────────┐
│ 74AHCT245 #2         │
│                      │
│ C                    ├────────► HUB75 C
│ D                    ├────────► HUB75 D
│ CLK ── 33–68 Ω ──────┼────────► HUB75 CLK
│ LAT                  ├────────► HUB75 LAT
│ OE                   ├────────► HUB75 OE
└──────────────────────┘

74AHCT245 supply = 5 V
74AHCT245 grounds = common ground
OE pins on level shifters configured permanently enabled unless a future hardware-disable feature is required.

HUB75 power:
5 V BUS ── 5 A fuse ──────────────► MATRIX +5 V
GND BUS ───────────────────────────► MATRIX GND

Across matrix power input:
+5 V ───────┬────────────
            │
          1000 µF
            │
GND ────────┴────────────
```

For PCB routing:

- keep HUB75 clock short
- keep RGB/data lines grouped and short
- place level shifters close to the HUB75 connector
- provide a continuous ground reference under high-speed digital traces
- avoid routing microphone signals alongside HUB75 clock lines
- physically separate microphone and matrix power/current paths

---

# 34. Connector specification

| Connector | Purpose | Proposed type |
|---|---|---|
| DC power input | 5 V / high current | locking DC connector or GX-style low-voltage connector rated above required current |
| Matrix power | 5 V / GND | 2-pin locking high-current connector |
| HUB75 data | Matrix signals | standard 2×8 HUB75 IDC |
| Microphone | I²S mic daughterboard | 1×5 or 1×6 locking JST |
| Encoder 1 | A/B/SW/GND | locking JST |
| Encoder 2 | A/B/SW/GND | locking JST |
| Encoder 3 | A/B/SW/GND | locking JST |
| Pi↔ESP32 | Data | USB |
| LCD video | Video | micro-HDMI/HDMI as required |
| LCD power | 5 V | dedicated locking connector |
| Debug | Serial console | 3-pin 2.54 mm header |
| Future I²C | Sensors | 4-pin JST-SH/Qwiic-compatible |
| Fan/service | Future auxiliary | keyed 2/3-pin header |

Where possible, the production board should use keyed connectors so that no internal cable can be connected backwards.

---

# 35. Front mechanical drawing

Nominal V1 front dimensions:

```text
                         330 mm
        ◄────────────────────────────────────►

        ┌────────────────────────────────────┐
        │                                    │
        │ ┌────────────────────┐      ○      │
        │ │                    │             │
        │ │      5" LCD        │      ○      │
        │ │    1024 × 600      │             │
        │ │                    │      ○      │
        │ └────────────────────┘             │
        │                                    │
        ├────────────────────────────────────┤
        │                                    │
        │       64 × 32 RGB MATRIX           │
        │          256 × 128 mm              │
        │                                    │
        └────────────────────────────────────┘
        ▲                                    ▲
        │                                    │
        └──────────── 245 mm ────────────────┘
```

Nominal enclosure:

- width: 330 mm
- height: 245 mm
- depth: 145 mm
- front bezel wall: approximately 3–4 mm
- minimum structural wall thickness: approximately 3 mm
- matrix viewing opening: approximately 256 × 128 mm, adjusted to actual module bezel
- LCD opening: defined from final Waveshare CAD/mechanical drawing
- control knob diameter: approximately 22 mm
- encoder shaft centre spacing: approximately 35–40 mm
- front matrix and LCD should sit slightly recessed behind the front bezel
- internal removable mounting plates should be used rather than screwing components directly into printed plastic

---

# 36. Rear mechanical drawing

```text
                         330 mm
        ◄────────────────────────────────────►

        ┌────────────────────────────────────┐
        │     318 × 10 mm ALBUM SLOT         │
        │ ────────────────────────────────── │
        │                                    │
        │      ventilation / exhaust         │
        │   :::::::::::::::::::::::::::::    │
        │                                    │
        │  SERVICE USB   NETWORK   POWER     │
        │      □            □        ○       │
        │                                    │
        │       optional future LINE IN      │
        │                 ○  ○               │
        └────────────────────────────────────┘
```

Rear design:

- album sleeve slot: nominal 318 × 10 mm
- replaceable slot inserts
- sleeve angle: 7–10° rearward
- removable rear support lip
- ventilation above the Pi/matrix electronics
- external low-voltage power inlet
- recessed service USB access
- optional Ethernet access
- reserved pair of RCA holes for future line-level input
- removable rear cover using M3 screws and heat-set inserts

The rear cover should be removable without removing the album support structure.

---

# 37. Internal mechanical arrangement

```text
SIDE VIEW

                album sleeve
                    /
                   / 8°
                  /
        ┌────────/──────────────────┐
        │       sleeve slot         │
        │                           │
        │ LCD      control PCB      │
        │                           │
        │ MATRIX      Raspberry Pi  │
        │              + cooler     │
        │                           │
        │ power distribution PCB    │
        └───────────────────────────┘
             145 mm nominal depth
```

Design the electronics on removable trays.

Suggested internal modules:

1. front display carrier
2. Raspberry Pi carrier
3. ESP32/control PCB carrier
4. power PCB carrier
5. microphone daughterboard
6. rear I/O plate
7. album-support assembly

---

# 38. Repository/project structure

```text
now-playing/
│
├── README.md
├── LICENSE
├── CONTRIBUTING.md
├── docs/
│   ├── architecture/
│   ├── electronics/
│   ├── mechanical/
│   ├── protocols/
│   ├── api/
│   └── adr/
│
├── firmware/
│   └── esp32/
│       ├── CMakeLists.txt
│       ├── sdkconfig.defaults
│       ├── main/
│       │   ├── main.cpp
│       │   ├── audio/
│       │   │   ├── audio_capture.cpp
│       │   │   ├── audio_capture.hpp
│       │   │   ├── fft_engine.cpp
│       │   │   ├── fft_engine.hpp
│       │   │   ├── beat_detector.cpp
│       │   │   └── beat_detector.hpp
│       │   ├── display/
│       │   │   ├── hub75_driver.cpp
│       │   │   ├── hub75_driver.hpp
│       │   │   └── visualizers/
│       │   ├── controls/
│       │   │   ├── encoders.cpp
│       │   │   └── encoders.hpp
│       │   ├── protocol/
│       │   │   ├── protocol.cpp
│       │   │   └── protocol.hpp
│       │   └── system/
│       └── test/
│
├── pi/
│   ├── core/
│   │   ├── Cargo.toml
│   │   └── src/
│   │       ├── main.rs
│   │       ├── recognition/
│   │       ├── metadata/
│   │       ├── artwork/
│   │       ├── cache/
│   │       ├── device/
│   │       ├── state/
│   │       └── config/
│   │
│   ├── ui/
│   │   ├── CMakeLists.txt
│   │   ├── src/
│   │   ├── qml/
│   │   │   ├── Main.qml
│   │   │   ├── screens/
│   │   │   ├── components/
│   │   │   ├── transitions/
│   │   │   └── themes/
│   │   └── assets/
│   │
│   └── systemd/
│
├── protocol/
│   ├── specification.md
│   ├── messages.md
│   └── test-vectors/
│
├── electronics/
│   ├── kicad/
│   │   ├── control-board/
│   │   └── power-board/
│   ├── schematics/
│   ├── gerbers/
│   └── bom/
│
├── mechanical/
│   ├── cad/
│   ├── step/
│   ├── stl/
│   ├── drawings/
│   └── print-profiles/
│
├── scripts/
│
├── tests/
│   ├── integration/
│   ├── hardware-in-loop/
│   └── protocol/
│
└── .github/
    └── workflows/
```

---

# 39. Software technology stack

The production architecture uses the **best languages for each subsystem rather than choosing a single language for convenience**.

## ESP32 firmware: C++ with ESP-IDF

**Language: modern C++**

**Framework: ESP-IDF**

Use C++ for:

- I²S audio capture
- FFT processing
- beat detection
- DMA-backed HUB75 rendering
- visualizer algorithms
- rotary encoders
- binary protocol
- watchdogs
- hardware state management

Why:

- direct access to Espressif's native ESP-IDF platform
- deterministic performance
- mature embedded ecosystem
- no garbage collector
- straightforward use of DMA, FreeRTOS and hardware peripherals
- excellent support for DSP and matrix-driver libraries
- suitable for tight memory and latency constraints

Use modern C++ features where they improve safety, but avoid dynamic allocation in real-time processing paths.

## Raspberry Pi core service: Rust

**Language: Rust**

Use Rust for:

- AudD API integration
- MusicBrainz integration
- Cover Art Archive integration
- recognition state machine
- metadata normalization
- SQLite persistence
- caching
- device protocol
- USB serial communication
- network resilience
- configuration
- telemetry
- lifecycle/state management

Recommended libraries include:

- Tokio for asynchronous runtime
- reqwest for HTTP
- serde for serialization
- sqlx or rusqlite for SQLite
- tracing for structured logging
- thiserror / anyhow as appropriate for error management
- serialport or an asynchronous serial implementation for ESP32 communication

Why Rust:

- memory safety without garbage collection
- excellent asynchronous I/O
- strong type system
- high reliability for a long-running appliance
- strong concurrency model
- very good performance
- single native binaries
- no Python runtime/environment management
- excellent fit for a service that must run unattended for months

## Raspberry Pi UI: C++20/23 + Qt 6 + QML

**Language: C++ for the application layer, QML for declarative UI**

Use Qt 6/QML for:

- album artwork rendering
- typography
- animated transitions
- Now Playing screens
- configuration screens
- visual effects
- screen modes
- GPU-accelerated UI
- kiosk-mode appliance behavior

Why:

- Qt/QML is one of the best native embedded-display stacks available
- highly performant
- GPU accelerated
- mature typography and image handling
- excellent animation capabilities
- avoids embedding Chromium
- avoids a browser/server architecture for a single-purpose appliance
- can communicate with the Rust service over a local IPC interface

## Pi service ↔ UI communication

Use a local IPC boundary rather than linking everything into one process.

Recommended:

**Unix domain socket with a versioned message protocol**

Alternative:

**D-Bus**

The Rust daemon owns the authoritative system state.

The Qt/QML UI subscribes to state updates and sends user intents.

```text
ESP32
  │
  │ binary USB protocol
  ▼
Rust core daemon
  │
  │ Unix domain socket
  ▼
Qt 6 / QML UI
```

This separation means either process can restart independently.

## Database

**SQLite**

Use WAL mode.

The database stores:

- recognized tracks
- albums
- artists
- artwork cache metadata
- recognition history
- user settings
- device settings
- API cache
- optional favourites
- optional listening history

## Build systems

ESP32:

**CMake + ESP-IDF**

Raspberry Pi Rust:

**Cargo**

Qt:

**CMake**

## Testing

ESP32:

- host-side unit tests where possible
- ESP-IDF Unity tests for hardware-specific code
- hardware-in-the-loop tests for I²S/HUB75/encoder functionality

Rust:

- native unit tests
- integration tests
- mock HTTP services
- property-based testing where useful
- protocol test vectors shared with ESP32

Qt/QML:

- Qt Test
- QML Test
- screenshot/golden rendering tests where practical

Every software component should follow TDD where the behavior is testable before implementation.

## Formatting / static analysis

C++:

- clang-format
- clang-tidy
- compiler warnings treated as errors where practical

Rust:

- rustfmt
- clippy
- cargo audit

QML:

- qmllint

## CI

GitHub Actions should:

1. build ESP32 firmware
2. run firmware host tests
3. build Rust daemon
4. run Rust tests
5. run clippy
6. build Qt/QML application
7. run Qt/QML tests
8. validate protocol test vectors
9. validate KiCad files where tooling permits
10. package Pi binaries
11. produce versioned ESP32 firmware artifacts

## Primary-language decision

The resulting production stack is:

```text
ESP32 real-time firmware   → C++
Raspberry Pi core          → Rust
Raspberry Pi UI            → C++ + Qt 6/QML
Database                   → SQLite
PCB                         → KiCad
Mechanical CAD             → Fusion 360 or FreeCAD
Automation/tooling         → Python only where useful
```

Python remains acceptable for manufacturing scripts, test tooling, conversion utilities and development automation, but it is not part of the production runtime architecture.

---

# 40. Implementation plan

## Phase 0 — repository and engineering baseline

- create repository structure
- add architecture decision records
- document V1 hardware choices
- freeze communication boundaries
- define coding standards
- create CI
- define binary protocol
- create protocol test vectors

Deliverable:

A buildable empty skeleton for ESP32 firmware, Rust daemon and Qt/QML UI.

## Phase 1 — ESP32 audio prototype

- connect INMP441
- configure 48 kHz I²S capture
- verify PCM capture
- implement ring buffer
- implement 1024-point FFT
- implement logarithmic band mapping
- implement smoothing
- expose diagnostics over USB

Acceptance:

Stable continuous audio processing without dropped frames.

## Phase 2 — HUB75 prototype

- connect level shifters
- connect 64×32 matrix
- implement DMA-backed output
- implement brightness control
- implement frame-rate instrumentation
- build static test patterns

Acceptance:

Stable 60 FPS-class visual rendering with no visible flicker.

## Phase 3 — audio-reactive visualizers

Implement:

1. spectrum
2. mirrored spectrum
3. peak spectrum
4. waveform
5. VU
6. generative visualizer

Add:

- beat detection
- automatic gain
- attack/release smoothing
- palette system

Acceptance:

All six visualizers respond correctly to live music.

## Phase 4 — physical controls

- connect three PEC11R encoders
- implement quadrature decoding
- implement push switches
- implement debounce
- implement control events
- connect control events to visualizer state

Acceptance:

All physical controls operate without missed or duplicate events.

## Phase 5 — Pi/ESP32 protocol

- define binary framing
- implement CRC
- implement sequence numbers
- implement version negotiation
- implement command/response messages
- implement audio transport
- implement reconnect behavior
- build shared protocol test vectors

Acceptance:

ESP32 and Pi can be disconnected/reconnected without requiring restart.

## Phase 6 — Rust core daemon

Implement:

- device manager
- state machine
- AudD client
- MusicBrainz client
- artwork client
- SQLite persistence
- caching
- configuration
- structured logging
- offline behavior

Acceptance:

Headless Pi can recognize a playing track and persist normalized metadata.

## Phase 7 — track-change intelligence

Implement local feature extraction/state tracking for:

- RMS energy
- spectral centroid
- chroma
- spectral flux
- band distribution
- silence detection

Build change-confidence logic.

Acceptance:

Normal track transitions trigger recognition while API polling remains substantially lower than fixed-interval recognition.

## Phase 8 — Qt/QML UI

Implement:

- startup screen
- listening state
- recognized-track state
- album artwork
- artist/title/album/year
- offline state
- unknown-track state
- animation transitions
- visualizer/mode indicators
- configuration screen

Acceptance:

The device visually behaves like a finished appliance rather than a development computer.

## Phase 9 — breadboard integration

Assemble:

- Pi 5
- ESP32-S3 DevKitC
- INMP441
- HUB75
- LCD
- encoders
- power supplies

Run continuously for at least 24 hours.

Acceptance:

No crashes, thermal problems, memory leaks, USB instability or visual corruption.

## Phase 10 — custom PCB revision A

Create in KiCad:

- ESP32 carrier/control board
- HUB75 level shifting
- microphone connector
- encoder connectors
- power connectors
- USB
- debug interface
- decoupling
- test points

Separately create the protected power-distribution PCB.

Acceptance:

Revision A reproduces breadboard functionality without bodge wiring.

## Phase 11 — enclosure prototype

Create CAD around actual measured components.

Print:

- front bezel
- side/body sections
- rear panel
- electronics trays
- album holder
- slot inserts

Acceptance:

All components fit, connectors are accessible, airflow works and LP sleeves are supported safely.

## Phase 12 — thermal/power testing

Measure:

- Pi temperature
- ESP32 temperature
- matrix temperature
- 5 V rail voltage under load
- current consumption
- connector temperature
- worst-case display load

Acceptance:

Stable electrical and thermal operation under sustained maximum-brightness testing.

## Phase 13 — final industrial-design iteration

Refine:

- walnut side panels
- matte front
- aluminium controls
- vent geometry
- cable management
- panel gaps
- screw visibility
- display recesses
- album angle
- lighting diffusion

## Phase 14 — production-quality V1

Produce:

- PCB revision B
- final enclosure
- final firmware
- reproducible Pi image/install procedure
- automated update mechanism
- calibration process
- assembly documentation
- full BOM
- wiring harness documentation
- test procedure

---

# 41. Product name

## WAVEFORM ONE

The selected working product name is:

**WAVEFORM ONE**

The name directly connects the product to the physical representation of sound while still feeling appropriate for a premium piece of audio equipment.

“Waveform” describes one of the central ideas behind the device: music is captured, analyzed and transformed into a live visual representation. “One” identifies this as the first physical product in the product family without forcing future products to use the same hardware architecture.

Potential future product-family naming could include:

```text
WAVEFORM ONE
WAVEFORM MINI
WAVEFORM STUDIO
WAVEFORM PRO
```

For repositories and technical identifiers, use:

```text
waveform-one
```

The product should be displayed publicly as:

```text
WAVEFORM ONE
```

Trademark and naming status:

- preliminary exact-name web screening did not identify an obvious existing audio-hardware product called **WAVEFORM ONE**
- “Waveform” itself is a common technical word and is already used by numerous products and trademark owners in unrelated and adjacent fields
- Bose filed US applications for **WAVEFORM** in 2026 covering digital-signal-processing hardware/software and related services; current trademark-docket sources indicate those applications were subsequently expressly abandoned
- because “Waveform” is descriptive and crowded, **WAVEFORM ONE should be treated as the selected working product name, not as formally legally cleared for commercial launch**
- before commercial manufacture, sales, crowdfunding or a trademark filing, conduct a professional clearance search covering at minimum France/INPI, EUIPO, WIPO/Madrid and relevant international markets in the appropriate Nice classes


---

# 42. Social media strategy

The development should be publicized as a **build-in-public hardware project**, not only as a finished product.

The strongest story is:

> **“I’m designing and building a physical device from scratch that listens to vinyl, identifies what is playing, retrieves the album information, and creates real-time visuals from the music.”**

And:

> **“I’m designing the electronics, firmware, software and enclosure myself.”**

That makes the project interesting to several overlapping audiences:

- vinyl / hi-fi enthusiasts
- electronics makers
- Raspberry Pi / ESP32 developers
- embedded systems engineers
- 3D-printing communities
- software developers
- music lovers
- product designers
- DIY / maker audiences

The strategy should deliberately show both progress and final polish.

## Platform priorities

### YouTube

YouTube should become the canonical long-form record of the project.

Use:

- Shorts for 15–60 second development moments
- 5–15 minute milestone videos
- one polished final build video

Potential final-video concept:

> **I Built a Device That Knows What Vinyl I'm Playing**

YouTube content should document major engineering milestones rather than every minor change.

### Instagram

Use Instagram for:

- Reels
- finished-product shots
- enclosure design
- album-sleeve presentation
- visualizer clips
- 3D-printing progress
- PCB arrival
- high-quality close-ups

The visual language should remain consistent across the project.

### TikTok

Use TikTok primarily for discovery.

Short, direct progress clips should work well, especially when framed as a continuing series.

Example format:

> **Day 12 of building a device that listens to my records and automatically knows what I'm playing.**

Then immediately show the result.

### X

Use X for the engineering audience.

Good post types:

- architecture diagrams
- ESP32 performance notes
- FFT screenshots
- PCB progress
- protocol design
- short hardware clips
- technical lessons learned

Example:

> ESP32-S3 is now doing:
>
> - 48 kHz I²S capture
> - FFT
> - beat detection
> - HUB75 rendering
>
> while the Pi handles track recognition.

### Reddit

Reddit should be approached as community participation rather than promotion.

Relevant communities include topics around:

- vinyl
- Raspberry Pi
- ESP32
- electronics
- 3D printing
- embedded systems
- DIY audio

A good Reddit post is:

> “I've been designing this automatic Now Playing display for my turntable. This is prototype three. Would love feedback on the enclosure.”

Avoid promotional language.

### LinkedIn

Use LinkedIn to emphasize the engineering and product-development dimension.

Example angle:

> “I wanted to explore what happens when embedded systems, real-time audio processing, cloud APIs and industrial design are treated as one product engineering problem.”

LinkedIn should demonstrate systems thinking and engineering depth rather than hobby activity.

### GitHub

GitHub should form part of the public identity of the project.

Publish, when appropriate:

- architecture
- selected firmware
- selected software
- protocol documentation
- screenshots
- build logs
- hardware documentation
- CAD or selected mechanical files
- release notes

A strong README should include short videos or GIFs showing the device working.

## Do not wait until the project is finished

The first post can simply introduce the concept.

Example:

> **I'm building this.**
>
> Vinyl goes on the turntable.
>
> The device listens to the room.
>
> It identifies the track.
>
> It retrieves the artwork.
>
> And the LEDs react to the actual audio.
>
> I'm building the enclosure, electronics and software from scratch.

This gives people a reason to follow before the product exists.

## Make the development episodic

Number the development publicly.

Example sequence:

- Building WAVEFORM ONE — #01: The idea
- #02: Choosing the architecture
- #03: First ESP32 boot
- #04: Capturing audio
- #05: The first FFT
- #06: Making LEDs react to music
- #07: Beat detection
- #08: First song recognition
- #09: Showing album artwork
- #10: Raspberry Pi talks to ESP32
- #11: Designing the enclosure
- #12: First 3D print
- #13: It does not fit
- #14: Second enclosure
- #15: Designing the PCB
- #16: PCB arrived
- #17: First power-up
- #18: Full integration
- #19: Walnut / finishing
- #20: Final reveal

This gives the audience continuity and a reason to return.

## Show failures

Do not make the development appear unrealistically linear.

Show:

- wiring mistakes
- LED glitches
- failed prints
- enclosure-fit problems
- recognition failures
- timing issues
- power problems
- bad first designs

Example:

```text
Expected:
beautiful spectrum

Actual:
████████████████████
random flashing garbage
```

Then explain what caused it and what fixed it.

Failures create story and make the final result more satisfying.

## Give every short clip an immediate visual payoff

Avoid opening with lengthy explanation.

Do not begin with:

> “Hi everyone, today I'm going to talk about...”

Begin with the device doing something.

Example:

```text
[Record spinning]

[LED matrix responds]

[Screen changes]

RADIOHEAD
Everything In Its Right Place
Kid A
```

Then explain:

> “This device just identified that record by listening to the room.”

The first 1–2 seconds should contain something visually interesting.

## Reusable content formats

### Format A — “Today WAVEFORM ONE learned…”

Example:

> Today WAVEFORM ONE learned how to detect beats.

Show before and after.

### Format B — engineering problem

Example:

> “How do you know when one vinyl track ends and another begins?”

Then show the engineering solution.

### Format C — satisfying visual

Record spinning → LEDs → artwork → track information.

Minimal explanation.

### Format D — prototype comparison

```text
V1
V2
V3
```

Physical iteration is inherently interesting.

### Format E — technical deep dive

Example:

> Why I'm using both a Raspberry Pi and an ESP32 instead of one computer.

This suits YouTube, X and LinkedIn.

## Establish a visual identity early

The product identity should be visually restrained.

Recommended direction:

- black
- walnut
- brushed aluminium
- warm white typography
- RGB colour only where the visualizer needs it

Avoid making the branding itself look like RGB gaming hardware.

The physical product should feel like hi-fi equipment.

## Film consistently from the beginning

Capture:

- close-ups of records spinning
- straight-on device shots
- 45° product shots
- hands assembling electronics
- soldering
- PCB close-ups
- CAD rotating on screen
- 3D-print timelapses
- code-to-hardware results
- oscilloscope or logic-analyser traces when relevant

Save all footage.

Early prototype footage will be extremely useful in the eventual long-form build video.

## Show code as a cause, not the entire content

Instead of focusing on code alone:

```text
CODE
 ↓
compile
 ↓
upload
 ↓
hardware result
```

The physical payoff should be the center of the content.

Technical viewers can be directed to GitHub.

## Recommended cadence

| Content | Frequency |
|---|---:|
| Short video | 2/week |
| Development photo/update | 1–2/week |
| Technical post | 1/week |
| Longer YouTube update | Every major milestone |
| GitHub update | Continuously |

One engineering milestone should generate multiple pieces of content.

For example, getting music recognition working can generate:

1. TikTok/Reel/Short showing first successful recognition
2. X post explaining the architecture
3. LinkedIn post about audio fingerprinting
4. GitHub commit/documentation
5. footage for the eventual long-form YouTube video

Do not create five separate stories. Reuse one genuine engineering event across platforms.

## Recommended first 10 posts

1. **“I'm building this.”**
   Concept, inspiration and goal.

2. **System architecture**
   Raspberry Pi + ESP32 + microphone + LCD + RGB matrix.

3. **Parts arrive**
   Visual hardware introduction.

4. **First LED matrix test**
   Immediate visual payoff.

5. **First microphone data**
   Show raw waveform.

6. **First FFT**
   Make LEDs genuinely react to music.

7. **First record test**
   Use actual vinyl.

8. **Automatic recognition**
   Major milestone.

9. **CAD enclosure**
   Show the physical-design process.

10. **First integrated prototype**
    Everything running together, even if the wiring is still rough.

## Keep the final industrial design partially hidden

Reveal functionality throughout development, but do not reveal the final polished physical design too early.

Show:

- CAD fragments
- test chassis
- failed prints
- partial prototypes
- internal hardware

Save the final walnut/aluminium finished version for the reveal.

## Final reveal strategy

The final reveal should be cinematic.

Example sequence:

```text
Black screen.

Needle drops.

Music starts.

Device wakes.

LISTENING...

Visualizer responds.

Track is recognized.

MASSIVE ATTACK
TEARDROP
MEZZANINE
1998

Album art appears.
```

Then rapidly show several records and different visualizer modes.

Finish with the final product name and identity.

This becomes the hero video for every platform.

## Let the audience influence selected decisions

Use real engineering/design choices to engage people.

Examples:

- walnut or oak?
- 5-inch display or 7-inch?
- which spectrum style?
- album art or typography by default?
- three knobs or one?
- which visualizer should be implemented next?

The audience should contribute to selected decisions, but should not control the product architecture.

## Do not start by presenting this as a commercial product

Initially, frame it as:

> **I'm building something I want to exist.**

Do not lead with:

> “Launching soon.”

If significant numbers of people later ask whether they can buy one, that becomes useful market validation.

Possible evolution:

```text
personal project
        ↓
open-source project?
        ↓
DIY kit?
        ↓
assembled product?
        ↓
commercial product?
```

Commercialization should remain an option rather than the initial narrative.

## Story structure

Think of the public development as three acts.

```text
ACT I
“I have an idea.”
      │
      ▼
architecture
experiments
parts


ACT II
“Can I actually make it work?”
      │
      ▼
bugs
audio
FFT
recognition
PCB
CAD
failures
iterations


ACT III
“It works.”
      │
      ▼
final enclosure
beautiful product
records
visualizers
final reveal
```

The key strategy is to let people watch the device gradually come into existence rather than revealing it only after completion.

When the final product is shown, there should already be an audience that has followed the engineering journey and is invested in seeing the finished result.

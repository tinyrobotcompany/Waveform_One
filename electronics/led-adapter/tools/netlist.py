"""The LED adapter board's circuit: every part, its value and footprint, and the
net on each pin. The schematic and the PCB's pad nets are both generated from
this, as for the carrier board (whose generators these tools reuse).

The board links the P4's two 12-pin GPIO sockets to the LED panel's HUB75
ribbon, with every cable plugged in at both ends:

- J1 and J2 take two PH2.0 12-pin cables, plugged into the P4's P3 and P1
  sockets. The cables must be "same direction": wire 1 at one end goes to
  pin 1 at the other.
- Two 74AHCT245 buffers turn the P4's 3.3 V signals into the 5 V the panel
  expects (the same buffers as on the carrier board).
- J3 takes the panel's own 16-pin ribbon cable.
- J4 takes 5 V for the buffers (a few mA) from the LED panel's supply. The
  P4's sockets carry no 5 V.

The buffers drive the panel only while the P4 is powered: Q1 enables them
from the P4's 3.3 V. With the P4 off or unplugged, the buffer outputs float
and R4 holds the panel's OE high, so the panel stays dark.

Sources:
- P4 socket pinout: Waveshare ESP32-P4-WIFI6-Touch-LCD-7B schematic, P1 and P3
  (both "PH2.0 12P" vertical sockets).
- GPIO assignment: docs/commissioning/2026-10-14-p4-led-takeover.md.
"""
import importlib.util
import sys
from pathlib import Path

CARRIER_TOOLS = Path(__file__).resolve().parents[2] / "carrier" / "tools"
if str(CARRIER_TOOLS) not in sys.path:
    sys.path.append(str(CARRIER_TOOLS))  # kicad_lib and the carrier's generators

# The carrier's netlist module, for its Part, R and C helpers.
_spec = importlib.util.spec_from_file_location("carrier_netlist", CARRIER_TOOLS / "netlist.py")
_carrier = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_carrier)
C, R, Part = _carrier.C, _carrier.R, _carrier.Part

C1206 = "Capacitor_SMD:C_1206_3216Metric"
PH12 = "Connector_JST:JST_PH_B12B-PH-K_1x12_P2.00mm_Vertical"

# LED panel signals, in HUB75 order, with their P4 GPIO and where that GPIO
# arrives: (cable socket, pin).
HUB75 = [("R1", 2, "J1", "10"), ("G1", 3, "J1", "9"), ("B1", 4, "J1", "8"), ("R2", 5, "J1", "7"),
         ("G2", 28, "J1", "6"), ("B2", 29, "J1", "5"), ("A", 30, "J1", "4"), ("B", 31, "J1", "3"),
         ("C", 34, "J1", "2"), ("D", 49, "J2", "4"), ("CLK", 52, "J2", "1"), ("LAT", 50, "J2", "3"),
         ("OE", 51, "J2", "2")]

PARTS = []
add = PARTS.append

# --- Cables from the P4 ------------------------------------------------------------
# P3 on the P4: 1 GPIO36, 2 GPIO34, 3 GPIO31, 4 GPIO30, 5 GPIO29, 6 GPIO28,
# 7 GPIO5, 8 GPIO4, 9 GPIO3, 10 GPIO2, 11 GND, 12 3V3.
socket_a = {"11": "GND", "12": "P4_3V3"}
# P1 on the P4: 1 GPIO52, 2 GPIO51, 3 GPIO50, 4 GPIO49, 5-7 GPIO48-46, 8 GND,
# 9 the P4's internal LDO output, 10 3V3, 11 GND, 12 battery. Only the
# signals and grounds are used: power comes in once, through J1.
socket_b = {"8": "GND", "11": "GND"}
for signal, _, socket, pin in HUB75:
    (socket_a if socket == "J1" else socket_b)[pin] = f"P4_{signal}"
add(Part("J1", "Connector_Generic:Conn_01x12", "PH2.0 12-pin", PH12, socket_a, "p4",
         note="Cable to the P4's P3 socket (GPIO2-5, 28-31, 34). Same-direction cable: pin 1 to pin 1."))
add(Part("J2", "Connector_Generic:Conn_01x12", "PH2.0 12-pin", PH12, socket_b, "p4",
         note="Cable to the P4's P1 socket (GPIO49-52). Same-direction cable: pin 1 to pin 1."))
add(R("R1", "10k", "P4_3V3", "P4_OE", "p4", note="Keeps the panel dark (OE high) while the P4 starts."))

# --- Buffer enable: on only while the P4 is powered -------------------------------
add(Part("Q1", "Transistor_FET:2N7002", "2N7002", "Package_TO_SOT_SMD:SOT-23",
         {"1": "P4_3V3", "2": "GND", "3": "BUF_EN"}, "enable",
         note="P4 3.3 V on: pulls the buffers' enable low (enabled)."))
add(R("R2", "100k", "P4_3V3", "GND", "enable", note="Holds Q1 off with the P4 unplugged."))
add(R("R3", "10k", "+5V", "BUF_EN", "enable", note="Buffers disabled unless Q1 is on."))
add(R("R4", "10k", "+5V", "HUB_OE", "enable", note="Panel dark while the buffers are disabled."))

# --- 3.3 V to 5 V buffers -------------------------------------------------------------
buffer_a = {"1": "+5V", "19": "BUF_EN", "20": "+5V", "10": "GND"}
buffer_b = dict(buffer_a)
a_pins = ["2", "3", "4", "5", "6", "7", "8", "9"]
b_pins = ["18", "17", "16", "15", "14", "13", "12", "11"]
for index, (signal, *_) in enumerate(HUB75[:8]):
    buffer_a[a_pins[index]] = f"P4_{signal}"
    buffer_a[b_pins[index]] = f"HUB_{signal}"
for index, (signal, *_) in enumerate(HUB75[8:]):
    buffer_b[a_pins[index]] = f"P4_{signal}"
    buffer_b[b_pins[index]] = f"HUB_{signal}"
for index in range(5, 8):
    buffer_b[a_pins[index]] = "GND"  # unused inputs must not float
    buffer_b[b_pins[index]] = None
add(Part("U1", "74xx:74HC245", "74AHCT245", "Package_SO:TSSOP-20_4.4x6.5mm_P0.65mm", buffer_a, "buffers",
         note="AHCT: TTL inputs accept 3.3 V; drives 5 V."))
add(Part("U2", "74xx:74HC245", "74AHCT245", "Package_SO:TSSOP-20_4.4x6.5mm_P0.65mm", buffer_b, "buffers"))
add(C("C1", "100nF", "+5V", "GND", "buffers"))
add(C("C2", "100nF", "+5V", "GND", "buffers"))

# --- LED panel ----------------------------------------------------------------------
hub = {"1": "HUB_R1", "2": "HUB_G1", "3": "HUB_B1", "4": "GND", "5": "HUB_R2", "6": "HUB_G2", "7": "HUB_B2",
       "8": "GND", "9": "HUB_A", "10": "HUB_B", "11": "HUB_C", "12": "HUB_D", "13": "HUB_CLK", "14": "HUB_LAT",
       "15": "HUB_OE", "16": "GND"}
add(Part("J3", "Connector_Generic:Conn_02x08_Odd_Even", "HUB75", "Connector_IDC:IDC-Header_2x08_P2.54mm_Vertical", hub, "panel",
         note="The panel's own ribbon. Standard HUB75 order; pin 8 (E) grounded: the 64x32 panel scans 1/16."))
add(Part("J4", "Connector:Screw_Terminal_01x02", "5V in", "TerminalBlock_Phoenix:TerminalBlock_Phoenix_MKDS-1,5-2-5.08_1x02_P5.08mm_Horizontal",
         {"1": "GND", "2": "+5V"}, "panel",
         note="From the LED panel's 5 V supply; the buffers draw a few mA. Wire entries face the board's front edge."))
add(C("C3", "22uF 10V", "+5V", "GND", "panel", C1206))

# --- Mounting: four M3 holes ----------------------------------------------------------
for index in range(1, 5):
    add(Part(f"H{index}", "Mechanical:MountingHole", "M3", "MountingHole:MountingHole_3.2mm_M3", {}, "panel"))

# Nets driven only through passive parts (connectors).
POWER_FLAGS = ["+5V", "GND", "P4_3V3"]

GROUP_TITLES = {
    "p4": "Cables from the P4 (sockets P3 and P1)",
    "enable": "Buffers on only while the P4 is powered",
    "buffers": "3.3 V to 5 V buffers",
    "panel": "LED panel ribbon and 5 V",
}

# Top-left corner of each group on the schematic sheet.
GROUPS = {
    "p4": (25.4, 40.64),
    "enable": (25.4, 200.66),
    "buffers": (325.12, 40.64),
    "panel": (325.12, 200.66),
}

SUPPLY_NETS = ["+5V", "GND", "P4_3V3"]

# LCSC part numbers for JLCPCB assembly. The carrier board's numbers where the
# part is the same; recheck stock when ordering.
LCSC = {
    "U1": "C173388", "U2": "C173388",      # 74AHCT245PW,118 (Nexperia)
    "Q1": "C8545",                         # 2N7002 (CJ)
    "R1": "C25804", "R3": "C25804", "R4": "C25804",  # 10k 0603
    "R2": "C25803",                        # 100k 0603
    "C1": "C14663", "C2": "C14663",        # 100 nF 0603
    "C3": "C12891",                        # 22 µF 25 V X5R 1206
    "J1": "C158028", "J2": "C158028",      # JST B12B-PH-K-S(LF)(SN), vertical
    "J3": "C492443",                       # BH254V-16P box header
    "J4": "C480516",                       # Phoenix 1715721 (MKDS 1,5/2-5,08)
}

"""The LED adapter board's circuit: every part, its value and footprint, and the
net on each pin. The schematic and the PCB's pad nets are both generated from
this, as for the carrier board (whose generators these tools reuse).

Revision 2. The board is the system's one power point and links the P4's two
12-pin GPIO sockets to the LED panel's HUB75 ribbon, every cable plugged in at
both ends:

- J5 takes the 5 V supply's barrel plug (MEAN WELL GST60A05-P1J: 5 V, 6 A,
  5.5 x 2.1 mm, centre +). Everything is powered from here.
- J4 feeds the LED panel's own power cable (up to 4 A).
- J6, a USB-C socket, feeds the P4 through a short USB-C to USB-C cable:
  Waveshare documents USB-C (or a battery) as the P4's only power inputs.
- D2 lights while 5 V is present.
- J1 and J2 take the PH2.0 12-pin double-ended cables exactly as they come from
  the bag. Those cables join pin 1 at one end to pin 12 at the other (seen in
  the owner's photos, 2026-11-04: the wire colours run in opposite orders at
  the two ends). The board makes the cross-over: J1/J2 pin n carries the P4
  socket's pin 13 - n.
- Two 74AHCT245 buffers turn the P4's 3.3 V signals into the 5 V the panel
  expects (the same buffers as on the carrier board).
- J3 takes the panel's own 16-pin ribbon cable.

The buffers drive the panel only while the P4 is powered: Q1 enables them
from the P4's 3.3 V. With the P4 off or unplugged, the buffer outputs float
and R4 holds the panel's OE high, so the panel stays dark.

Sources:
- P4 socket pinout: Waveshare ESP32-P4-WIFI6-Touch-LCD-7B schematic, P1 and P3
  (both "PH2.0 12P" vertical sockets).
- GPIO assignment: docs/commissioning/2026-10-14-p4-led-takeover.md.
- LED panel power: Waveshare RGB-Matrix-P4-64x32 wiki, 5 V / 4 A.
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

# The P4's two sockets, pin by pin (Waveshare 7B schematic, pin 1 first).
P3_PINS = ["GPIO36", "GPIO34", "GPIO31", "GPIO30", "GPIO29", "GPIO28", "GPIO5", "GPIO4", "GPIO3", "GPIO2", "GND", "3V3"]
P1_PINS = ["GPIO52", "GPIO51", "GPIO50", "GPIO49", "GPIO48", "GPIO47", "GPIO46", "GND", "VO4", "3V3", "GND", "BAT"]


def board_pin(p4_pin):
    """The J1/J2 pin that a P4 socket pin reaches through an as-delivered cable."""
    return str(13 - p4_pin)


# LED panel signals, in HUB75 order, with their P4 GPIO.
HUB75 = [("R1", 2), ("G1", 3), ("B1", 4), ("R2", 5), ("G2", 28), ("B2", 29), ("A", 30), ("B", 31),
         ("C", 34), ("D", 49), ("CLK", 52), ("LAT", 50), ("OE", 51)]
GPIO_SIGNAL = {f"GPIO{gpio}": f"P4_{signal}" for signal, gpio in HUB75}


def socket_nets(pins, used_supplies):
    """Board-pin -> net for one cable socket. Unused GPIOs and supplies stay open."""
    nets = {}
    for index, name in enumerate(pins, start=1):
        if name in GPIO_SIGNAL:
            net = GPIO_SIGNAL[name]
        elif name in used_supplies:
            net = used_supplies[name]
        else:
            net = None
        nets[board_pin(index)] = net
    return nets


PARTS = []
add = PARTS.append

# --- Cables from the P4 ------------------------------------------------------------
# J1 (from P3) brings the P4's 3.3 V, which enables the buffers, and a ground.
# J2 (from P1) brings only grounds: the P4's VO4, 3V3 and BAT pins stay open.
socket_a = socket_nets(P3_PINS, {"GND": "GND", "3V3": "P4_3V3"})
socket_b = socket_nets(P1_PINS, {"GND": "GND"})
add(Part("J1", "Connector_Generic:Conn_01x12", "PH2.0 12-pin", PH12, socket_a, "p4",
         note="Cable to the P4's P3 socket, as delivered: pin n here is P3 pin 13-n (pin 1 = 3V3, pin 12 = GPIO36)."))
add(Part("J2", "Connector_Generic:Conn_01x12", "PH2.0 12-pin", PH12, socket_b, "p4",
         note="Cable to the P4's P1 socket, as delivered: pin n here is P1 pin 13-n (pin 1 = BAT, open; pin 12 = GPIO52)."))
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
add(Part("J4", "Connector:Screw_Terminal_01x02", "5V to LED panel", "TerminalBlock_Phoenix:TerminalBlock_Phoenix_MKDS-1,5-2-5.08_1x02_P5.08mm_Horizontal",
         {"1": "GND", "2": "+5V"}, "panel",
         note="The LED panel's own power cable, up to 4 A. Wire entries face the board's front edge."))

# --- 5 V input: the system's one power point --------------------------------------
add(Part("J5", "Connector:Barrel_Jack", "PJ-063AH", "Connector_BarrelJack:BarrelJack_CUI_PJ-063AH_Horizontal",
         {"1": "+5V", "2": "GND"}, "power",
         note="5 V supply in: pin 1 centre (+). Takes 5.5 x 2.1 mm plugs; rated 8 A. Opens towards the case's rear."))
add(Part("D1", "Device:D_Zener", "SMAJ5.0A", "Diode_SMD:D_SMA", {"1": "+5V", "2": "GND"}, "power",
         note="Unidirectional TVS: cathode (pad 1) to +5 V. Clamps plug-in spikes."))
add(C("C3", "22uF 10V", "+5V", "GND", "power", C1206))
add(C("C4", "22uF 10V", "+5V", "GND", "power", C1206))
add(C("C5", "22uF 10V", "+5V", "GND", "power", C1206))
add(R("R7", "1k", "+5V", "LED_5V", "power"))
add(Part("D2", "Device:LED", "Green", "LED_SMD:LED_0603_1608Metric", {"1": "GND", "2": "LED_5V"}, "power",
         note="On while 5 V is present."))

# --- 5 V out to the P4 (USB-C) ----------------------------------------------------
add(Part("F1", "Device:Polyfuse", "2.5A hold", "Fuse:Fuse_1812_4532Metric", {"1": "+5V", "2": "P4_5V"}, "p4power",
         note="A short in the P4's cable trips this, not the panel's supply."))
add(Part("J6", "Connector:USB_C_Receptacle_USB2.0_16P", "5V to P4", "Connector_USB:USB_C_Receptacle_GCT_USB4105-xx-A_16P_TopMnt_Horizontal",
         {"A1": "GND", "B1": "GND", "A12": "GND", "B12": "GND", "A4": "P4_5V", "B4": "P4_5V", "A9": "P4_5V", "B9": "P4_5V",
          "A5": "P4_CC1", "B5": "P4_CC2", "A6": None, "B6": None, "A7": None, "B7": None, "A8": None, "B8": None, "SH": "GND"},
         "p4power", note="Power only, to the P4's USB-C through a USB-C to USB-C cable."))
add(R("R5", "56k", "P4_5V", "P4_CC1", "p4power", note="USB-C source (Rp): default USB power."))
add(R("R6", "56k", "P4_5V", "P4_CC2", "p4power"))
add(C("C6", "22uF 10V", "P4_5V", "GND", "p4power", C1206))

# --- Mounting: four M3 holes ----------------------------------------------------------
for index in range(1, 5):
    add(Part(f"H{index}", "Mechanical:MountingHole", "M3", "MountingHole:MountingHole_3.2mm_M3", {}, "panel"))

# Nets driven only through passive parts (connectors).
POWER_FLAGS = ["+5V", "GND", "P4_3V3", "P4_5V"]

GROUP_TITLES = {
    "p4": "Cables from the P4 (sockets P3 and P1)",
    "enable": "Buffers on only while the P4 is powered",
    "buffers": "3.3 V to 5 V buffers",
    "panel": "LED panel ribbon and power",
    "power": "5 V input (barrel), protection, power LED",
    "p4power": "5 V to the P4 (USB-C)",
}

# Top-left corner of each group on the schematic sheet.
GROUPS = {
    "p4": (25.4, 40.64),
    "enable": (25.4, 200.66),
    "buffers": (325.12, 40.64),
    "panel": (325.12, 200.66),
    "power": (25.4, 300.0),
    "p4power": (325.12, 300.0),
}

SUPPLY_NETS = ["+5V", "GND", "P4_3V3", "P4_5V"]

# LCSC part numbers for JLCPCB assembly. The carrier board's numbers where the
# part is the same; recheck stock when ordering.
LCSC = {
    "U1": "C173388", "U2": "C173388",      # 74AHCT245PW,118 (Nexperia)
    "Q1": "C8545",                         # 2N7002 (CJ)
    "R1": "C25804", "R3": "C25804", "R4": "C25804",  # 10k 0603
    "R2": "C25803",                        # 100k 0603
    "C1": "C14663", "C2": "C14663",        # 100 nF 0603
    "C3": "C12891", "C4": "C12891", "C5": "C12891", "C6": "C12891",  # 22 µF 25 V X5R 1206
    "R5": "C23206", "R6": "C23206",        # 56k 0603
    "R7": "C21190",                        # 1k 0603
    "D1": "C78401",                        # SMAJ5.0A
    "D2": "C12624",                        # KT-0603G green
    "F1": "C19078727",                     # 1812L250/16GR, 2.5 A hold
    "J5": "C22434582",                     # CUI PJ-063AH barrel socket
    "J6": "C3020560",                      # GCT USB4105-GF-A (the -060 was out of stock at order time)
    "J1": "C158028", "J2": "C158028",      # JST B12B-PH-K-S(LF)(SN), vertical
    "J3": "C492443",                       # BH254V-16P box header
    "J4": "C480516",                       # Phoenix 1715721 (MKDS 1,5/2-5,08)
}

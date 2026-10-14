"""The carrier board's circuit: every part, its value and footprint, and the net
on each pin. The schematic and the PCB's pad nets are both generated from this,
so they cannot disagree.

Pins are given by number, or by name for parts whose pins are better known by
name (the ESP32 module). Pins not listed get a no-connect flag.

Sources for values:
- TPS568215: TI SLVSDI8C, tables 7-3 (MODE) and 8-2 (components); VFB = 0.6 V.
- CH224A (pin-compatible successor of the CH224K, recommended by WCH for new
  designs): WCH CH224 datasheet V2.0, single-resistor table (56 kΩ = 15 V);
  VHV takes 1 µF, VBUS is tied to VHV.
- LED panel pins and microphone pins follow firmware/esp32/visualizer.
"""
from dataclasses import dataclass, field


@dataclass
class Part:
    ref: str
    symbol: str
    value: str
    footprint: str
    pins: dict
    group: str
    dnp: bool = False
    note: str = ""


R0603 = "Resistor_SMD:R_0603_1608Metric"
C0603 = "Capacitor_SMD:C_0603_1608Metric"
C0805 = "Capacitor_SMD:C_0805_2012Metric"
C1206 = "Capacitor_SMD:C_1206_3216Metric"


def R(ref, value, a, b, group, dnp=False, note=""):
    return Part(ref, "Device:R", value, R0603, {"1": a, "2": b}, group, dnp, note)


def C(ref, value, a, b, group, footprint=C0603, dnp=False, note=""):
    return Part(ref, "Device:C", value, footprint, {"1": a, "2": b}, group, dnp, note)


# Nets carrying the LED panel signals, in HUB75 order, with their S3 GPIOs.
HUB75 = [("R1", "IO4"), ("G1", "IO8"), ("B1", "IO9"), ("R2", "IO10"), ("G2", "IO11"), ("B2", "IO12"),
         ("A", "IO13"), ("B", "IO14"), ("C", "IO15"), ("D", "IO16"), ("CLK", "IO17"), ("LAT", "IO18"), ("OE", "IO21")]

PARTS = []
add = PARTS.append

# --- Power input: 5 V barrel (fitted) -------------------------------------------
add(Part("J1", "Connector:Barrel_Jack", "PJ-063AH", "Connector_BarrelJack:BarrelJack_CUI_PJ-063AH_Horizontal",
         {"1": "VIN_BARREL", "2": "GND"}, "power", note="Pin 1 centre (+5 V). 2.0 mm pin for 5.5 x 2.1 mm plugs; 8 A."))
add(Part("F1", "Device:Polyfuse", "6A hold", "Fuse:Fuse_2920_7451Metric", {"1": "VIN_BARREL", "2": "+5V"}, "power",
         note="Also selects the barrel input: fit F1 or the USB-C power section, never both."))
add(Part("D1", "Device:D_Zener", "SMAJ5.0A", "Diode_SMD:D_SMA", {"1": "+5V", "2": "GND"}, "power",
         note="Unidirectional TVS: cathode (pad 1) to +5 V."))
add(Part("C1", "Device:C_Polarized", "1000uF 10V", "Capacitor_SMD:CP_Elec_10x10.5", {"1": "+5V", "2": "GND"}, "power"))
add(C("C2", "22uF 10V", "+5V", "GND", "power", C1206))
add(C("C3", "100nF", "+5V", "GND", "power"))

# --- USB-C power: PD request for 15 V, then 15 V to 5 V (laid out, not fitted) ---
add(Part("J2", "Connector:USB_C_Receptacle_USB2.0_16P", "USB-C power in", "Connector_USB:USB_C_Receptacle_HRO_TYPE-C-31-M-12",
         {"A1": "GND", "B1": "GND", "A12": "GND", "B12": "GND", "A4": "VBUS_PD", "B4": "VBUS_PD", "A9": "VBUS_PD", "B9": "VBUS_PD",
          "A5": "PD_CC1", "B5": "PD_CC2", "A6": None, "B6": None, "A7": None, "B7": None, "A8": None, "B8": None, "SH": "GND"},
         "usbc_power", dnp=True))
add(Part("U2", "Interface_USB:CH224K", "CH224A", "Package_SO:SSOP-10-1EP_3.9x4.9mm_P1mm_EP2.1x3.3mm",
         {"1": "VBUS_PD", "8": "VBUS_PD", "9": "PD_CFG1", "2": None, "3": None, "4": None, "5": None,
          "6": "PD_CC2", "7": "PD_CC1", "10": None, "11": "GND"}, "usbc_power", dnp=True,
         note="CH224A in the CH224K footprint. Requests 15 V."))
add(R("R10", "56k", "PD_CFG1", "GND", "usbc_power", dnp=True, note="CFG1: 56 kΩ requests 15 V."))
add(C("C10", "1uF 25V", "VBUS_PD", "GND", "usbc_power", C0805, dnp=True))
add(Part("U3", "Regulator_Switching:TPS568215RNN", "TPS568215", "Package_DFN_QFN:Texas_RNN0018A",
         {"1": "BUCK_BOOT", "2": "VBUS_PD", "11": "VBUS_PD", "3": "GND", "4": "GND", "5": "GND", "8": "GND", "9": "GND", "10": "GND",
          "12": "GND", "6": "BUCK_SW", "7": "BUCK_SW", "13": "BUCK_FB", "14": None, "15": "BUCK_EN", "16": None,
          "17": "BUCK_VREG5", "18": "BUCK_MODE"}, "usbc_power", dnp=True,
         note="15 V to 5 V, 8 A limit, 800 kHz FCCM. SS open: 1 ms soft start."))
add(C("C11", "100nF", "BUCK_BOOT", "BUCK_SW", "usbc_power", dnp=True))
add(Part("L1", "Device:L", "2.2uH 10A", "Inductor_SMD:L_Coilcraft_XAL7070-XXX", {"1": "BUCK_SW", "2": "+5V"},
         "usbc_power", dnp=True, note="Saturation current above 10 A."))
add(R("R11", "73.2k", "+5V", "BUCK_FB", "usbc_power", dnp=True, note="5.0 V = 0.6 V x (1 + 73.2k/10k)."))
add(R("R12", "10k", "BUCK_FB", "GND", "usbc_power", dnp=True))
add(C("C12", "150pF", "+5V", "BUCK_FB", "usbc_power", dnp=True, note="Feed-forward, 100-220 pF per TI."))
add(R("R13", "100k", "VBUS_PD", "BUCK_EN", "usbc_power", dnp=True, note="Starts above about 13.5 V: runs only on a 15 V contract."))
add(R("R14", "10k", "BUCK_EN", "GND", "usbc_power", dnp=True))
add(C("C13", "4.7uF", "BUCK_VREG5", "GND", "usbc_power", C0805, dnp=True))
add(R("R15", "120k", "BUCK_VREG5", "BUCK_MODE", "usbc_power", dnp=True, note="MODE: FCCM, ILIM (8 A), 800 kHz."))
add(R("R16", "20k", "BUCK_MODE", "GND", "usbc_power", dnp=True))
for index, ref in enumerate(("C14", "C15", "C16", "C17")):
    add(C(ref, "10uF 25V", "VBUS_PD", "GND", "usbc_power", C1206, dnp=True))
add(C("C18", "100nF 25V", "VBUS_PD", "GND", "usbc_power", dnp=True))
for ref in ("C19", "C20", "C21", "C22", "C23"):
    add(C(ref, "22uF 10V", "+5V", "GND", "usbc_power", C1206, dnp=True))

# --- 3.3 V for the S3 and microphone ---------------------------------------------
add(Part("U5", "Regulator_Linear:AP2112K-3.3", "AP2112K-3.3", "Package_TO_SOT_SMD:SOT-23-5",
         {"1": "+5V", "3": "+5V", "2": "GND", "5": "+3V3", "4": None}, "supply"))
add(C("C7", "1uF", "+5V", "GND", "supply"))
add(C("C8", "1uF", "+3V3", "GND", "supply"))
add(Part("D2", "Device:LED", "Green", "LED_SMD:LED_0603_1608Metric", {"1": "GND", "2": "LED_3V3"}, "supply"))
add(R("R5", "1k", "+3V3", "LED_3V3", "supply"))

# --- ESP32-S3 module, buttons and USB --------------------------------------------
s3 = {"GND": "GND", "3V3": "+3V3", "EN": "S3_EN", "IO0": "S3_BOOT", "USB_D-": "USB_DM", "USB_D+": "USB_DP",
      "IO5": "MIC_SCK", "IO6": "MIC_WS", "IO7": "MIC_SD", "IO1": "S3_UART_TX", "IO2": "S3_UART_RX"}
for signal, gpio in HUB75:
    s3[gpio] = f"S3_{signal}"
add(Part("U1", "RF_Module:ESP32-S3-WROOM-1", "ESP32-S3-WROOM-1-N16R8", "RF_Module:ESP32-S3-WROOM-1", s3, "s3",
         note="Same chip, flash and PSRAM as the current dev board; firmware unchanged."))
add(R("R1", "10k", "+3V3", "S3_EN", "s3"))
add(C("C4", "1uF", "S3_EN", "GND", "s3"))
add(Part("SW1", "Switch:SW_Push", "Reset", "Button_Switch_SMD:SW_SPST_TL3342", {"1": "S3_EN", "2": "GND"}, "s3"))
add(R("R2", "10k", "+3V3", "S3_BOOT", "s3"))
add(Part("SW2", "Switch:SW_Push", "Boot", "Button_Switch_SMD:SW_SPST_TL3342", {"1": "S3_BOOT", "2": "GND"}, "s3"))
add(C("C5", "22uF", "+3V3", "GND", "s3", C1206))
add(C("C6", "100nF", "+3V3", "GND", "s3"))
add(Part("J3", "Connector:USB_C_Receptacle_USB2.0_16P", "S3 USB (P4 link, flashing)", "Connector_USB:USB_C_Receptacle_GCT_USB4105-xx-A_16P_TopMnt_Horizontal",
         {"A1": "GND", "B1": "GND", "A12": "GND", "B12": "GND", "A4": None, "B4": None, "A9": None, "B9": None,
          "A5": "S3_CC1", "B5": "S3_CC2", "A6": "USB_DP", "B6": "USB_DP", "A7": "USB_DM", "B7": "USB_DM",
          "A8": None, "B8": None, "SH": "GND"}, "s3",
         note="VBUS pins unconnected: the P4's 5 V never feeds the board's rail."))
add(R("R3", "5.1k", "S3_CC1", "GND", "s3", note="USB-C device identification."))
add(R("R4", "5.1k", "S3_CC2", "GND", "s3"))
add(Part("U4", "Power_Protection:USBLC6-2SC6", "USBLC6-2SC6", "Package_TO_SOT_SMD:SOT-23-6",
         {"1": "USB_DM", "6": "USB_DM", "3": "USB_DP", "4": "USB_DP", "2": "GND", "5": "+5V"}, "s3"))

# --- LED panel: 3.3 V to 5 V buffers and HUB75 socket ----------------------------
add(R("R6", "10k", "+3V3", "S3_OE", "hub75", note="Keeps the panel dark (OE high) while the S3 starts."))
buffer_a = {"1": "+5V", "19": "GND", "20": "+5V", "10": "GND"}
buffer_b = dict(buffer_a)
a_pins = ["2", "3", "4", "5", "6", "7", "8", "9"]
b_pins = ["18", "17", "16", "15", "14", "13", "12", "11"]
for index, (signal, _) in enumerate(HUB75[:8]):
    buffer_a[a_pins[index]] = f"S3_{signal}"
    buffer_a[b_pins[index]] = f"HUB_{signal}"
for index, (signal, _) in enumerate(HUB75[8:]):
    buffer_b[a_pins[index]] = f"S3_{signal}"
    buffer_b[b_pins[index]] = f"HUB_{signal}"
for index in range(5, 8):
    buffer_b[a_pins[index]] = "GND"  # unused inputs must not float
    buffer_b[b_pins[index]] = None
add(Part("U6", "74xx:74HC245", "74AHCT245", "Package_SO:TSSOP-20_4.4x6.5mm_P0.65mm", buffer_a, "hub75",
         note="AHCT: TTL inputs accept 3.3 V; drives 5 V."))
add(Part("U7", "74xx:74HC245", "74AHCT245", "Package_SO:TSSOP-20_4.4x6.5mm_P0.65mm", buffer_b, "hub75"))
add(C("C24", "100nF", "+5V", "GND", "hub75"))
add(C("C25", "100nF", "+5V", "GND", "hub75"))
hub = {"1": "HUB_R1", "2": "HUB_G1", "3": "HUB_B1", "4": "GND", "5": "HUB_R2", "6": "HUB_G2", "7": "HUB_B2",
       "8": "GND", "9": "HUB_A", "10": "HUB_B", "11": "HUB_C", "12": "HUB_D", "13": "HUB_CLK", "14": "HUB_LAT",
       "15": "HUB_OE", "16": "GND"}
add(Part("J5", "Connector_Generic:Conn_02x08_Odd_Even", "HUB75", "Connector_IDC:IDC-Header_2x08_P2.54mm_Vertical", hub, "hub75",
         note="Standard HUB75 order; pin 8 (E) grounded: the 64x32 panel scans 1/16."))
add(Part("J6", "Connector:Screw_Terminal_01x02", "LED panel 5V", "TerminalBlock_Phoenix:TerminalBlock_Phoenix_MKDS-1,5-2-5.08_1x02_P5.08mm_Horizontal",
         {"1": "+5V", "2": "GND"}, "hub75", note="Up to 4 A."))

# --- Other connectors -------------------------------------------------------------
add(Part("J7", "Connector_Generic:Conn_01x06", "INMP441", "Connector_JST:JST_GH_BM06B-GHS-TBT_1x06-1MP_P1.25mm_Vertical",
         {"1": "+3V3", "2": "GND", "3": "MIC_SCK", "4": "MIC_WS", "5": "MIC_SD", "6": "GND"}, "connectors",
         note="Pin 6 sets L/R low: left channel, as the firmware reads."))
add(Part("F2", "Device:Polyfuse", "2.5A hold", "Fuse:Fuse_1812_4532Metric", {"1": "+5V", "2": "P4_5V"}, "connectors"))
add(Part("J4", "Connector:USB_A", "5V to P4", "Connector_USB:USB_A_Molex_67643_Horizontal",
         {"1": "P4_5V", "2": None, "3": None, "4": "GND", "SH": "GND"}, "connectors",
         note="Power only, to the P4 board's USB-C through a USB-A to USB-C cable."))
add(Part("J8", "Connector_Generic:Conn_01x04", "P4 UART (future)", "Connector_JST:JST_PH_B4B-PH-K_1x04_P2.00mm_Vertical",
         {"1": "GND", "2": "S3_UART_TX", "3": "S3_UART_RX", "4": None}, "connectors", dnp=True,
         note="Check the order against the P4 board's PH2.0 UART socket before use."))

# --- Mounting: four M3 holes into heat-set inserts in the floor posts --------------
for index in range(1, 5):
    add(Part(f"H{index}", "Mechanical:MountingHole", "M3", "MountingHole:MountingHole_3.2mm_M3", {}, "connectors"))

# Nets driven only through passive parts (fuses, jumpers, connectors).
POWER_FLAGS = ["+5V", "GND", "VBUS_PD", "P4_5V"]

GROUP_TITLES = {
    "power": "5 V input (barrel), protection",
    "usbc_power": "USB-C power: PD 15 V request and 15 V to 5 V (not fitted in revision 1)",
    "supply": "3.3 V supply",
    "s3": "ESP32-S3, reset/boot, USB to the P4",
    "hub75": "LED panel: level shifting and HUB75",
    "connectors": "Microphone, P4 power, P4 UART",
}

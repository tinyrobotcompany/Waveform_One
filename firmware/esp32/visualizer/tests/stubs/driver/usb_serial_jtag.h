#pragma once
#include <cstddef>
#include <cstdint>
struct usb_serial_jtag_driver_config_t { size_t rx_buffer_size=256, tx_buffer_size=256; };
#define USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT() usb_serial_jtag_driver_config_t{}
inline int usb_serial_jtag_driver_install(const usb_serial_jtag_driver_config_t*) {return 0;}
int usb_serial_jtag_write_bytes(const void*, size_t, uint32_t);

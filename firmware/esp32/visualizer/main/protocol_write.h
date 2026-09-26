#pragma once
#include <cstddef>
#include "driver/usb_serial_jtag.h"
#include "freertos/FreeRTOS.h"
namespace control {
inline bool write_reply(const char* data, size_t size) {
  // The driver queues a whole line or returns failure. Console VFS writes can
  // silently lose individual characters when the USB buffer fills.
  return usb_serial_jtag_write_bytes(data, size, pdMS_TO_TICKS(100)) == int(size);
}
}

#pragma once
#include "usb_controller.h"
bool p4_led_enabled();
void p4_led_start(LedControllerStatusCallback callback);
StyleRequestResult p4_led_set_style(LedStyle style);

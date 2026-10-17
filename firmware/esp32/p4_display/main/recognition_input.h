#pragma once
#include "usb_controller.h"

using AudioReadyCallback = void (*)(bool);
bool recognition_input_is_local();
void recognition_input_start(AudioReadyCallback ready);
const RecognitionAudio *recognition_input_audio();

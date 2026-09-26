#pragma once
#include "audio_pipeline.h"
#include "styles.h"
void panel_start();
void panel_publish(const audio::Frame& frame);
void panel_set_mode(visual::Mode mode);
visual::Mode panel_mode();

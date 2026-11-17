#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#define WAVEFORM_UPDATE_TARGET "waveshare-p4-7b-rev3-32mb"
#define WAVEFORM_UPDATE_LAYOUT "waveform-ab8-model-v1"
#define WAVEFORM_UPDATE_BASE "https://github.com/tinyrobotcompany/Waveform_One/releases/download/"
typedef enum { UPDATE_IDLE, UPDATE_CHECKING, UPDATE_AVAILABLE, UPDATE_DOWNLOADING,
 UPDATE_VERIFYING, UPDATE_RESTARTING, UPDATE_ERROR } waveform_update_state_t;
typedef struct {
 waveform_update_state_t state;
 bool automatic;
 unsigned interval_hours, window_start, window_end, progress, countdown_seconds;
 int64_t last_check, next_check;
 char installed[32], available[32], message[160], notes_url[160];
} waveform_update_status_t;
typedef struct {
 char version[32], sha256[65], url[256];
 unsigned size;
} waveform_update_manifest_t;
/* Pure policy, shared with host tests. */
bool waveform_update_version(const char *text, unsigned parts[3]);
bool waveform_update_newer(const char *candidate,const char *current);
bool waveform_update_manifest(const char *json,const char *current,waveform_update_manifest_t *out);
bool waveform_update_window(unsigned minute,unsigned start,unsigned end);
uint32_t waveform_update_retry(unsigned failures);
int64_t waveform_update_next(int64_t now,unsigned hours,unsigned jitter);

# Touchscreen microphone mute

On the current P4 device, open **Settings → Microphone → Mute microphone**.
The screen and paired phone show **Microphone muted**. To resume, open the same
page and choose **Unmute microphone**. No hardware button, wiring or GPIO is
required. Phone status follows the existing state polling; the phone cannot
remotely unmute the microphone.

Muting immediately inhibits new recognition requests. The capture task closes
its ADC stream, clears its rolling audio buffer and working samples, and stays
paused. An already-running codec read can take up to its one-second timeout to
return; those samples are discarded. Recognition audio and old track results
are invalidated across mute/unmute transitions. Microphone-reactive LEDs stop
receiving new sound and settle to idle. Display, settings, pairing and OTA remain
available. Unmuting begins a fresh capture segment rather than replaying queued
audio. The feature applies to local P4 microphone capture, not physical capture
on an independently running legacy S3 microphone controller.

## Persistence and errors

The versioned mute setting is saved in the existing NVS partition, separately
from recognition credentials, and loaded before capture/network workers start.
Normal restart and application OTA updates preserve it. A factory erase/reset
that removes NVS also removes this setting; a new device defaults to listening.
Unknown/corrupt settings or storage initialization failures default to muted.
The firmware never erases NVS to recover from an error.

A mute takes effect in RAM even when saving fails. The screen reports the error
and offers **Save muted setting** to retry. Do not assume that an unsaved mute
will survive a restart: the previous durable setting may return. Unmute is
applied only after its save commits successfully; a failed save leaves the
microphone muted. Recover storage and retry rather than clearing other settings.

## Privacy boundary

This is a **software mute**, not an electrical microphone disconnect. The
microphones remain physically connected and may remain powered. It stops the
firmware's capture/recognition path; it is not a guarantee against malicious or
replacement firmware. Waveform One does not save an audio recording archive.
Temporary RAM buffers are reused/cleared, without claiming forensic erasure of
every internal driver/model buffer.

Uploads check permission before opening, between bounded writes and while
reading responses. Cancellation closes the HTTP client and is not retried.
Normal and bench-only diagnostic PCM uploads use the same permission check.
An in-flight network operation can take its configured timeout to return, and
bytes already sent to ACRCloud cannot be recalled. Nothing changes the
provider's retention commitments for data already received.

## Validation

Host tests cover fresh-device default, retained mute/unmute across simulated
restart/OTA, corrupt settings, NVS initialization/open/read/write/commit failures,
retry, transition invalidation, ring clearing and HTTP cancellation/cleanup at
multiple phases on fresh/reused connections. Remote-state tests prevent an old
track reappearing after a fast mute/unmute. Browser fixtures check paired-phone
muted status. P4 builds verify the actual touchscreen code compiles.

Physical acceptance still requires: use the touchscreen while music plays;
confirm phone state, LED idle, request cessation, fresh recognition after unmute,
restart while muted and two OTA installs while muted. No physical acceptance is
claimed while the P4 is disconnected. The earlier hardware-switch design is
superseded and removed by the owner's clarified requirement.

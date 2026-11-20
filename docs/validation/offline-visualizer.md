# Offline visualizer acceptance

Status: NOT EXECUTED. This is a test procedure, not an offline guarantee.
The P4 may be disconnected from the development Mac while documentation is
prepared. Schedule a supervised device session for this procedure; it does not
require flashing, erasing credentials or changing the home router globally.

## Source evidence and limits

At commit 55e625c, `p4_led.cpp` reads onboard PCM and computes the spectrum
locally. Its display gate reads `recognition_music_heard()`, set by the local
presence/classification task in `recognition.cpp`, rather than requiring a
successful cloud identification. However, `recognition_start()` returns before
starting presence tasks when ACRCloud credentials are missing/invalid. Therefore
offline operation on a provisioned device and operation without configured
recognition credentials are separate tests. The latter may reveal a dependency
that needs a focused fix; do not describe it as already supported.

## Equipment and safety

Record hardware revision, firmware/version/commit and image hash, local audio
source, power supply and pre-test settings. Use a controllable test access point
or per-device firewall rules, and a local playback file/source that does not
itself depend on internet. Prepare network restoration before starting.
Use USB logs when useful, with secrets/tokens redacted; standalone-power tests
prove that a connected development computer is not supplying application logic.

| Case | Setup | Minimum observation |
| --- | --- | --- |
| Online control | Provisioned device, normal LAN/WAN | 10 minutes music, 3 stops/resumes, all LED styles |
| WAN loss, LAN intact | Block internet for P4 only, retain DHCP/local phone | 30 minutes including a cold boot, music/quiet and controls |
| No Wi-Fi | Test AP off/unreachable; standalone power and cold boot | 30 minutes, no cached network time assumed |
| Provider unavailable | Permit LAN/time, block identification endpoint using test gateway | 30 minutes; inspect retries and UI responsiveness |
| Sustained outage | WAN blocked | 2 hours; watch memory, watchdogs and frozen frames |
| Recovery | Restore network after each failure case | 10 minutes, automatic recovery without reboot/duplicate work |
| No credentials | Separate spare device/test image only | Cold boot; record whether local visualization starts |

For every case start with a cold device as well as transitioning from online.
Record LED response to music start, each style selection, quiet passage, silence
and resumption. Verify touchscreen remains responsive, no reboot/watchdog loop,
and network retries do not accumulate work. Compare timings against the online
control; proposed acceptance is no visible freeze over one second during music
and no added onset/settle delay over one second versus the same online playback.
Publish observed values rather than retrospectively relaxing limits.

For LAN-only phone access, test existing session restoration and record whether
an unavailable valid clock delays it. This is a phone-access limitation, not a
failure of microphone-to-LED processing. Do not imply the idle clock can know
correct wall time after an unpowered interval without a verified time source.

Record title/artwork behavior honestly: offline visualization is not offline
song identification. Fresh cloud recognition, uncached artwork and update
downloads should be unavailable without internet. Document stale-data handling.

## Evidence and publication

Create a dated report with rows for case, actual duration, cold/warm boot,
network isolation evidence, response measurements, log/video links, pass/fail,
exceptions and restoration outcome. Mark unexecuted rows NOT TESTED. Any failure
gets an issue and a targeted reproduction before a firmware fix.

Record a short consented demo showing the firmware version, standalone power,
network disconnected, music from a local source, reactive LEDs, style changes,
silence/resume and cold boot. Keep raw household conversations and identifying
network information out of published video. Link the full report beside the
demo: a short video cannot establish a two-hour endurance result.

Only after passing publish an offline capability statement scoped to tested
hardware/configuration. A missing-credentials failure must be fixed and retested
before claiming independence from provider provisioning or company services.

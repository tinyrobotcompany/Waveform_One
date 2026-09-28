# ESP boot failure — 27 September 2026

Branch: fix/runtime-stability, based on main 63104e1.

## Verified evidence

Pi uptime exceeded 26 hours; temperature 51.1 C; get_throttled=0x0. The Pi had
not rebooted. Both application services were running but the API reported ESP
disconnected. The attempt to install release sequence 4 failed at the initial
ESP STATUS request, before firmware transfer. The transaction recorded
esp_changed=false; installed version remained v0.3.0/sequence 3. Recovery also
could not obtain STATUS. The transaction journal is retained.

Direct USB capture showed repeated bootloader Image hash failed messages for both
OTA slots and No bootable app partitions. ROM reads of the 295616-byte images at
0x20000 and 0x320000 both exactly matched the verified v0.3.0 application file.
Both dump SHA-256 values:
323eaa36e76cc605fc1c057b6a4127a6afd363906e64280e4e9708ff35ca256a

A reversible bootloader flash-read-speed test at 40 MHz did not yield a healthy
application. The original 80 MHz setting was then restored. No application or
partition table was changed during this investigation. Evidence files and tool
logs are on the Pi at ~/waveform-incident-20260927/.

## Operational state and next investigation

Automatic update timer paused; Pi display and recognition services restarted.
Root cause remains unresolved: bytes read through the ROM are correct, whereas
bootloader image validation fails. This does not prove permanent flash corruption,
thermal failure or power failure. A controlled cold power cycle and independent
power/wiring checks are the next hardware steps. Retain incident images and the
pre-commissioning backup. Resume update/recovery only after stable ESP boot.

## Artwork and latency findings

Recognition overwrote the last confirmed match on every no-match result and
expired it after 90 seconds. Branch tests now require it to persist within the
same playing session, while preserving clearing on silence/session change.
Missing artwork in a same-title/artist match preserves the previous cover; a new
track replaces it. These branch changes are not deployed.

The current protocol acquires eight seconds of audio before identification, with
a two-second gap after results and separate provider processing/network latency.
Timing instrumentation now records capture and identification durations without
logging audio or credentials. No recognition-speed improvement is claimed yet;
shorter or overlapping capture must be evaluated against successful-match rate
on working hardware. Provider or worker outage freshness checks remain intact.

## Cold power cycle result

After the user switched off the panel supply and disconnected ESP USB for ten
seconds, the ESP responded with the expected v0.3.0 digest, `valid` and `healthy`.
Microphone diagnostics continued normally. No application reflash was necessary.
This rules out persistently changed application bytes in this observed failure,
but does not distinguish transient flash/controller state, power integrity or
panel-related electrical effects. Automatic updates remain paused pending a
powered-panel observation. Do not treat the cold restart as a proven root-cause fix.

## Controlled investigation plan

1. Keep automatic updates stopped and preserve the working v0.3.0 application.
2. Install the diagnostic Pi build explicitly as a development test before trying
   to reproduce. The serial consumer records selected boot/error messages with
   Pi wall-clock timestamps in `~/.config/waveform-one/esp-boot.log`. Rotation retains
   one previous file at approximately 1 MiB each; boot loops are rate-limited to
   120 selected lines/minute. PCM and routine audio telemetry are excluded. This
   is application-level evidence and cannot capture events before the Pi opens
   USB or while the updater owns USB.
3. Record exact panel-power condition and time; observe with panel off first,
   then panel on at normal brightness. Do not simultaneously change firmware,
   supply, cables or recognition settings.
4. If stable in both cases, perform controlled software resets and only then an
   explicit update/recovery exercise, retaining journal and boot logs.
5. If the failure returns, preserve the logs and repeat slot readback before
   resetting. If correlated with electrical load, measure voltage at the ESP
   supply pins with appropriate equipment; Pi undervoltage flags do not measure
   the ESP rail. If correlated with reset/update operations, trace IDF/bootloader
   state and compare a known-good identical board under the same conditions.
6. Resume unattended updates only after a reproducible cause, corrective change
   and relevant hardware regression test. A successful short observation alone
   does not establish long-term reliability.

## Panel-on baseline

User confirmed panel power on. Three live API samples over 20 seconds at
2026-09-27 13:53 CEST reported the same connected ESP session in mirrored mode,
with changing band data. Pi temperature was 54.0 C, get_throttled=0x0, uptime over
27 hours; display and recognition active, update timer inactive. This short
observation does not reproduce the fault or establish its cause.

## Diagnostic deployment

Activated Pi build `diagnostic-63104e1-bootlogs`, compiled on the Pi from this
worktree's core/web sources. Post-start API health verified that build identity
and ESP connectivity. ESP application remains signed v0.3.0; recognition worker
has not been replaced with the branch's artwork changes. Automatic update timer
remains stopped.

The diagnostic binary is outside the managed release tree at
`~/waveform-incident-20260927/diagnostic-build/pi/core/target/release/waveform-display`.
Temporary override: `~/.config/systemd/user/waveform-display.service.d/zzz-diagnostics.conf`.
To return to the signed Pi release, move that override outside the service drop-in
directory, run `systemctl --user daemon-reload`, and restart waveform-display.
Remove the override before resuming automatic updates so it cannot mask a newly
installed release. The normal `zz-updates.conf` and release symlink were preserved.

## Recognition-worker controlled test

Activated the artwork-retention worker from this branch at
`~/waveform-incident-20260927/recognition-worker.py` using the current release's
Python environment. Temporary service override:
`~/.config/systemd/user/waveform-recognition.service.d/zzz-diagnostics.conf`.
Remove both recognition and display diagnostic overrides before resuming automatic
updates. The worker now emits capture/identification timing diagnostics; music
recognition quality and latency require a known-song playback test. Initial logs
contained no-match results and a capture HTTP error around the worker handover;
these do not establish recognition speed under known music.

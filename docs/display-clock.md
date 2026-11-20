# Idle clock and night dimming

When no song is displayed, recognition is not searching, and local audio is not
classified as music, the artwork placeholder shows a 24-hour clock. The clock
only appears with valid device time. Song identification/artwork and search
feedback take precedence when music returns.

Open **Settings → Clock & night dimming** to enable/disable automatic dimming,
choose start/end times in 15-minute increments, and select a night brightness
between 10% and 100%. Press **Save display settings** to apply and persist the
schedule. Closing without saving discards edits. The default is enabled from
23:00 to 07:00, at 10%. Equal start/end times are rejected; disable the checkbox
to turn the schedule off. A failed save leaves the previous configuration active.

The schedule uses the device's existing local timezone (currently Central
European time with the existing daylight-saving rule). This change does not add
a timezone selector or a real-time clock battery. After a cold boot without a
valid time source, the idle clock remains hidden and scheduled dimming stays off.
During a DST change the window follows displayed local time, including repeated
or skipped clock hours. Overnight and same-day windows are both supported.

Only the touchscreen backlight is dimmed, and only while idle. The LED panel
already settles when music stops; its gain, brightness and recognition behavior
are unchanged. Touching the screen, or adjusting brightness from the phone,
restores the selected brightness for at least 60 seconds. Music restores it on
the next one-second display tick. Automatic dimming never increases brightness
above the user's selected value.

The phone's brightness slider continues to represent selected brightness. When
the actual screen is dimmer, a separate status reports both values. This avoids
moving the slider underneath a user while automatic dimming is active. Schedule
configuration is on the touchscreen in this version; it is not a new phone form.

Schedule settings survive restart and ordinary OTA updates in their own NVS
namespace. Read/corruption failures disable automatic dimming for that boot until
the owner saves a valid schedule; no storage is erased. Manual brightness retains
its existing behavior and is not newly persisted by this change.

## Validation still needed on hardware

Check idle/song transitions, touch wake and remote manual override, overnight
start/end boundaries, same-day schedules, clock correction/DST, reboot/OTA
persistence, unreadable storage, and cold boot without internet/time. Check the
new settings layout and brightness at night on the physical display. Host tests
cover schedule/persistence policy and phone-state reporting; they do not measure
panel luminance or confirm physical touch behavior.

# Your ACRCloud recognition account

Waveform One can use your own ACRCloud project. Open **Settings → Recognition account** on the touchscreen. Enter the project's regional host, access key and access secret using the three tabs, then choose **Save for next restart**. After any software update has finished, restart the device to apply the change. Saving does not interrupt the current recognition session or reboot during an update.

The host is a hostname such as `identify-eu-west-1.acrcloud.com`, without `https://`, a path or spaces. Use the actual host assigned to your project; the example is not a universal region. This first implementation accepts regional hosts under `acrcloud.com`, not `acrcloud.cn`. The fields correspond to ACRCloud's [official identification API](https://docs.acrcloud.com/reference/identification-api/identification-api) (checked 19 November 2026). Your project must have the required music-recognition entitlement and available quota. A saved configuration means its format and storage passed checks, **not** that ACRCloud has accepted the account. No extra paid validation lookup is sent when saving. Test recognition after restarting and check your ACRCloud usage/plan if identification fails.

To replace an account, enter all three fields again. Existing secrets are never shown or prefilled. An owner configuration takes precedence over earlier USB/bootstrap credentials, including after reboot and public OTA updates.

To stop using the account, tap **Disable recognition**, then **Confirm disable**. This replaces the owner record with a disabled marker. **Current recognition and uploads continue until restart.** Restart after an update finishes to apply it. The disabled marker prevents an old embedded/bootstrap key from silently reactivating recognition. Adding a complete new account enables recognition again on the following restart. Disabling cloud recognition is not microphone mute: local audio analysis/visualization continues. Missing credentials also allow local startup and show the configuration-required message; this relies on PR #28.

## Privacy and storage

- Credential entry is on the touchscreen. No new credential HTTP endpoint exists, and secrets are not sent to the paired phone, logged or included in public firmware/release notes.
- Both access-key and secret entry are masked. The form is cleared on save and close; saved credentials are never read back into it.
- Recognition still uses ACRCloud over the existing HTTPS path. Provider, audio upload format and lookup cadence are unchanged. Your plan bears the resulting usage; no free quota or commercial entitlement is implied.
- Settings are stored in device NVS. This change does not enable flash encryption or claim protection against physical flash/RAM extraction. Logical replacement/disable is not forensic erasure: old flash pages and legacy/bootstrap records can retain credential material. Revoke old keys in ACRCloud when retiring an account or device. A physical user can replace the recognition configuration; there is no owner PIN in this version.
- A failed save reports failure and keeps the previous committed settings. A corrupt/unreadable owner record fails closed for cloud recognition instead of falling back to another account. Local operation continues; storage errors still prevent automatic OTA trial acceptance.

## Validation

Sanitized host tests cover valid/invalid input, first owner save, OTA/reboot loading, owner precedence over bootstrap, replacement, disabling and re-enabling, corruption and failed storage writes/commits. The full firmware build checks touchscreen integration.

The P4 is disconnected. Before release, test touchscreen layout/key entry, cancellation, invalid account feedback after restart, account replacement, disabled boot, settings retention across two signed OTAs, and local visualization without credentials. Use test credentials/a spare device; do not erase the working device's account merely to test this path. No live paid recognition request has been made for these tests.

# Set up Waveform One and your phone

## 1. Power on and connect Wi-Fi

On an unconfigured device the phone setup guide opens automatically. You may
choose **Done / later** to close it. You can reopen it at any time using
**Settings → Phone setup → Show pairing QR**.

Tap **Choose Wi-Fi** (or **Settings → Wi-Fi**). Choose your home network, tap **Enter password**,
enter the Wi-Fi password on the Waveform One touchscreen, then tap **Connect**.
Wait for confirmation. The device checks the connection and saves credentials
before advancing the guide. Passwords stay in the device's existing Wi-Fi
configuration; the phone QR does not contain your Wi-Fi password.

If connection fails, check the selected network/password and retry. **Scan**
refreshes the network list. Only supported networks appear. A storage-recovery
message is not a wrong-password message; do not erase the device to dismiss it.

This flow uses the touchscreen for Wi-Fi entry. It does not create a temporary
Wi-Fi access point or provision the home password through your phone.

## 2. Scan the phone QR

Connect your iPhone or Android phone to the same home network. On Waveform One,
open **Settings → Phone setup → Show pairing QR** to show the large QR, then scan it with the
phone's camera and open the link. No companion app download is required.

The link opens the local Waveform One remote in the browser. Use it for available
display, LED style and software-update controls. The setup guide can be closed
with **Done / later**. It does not claim successful pairing merely because you
closed the guide; verify that the remote shows live device state and responds.

The QR grants access to the local remote: do not publish its image or link. It
rotates and expires; scan the current on-device QR if an old link is rejected.
The large guide QR follows the same code as Settings, including refresh after
pairing. Both disappear when the connection is unavailable.

## Reconnecting and troubleshooting

| Symptom | Action |
| --- | --- |
| No QR yet | Finish connecting Waveform One to Wi-Fi, or wait for its saved network to return. |
| QR link expired | Reopen Phone setup and scan the current code. |
| Browser cannot reach the device | Confirm both devices are on the same LAN; guest/client isolation or a VPN can block local access. |
| Phone asks to pair again | Scan the current QR. Changed device address, cleared browser data or expired/revoked sessions require pairing. |
| Device is reconnecting | The old QR is hidden. Wait, or use Choose Wi-Fi to select another network. |
| Failed Wi-Fi save | Resolve the storage error; joining the access point alone does not complete saved setup. |
| ACRCloud configuration required | Phone pairing and recognition credentials are separate. Pairing does not provision an ACRCloud account. |

## Settings pages and phone capabilities

The touchscreen Settings hub opens separate pages for Microphone, Display,
Appearance, Wi-Fi, Phone setup, LED style, Software updates and Recognition account.
The Microphone entry shows whether it is on or muted. Appearance includes a
preview and a saved-selection message; album colours follow the current artwork,
while the settings pages retain their fixed dark theme.

The paired phone shows the current song, artist, album, artwork, recognition
activity and microphone-muted status. It provides name, appearance, LED style,
brightness and software-update controls. Use **phone Settings → Listening history**
to enable history, view the latest 50 tracks, export JSON, clear it or disable it.
History is opt-in and held in device memory: restarting or disabling history
clears it. A Discogs search opens an external site; Last.fm submission requires
the separately configured desktop bridge and is not automatic from the device.

Wi-Fi passwords, ACRCloud credentials, microphone mute and clock settings are
configured on the touchscreen. Pairing does not provision recognition credentials.
Use **Settings → Recognition account** for ACRCloud keys. Without configured
credentials, the device boots and local visualization remains available;
cloud music recognition needs valid credentials and internet access.

## Physical acceptance still required

Test from an unconfigured device with an iPhone and Android phone: initial guide,
incorrect password, successful saved join, QR scan, live remote action, code
rotation, reconnect, dismissal and reopen. Verify QR scan distance/readability
on the actual screen and that the guide does not obscure a password error.
Host state tests and firmware compilation do not replace this acceptance.

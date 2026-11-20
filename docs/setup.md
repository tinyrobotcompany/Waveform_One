# Set up Waveform One and your phone

## 1. Power on and connect Wi-Fi

On an unconfigured device the phone setup guide opens automatically. You may
choose **Done / later** to close it. You can reopen it at any time using
**Settings → Phone setup**, or by tapping the QR area in Settings.

Tap **Choose Wi-Fi**. In Settings, choose your home network, tap **Enter password**,
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
open **Settings → Phone setup** to show the large QR, then scan it with the
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

The persistent-pairing work is tracked in PR #24; follow the session lifetime
documented for the firmware actually installed. User entry of ACRCloud keys is
a separate planned feature, not part of this Wi-Fi/phone guide. Cloud recognition
requires valid provisioned credentials and internet. The local-only startup fix
is tracked in PR #28; do not assume it is installed before that release.

## Physical acceptance still required

Test from an unconfigured device with an iPhone and Android phone: initial guide,
incorrect password, successful saved join, QR scan, live remote action, code
rotation, reconnect, dismissal and reopen. Verify QR scan distance/readability
on the actual screen and that the guide does not obscure a password error.
Host state tests and firmware compilation do not replace this acceptance.

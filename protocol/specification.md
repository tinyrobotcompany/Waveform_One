# USB control protocol WF1 (prototype)

115200 baud, 8N1, no flow control on the ESP32-S3 native USB Serial/JTAG console.
ASCII LF-terminated commands; CRLF is accepted. Logs share the output stream.
Requests contain a nonzero decimal ID (1–65535) and are at most 96 bytes before
the newline. Overlong/non-printable lines are discarded through the next newline.
Only one client should own the port. It should wait for an acknowledgement before
sending another command; a disconnected mid-line request is discarded by sending
a newline before retrying. This is local USB control, not a network API.

```
WF1 42 MODE mirrored
WF1 42 OK MODE mirrored
WF1 43 STATUS
WF1 43 OK MODE mirrored
```

Modes: `classic`, `mirrored`, `waterfall`. Requests are case-sensitive. A valid
MODE request atomically updates the desired style; the panel applies it at its
next complete scan. The acknowledgement means accepted, not visual confirmation.
STATUS returns the desired mode. Unknown commands/versions produce
`WF1 0 ERR BAD_COMMAND`; no settings change. Client requests time out after three
seconds. Clients match both version and request ID and ignore unrelated logs.

Commands are idempotent, but there is no replay cache or persistent settings.
The renderer clears style-specific history on a mode change. Calibration and
audio sensitivity are unaffected. Boot always selects classic. There is no PCM
streaming, brightness control, OTA or authentication in WF1. The future binary
protocol will use separate framing/version negotiation and CRC; do not mix raw
audio with this line protocol.

---
"waveform-one": patch
---
Initialize the ESP32 USB Serial/JTAG receive driver before enabling the command
reader. ESP-IDF v6.1 nonblocking VFS reads otherwise report no available data,
preventing Pi style commands from being acknowledged.
Write acknowledgements with an explicit leading newline so diagnostic output
truncated while the host was disconnected cannot hide a valid response.

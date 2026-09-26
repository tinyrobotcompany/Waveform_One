# Messages

| Request | Reply | Effect |
| --- | --- | --- |
| `WF1 1 STATUS` | `WF1 1 OK MODE classic` | Read current desired mode |
| `WF1 2 MODE classic` | `WF1 2 OK MODE classic` | Original 24-band bars |
| `WF1 3 MODE mirrored` | `WF1 3 OK MODE mirrored` | Symmetric centre-out spectrum |
| `WF1 4 MODE waterfall` | `WF1 4 OK MODE waterfall` | New frequency row every 50 ms, history below |

See [specification](specification.md) for limits, errors and reconnect behaviour.

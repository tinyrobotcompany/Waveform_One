use std::io::{self, Read, Write};
use std::time::{Duration, Instant};

pub fn request(id: u16, command: &str) -> Result<String, String> {
    if id == 0 {
        return Err("Request ID must be nonzero".into());
    }
    match command {
        "status" => Ok(format!("WF1 {id} STATUS\n")),
        "classic" | "mirrored" | "waterfall" => Ok(format!("WF1 {id} MODE {command}\n")),
        _ => Err("Choose status, classic, mirrored or waterfall".into()),
    }
}

pub struct Replies {
    id: u16,
    line: Vec<u8>,
    discard: bool,
}
impl Replies {
    pub fn new(id: u16) -> Self {
        Self {
            id,
            line: Vec::new(),
            discard: false,
        }
    }
    pub fn push(&mut self, byte: u8) -> Option<Result<String, String>> {
        if byte != b'\n' {
            if byte != b'\r' && !self.discard {
                if self.line.len() == 512 {
                    self.discard = true;
                } else {
                    self.line.push(byte);
                }
            }
            return None;
        }
        let line = std::mem::take(&mut self.line);
        if std::mem::replace(&mut self.discard, false) {
            return None;
        }
        let text = std::str::from_utf8(&line).ok()?;
        let parts: Vec<_> = text.split_whitespace().collect();
        if parts.len() < 3 || parts[0] != "WF1" || parts[1].parse::<u16>().ok() != Some(self.id) {
            return None;
        }
        if parts.len() == 5
            && parts[2] == "OK"
            && parts[3] == "MODE"
            && matches!(parts[4], "classic" | "mirrored" | "waterfall")
        {
            return Some(Ok(parts[4].to_string()));
        }
        Some(Err(format!("Invalid or rejected response: {text}")))
    }
}

// The transport must use bounded reads (the serial adapter uses 100 ms).
pub fn exchange<T: Read + Write + ?Sized>(
    port: &mut T,
    id: u16,
    command: &str,
    timeout: Duration,
) -> Result<String, String> {
    let message = request(id, command)?;
    port.write_all(message.as_bytes())
        .map_err(|e| e.to_string())?;
    let deadline = Instant::now() + timeout;
    let mut replies = Replies::new(id);
    let mut data = [0; 128];
    while Instant::now() < deadline {
        match port.read(&mut data) {
            Ok(0) => return Err("ESP disconnected".into()),
            Ok(size) => {
                for byte in &data[..size] {
                    if let Some(result) = replies.push(*byte) {
                        let mode = result?;
                        if command != "status" && mode != command {
                            return Err("ESP acknowledged a different mode".into());
                        }
                        return Ok(mode);
                    }
                }
            }
            Err(e)
                if matches!(
                    e.kind(),
                    io::ErrorKind::TimedOut
                        | io::ErrorKind::WouldBlock
                        | io::ErrorKind::Interrupted
                ) => {}
            Err(e) => return Err(e.to_string()),
        }
    }
    Err("No matching ESP reply: check firmware, USB connection and port ownership".into())
}

#[cfg(test)]
mod tests {
    use super::*;

    struct FakePort {
        incoming: std::io::Cursor<Vec<u8>>,
        sent: Vec<u8>,
    }
    impl Read for FakePort {
        fn read(&mut self, bytes: &mut [u8]) -> io::Result<usize> {
            self.incoming.read(bytes)
        }
    }
    impl Write for FakePort {
        fn write(&mut self, bytes: &[u8]) -> io::Result<usize> {
            self.sent.extend_from_slice(bytes);
            Ok(bytes.len())
        }
        fn flush(&mut self) -> io::Result<()> {
            Ok(())
        }
    }
    #[test]
    fn exchange_requires_the_requested_mode_and_reports_disconnection() {
        let mut port = FakePort {
            incoming: io::Cursor::new(b"WF1 3 OK MODE mirrored\n".to_vec()),
            sent: vec![],
        };
        assert_eq!(
            exchange(&mut port, 3, "mirrored", Duration::from_secs(1)).unwrap(),
            "mirrored"
        );
        assert_eq!(port.sent, b"WF1 3 MODE mirrored\n");
        assert!(exchange(&mut port, 4, "status", Duration::from_secs(1))
            .unwrap_err()
            .contains("disconnected"));
        port.incoming = io::Cursor::new(b"WF1 3 OK MODE classic\n".to_vec());
        assert!(exchange(&mut port, 3, "mirrored", Duration::from_secs(1)).is_err());
    }
    #[test]
    fn exchange_does_not_wait_past_its_deadline() {
        let mut port = FakePort {
            incoming: io::Cursor::new(vec![]),
            sent: vec![],
        };
        assert!(exchange(&mut port, 1, "status", Duration::ZERO)
            .unwrap_err()
            .contains("No matching"));
    }

    #[test]
    fn commands_are_validated_before_transmission() {
        assert_eq!(request(7, "mirrored").unwrap(), "WF1 7 MODE mirrored\n");
        assert_eq!(request(8, "status").unwrap(), "WF1 8 STATUS\n");
        assert!(request(0, "status").is_err());
        assert!(request(9, "classic\nWF1 10 MODE waterfall").is_err());
    }

    #[test]
    fn fragmented_reply_ignores_logs_and_old_requests() {
        let mut decoder = Replies::new(42);
        for byte in b"SHUT RMS=0.004\nWF1 41 OK MODE classic\nWF1 42 OK MO" {
            assert!(decoder.push(*byte).is_none());
        }
        let mut result = None;
        for byte in b"DE waterfall\r\n" {
            result = decoder.push(*byte).or(result);
        }
        assert_eq!(result, Some(Ok("waterfall".to_string())));
    }

    #[test]
    fn oversize_lines_cannot_be_interpreted_as_replies() {
        let mut decoder = Replies::new(1);
        for byte in vec![b'x'; 600]
            .into_iter()
            .chain(b"WF1 1 OK MODE classic\n".iter().copied())
        {
            assert!(decoder.push(byte).is_none());
        }
        let mut result = None;
        for byte in b"WF1 1 OK MODE mirrored\n" {
            result = decoder.push(*byte).or(result);
        }
        assert_eq!(result, Some(Ok("mirrored".to_string())));
    }
}

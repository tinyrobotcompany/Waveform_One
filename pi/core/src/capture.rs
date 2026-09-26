//! Bounded, sequenced audio transfer over the same USB console as diagnostics.
pub const SAMPLE_BYTES: usize = 16000 * 8 * 2;
pub struct Capture {
    id: u16,
    started: bool,
    bytes: Vec<u8>,
}
impl Capture {
    pub fn new(id: u16) -> Self {
        Self {
            id,
            started: false,
            bytes: Vec::new(),
        }
    }
    pub fn line(&mut self, line: &str) -> Option<Result<Vec<u8>, String>> {
        let p: Vec<_> = line.split_whitespace().collect();
        if p.len() < 3 || p[0] != "WF1" || p[1].parse::<u16>().ok() != Some(self.id) {
            return None;
        }
        if p.as_slice() == ["WF1", &self.id.to_string(), "AUDIO", "16000", "128000"]
            && !self.started
        {
            self.started = true;
            return None;
        }
        if self.started
            && p.len() == 6
            && p[2] == "PCM"
            && p[3].parse::<usize>().ok() == Some(self.bytes.len() / 256)
            && p[4].len() == 512
            && self.bytes.len() < SAMPLE_BYTES
        {
            let decoded: Option<Vec<u8>> = p[4]
                .as_bytes()
                .chunks_exact(2)
                .map(|c| {
                    std::str::from_utf8(c)
                        .ok()
                        .and_then(|s| u8::from_str_radix(s, 16).ok())
                })
                .collect();
            if let Some(bytes) = decoded {
                let hash = bytes.iter().fold(2166136261u32, |h, b| {
                    (h ^ u32::from(*b)).wrapping_mul(16777619)
                });
                if u32::from_str_radix(p[5], 16).ok() == Some(hash) {
                    self.bytes.extend(bytes);
                    return None;
                }
            }
        }
        if self.started
            && p.len() == 4
            && p[2] == "END"
            && p[3] == "1000"
            && self.bytes.len() == SAMPLE_BYTES
        {
            return Some(Ok(std::mem::take(&mut self.bytes)));
        }
        let expected = self.bytes.len() / 256;
        let detail = if p[2] == "PCM" {
            match p.get(3).and_then(|s| s.parse::<usize>().ok()) {
                Some(received) => format!("expected packet {expected}, received packet {received}; header={}, fields={}, payload_chars={}", self.started, p.len(), p.get(4).map_or(0, |s| s.len())),
                None => format!("invalid sequence after {expected} packets"),
            }
        } else if p[2] == "ERR" {
            match p.get(3).copied() {
                Some("AUDIO_LOST") => format!("ESP reported AUDIO_LOST after {expected} packets"),
                Some("BUSY") => "ESP reported BUSY".into(),
                _ => "ESP reported an unknown capture error".into(),
            }
        } else {
            format!("invalid capture response after {expected} packets")
        };
        Some(Err(format!(
            "Audio capture incomplete, corrupted or unsupported: {detail}"
        )))
    }
}

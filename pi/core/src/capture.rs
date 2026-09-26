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
        Some(Err(
            "Audio capture incomplete, corrupted or unsupported".into()
        ))
    }
}

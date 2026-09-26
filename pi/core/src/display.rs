use serde::Serialize;

#[derive(Clone, Debug)]
pub struct Telemetry {
    pub calibrated: bool,
    pub active: bool,
    pub bands: [u8; 24],
}
impl Telemetry {
    pub fn parse(line: &str) -> Option<Self> {
        let (prefix, values) = line.split_once('|')?;
        let digits = values.strip_suffix('|')?;
        if digits.len() != 24 || !digits.bytes().all(|b| b.is_ascii_digit()) {
            return None;
        }
        let fields: Vec<_> = prefix.split_whitespace().collect();
        let calibrated = if fields.contains(&"DISPLAY=CALIBRATING") {
            false
        } else if fields.iter().any(|f| {
            matches!(
                *f,
                "DISPLAY=BARS"
                    | "DISPLAY=GATE_CLOSED"
                    | "DISPLAY=NO_CLEAN_BANDS"
                    | "DISPLAY=BELOW_DISPLAY"
            )
        }) {
            true
        } else {
            return None;
        };
        let mut bands = [0; 24];
        for (target, byte) in bands.iter_mut().zip(digits.bytes()) {
            *target = byte - b'0';
        }
        Some(Self {
            calibrated,
            active: calibrated && fields.contains(&"OPEN"),
            bands,
        })
    }
}

pub struct DisplayState {
    session: u64,
    mode: String,
    connected: bool,
    error: String,
    last_audio: Option<u64>,
    last_music: Option<u64>,
    music_candidate: Option<u64>,
    telemetry: Option<Telemetry>,
}
impl Default for DisplayState {
    fn default() -> Self {
        let session = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap_or_default()
            .as_micros() as u64;
        Self {
            session,
            mode: String::new(),
            connected: false,
            error: String::new(),
            last_audio: None,
            last_music: None,
            music_candidate: None,
            telemetry: None,
        }
    }
}
#[derive(Serialize)]
pub struct View {
    pub session: u64,
    pub phase: &'static str,
    pub connected: bool,
    pub mode: String,
    pub bands: [u8; 24],
    pub error: String,
    pub idle_in_seconds: u64,
}
impl DisplayState {
    pub fn connected(&mut self, mode: &str, _now: u64) {
        self.connected = true;
        self.mode = mode.to_string();
        self.error.clear();
    }
    pub fn disconnected(&mut self, error: &str) {
        self.connected = false;
        self.error = error.to_string();
        self.last_audio = None;
        self.last_music = None;
        self.music_candidate = None;
        self.telemetry = None;
    }
    pub fn telemetry(&mut self, data: Telemetry, now: u64) {
        let was_playing = self.view(now).phase == "playing";
        // The visualizer gate is intentionally sensitive. Screen idle detection
        // requires sustained, meaningful spectral activity instead of any OPEN.
        let substantial = data.active
            && data.bands.iter().filter(|&&v| v >= 2).count() >= 3
            && data.bands.iter().any(|&v| v >= 3)
            && data.bands.iter().map(|&v| u16::from(v)).sum::<u16>() >= 12;
        if substantial {
            if self.last_audio.is_some_and(|t| now.saturating_sub(t) > 2) {
                self.music_candidate = None;
            }
            let since = *self.music_candidate.get_or_insert(now);
            if now.saturating_sub(since) >= 1 {
                self.last_music = Some(now);
            }
        } else {
            self.music_candidate = None;
        }
        self.last_audio = Some(now);
        self.telemetry = Some(data);
        if !was_playing && self.view(now).phase == "playing" {
            self.session = self.session.wrapping_add(1);
        }
    }
    pub fn view(&self, now: u64) -> View {
        let fresh = self.last_audio.is_some_and(|t| now.saturating_sub(t) < 5);
        let phase = if !self.connected {
            "disconnected"
        } else if !fresh {
            "waiting"
        } else if self.telemetry.as_ref().is_some_and(|t| !t.calibrated) {
            "calibrating"
        } else if self.last_music.is_some_and(|t| now.saturating_sub(t) < 30) {
            "playing"
        } else {
            "idle"
        };
        View {
            session: self.session,
            phase,
            connected: self.connected,
            mode: self.mode.clone(),
            bands: if fresh && self.connected {
                self.telemetry.as_ref().map_or([0; 24], |t| t.bands)
            } else {
                [0; 24]
            },
            error: self.error.clone(),
            idle_in_seconds: self
                .last_music
                .map_or(0, |t| 30u64.saturating_sub(now.saturating_sub(t))),
        }
    }
}

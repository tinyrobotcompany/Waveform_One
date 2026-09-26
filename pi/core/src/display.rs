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

#[derive(Default)]
pub struct DisplayState {
    mode: String,
    connected: bool,
    error: String,
    last_audio: Option<u64>,
    last_music: Option<u64>,
    telemetry: Option<Telemetry>,
}
#[derive(Serialize)]
pub struct View {
    pub phase: &'static str,
    pub connected: bool,
    pub mode: String,
    pub bands: [u8; 24],
    pub error: String,
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
        self.telemetry = None;
    }
    pub fn telemetry(&mut self, data: Telemetry, now: u64) {
        if data.active {
            self.last_music = Some(now);
        }
        self.last_audio = Some(now);
        self.telemetry = Some(data);
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
            phase,
            connected: self.connected,
            mode: self.mode.clone(),
            bands: if fresh && self.connected {
                self.telemetry.as_ref().map_or([0; 24], |t| t.bands)
            } else {
                [0; 24]
            },
            error: self.error.clone(),
        }
    }
}

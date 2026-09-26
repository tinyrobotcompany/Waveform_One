use serde::{Deserialize, Serialize};
use std::{
    fs,
    io::{Read, Write},
    path::PathBuf,
    sync::{mpsc, Arc, Mutex},
    thread,
    time::{Duration, Instant},
};
use tiny_http::{Header, Method, Response, Server};
use waveform_control::{
    display::{DisplayState, Telemetry},
    request, Replies,
};

#[derive(Debug)]
enum Answer {
    Mode(String),
    Audio(Vec<u8>),
}
type Shared = Arc<Mutex<DisplayState>>;
struct Pending {
    expected: String,
    sent: Instant,
    reply: Option<mpsc::Sender<Result<Answer, String>>>,
    decoder: Replies,
    capture: Option<waveform_control::capture::Capture>,
}
struct Command {
    mode: String,
    reply: mpsc::Sender<Result<Answer, String>>,
}
#[derive(Clone, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
struct Preferences {
    name: String,
    #[serde(default = "full_brightness")]
    screen_brightness: u8,
}
fn full_brightness() -> u8 {
    100
}
fn seconds(start: Instant) -> u64 {
    start.elapsed().as_secs()
}

fn serial_worker(path: String, state: Shared, commands: mpsc::Receiver<Command>, start: Instant) {
    let mut id = 1u16;
    loop {
        let opened = serialport::new(&path, 115200)
            .timeout(Duration::from_millis(100))
            .preserve_dtr_on_open()
            .open();
        let mut port = match opened {
            Ok(p) => p,
            Err(_) => {
                state
                    .lock()
                    .unwrap()
                    .disconnected("ESP unavailable. Reconnecting automatically…");
                while let Ok(c) = commands.try_recv() {
                    let _ = c.reply.send(Err("ESP disconnected".into()));
                }
                thread::sleep(Duration::from_secs(2));
                continue;
            }
        };
        let mut line = Vec::new();
        let mut overflow = false;
        let mut pending: Option<Pending> = None;
        let mut poll_at = Instant::now();
        'connected: loop {
            if pending.is_none() {
                let next = commands.try_recv().ok();
                if next.is_some() || Instant::now() >= poll_at {
                    let (mode, reply) =
                        next.map_or(("status".to_string(), None), |c| (c.mode, Some(c.reply)));
                    id = if id == u16::MAX { 1 } else { id + 1 };
                    if port
                        .write_all(
                            (if mode == "capture" {
                                format!("WF1 {id} CAPTURE\n")
                            } else {
                                request(id, &mode).unwrap()
                            })
                            .as_bytes(),
                        )
                        .is_err()
                    {
                        if let Some(tx) = reply {
                            let _ = tx.send(Err("USB write failed".into()));
                        }
                        break;
                    }
                    pending = Some(Pending {
                        capture: if mode == "capture" {
                            Some(waveform_control::capture::Capture::new(id))
                        } else {
                            None
                        },
                        expected: mode,
                        sent: Instant::now(),
                        reply,
                        decoder: Replies::new(id),
                    });
                }
            }
            let mut bytes = [0u8; 512];
            match port.read(&mut bytes) {
                Ok(0) => break,
                Ok(n) => {
                    for byte in &bytes[..n] {
                        let mut captured = None;
                        if *byte == b'\n' {
                            if !overflow {
                                if let Ok(text) = std::str::from_utf8(&line) {
                                    if let Some(capture) =
                                        pending.as_mut().and_then(|p| p.capture.as_mut())
                                    {
                                        captured = capture.line(text).map(|r| r.map(Answer::Audio));
                                        if let Some(Err(error)) = &captured {
                                            eprintln!("Audio transfer rejected: {error}");
                                        }
                                    }
                                    if let Some(data) = Telemetry::parse(text) {
                                        state.lock().unwrap().telemetry(data, seconds(start));
                                    }
                                }
                            }
                            line.clear();
                            overflow = false;
                        } else if *byte != b'\r' && !overflow {
                            if line.len() >= 1024 {
                                overflow = true;
                            } else {
                                line.push(*byte);
                            }
                        }
                        let answer = captured.or_else(|| {
                            pending
                                .as_mut()
                                .filter(|p| p.capture.is_none())
                                .and_then(|p| p.decoder.push(*byte))
                                .map(|r| r.map(Answer::Mode))
                        });
                        if let Some(answer) = answer {
                            let Pending {
                                expected, reply, ..
                            } = pending.take().unwrap();
                            let answer = answer.and_then(|answer| match answer {
                                Answer::Mode(ref mode)
                                    if expected != "status" && expected != *mode =>
                                {
                                    Err("ESP acknowledged a different mode".into())
                                }
                                other => Ok(other),
                            });
                            match &answer {
                                Ok(Answer::Mode(mode)) => {
                                    state.lock().unwrap().connected(mode, seconds(start))
                                }
                                Ok(Answer::Audio(_)) => {}
                                // Clip validation failure is not a USB disconnect.
                                Err(_) if expected == "capture" => {}
                                Err(_) => state
                                    .lock()
                                    .unwrap()
                                    .disconnected("ESP rejected a command; reconnecting…"),
                            }
                            let failed = answer.is_err() && expected != "capture";
                            if let Some(tx) = reply {
                                let _ = tx.send(answer);
                            }
                            if failed {
                                break 'connected;
                            }
                            poll_at = Instant::now() + Duration::from_secs(2);
                        }
                    }
                }
                Err(e)
                    if matches!(
                        e.kind(),
                        std::io::ErrorKind::TimedOut
                            | std::io::ErrorKind::WouldBlock
                            | std::io::ErrorKind::Interrupted
                    ) => {}
                Err(_) => break,
            }
            if pending.as_ref().is_some_and(|p| {
                p.sent.elapsed() >= Duration::from_secs(if p.capture.is_some() { 15 } else { 3 })
            }) {
                break;
            }
        }
        if let Some(Pending {
            reply: Some(tx), ..
        }) = pending
        {
            let _ = tx.send(Err("ESP did not acknowledge; reconnecting".into()));
        }
        state
            .lock()
            .unwrap()
            .disconnected("ESP connection lost. Reconnecting automatically…");
        drop(port);
        thread::sleep(Duration::from_secs(1));
    }
}

fn respond(req: tiny_http::Request, status: u16, content: String, mime: &str) {
    let mut response = Response::from_string(content).with_status_code(status);
    for (name, value) in [
        ("Content-Type", mime), ("Cache-Control", "no-store"),
        ("X-Content-Type-Options", "nosniff"), ("Referrer-Policy", "no-referrer"),
        ("Content-Security-Policy", "default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self' data: https:; connect-src 'self'; frame-ancestors 'none'; base-uri 'none'; form-action 'self'")
    ] { response.add_header(Header::from_bytes(name, value).unwrap()); }
    let _ = req.respond(response);
}
fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<_> = std::env::args().collect();
    if args.len() != 2 {
        return Err(
            "Usage: waveform-display SERIAL_PORT (WAVEFORM_BIND defaults to 127.0.0.1:8080)".into(),
        );
    }
    let dir =
        PathBuf::from(std::env::var("WAVEFORM_CONFIG_DIR").unwrap_or_else(|_| {
            format!("{}/.config/waveform-one", std::env::var("HOME").unwrap())
        }));
    fs::create_dir_all(&dir)?;
    let token_path = dir.join("remote-token");
    let token = match fs::read_to_string(&token_path) {
        Ok(token)
            if token.trim().len() == 48 && token.trim().bytes().all(|b| b.is_ascii_hexdigit()) =>
        {
            token.trim().to_string()
        }
        Ok(_) => return Err("Invalid remote-token file".into()),
        Err(e) if e.kind() == std::io::ErrorKind::NotFound => {
            let mut bytes = [0u8; 24];
            fs::File::open("/dev/urandom")?.read_exact(&mut bytes)?;
            let token: String = bytes.iter().map(|b| format!("{b:02x}")).collect();
            use std::os::unix::fs::OpenOptionsExt;
            let mut file = fs::OpenOptions::new()
                .write(true)
                .create_new(true)
                .mode(0o600)
                .open(&token_path)?;
            file.write_all(token.as_bytes())?;
            token
        }
        Err(e) => return Err(e.into()),
    };
    let remote_url = std::env::var("WAVEFORM_REMOTE_URL")
        .unwrap_or_else(|_| "http://raspberrypi.local:8080".into());
    let qr = qrcode::QrCode::new(format!("{remote_url}/#token={token}"))?
        .render::<qrcode::render::svg::Color>()
        .min_dimensions(220, 220)
        .build();
    let preferences_path = dir.join("preferences.json");
    let preferences: Preferences = match fs::read(&preferences_path) {
        Ok(bytes) => serde_json::from_slice(&bytes)?,
        Err(e) if e.kind() == std::io::ErrorKind::NotFound => Preferences {
            name: String::new(),
            screen_brightness: 100,
        },
        Err(e) => return Err(e.into()),
    };
    let preferences = Arc::new(Mutex::new(preferences));
    let state = Arc::new(Mutex::new(DisplayState::default()));
    let backlight = std::env::var("WAVEFORM_BACKLIGHT").ok().map(PathBuf::from);
    let brightness_supported = backlight
        .as_ref()
        .is_some_and(|p| p.join("brightness").exists());
    let (tx, rx) = mpsc::sync_channel::<Command>(4);
    let start = Instant::now();
    let wake_until = Arc::new(Mutex::new(0u64));
    if let Some(path) = backlight {
        let state = state.clone();
        let preferences = preferences.clone();
        let wake_until = wake_until.clone();
        thread::spawn(move || {
            let maximum = fs::read_to_string(path.join("max_brightness"))
                .ok()
                .and_then(|v| v.trim().parse::<u32>().ok())
                .unwrap_or(255);
            let mut last = None;
            loop {
                let idle = state.lock().unwrap().view(seconds(start)).phase == "idle";
                let chosen = preferences.lock().unwrap().screen_brightness;
                let percent = if idle && seconds(start) >= *wake_until.lock().unwrap() {
                    chosen.min(15)
                } else {
                    chosen
                };
                if last != Some(percent) {
                    if fs::write(
                        path.join("brightness"),
                        (maximum * u32::from(percent) / 100).to_string(),
                    )
                    .is_ok()
                    {
                        last = Some(percent);
                    } else {
                        eprintln!("Could not change screen backlight");
                    }
                }
                thread::sleep(Duration::from_secs(1));
            }
        });
    }
    let worker_state = state.clone();
    let serial_path = args[1].clone();
    // One thread owns the port for its lifetime; browser requests never open it.
    thread::spawn(move || serial_worker(serial_path, worker_state, rx, start));
    let bind = std::env::var("WAVEFORM_BIND").unwrap_or_else(|_| "127.0.0.1:8080".into());
    let server = Arc::new(Server::http(&bind).map_err(|e| e.to_string())?);
    eprintln!(
        "Waveform display listening on {bind}; pairing token stored in {}",
        token_path.display()
    );
    let capture_lock = Arc::new(Mutex::new(()));
    let mut workers = vec![];
    for _ in 0..4 {
        let (server, state, preferences, tx, token, preferences_path) = (
            server.clone(),
            state.clone(),
            preferences.clone(),
            tx.clone(),
            token.clone(),
            preferences_path.clone(),
        );
        let capture_lock = capture_lock.clone();
        let recognition_path = dir.join("recognition.json");
        let qr = qr.clone();
        let wake_until = wake_until.clone();
        workers.push(thread::spawn(move || for mut req in server.incoming_requests() {
            let path = req.url().split('?').next().unwrap_or("/").to_string();
            if req.method() == &Method::Get {
                let asset = match path.as_str() {
                    "/" => Some((include_str!("../../../web/index.html"), "text/html; charset=utf-8")),
                    "/ui.mjs" => Some((include_str!("../../../web/ui.mjs"), "text/javascript; charset=utf-8")),
                    "/app.js" => Some((include_str!("../../../web/app.js"), "text/javascript; charset=utf-8")),
                    "/style.css" => Some((include_str!("../../../web/style.css"), "text/css; charset=utf-8")),
                    _ => None,
                };
                if let Some((data, mime)) = asset { respond(req, 200, data.into(), mime); continue; }
            }
            let authorized = req.headers().iter().any(|h| h.field.equiv("Authorization") && h.value.as_str() == format!("Bearer {token}"));
            if !authorized { respond(req, 401, "Pair this browser using the device token".into(), "text/plain"); continue; }
            if req.method() == &Method::Get && path == "/api/pairing-qr" {
                respond(req, 200, qr.clone(), "image/svg+xml");
            } else if req.method() == &Method::Post && path == "/api/wake" {
                *wake_until.lock().unwrap() = seconds(start) + 60;
                respond(req, 200, "{}".into(), "application/json");
            } else if req.method() == &Method::Get && path == "/api/state" {
                let view = state.lock().unwrap().view(seconds(start));
                let prefs = preferences.lock().unwrap().clone();
                let (recognition,track)=waveform_control::recognition::snapshot(&recognition_path,&view);
                respond(req, 200, serde_json::json!({"device":view,"preferences":prefs,"recognition":recognition,"track":track,"screen_brightness_supported":brightness_supported}).to_string(), "application/json");
            } else if req.method() == &Method::Post && path == "/api/capture" {
                if !req.remote_addr().is_some_and(|a|a.ip().is_loopback()) {respond(req,403,"Audio capture is local to the Pi".into(),"text/plain");continue;}
                let view=state.lock().unwrap().view(seconds(start));
                if view.phase!="playing" {respond(req,409,"Waiting for music".into(),"text/plain");continue;}
                let (reply,received)=mpsc::channel();
                // One capture at a time, including queued requests. Never hold up HTTP status.
                let Ok(_guard)=capture_lock.try_lock() else {respond(req,409,"Capture already running".into(),"text/plain");continue;};
                if tx.try_send(Command{mode:"capture".into(),reply}).is_err(){respond(req,503,"Device busy".into(),"text/plain");continue;}
                match received.recv_timeout(Duration::from_secs(20)) {
                    Ok(Ok(Answer::Audio(bytes))) => {
                        let current=state.lock().unwrap().view(seconds(start));
                        if current.phase!="playing" || current.session!=view.session {respond(req,409,"Listening session changed".into(),"text/plain");continue;}
                        let mut response=Response::from_data(bytes);
                        response.add_header(Header::from_bytes("Content-Type","application/octet-stream").unwrap());
                        response.add_header(Header::from_bytes("Cache-Control","no-store").unwrap());
                        let _=req.respond(response);
                    },
                    _=>respond(req,503,"Could not capture a complete audio clip".into(),"text/plain"),
                }
            } else if req.method() == &Method::Post && path.starts_with("/api/mode/") {
                let mode = path.trim_start_matches("/api/mode/");
                if !matches!(mode, "classic" | "mirrored" | "waterfall") { respond(req, 400, "Unknown style".into(), "text/plain"); continue; }
                if !state.lock().unwrap().view(seconds(start)).connected { respond(req, 503, "ESP disconnected".into(), "text/plain"); continue; }
                let (reply, received) = mpsc::channel();
                if tx.try_send(Command { mode: mode.into(), reply }).is_err() { respond(req, 503, "Device busy; try again".into(), "text/plain"); continue; }
                match received.recv_timeout(Duration::from_secs(16)) {
                    Ok(Ok(Answer::Mode(mode))) => respond(req, 200, serde_json::json!({"mode":mode}).to_string(), "application/json"),
                    Ok(Ok(Answer::Audio(_))) => respond(req, 500, "Unexpected audio reply".into(), "text/plain"),
                    Ok(Err(e)) => respond(req, 503, e, "text/plain"),
                    Err(_) => respond(req, 504, "Device command timed out; refresh status".into(), "text/plain"),
                }
            } else if req.method() == &Method::Post && path == "/api/preferences" {
                if req.body_length().is_none_or(|size| size > 512) { respond(req, 413, "Request too large".into(), "text/plain"); continue; }
                let mut data = String::new();
                if req.as_reader().take(513).read_to_string(&mut data).is_err() { respond(req, 400, "Invalid request".into(), "text/plain"); continue; }
                match serde_json::from_str::<Preferences>(&data) {
                    Ok(next) if (10..=100).contains(&next.screen_brightness) && next.name.chars().count() <= 40 && !next.name.chars().any(char::is_control) => {
                        let mut prefs = preferences.lock().unwrap();
                        let temporary = preferences_path.with_extension("tmp");
                        let saved = fs::write(&temporary, serde_json::to_vec(&next).unwrap()).and_then(|_| fs::rename(&temporary, &preferences_path));
                        if saved.is_ok() { *prefs = next; respond(req, 200, "{}".into(), "application/json"); }
                        else { respond(req, 500, "Could not save preferences".into(), "text/plain"); }
                    },
                    _ => respond(req, 400, "Use a name of at most 40 printable characters and screen brightness 10–100".into(), "text/plain"),
                }
            } else { respond(req, 404, "Not found".into(), "text/plain"); }
        }));
    }
    for worker in workers {
        worker.join().unwrap();
    }
    Ok(())
}

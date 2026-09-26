use std::{
    fs,
    io::{Read, Write},
    net::{TcpListener, TcpStream},
    process::{Child, Command, Stdio},
    thread,
    time::{Duration, Instant},
};
struct Running {
    process: Child,
    config: std::path::PathBuf,
}
impl Drop for Running {
    fn drop(&mut self) {
        let _ = self.process.kill();
        let _ = self.process.wait();
        let _ = fs::remove_dir_all(&self.config);
    }
}
fn http(port: u16, request: &str) -> String {
    let mut stream = TcpStream::connect(("127.0.0.1", port)).unwrap();
    stream
        .set_read_timeout(Some(Duration::from_secs(3)))
        .unwrap();
    stream.write_all(request.as_bytes()).unwrap();
    let mut answer = String::new();
    stream.read_to_string(&mut answer).unwrap();
    answer
}
#[test]
fn service_requires_pairing_rejects_invalid_controls_and_persists_greeting() {
    let listener = TcpListener::bind("127.0.0.1:0").unwrap();
    let port = listener.local_addr().unwrap().port();
    drop(listener);
    let config = std::env::temp_dir().join(format!("waveform-http-{}", std::process::id()));
    fs::create_dir_all(&config).unwrap();
    let token = "a".repeat(48);
    fs::write(config.join("remote-token"), &token).unwrap();
    let process = Command::new(env!("CARGO_BIN_EXE_waveform-display"))
        .arg("/missing-waveform-serial")
        .env("WAVEFORM_BIND", format!("127.0.0.1:{port}"))
        .env("WAVEFORM_CONFIG_DIR", &config)
        .stdout(Stdio::null())
        .stderr(Stdio::null())
        .spawn()
        .unwrap();
    let mut running = Running { process, config };
    let deadline = Instant::now() + Duration::from_secs(5);
    while TcpStream::connect(("127.0.0.1", port)).is_err() {
        assert!(
            Instant::now() < deadline,
            "server did not start: {:?}",
            running.process.try_wait()
        );
        thread::sleep(Duration::from_millis(20));
    }
    assert!(http(
        port,
        "GET /api/state HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n"
    )
    .starts_with("HTTP/1.1 401"));
    let headers =
        format!("Host: localhost\r\nAuthorization: Bearer {token}\r\nConnection: close\r\n");
    let state = http(port, &format!("GET /api/state HTTP/1.1\r\n{headers}\r\n"));
    assert!(state.contains("\"phase\":\"disconnected\""));
    assert!(http(
        port,
        &format!("POST /api/mode/mirrored HTTP/1.1\r\n{headers}Content-Length: 0\r\n\r\n")
    )
    .starts_with("HTTP/1.1 503"));
    assert!(http(
        port,
        &format!("POST /api/mode/bogus HTTP/1.1\r\n{headers}Content-Length: 0\r\n\r\n")
    )
    .starts_with("HTTP/1.1 400"));
    assert!(http(port,"POST /api/capture HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\nContent-Length: 0\r\n\r\n").starts_with("HTTP/1.1 401"));
    assert!(http(
        port,
        &format!("POST /api/capture HTTP/1.1\r\n{headers}Content-Length: 0\r\n\r\n")
    )
    .starts_with("HTTP/1.1 409"));
    let body = r#"{"name":"Simon"}"#;
    assert!(http(
        port,
        &format!(
            "POST /api/preferences HTTP/1.1\r\n{headers}Content-Length: {}\r\n\r\n{body}",
            body.len()
        )
    )
    .starts_with("HTTP/1.1 200"));
    assert!(fs::read_to_string(running.config.join("preferences.json"))
        .unwrap()
        .contains("Simon"));
    let body = format!("{{\"name\":\"{}\"}}", "a".repeat(41));
    assert!(http(
        port,
        &format!(
            "POST /api/preferences HTTP/1.1\r\n{headers}Content-Length: {}\r\n\r\n{body}",
            body.len()
        )
    )
    .starts_with("HTTP/1.1 400"));
    let body = r#"{"name":"Simon","screen_brightness":101}"#;
    assert!(http(
        port,
        &format!(
            "POST /api/preferences HTTP/1.1\r\n{headers}Content-Length: {}\r\n\r\n{body}",
            body.len()
        )
    )
    .starts_with("HTTP/1.1 400"));
    assert!(http(
        port,
        "GET /api/pairing-qr HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n"
    )
    .starts_with("HTTP/1.1 401"));
    assert!(http(
        port,
        &format!("GET /api/pairing-qr HTTP/1.1\r\n{headers}\r\n")
    )
    .contains("<svg"));
    assert!(http(
        port,
        "GET /?kiosk=1 HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n"
    )
    .starts_with("HTTP/1.1 200"));
    assert!(http(
        port,
        &format!("POST /api/wake HTTP/1.1\r\n{headers}Content-Length: 0\r\n\r\n")
    )
    .starts_with("HTTP/1.1 200"));
    let asset = http(
        port,
        "GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n",
    );
    assert!(asset.contains("frame-ancestors 'none'"));
    assert!(!asset.contains(&token));
}

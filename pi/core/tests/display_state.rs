use waveform_control::display::{DisplayState, Telemetry};

#[test]
fn quiet_passages_hold_then_idle_and_disconnect_takes_precedence() {
    let mut state = DisplayState::default();
    state.connected("mirrored", 0);
    state.telemetry(
        Telemetry::parse("OPEN DISPLAY=BARS |334453343000000000000000|").unwrap(),
        0,
    );
    state.telemetry(
        Telemetry::parse("OPEN RMS=0.01 DISPLAY=BARS |334453343000000000000000|").unwrap(),
        1,
    );
    state.telemetry(
        Telemetry::parse(
            "SHUT RMS=0.00 DISPLAY=GATE_CLOSED BANDS=0 PEAK_PX=0 |000000000000000000000000|",
        )
        .unwrap(),
        29,
    );
    assert_eq!(state.view(29).phase, "playing");
    assert_eq!(state.view(31).phase, "idle");
    state.disconnected("USB unplugged");
    assert_eq!(state.view(32).phase, "disconnected");
}

#[test]
fn malformed_logs_do_not_become_music_and_missing_telemetry_is_explicit() {
    assert!(Telemetry::parse("WF1 1 OK MODE mirrored").is_none());
    assert!(Telemetry::parse("OPEN DISPLAY=BARS |123|").is_none());
    let mut state = DisplayState::default();
    state.connected("classic", 0);
    assert_eq!(state.view(6).phase, "waiting");
}

#[test]
fn calibration_and_stale_audio_are_not_reported_as_silence() {
    let mut state = DisplayState::default();
    state.connected("classic", 0);
    state.telemetry(
        Telemetry::parse("CAL RMS=0 DISPLAY=CALIBRATING |000000000000000000000000|").unwrap(),
        1,
    );
    assert_eq!(state.view(2).phase, "calibrating");
    assert_eq!(state.view(7).phase, "waiting");
}

#[test]
fn faint_open_gate_and_low_frequency_vibration_do_not_keep_display_awake() {
    let mut state = DisplayState::default();
    state.connected("mirrored", 0);
    for now in 0..40 {
        let digits = if now % 2 == 0 {
            "111111111111111111111111"
        } else {
            "500000000000000000000000"
        };
        state.telemetry(
            Telemetry::parse(&format!("OPEN DISPLAY=BARS |{digits}|")).unwrap(),
            now,
        );
    }
    assert_eq!(state.view(39).phase, "idle");
}

#[test]
fn brief_impulse_does_not_wake_but_sustained_music_does() {
    let mut state = DisplayState::default();
    state.connected("mirrored", 0);
    let music = || Telemetry::parse("OPEN DISPLAY=BARS |334453343000000000000000|").unwrap();
    state.telemetry(music(), 0);
    assert_eq!(state.view(0).phase, "idle");
    state.telemetry(
        Telemetry::parse("SHUT DISPLAY=GATE_CLOSED |000000000000000000000000|").unwrap(),
        1,
    );
    state.telemetry(music(), 10);
    assert_eq!(state.view(10).phase, "idle");
    state.telemetry(music(), 11);
    assert_eq!(state.view(11).phase, "playing");
}

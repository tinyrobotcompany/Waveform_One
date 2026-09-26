use waveform_control::capture::{Capture, SAMPLE_BYTES};
fn packet(seq: usize, bytes: &[u8]) -> String {
    let hash = bytes.iter().fold(2166136261u32, |h, b| {
        (h ^ u32::from(*b)).wrapping_mul(16777619)
    });
    format!(
        "WF1 7 PCM {seq} {} {hash:08x}",
        bytes.iter().map(|b| format!("{b:02x}")).collect::<String>()
    )
}
#[test]
fn complete_capture_requires_exact_sequence_checksum_and_end() {
    let mut c = Capture::new(7);
    assert!(c.line("diagnostic").is_none());
    assert!(c.line("WF1 6 ERR BUSY").is_none());
    assert!(c.line("WF1 7 AUDIO 16000 128000").is_none());
    for seq in 0..SAMPLE_BYTES / 256 {
        assert!(c.line(&packet(seq, &[42; 256])).is_none());
    }
    let audio = c.line("WF1 7 END 1000").unwrap().unwrap();
    assert_eq!(audio.len(), SAMPLE_BYTES);
    assert!(audio.iter().all(|b| *b == 42));
}
#[test]
fn missing_corrupt_or_oversize_audio_is_rejected() {
    for line in [
        packet(1, &[0; 256]),
        packet(0, &[0; 257]),
        "WF1 7 PCM 0 0000 deadbeef".into(),
        "WF1 7 END 1000".into(),
    ] {
        let mut c = Capture::new(7);
        c.line("WF1 7 AUDIO 16000 128000");
        assert!(c.line(&line).unwrap().is_err());
    }
    assert!(Capture::new(7)
        .line(&packet(0, &[0; 256]))
        .unwrap()
        .is_err());
}

#[test]
fn capture_failure_reports_progress_without_audio_payload() {
    let mut c = Capture::new(7);
    c.line("WF1 7 AUDIO 16000 128000");
    c.line(&packet(0, &[42; 256]));
    let error = c.line(&packet(3, &[42; 256])).unwrap().unwrap_err();
    assert!(error.contains("expected packet 1"), "{error}");
    assert!(error.contains("received packet 3"), "{error}");
    assert!(!error.contains("2a2a"));
}

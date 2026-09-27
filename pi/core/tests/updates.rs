use serde_json::json;
use waveform_control::updates::install_request;
#[test]
fn installation_requires_the_version_shown_as_available() {
    let available = json!({"phase":"available","version":"v1.2.3"});
    assert!(install_request(&available, "v1.2.3").is_ok());
    assert!(install_request(&available, "v1.2.4").is_err());
    assert!(install_request(&json!({"phase":"installing","version":"v1.2.3"}), "v1.2.3").is_err());
    assert!(install_request(&json!({"phase":"available","version":"../../x"}), "../../x").is_err());
}

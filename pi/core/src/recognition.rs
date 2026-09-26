//! Only fresh metadata from the local recognition worker reaches the screen.
use crate::display::View;
use serde_json::Value;
use std::{
    fs::File,
    io::Read,
    path::Path,
    time::{SystemTime, UNIX_EPOCH},
};
pub fn snapshot(path: &Path, view: &View) -> (String, Option<Value>) {
    let mut bytes = Vec::new();
    if File::open(path)
        .and_then(|f| f.take(16385).read_to_end(&mut bytes))
        .is_err()
        || bytes.len() > 16384
    {
        return ("unavailable".into(), None);
    }
    let now = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_secs();
    let data = serde_json::from_slice(&bytes).unwrap_or(Value::Null);
    visible(&data, view, now)
}
pub fn visible(data: &Value, view: &View, now: u64) -> (String, Option<Value>) {
    let fresh = data["updated_at"]
        .as_u64()
        .is_some_and(|t| t <= now && now - t < 90);
    if !fresh {
        return ("unavailable".into(), None);
    }
    if view.phase != "playing" || data["session"].as_u64() != Some(view.session) {
        return ("waiting".into(), None);
    }
    let status = data["status"]
        .as_str()
        .filter(|s| {
            matches!(
                *s,
                "listening" | "recognizing" | "matched" | "no_match" | "unavailable"
            )
        })
        .unwrap_or("unavailable")
        .to_string();
    let track = data
        .get("track")
        .filter(|t| {
            t["title"]
                .as_str()
                .is_some_and(|s| !s.is_empty() && s.len() <= 512)
        })
        .cloned();
    (status, track)
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn stale_wrong_session_and_idle_metadata_are_hidden() {
        let mut v = crate::display::DisplayState::default().view(0);
        v.phase = "playing";
        v.session = 42;
        let data = serde_json::json!({"updated_at":100,"session":42,"status":"matched","track":{"title":"Song"}});
        assert!(visible(&data, &v, 101).1.is_some());
        assert!(visible(&data, &v, 190).1.is_none());
        assert!(visible(&data, &v, 99).1.is_none());
        v.session = 43;
        assert!(visible(&data, &v, 101).1.is_none());
        v.session = 42;
        v.phase = "idle";
        assert!(visible(&data, &v, 101).1.is_none());
    }
}

use serde_json::{json, Value};
use std::{fs, path::Path, process::Command};

pub fn snapshot(dir: &Path) -> Value {
    fs::read(dir.join("update-status.json"))
        .ok()
        .filter(|data| data.len() <= 65536)
        .and_then(|data| serde_json::from_slice(&data).ok())
        .unwrap_or_else(
            || json!({"phase":"unconfigured","message":"Updates need one-time device setup."}),
        )
}

pub fn install_request(state: &Value, version: &str) -> Result<Value, String> {
    let valid = version.strip_prefix('v').is_some_and(|v| {
        let parts: Vec<_> = v.split('.').collect();
        parts.len() == 3
            && parts
                .iter()
                .all(|p| !p.is_empty() && p.bytes().all(|c| c.is_ascii_digit()))
    });
    if !valid || version.len() > 64 || state["phase"] != "available" || state["version"] != version
    {
        return Err("Check for updates and select the available version again.".into());
    }
    Ok(json!({"version":version}))
}

pub fn start(dir: &Path, action: &str, version: &str) -> Result<(), String> {
    if !dir.join("updater.json").exists() || !dir.join("update-public.pem").exists() {
        return Err("Updates need one-time device setup.".into());
    }
    let unit = match action {
        "check" => "waveform-update-check.service",
        "install" => {
            let request = install_request(&snapshot(dir), version)?;
            let path = dir.join("update-request.json");
            let temporary = dir.join("update-request.tmp");
            fs::write(&temporary, request.to_string())
                .and_then(|_| fs::rename(&temporary, path))
                .map_err(|_| "Could not save update request.")?;
            "waveform-update-install.service"
        }
        _ => return Err("Unknown update action.".into()),
    };
    let result = Command::new("systemctl")
        .args(["--user", "start", "--no-block", unit])
        .output()
        .map_err(|_| "Update service is unavailable.")?;
    if result.status.success() {
        Ok(())
    } else {
        Err("Could not start update service.".into())
    }
}

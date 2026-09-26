use std::io::{self, Write};
use std::time::{Duration, SystemTime, UNIX_EPOCH};
use waveform_control::{exchange, request};

fn run() -> Result<(), String> {
    let args: Vec<_> = std::env::args().collect();
    if args.len() != 3 {
        return Err(
            "Usage: waveform-control PORT status|classic|mirrored|waterfall|interactive".into(),
        );
    }
    if args[2] != "interactive" {
        request(1, &args[2])?;
    }
    let mut port = serialport::new(&args[1], 115200)
        .timeout(Duration::from_millis(100))
        .preserve_dtr_on_open()
        .open()
        .map_err(|e| format!("Cannot open {}: {e}", args[1]))?;
    // Drop stale diagnostics/acknowledgements before issuing a new request.
    port.clear(serialport::ClearBuffer::Input)
        .map_err(|e| e.to_string())?;
    let mut id = (SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_millis()
        % 65535
        + 1) as u16;
    if args[2] != "interactive" {
        println!(
            "Mode: {}",
            exchange(&mut *port, id, &args[2], Duration::from_secs(3))?
        );
        return Ok(());
    }
    println!("Choose classic, mirrored, waterfall, status or quit. Settings reset on ESP reboot.");
    loop {
        print!("waveform> ");
        io::stdout().flush().map_err(|e| e.to_string())?;
        let mut line = String::new();
        if io::stdin()
            .read_line(&mut line)
            .map_err(|e| e.to_string())?
            == 0
        {
            break;
        }
        let command = line.trim();
        if command == "quit" {
            break;
        }
        id = if id == u16::MAX { 1 } else { id + 1 };
        match exchange(&mut *port, id, command, Duration::from_secs(3)) {
            Ok(mode) => println!("Mode: {mode}"),
            Err(error) => eprintln!("{error}"),
        }
    }
    Ok(())
}
fn main() {
    if let Err(error) = run() {
        eprintln!("{error}");
        std::process::exit(1);
    }
}

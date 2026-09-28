//! Bounded boot/error evidence; never record PCM or routine microphone telemetry.
use std::{fs::{self,OpenOptions},io::Write,path::PathBuf,time::{SystemTime,UNIX_EPOCH}};
pub fn relevant(line: &str) -> bool {
    let line=line.trim();
    line.starts_with("ESP-ROM:") || line.starts_with("rst:") ||
    line.starts_with("Guru Meditation") || line.starts_with("Brownout") ||
    line.starts_with("E (") || line.starts_with("W (") ||
    (line.starts_with("I (") && (line.contains("boot:") || line.contains("boot.esp32s3:")))
}
pub struct Diagnostics {path:PathBuf, window:u64, count:u32}
impl Diagnostics {
    pub fn new(path:PathBuf)->Self {Self{path,window:0,count:0}}
    pub fn record(&mut self,line:&str) {
        let now=SystemTime::now().duration_since(UNIX_EPOCH).unwrap_or_default().as_secs();
        let _=self.record_at(line,now);
    }
    fn record_at(&mut self,line:&str,now:u64)->std::io::Result<()> {
        if line.len()>1024 || !relevant(line){return Ok(());}
        if now/60!=self.window {self.window=now/60;self.count=0;}
        if self.count>=120{return Ok(());}
        self.count+=1;
        if let Some(parent)=self.path.parent(){fs::create_dir_all(parent)?;}
        if fs::metadata(&self.path).is_ok_and(|m|m.len()>=1024*1024) {
            fs::rename(&self.path,self.path.with_extension("previous.log"))?;
        }
        let mut file=OpenOptions::new().create(true).append(true).open(&self.path)?;
        let safe:String=line.trim().chars().filter(|c|!c.is_control()).collect();
        writeln!(file,"{now} {safe}")
    }
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn captures_boot_failure_without_audio_and_bounds_disk_usage() {
        let root=std::env::temp_dir().join(format!("wf-diagnostics-{}",std::process::id()));
        fs::create_dir_all(&root).unwrap();let path=root.join("esp.log");
        let mut logger=Diagnostics::new(path.clone());
        logger.record_at("WF1 4 PCM 0 private-audio",60).unwrap();
        logger.record_at("OPEN RMS=0.03",60).unwrap();
        assert!(!path.exists());
        logger.record_at("rst:0x3 (RTC_SW_SYS_RST)",60).unwrap();
        logger.record_at("E (158) esp_image: Image hash failed",60).unwrap();
        assert!(fs::read_to_string(&path).unwrap().contains("Image hash failed"));
        for _ in 0..200 {logger.record_at("ESP-ROM:esp32s3",60).unwrap();}
        assert_eq!(fs::read_to_string(&path).unwrap().lines().count(),120);
        fs::write(&path,vec![b'x';1024*1024]).unwrap();
        logger.record_at("E (240) boot: No bootable app partitions",120).unwrap();
        assert_eq!(fs::metadata(path.with_extension("previous.log")).unwrap().len(),1024*1024);
        assert!(fs::metadata(&path).unwrap().len()<1024);
        fs::remove_dir_all(root).unwrap();
    }
}

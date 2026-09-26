#include <cassert>
#include <string>
#include <vector>
#include "../main/capture.cpp"
#include "../main/control.cpp"

static visual::Mode mode = visual::Mode::Mirrored;
void panel_set_mode(visual::Mode next) {mode=next;}
visual::Mode panel_mode() {return mode;}

static std::vector<std::string> output;
static bool failWrite=false;
int usb_serial_jtag_write_bytes(const void* data, size_t size, uint32_t timeout) {
  assert(timeout==100);
  if(failWrite) return 0;
  output.emplace_back(static_cast<const char*>(data),size);
  return int(size);
}
int main() {
  capture_init();
  std::vector<int32_t> audio(2*14000, 0);
  assert(capture_start(1));
  capture_audio(audio.data(),14000); // Saturate the 32-packet queue.
  capture_send();
  assert(output.back()=="\nWF1 1 ERR AUDIO_LOST\n");
  output.clear();
  assert(capture_start(2));
  // A complete recording must succeed immediately after the overflow.
  for(int i=0;i<188;++i) {capture_audio(audio.data(),2048);capture_send();}
  assert(output.size()==1001);
  assert(output.front().find("\nWF1 2 PCM 0 ")==0);
  assert(output.back()=="\nWF1 2 END 1000\n");
  output.clear();
  control_handle_line("WF1 3 STATUS");
  assert(output.back()=="\nWF1 3 OK MODE mirrored\n");
  control_handle_line("WF1 4 MODE classic");
  assert(output.back()=="\nWF1 4 OK MODE classic\n");
  control_handle_line("invalid");
  assert(output.back()=="\nWF1 0 ERR BAD_COMMAND\n");
  control_handle_line("WF1 5 CAPTURE");
  assert(output.back()=="\nWF1 5 AUDIO 16000 128000\n");
  control_handle_line("WF1 6 CAPTURE");
  assert(output.back()=="\nWF1 6 ERR BUSY\n");
  // Failing a BUSY reply must not abort the capture already in progress.
  failWrite=true;
  control_handle_line("WF1 7 CAPTURE");
  assert(!capture_start(8));
  failWrite=false;
  capture_abort(5);
  // A failed AUDIO header must release the new capture and its queued data.
  failWrite=true;
  control_handle_line("WF1 9 CAPTURE");
  failWrite=false;
  output.clear();
  control_handle_line("WF1 9 CAPTURE"); // Reusing the request ID resets samples too.
  assert(output.back()=="\nWF1 9 AUDIO 16000 128000\n");
  capture_audio(audio.data(),2048);
  capture_send();
  assert(output.at(1).find("\nWF1 9 PCM 0 ")==0);
  // Failed PCM writes abort safely and permit another complete capture.
  failWrite=true;
  capture_audio(audio.data(),2048);
  capture_send();
  failWrite=false;
  capture_send();
  assert(output.back()=="\nWF1 9 ERR AUDIO_LOST\n");
  output.clear();
  assert(capture_start(10));
  for(int i=0;i<188;++i) {capture_audio(audio.data(),2048);capture_send();}
  assert(output.size()==1001);
  assert(output.back()=="\nWF1 10 END 1000\n");
}

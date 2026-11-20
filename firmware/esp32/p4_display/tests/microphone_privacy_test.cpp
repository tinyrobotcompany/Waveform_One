#include "microphone_privacy.h"
#include "audio_stream.h"
#include <cassert>
int main() {
 using namespace microphone_privacy;
 Permit before; assert(before.valid());
 set_muted(true);assert(muted() && !before.valid());
 set_muted(false);assert(!muted() && !before.valid()); // Edge cannot revive old audio.
 Permit resumed;assert(resumed.valid());
 set_muted(false);assert(resumed.valid());
 bool direct=false; (void)direct;
 read_muted=[] { return true; };assert(!resumed.valid());read_muted=nullptr;
 char secret[]="audio";{Scrub clear{secret,sizeof(secret)};}for(char c:secret)assert(c==0);
 audio_stream::Ring<256> ring;int16_t pcm[128]{};pcm[0]=42;ring.write(pcm,128);
 auto initial=ring.read(0,pcm,128);assert(initial.count==128);
 ring.erase();auto empty=ring.read(0,pcm,128);assert(empty.count==0 && empty.segment!=initial.segment);
 ring.write(pcm,128);assert(ring.read(0,pcm,128).position==128);
}

#include "listening_history.h"
#include <cassert>
#include <cstring>
int main() {
 ListeningHistory fresh; fresh.begin("fresh"); fresh.enable(true);
 RecognitionTrack initial; std::strcpy(initial.title,"First");std::strcpy(initial.artist,"Artist");
 for (int n : {1,11,21}) { initial.matched_at_us=int64_t(n)*1000000;fresh.observe(&initial,1800000000+n); }
 const auto first=fresh.json();assert(first.find("\"observed_seconds\":20")!=std::string::npos);
 assert(first.find("\"started\":1800000001")!=std::string::npos);assert(first.find("First")!=std::string::npos);
 ListeningHistory history; history.begin("test-session");
 RecognitionTrack track; std::strcpy(track.title,"A <script>"); std::strcpy(track.artist,"Artist"); track.duration_s=120;
 auto observe=[&](int second){track.matched_at_us=int64_t(second)*1000000;history.observe(&track,1800000000+second);};
 auto count=[&](){auto *j=cJSON_Parse(history.json().c_str());assert(j);int n=cJSON_GetArraySize(cJSON_GetObjectItem(j,"entries"));cJSON_Delete(j);return n;};
 observe(1);observe(11);observe(21);assert(count()==0);
 history.enable(true);observe(22);observe(32);observe(42);assert(count()==1);
 observe(42);assert(count()==1); // Same match repeated by UI is not additional play time.
 auto encoded=history.json();assert(encoded.find("\"observed_seconds\":20")!=std::string::npos);
 observe(52);assert(history.json().find("\"observed_seconds\":30")!=std::string::npos);
 history.observe(nullptr,1800000053);observe(54);observe(64);observe(74);assert(count()==2);
 observe(100);observe(110);observe(120);assert(count()==3); // Outage separates evidence.
 track.offset_s=50;observe(130);track.offset_s=0;observe(140);observe(150);observe(160);assert(count()==4);
 history.clear();assert(count()==0);observe(170);observe(180);observe(190);assert(count()==1);
 assert(history.json().find("\"id\":5")!=std::string::npos);
 for(int i=0;i<55;++i){history.observe(nullptr,0);observe(200+i*30);observe(210+i*30);observe(220+i*30);}assert(count()==50);
 history.enable(false);assert(count()==0);observe(2000);observe(2010);observe(2020);assert(count()==0);
 history.enable(true);track.matched_at_us=3000000000;history.observe(&track,0);track.matched_at_us+=10000000;history.observe(&track,0);track.matched_at_us+=10000000;history.observe(&track,0);
 assert(history.json().find("\"started\":0")!=std::string::npos);
 history.begin("next-boot");assert(count()==0);assert(history.json().find("\"enabled\":false")!=std::string::npos);
}

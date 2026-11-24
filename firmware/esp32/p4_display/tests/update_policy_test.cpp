#include "waveform_update.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){unsigned parts[3];const char *bad[]={"","v1.2.3","1.2","01.2.3","1.2.3x","1.2.-1","1.2.1234567","1/2.3"};for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);i++)assert(!waveform_update_version(bad[i],parts));assert(!waveform_update_version(NULL,parts));assert(!waveform_update_version("1.2.3",NULL));assert(waveform_update_version("0.9.1",parts)&&parts[2]==1);
 assert(waveform_update_newer("1.0.0","0.9.9"));assert(waveform_update_newer("0.10.0","0.9.9"));assert(!waveform_update_newer("0.9.1","0.9.1"));assert(!waveform_update_newer("0.8.9","0.9.1"));assert(!waveform_update_newer("bad","0.9.1"));
 assert(waveform_update_window(23*60,1380,420));assert(waveform_update_window(0,1380,420));assert(!waveform_update_window(420,1380,420));assert(waveform_update_window(180,180,240));assert(!waveform_update_window(180,180,180));assert(!waveform_update_window(1440,180,240));
 assert(waveform_update_retry(1)==900&&waveform_update_retry(2)==3600&&waveform_update_retry(3)==21600);
 assert(!waveform_update_next(0,720,0));assert(!waveform_update_next(1800000000,2,0));assert(waveform_update_next(1800000000,720,0)==1802592000);assert(waveform_update_next(1800000000,1,1)==1800003601);
 const char *json="{\"schema\":1,\"version\":\"0.9.1\",\"target\":\"" WAVEFORM_UPDATE_TARGET "\",\"layout\":\"" WAVEFORM_UPDATE_LAYOUT "\",\"size\":1024,\"sha256\":\"0000000000000000000000000000000000000000000000000000000000000000\",\"url\":\"" WAVEFORM_UPDATE_BASE "v0.9.1/waveform-p4.bin\"}";
 waveform_update_manifest_t m={};assert(waveform_update_manifest(json,"0.9.0",&m));assert(m.size==1024&&!strcmp(m.version,"0.9.1"));waveform_update_manifest_t before=m;
 assert(!waveform_update_manifest(json,"0.9.1",&m)&&!memcmp(&m,&before,sizeof(m)));assert(!waveform_update_manifest("{","0.9.0",&m));assert(!waveform_update_manifest(NULL,"0.9.0",&m));assert(!waveform_update_manifest(json,"0.9.0",NULL));
 char modified[4098];strcpy(modified,json);strstr(modified,"00000000")[0]='g';assert(!waveform_update_manifest(modified,"0.9.0",&m));strcpy(modified,json);strstr(modified,WAVEFORM_UPDATE_TARGET)[0]='x';assert(!waveform_update_manifest(modified,"0.9.0",&m));memset(modified,'x',4097);modified[4097]=0;assert(!waveform_update_manifest(modified,"0.9.0",&m));
 puts("PASS: monthly/hourly schedule, retry, maintenance windows, monotonic versions and manifest compatibility");}

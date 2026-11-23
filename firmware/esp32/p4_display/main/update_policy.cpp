#include "waveform_update.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
bool waveform_update_version(const char *text,unsigned parts[3])
{
 if(!text||!parts||strlen(text)>31)return false;
 const char *p=text;
 for(unsigned i=0;i<3;i++){
  if(*p<'0'||*p>'9')return false;
  bool zero=*p=='0';unsigned n=0,count=0;
  while(*p>='0'&&*p<='9'){if(++count>6)return false;n=n*10+(unsigned)(*p++-'0');}
  if(zero&&count>1)return false;
  parts[i]=n;if(i<2){if(*p++!='.')return false;}else if(*p)return false;
 }
 return true;
}
bool waveform_update_newer(const char *candidate,const char *current)
{
 unsigned a[3],b[3];if(!waveform_update_version(candidate,a)||!waveform_update_version(current,b))return false;
 for(unsigned i=0;i<3;i++){if(a[i]!=b[i])return a[i]>b[i];}return false;
}
static const char *str(cJSON *j,const char *key){cJSON *v=cJSON_GetObjectItemCaseSensitive(j,key);return cJSON_IsString(v)?v->valuestring:"";}
bool waveform_update_manifest(const char *text,const char *current,waveform_update_manifest_t *out)
{
 if(!text||!out||strlen(text)>4096)return false;
 cJSON *j=cJSON_ParseWithOpts(text,NULL,true);if(!j)return false;
 cJSON *schema=cJSON_GetObjectItemCaseSensitive(j,"schema"),*size=cJSON_GetObjectItemCaseSensitive(j,"size");
 const char *version=str(j,"version"),*hash=str(j,"sha256");char expected[256];
 snprintf(expected,sizeof(expected),WAVEFORM_UPDATE_BASE "v%s/waveform-p4.bin",version);
 bool ok=cJSON_IsNumber(schema)&&schema->valuedouble==1&&cJSON_IsNumber(size)&&isfinite(size->valuedouble)&&
  size->valuedouble>=1024&&size->valuedouble<=0x800000&&floor(size->valuedouble)==size->valuedouble&&
  !strcmp(str(j,"target"),WAVEFORM_UPDATE_TARGET)&&!strcmp(str(j,"layout"),WAVEFORM_UPDATE_LAYOUT)&&
  waveform_update_newer(version,current)&&strlen(hash)==64&&!strcmp(str(j,"url"),expected);
 for(unsigned i=0;ok&&i<64;i++)if(!((hash[i]>='0'&&hash[i]<='9')||(hash[i]>='a'&&hash[i]<='f')))ok=false;
 if(ok){waveform_update_manifest_t result{};result.size=(unsigned)size->valuedouble;snprintf(result.version,sizeof(result.version),"%s",version);memcpy(result.sha256,hash,65);snprintf(result.url,sizeof(result.url),"%s",expected);*out=result;}
 cJSON_Delete(j);return ok;
}
bool waveform_update_window(unsigned minute,unsigned start,unsigned end)
{
 if(minute>=1440||start>=1440||end>=1440||start==end)return false;
 return start<end?minute>=start&&minute<end:minute>=start||minute<end;
}
uint32_t waveform_update_retry(unsigned failures){return failures<=1?900:failures==2?3600:21600;}
int64_t waveform_update_next(int64_t now,unsigned hours,unsigned jitter)
{
 if(now<1577836800||(hours!=1&&hours!=720))return 0;
 return now+(int64_t)hours*3600+(jitter%(hours==1?301:3601));
}

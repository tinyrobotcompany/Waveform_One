#include "update_service.h"
#include "update_platform.h"
#include "esp_app_desc.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "mbedtls/md.h"
#include "mbedtls/pk.h"
#include "cJSON.h"
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#define TAG "ota"
#define LATEST "https://api.github.com/repos/tinyrobotcompany/Waveform_One/releases/latest"
static portMUX_TYPE lock=portMUX_INITIALIZER_UNLOCKED;
static waveform_update_status_t status;
static QueueHandle_t commands;
static bool trial_validated,trial_accepted;
static waveform_update_manifest_t candidate;
static int64_t rate_limited_until;
typedef struct {char location[2048];} response_t;
/* Kept in a separate NVS namespace: normal settings saves and rollback never erase it. */
typedef struct { uint32_t schema,hours,automatic,start,end; int64_t last,next,postponed; char pending[32],blocked[32]; } saved_t;
static saved_t saved;
typedef struct { unsigned action,hours,start,end; bool automatic; } command_t;
extern const char public_key[] __asm__("_binary_ota_public_pem_start");
extern const char public_key_end[] __asm__("_binary_ota_public_pem_end");
static void message(waveform_update_state_t state,const char *text)
{
 portENTER_CRITICAL(&lock);status.state=state;snprintf(status.message,sizeof(status.message),"%s",text);portEXIT_CRITICAL(&lock);
 ESP_LOGI(TAG,"%s",text);
}
static bool persist(void)
{
 nvs_handle_t h;esp_err_t err=nvs_open("waveform_ota",NVS_READWRITE,&h);if(err!=ESP_OK)return false;
 err=nvs_set_blob(h,"v1",&saved,sizeof(saved));if(err==ESP_OK)err=nvs_commit(h);nvs_close(h);return err==ESP_OK;
}
static void accept_trial(void)
{
 esp_ota_img_states_t state;
 if(esp_ota_get_state_partition(esp_ota_get_running_partition(),&state)!=ESP_OK)return;
 if(state==ESP_OTA_IMG_PENDING_VERIFY){
 snprintf(saved.pending,sizeof(saved.pending),"%s",status.installed);
 if(!persist()){message(UPDATE_ERROR,"Trial state save failed; acceptance deferred.");return;}
 nvs_handle_t h;saved_t readback;size_t size=sizeof(readback);
 if(nvs_open("waveform_ota",NVS_READONLY,&h)!=ESP_OK)return;
 esp_err_t err=nvs_get_blob(h,"v1",&readback,&size);nvs_close(h);
 if(err!=ESP_OK||size!=sizeof(readback)||memcmp(&saved,&readback,size))return;
 if(esp_ota_mark_app_valid_cancel_rollback()!=ESP_OK){message(UPDATE_ERROR,"Trial acceptance failed; will retry.");return;}
 trial_accepted=true;
 }else if(state!=ESP_OTA_IMG_VALID||!trial_accepted)return;
 char pending[32];memcpy(pending,saved.pending,32);saved.pending[0]=0;
 if(!persist()){memcpy(saved.pending,pending,32);return;}
 portENTER_CRITICAL(&lock);trial_validated=true;portEXIT_CRITICAL(&lock);
}
bool waveform_update_validate_trial(void)
{
 portENTER_CRITICAL(&lock);bool valid=trial_validated;portEXIT_CRITICAL(&lock);
 if(!valid&&commands){command_t cmd={.action=5};xQueueSend(commands,&cmd,0);}
 return valid;
}
static void publish(void)
{
 portENTER_CRITICAL(&lock);status.interval_hours=saved.hours;status.automatic=saved.automatic;
 status.window_start=saved.start;status.window_end=saved.end;status.last_check=saved.last;status.next_check=saved.next;portEXIT_CRITICAL(&lock);
}
void waveform_update_status(waveform_update_status_t *out)
{
 portENTER_CRITICAL(&lock);*out=status;portEXIT_CRITICAL(&lock);

}
bool waveform_update_request(const char *action)
{
 if(!commands||!action)return false;
 command_t c={0};c.action=!strcmp(action,"check")?1:!strcmp(action,"install")?2:!strcmp(action,"postpone")?3:0;
 return c.action&&xQueueSend(commands,&c,0)==pdTRUE;
}
bool waveform_update_configure(unsigned hours,bool automatic,unsigned start,unsigned end)
{
 if(!commands||(hours!=1&&hours!=720)||start>=1440||end>=1440||start==end)return false;
 command_t c={.action=4,.hours=hours,.automatic=automatic,.start=start,.end=end};return xQueueSend(commands,&c,0)==pdTRUE;
}
/* hop 0 is the URL we chose; later hops come from a Location header.
   Metadata never redirects. Release assets start at this repository's github.com release URL
   and may then move only to GitHub's asset CDN (never back to github.com or elsewhere).
   github.com issues that redirect over verified TLS, and every asset is signature/hash checked,
   so the redirect path is not matched against GitHub's undocumented CDN query format. */
static bool allowed_url(const char *url,bool metadata,unsigned hop)
{
 for(const unsigned char *p=(const unsigned char *)url;*p;p++)if(*p<=' '||*p>=0x7f||*p=='\\')return false;
 if(metadata)return hop==0&&!strcmp(url,LATEST);
 if(hop==0)return !strncmp(url,WAVEFORM_UPDATE_BASE,strlen(WAVEFORM_UPDATE_BASE));
 const char *cdn[]={"https://release-assets.githubusercontent.com/","https://objects.githubusercontent.com/"};
 for(unsigned i=0;i<2;i++)if(!strncmp(url,cdn[i],strlen(cdn[i]))&&url[strlen(cdn[i])])return true;
 return false;
}
static esp_err_t response_header(esp_http_client_event_t *event)
{
 if(event->event_id==HTTP_EVENT_ON_HEADER && event->header_key && event->header_value){
  response_t *response=event->user_data;
  if(!strcasecmp(event->header_key,"Location") && strlen(event->header_value)<sizeof(response->location))
   snprintf(response->location,sizeof(response->location),"%s",event->header_value);
  if(!strcasecmp(event->header_key,"Retry-After")){
   char *end;long seconds=strtol(event->header_value,&end,10);
   if(!*end&&seconds>0&&seconds<=604800)rate_limited_until=(int64_t)time(NULL)+seconds;
  }
  if(!strcasecmp(event->header_key,"X-RateLimit-Reset")){
   char *end;long long reset=strtoll(event->header_value,&end,10);
   if(!*end&&reset>(int64_t)time(NULL)&&reset<(int64_t)time(NULL)+604800)rate_limited_until=reset;
  }
 }
 return ESP_OK;
}
/* Follow only bounded, certificate-verified GitHub redirects. Never send credentials. */
static esp_http_client_handle_t open_url(const char *url,int *http,int64_t *body_length)
{
 *http=0;if(body_length)*body_length=-1;
 bool metadata=!strcmp(url,LATEST);
 /* Only the OTA worker opens clients; storage survives reads and cleanup callbacks. */
 static char current[2048];static response_t response;
 snprintf(current,sizeof(current),"%s",url);
 for(unsigned redirects=0;redirects<4;redirects++){
  if(!allowed_url(current,metadata,redirects))return NULL;
  memset(&response,0,sizeof(response));
  esp_http_client_config_t config={.event_handler=response_header,.user_data=&response,.url=current,.timeout_ms=15000,.buffer_size=4096,.buffer_size_tx=2048,
   .crt_bundle_attach=esp_crt_bundle_attach,.disable_auto_redirect=true};
  esp_http_client_handle_t h=esp_http_client_init(&config);if(!h)return NULL;
  esp_http_client_set_header(h,"User-Agent","Waveform One-OTA/1");esp_http_client_set_header(h,"Accept-Encoding","identity");
  if(esp_http_client_open(h,0)!=ESP_OK){esp_http_client_cleanup(h);return NULL;}
  int64_t length=esp_http_client_fetch_headers(h);
  if(length<0){esp_http_client_cleanup(h);return NULL;}
  *http=esp_http_client_get_status_code(h);
  if(*http==200){if(body_length)*body_length=length;rate_limited_until=0;return h;}
  bool redirect=(*http==301||*http==302||*http==303||*http==307||*http==308)&&
    response.location[0]&&strlen(response.location)<sizeof(current);
  if(redirect)snprintf(current,sizeof(current),"%s",response.location);
  esp_http_client_cleanup(h);if(!redirect)return NULL;
 }
 return NULL;
}
static unsigned char *download(const char *url,size_t limit,size_t *length,int *http)
{
 *length=0;*http=0;esp_http_client_handle_t h=open_url(url,http,NULL);if(!h)return NULL;
 unsigned char *data=malloc(limit+1);bool ok=data!=NULL;int64_t deadline=esp_timer_get_time()+60000000;
 while(ok&&*length<limit){int n=esp_http_client_read(h,(char *)data+*length,(int)(limit-*length));if(n<0){ok=false;break;}if(!n)break;*length+=(size_t)n;if(esp_timer_get_time()>deadline)ok=false;}
 ok=ok&&*length>0&&esp_http_client_is_complete_data_received(h);
 esp_http_client_cleanup(h);if(!ok){free(data);return NULL;}data[*length]=0;return data;
}
static bool verify(const unsigned char *data,size_t size,const unsigned char *signature,size_t signature_size)
{
 unsigned char hash[32];mbedtls_pk_context pk;mbedtls_pk_init(&pk);
 bool ok=mbedtls_md(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),data,size,hash)==0&&
  mbedtls_pk_parse_public_key(&pk,(const unsigned char *)public_key,(size_t)(public_key_end-public_key))==0&&
  mbedtls_pk_verify(&pk,MBEDTLS_MD_SHA256,hash,sizeof(hash),signature,signature_size)==0;
 mbedtls_pk_free(&pk);return ok;
}
static bool check_release(void)
{
 if(rate_limited_until>(int64_t)time(NULL)){message(UPDATE_ERROR,"Update server asked us to wait. Check will retry later.");return false;}
 memset(&candidate,0,sizeof(candidate));
 portENTER_CRITICAL(&lock);status.available[0]=0;status.notes_url[0]=0;portEXIT_CRITICAL(&lock);
 message(UPDATE_CHECKING,"Checking for updates…");size_t length;int http;
 unsigned char *data=download(LATEST,131072,&length,&http);
 if(!data){if(http==404){message(UPDATE_IDLE,"No published update yet.");return true;}message(UPDATE_ERROR,"Update check failed. Will retry automatically.");return false;}
 cJSON *j=cJSON_ParseWithOpts((char *)data,NULL,true);free(data);
 if(!cJSON_IsObject(j)){cJSON_Delete(j);message(UPDATE_ERROR,"Invalid release metadata.");return false;}
 const cJSON *tag=cJSON_GetObjectItemCaseSensitive(j,"tag_name"),*draft=cJSON_GetObjectItemCaseSensitive(j,"draft"),*pre=cJSON_GetObjectItemCaseSensitive(j,"prerelease");
 char version[32]={0};unsigned parts[3];bool valid=cJSON_IsString(tag)&&tag->valuestring[0]=='v'&&
  waveform_update_version(tag->valuestring+1,parts)&&cJSON_IsFalse(draft)&&cJSON_IsFalse(pre);
 if(valid)snprintf(version,sizeof(version),"%s",tag->valuestring+1);
 cJSON_Delete(j);
 if(!valid){message(UPDATE_ERROR,"Invalid release metadata.");return false;}
 if(!waveform_update_newer(version,status.installed)){message(UPDATE_IDLE,"Waveform One is up to date.");return true;}
 if(!strcmp(version,saved.blocked)){message(UPDATE_ERROR,"This version was rolled back. Waiting for a newer release.");return true;}
 char url[256];snprintf(url,sizeof(url),WAVEFORM_UPDATE_BASE "v%s/waveform-p4-manifest.json",version);
 data=download(url,4096,&length,&http);if(!data){message(UPDATE_ERROR,"Update manifest unavailable.");return false;}
 size_t sig_size;snprintf(url,sizeof(url),WAVEFORM_UPDATE_BASE "v%s/waveform-p4-manifest.sig",version);
 unsigned char *sig=download(url,1024,&sig_size,&http);
 waveform_update_manifest_t next;
 valid=sig&&verify(data,length,sig,sig_size)&&waveform_update_manifest((char *)data,status.installed,&next)&&!strcmp(next.version,version);
 free(data);free(sig);if(!valid){message(UPDATE_ERROR,"Update signature or compatibility check failed.");return false;}
 candidate=next;
 portENTER_CRITICAL(&lock);snprintf(status.available,sizeof(status.available),"%s",version);snprintf(status.notes_url,sizeof(status.notes_url),"https://github.com/tinyrobotcompany/Waveform_One/releases/tag/v%s",version);status.progress=0;portEXIT_CRITICAL(&lock);
 message(UPDATE_AVAILABLE,"Update available. See What’s new before installing.");return true;
}
/* Downloads candidate.url into partition and finalises it. Returns NULL on success or the
   user-facing error. Once the HTTP client, OTA handle and hash exist there are no early returns:
   all three are released in the single cleanup block at the end. */
static const char *write_image(const esp_partition_t *partition)
{
 int http=0;int64_t body_length=-1;
 esp_http_client_handle_t h=open_url(candidate.url,&http,&body_length);
 if(!h||http!=200){if(h)esp_http_client_cleanup(h);return "Download unavailable. Current firmware is unchanged.";}
 /* Require the signed size before erasing flash. */
 if(body_length<=0||(uint64_t)body_length!=candidate.size){esp_http_client_cleanup(h);return "Download size mismatch or unknown. Current firmware is unchanged.";}
 esp_ota_handle_t ota=0;bool begun=esp_ota_begin(partition,candidate.size,&ota)==ESP_OK;
 mbedtls_md_context_t hash;mbedtls_md_init(&hash);
 bool ok=begun&&mbedtls_md_setup(&hash,mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),0)==0&&mbedtls_md_starts(&hash)==0;
 size_t total=0;unsigned char buffer[4096],digest[32]={0};int64_t deadline=esp_timer_get_time()+300000000;
 while(ok&&total<candidate.size){size_t wanted=candidate.size-total;if(wanted>sizeof(buffer))wanted=sizeof(buffer);
  int n=esp_http_client_read(h,(char *)buffer,(int)wanted);if(n<=0||esp_timer_get_time()>deadline){ok=false;break;}
  ok=mbedtls_md_update(&hash,buffer,(size_t)n)==0&&esp_ota_write(ota,buffer,(size_t)n)==ESP_OK;total+=(size_t)n;
  portENTER_CRITICAL(&lock);status.progress=(unsigned)(total*100/candidate.size);portEXIT_CRITICAL(&lock);
 }
 bool extra_bytes=false;
 if(ok){char extra;int n=esp_http_client_read(h,&extra,1);extra_bytes=n>0;
  /* Require EOF and complete framing. */
  ok=n==0&&esp_http_client_is_complete_data_received(h);
 }
 message(UPDATE_VERIFYING,"Verifying downloaded firmware…");
 ok=ok&&total==candidate.size&&mbedtls_md_finish(&hash,digest)==0;
 char hex[65];for(unsigned i=0;i<32;i++)snprintf(hex+i*2,3,"%02x",digest[i]);ok=ok&&!strcmp(hex,candidate.sha256);
 /* esp_ota_end() only finalises and validates the written image; it never changes the boot
    selection (otadata). On any failure we return before esp_ota_set_boot_partition(),
    so the running image stays the boot image. tests/test_update_service.c pins this. */
 if(ok){ok=esp_ota_end(ota)==ESP_OK;begun=false;}
 /* Single cleanup: every path after the size check reaches here. */
 if(begun)esp_ota_abort(ota);
 mbedtls_md_free(&hash);
 esp_http_client_cleanup(h);
 if(!ok)return extra_bytes?"Download contains extra bytes. Current firmware is unchanged.":"Firmware verification failed. Current firmware is unchanged.";
 return NULL;
}
static bool install(void)
{
 if(!candidate.version[0]){message(UPDATE_ERROR,"Check for an update first.");return false;}
 esp_ota_img_states_t state;
 if(esp_ota_get_state_partition(esp_ota_get_running_partition(),&state)==ESP_OK&&state==ESP_OTA_IMG_PENDING_VERIFY){message(UPDATE_ERROR,"Current firmware is still completing startup checks.");return false;}
 const esp_partition_t *partition=esp_ota_get_next_update_partition(NULL);
 if(!partition||candidate.size>partition->size){message(UPDATE_ERROR,"Update does not fit this device.");return false;}
 message(UPDATE_DOWNLOADING,"Downloading update… Keep Waveform One powered on.");
 const char *error=write_image(partition);
 if(error){message(UPDATE_ERROR,error);return false;}
 esp_app_desc_t description;
 if(esp_ota_get_partition_description(partition,&description)!=ESP_OK||strcmp(description.version,candidate.version)||strcmp(description.project_name,"waveform_p4_display")){
  message(UPDATE_ERROR,"Firmware verification failed. Current firmware is unchanged.");return false;}
 snprintf(saved.pending,sizeof(saved.pending),"%s",candidate.version);
 if(!persist()){saved.pending[0]=0;message(UPDATE_ERROR,"Could not save activation intent. Current firmware is unchanged.");return false;}
 if(esp_ota_set_boot_partition(partition)!=ESP_OK){saved.pending[0]=0;persist();message(UPDATE_ERROR,"Could not activate update. Current firmware is unchanged.");return false;}
 message(UPDATE_RESTARTING,"Update verified. Restarting Waveform One…");vTaskDelay(pdMS_TO_TICKS(1500));esp_restart();return true;
}
static void worker(void *unused)
{
 (void)unused;unsigned failures=0;int64_t connected_since=0,postponed_until=saved.postponed,countdown=0;bool manual_install=false,force_check=false;
 for(;;){
  int64_t now=time(NULL),mono=esp_timer_get_time();waveform_update_network_t net;waveform_update_network(&net);
  bool ready=net.connected&&net.time_ready&&now>=1577836800;
  if(!ready)connected_since=0;else if(!connected_since)connected_since=mono;
  /* Preserve the trial marker until boot health is confirmed. A rollback reads it on old firmware. */
  if(saved.pending[0]&&!strcmp(saved.pending,status.installed)){
   esp_ota_img_states_t image_state;
   if(esp_ota_get_state_partition(esp_ota_get_running_partition(),&image_state)==ESP_OK&&image_state==ESP_OTA_IMG_VALID){
    char pending[32];memcpy(pending,saved.pending,32);saved.pending[0]=0;
    if(!persist())memcpy(saved.pending,pending,32);
   }
  }
  command_t cmd;
  while(xQueueReceive(commands,&cmd,0)==pdTRUE){
   if(cmd.action==5)accept_trial();
   if(cmd.action==1){if(ready){saved.next=0;force_check=true;}else message(UPDATE_ERROR,"Connect to Wi-Fi and synchronise time to check updates.");}
   if(cmd.action==2){if(ready&&candidate.version[0]){manual_install=true;countdown=mono+60000000;message(UPDATE_AVAILABLE,"Installing in 60 seconds. Choose Postpone to cancel.");}else message(UPDATE_ERROR,"Check for an available update while connected first.");}
   if(cmd.action==3){countdown=0;manual_install=false;postponed_until=now+86400;saved.postponed=postponed_until;if(!persist())message(UPDATE_ERROR,"Postponed, but saving failed. This may not survive a restart.");else message(candidate.version[0]?UPDATE_AVAILABLE:UPDATE_IDLE,"Installation postponed for 24 hours.");}
   if(cmd.action==4){saved_t old=saved;saved.hours=cmd.hours;saved.automatic=cmd.automatic;saved.start=cmd.start;saved.end=cmd.end;
    saved.next=waveform_update_next(now,saved.hours,esp_random());if(!persist()){saved=old;message(UPDATE_ERROR,"Could not save update settings.");}else message(candidate.version[0]?UPDATE_AVAILABLE:UPDATE_IDLE,"Update settings saved.");publish();}
  }
  if(ready&&!countdown&&(force_check||mono-connected_since>=30000000)&&(!saved.next||now>=saved.next)){
   force_check=false;bool ok=check_release();saved.last=now;failures=ok?0:failures+1;saved.next=ok?waveform_update_next(now,saved.hours,esp_random()):now+waveform_update_retry(failures);
   if(rate_limited_until>saved.next)saved.next=rate_limited_until;
   if(!persist())message(UPDATE_ERROR,"Could not save update check history.");
   publish();
  }
  time_t civil=(time_t)now;struct tm local={0};bool window=localtime_r(&civil,&local)&&waveform_update_window((unsigned)(local.tm_hour*60+local.tm_min),saved.start,saved.end);
  if(ready&&candidate.version[0]&&!countdown&&saved.automatic&&window&&now>=postponed_until){countdown=mono+60000000;message(UPDATE_AVAILABLE,"Scheduled update in 60 seconds. Choose Postpone to cancel.");}
  if(countdown&&mono>=countdown){
   countdown=0;
   portENTER_CRITICAL(&lock);status.countdown_seconds=0;portEXIT_CRITICAL(&lock);
   if(ready&&(manual_install||(saved.automatic&&window))){install();postponed_until=now+86400;}
   else message(UPDATE_AVAILABLE,"Installation deferred until connectivity and the installation window are available.");
   manual_install=false;
  }
  portENTER_CRITICAL(&lock);
  status.countdown_seconds=countdown&&countdown>mono?(unsigned)((countdown-mono+999999)/1000000):0;
  if(status.countdown_seconds)snprintf(status.message,sizeof(status.message),"Installing in %u seconds. Choose Postpone to cancel.",status.countdown_seconds);
  portEXIT_CRITICAL(&lock);
  vTaskDelay(pdMS_TO_TICKS(1000));
 }
}
esp_err_t waveform_update_start(void)
{
 if(commands)return ESP_OK;
 trial_validated=trial_accepted=false;
 saved=(saved_t){.schema=1,.hours=720,.automatic=1,.start=180,.end=240};
 nvs_handle_t h;esp_err_t err=nvs_open("waveform_ota",NVS_READONLY,&h);
 if(err==ESP_OK){saved_t value;size_t n=sizeof(value);err=nvs_get_blob(h,"v1",&value,&n);nvs_close(h);
  if(err==ESP_OK){if(n!=sizeof(value)||value.schema!=1||(value.hours!=1&&value.hours!=720)||value.automatic>1||value.start>=1440||value.end>=1440||value.start==value.end||!memchr(value.pending,0,32)||!memchr(value.blocked,0,32))return ESP_ERR_INVALID_ARG;saved=value;}
  else if(err!=ESP_ERR_NVS_NOT_FOUND)return err;
 }else if(err!=ESP_ERR_NVS_NOT_FOUND)return err;
 if(err==ESP_ERR_NVS_NOT_FOUND&&!persist())return ESP_FAIL; /* Create durable defaults on first use. */
 snprintf(status.installed,sizeof(status.installed),"%s",esp_app_get_description()->version);
 /* Installed identifies the running image, not acceptance: retain matching pending until VALID. */
 if(saved.pending[0]&&strcmp(saved.pending,status.installed)){
  const esp_partition_t *previous=esp_ota_get_next_update_partition(NULL);esp_app_desc_t image;esp_ota_img_states_t state;
  if(!previous||esp_ota_get_partition_description(previous,&image)!=ESP_OK)return ESP_FAIL;
  esp_err_t state_err=esp_ota_get_state_partition(previous,&state);
  if(state_err!=ESP_OK&&state_err!=ESP_ERR_NOT_FOUND)return state_err;
  bool failed=state_err==ESP_OK&&(state==ESP_OTA_IMG_INVALID||state==ESP_OTA_IMG_ABORTED)&&!strcmp(image.version,saved.pending);
  if(failed)snprintf(saved.blocked,sizeof(saved.blocked),"%s",saved.pending);
  message(failed?UPDATE_ERROR:UPDATE_IDLE,failed?"Trial firmware failed; version blocked.":"Activation was interrupted; update may be retried.");
  saved.pending[0]=0;if(!persist())return ESP_FAIL;
 }
 else message(UPDATE_IDLE,"Automatic update checks are ready.");
 publish();commands=xQueueCreate(4,sizeof(command_t));if(!commands)return ESP_ERR_NO_MEM;
 if(xTaskCreate(worker,"ota",16384,NULL,1,NULL)!=pdPASS){vQueueDelete(commands);commands=NULL;return ESP_ERR_NO_MEM;}return ESP_OK;
}

// Expose POSIX localtime_r before any libc header in strict Linux builds.
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <time.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
static time_t test_clock=1800000000;
static time_t fake_time(time_t *v){if(v)*v=test_clock;return test_clock;}
#define time fake_time
#include "../main/update_service.cpp"
__asm__(".globl _binary_ota_public_pem_start\n_binary_ota_public_pem_start:\n.asciz \"test public key\"\n.globl _binary_ota_public_pem_end\n_binary_ota_public_pem_end:\n");
static int64_t mono=1000000;
static waveform_update_network_t net{};
static esp_app_desc_t desc{};
static esp_partition_t part{};
static int open_error,read_error=ESP_ERR_NVS_NOT_FOUND,write_error,commit_error,crypto_error,ota_error,end_error,begin_error,boot_error,http_code=200;
static bool namespace_missing,queue_fail,task_fail,active_pending,trial_aborted,no_partition,truncated,read_failed,init_failed,connect_failed,has_redirect;
static const char *redirect_to="https://release-assets.githubusercontent.com/test";
static unsigned release_checks;
static unsigned accepted;static int accept_error;static bool cleanup_fails;
static unsigned writes,begins,activated,boot_calls,aborted,restarted,cleanup_count,loops,stop_loop,md_frees;
static int md_setup_error;
static unsigned verifying_messages;
static int64_t read_step_us;
static unsigned download_yields;
static saved_t persisted,staged;
static jmp_buf done;
struct test_queue {command_t item[8];unsigned n;} q;
static int64_t declared_size=-2;static int body_delta;
static const char *client_url;
static const char *response;static size_t offset,response_size;static char image[1025],manifest_text[1024];
static const char latest[]="{\"tag_name\":\"v0.9.1\",\"draft\":false,\"prerelease\":false}";
static const char *latest_body=latest;
void test_log(const char *tag,const char *fmt,...){(void)tag;(void)fmt;}
int64_t esp_timer_get_time(void){return mono;}
void waveform_update_network(waveform_update_network_t *v){*v=net;}
bool station_credential_configured(void){return true;}
bool station_credential_get(char *out,size_t n){snprintf(out,n,"host-test-only");return true;}
const esp_app_desc_t *esp_app_get_description(void){return &desc;}
uint32_t esp_random(void){return 0;}
int nvs_open(const char *name,int mode,nvs_handle_t *h){assert(!strcmp(name,"waveform_ota"));if(namespace_missing&&mode==NVS_READONLY)return ESP_ERR_NVS_NOT_FOUND;if(mode==NVS_READWRITE)namespace_missing=false;*h=1;return open_error;}
void nvs_close(nvs_handle_t h){(void)h;}
int nvs_get_blob(nvs_handle_t h,const char *key,void *out,size_t *n){(void)h;(void)key;if(read_error)return read_error;assert(*n==sizeof(persisted));memcpy(out,&persisted,*n);return 0;}
int nvs_set_blob(nvs_handle_t h,const char *key,const void *v,size_t n){(void)h;(void)key;assert(n==sizeof(persisted));if(write_error)return write_error;memcpy(&staged,v,n);return 0;}
int nvs_commit(nvs_handle_t h){(void)h;if(!commit_error)persisted=staged;return commit_error;}
QueueHandle_t xQueueCreate(unsigned count,size_t size){assert(count==4&&size==sizeof(command_t));return queue_fail?NULL:&q;}
int xQueueSend(QueueHandle_t handle,const void *v,unsigned ticks){(void)handle;(void)ticks;if(q.n==8)return 0;q.item[q.n++]=*(const command_t *)v;return pdTRUE;}
int xQueueReceive(QueueHandle_t handle,void *v,unsigned ticks){(void)handle;(void)ticks;if(!q.n)return 0;*(command_t *)v=q.item[0];memmove(q.item,q.item+1,--q.n*sizeof(command_t));return pdTRUE;}
void vQueueDelete(QueueHandle_t h){(void)h;}
int xTaskCreate(void (*fn)(void *),const char *name,unsigned stack,void *arg,unsigned priority,void *handle){(void)fn;(void)arg;(void)handle;assert(!strcmp(name,"ota")&&stack==16384&&priority==1);return task_fail?0:pdPASS;}
void vTaskDelay(unsigned ticks){if(ticks==1)download_yields++;mono+=(int64_t)ticks*1000;test_clock+=ticks/1000;if(stop_loop&&ticks==1000&&++loops>=stop_loop)longjmp(done,1);}
const esp_partition_t *esp_ota_get_running_partition(void){return &part;}
const esp_partition_t *esp_ota_get_next_update_partition(const void *p){(void)p;return no_partition?NULL:&part;}
int esp_ota_get_state_partition(const esp_partition_t *p,esp_ota_img_states_t *s){(void)p;*s=trial_aborted?ESP_OTA_IMG_ABORTED:active_pending?ESP_OTA_IMG_PENDING_VERIFY:ESP_OTA_IMG_VALID;return 0;}
int esp_ota_begin(const esp_partition_t *p,unsigned size,esp_ota_handle_t *h){(void)p;begins++;assert(size==OTA_WITH_SEQUENTIAL_WRITES);*h=1;return begin_error;}
int esp_ota_write(esp_ota_handle_t h,const void *data,size_t n){assert(h==1&&data&&n>0);writes++;return ota_error;}
int esp_ota_end(esp_ota_handle_t h){assert(h==1);return end_error?end_error:ota_error;}
int esp_ota_abort(esp_ota_handle_t h){assert(h==1);aborted++;return 0;}
int esp_ota_get_partition_description(const esp_partition_t *p,esp_app_desc_t *d){(void)p;*d=desc;strcpy(d->version,"0.9.1");return 0;}
int esp_ota_set_boot_partition(const esp_partition_t *p){(void)p;boot_calls++;if(!boot_error)activated++;return boot_error;}
int esp_ota_mark_app_valid_cancel_rollback(void){if(accept_error)return accept_error;accepted++;active_pending=false;if(cleanup_fails)commit_error=-1;return 0;}
void esp_restart(void){restarted++;}
void esp_crt_bundle_attach(void){}
void *esp_http_client_init(esp_http_client_config_t *c){assert(c->crt_bundle_attach&&c->disable_auto_redirect);offset=0;client_url=c->url;if(!strcmp(c->url,LATEST))release_checks++;
 if(has_redirect&&strstr(c->url,"github.com")){esp_http_client_event_t e{};e.event_id=HTTP_EVENT_ON_HEADER;e.header_key="Location";e.header_value=redirect_to;e.user_data=c->user_data;c->event_handler(&e);}
 if(!strstr(c->url,"release-assets.githubusercontent.com"))response=strstr(c->url,"/latest")?latest_body:strstr(c->url,"manifest.json")?manifest_text:strstr(c->url,"manifest.sig")?"signature":image;
 response_size=response==image?(size_t)(1024+body_delta):strlen(response);return init_failed?NULL:(void *)1;}
int esp_http_client_open(void *h,int n){(void)h;(void)n;return connect_failed?-1:0;}
int esp_http_client_set_header(void *h,const char *key,const char *value){(void)h;(void)value;assert(strcmp(key,"apikey"));return 0;}
int64_t esp_http_client_fetch_headers(void *h){(void)h;return declared_size==-2?(int64_t)response_size:declared_size;}
int esp_http_client_get_status_code(void *h){(void)h;if(has_redirect){has_redirect=false;return 302;}return http_code;}
void esp_http_client_cleanup(void *h){(void)h;cleanup_count++;}
int esp_http_client_read(void *h,char *out,int n){mono+=read_step_us;(void)h;assert(allowed_url(client_url,!strcmp(client_url,LATEST),0)||allowed_url(client_url,false,1));if(read_failed)return -1;size_t take=response_size-offset;if(take>(size_t)n)take=(size_t)n;memcpy(out,response+offset,take);offset+=take;return (int)take;}
bool esp_http_client_is_complete_data_received(void *h){(void)h;return !truncated&&offset==response_size;}
void mbedtls_md_init(mbedtls_md_context_t *c){(void)c;}void mbedtls_md_free(mbedtls_md_context_t *c){(void)c;md_frees++;if(status.state==UPDATE_VERIFYING)verifying_messages++;}
void *mbedtls_md_info_from_type(int t){assert(t==MBEDTLS_MD_SHA256);return (void *)1;}
int mbedtls_md_setup(mbedtls_md_context_t *c,const void *t,int n){(void)c;(void)t;(void)n;return md_setup_error?md_setup_error:crypto_error;}
int mbedtls_md_starts(mbedtls_md_context_t *c){(void)c;return 0;}
int mbedtls_md_update(mbedtls_md_context_t *c,const void *p,size_t n){(void)c;(void)p;(void)n;return 0;}
int mbedtls_md_finish(mbedtls_md_context_t *c,unsigned char *out){(void)c;memset(out,0,32);return 0;}
int mbedtls_md(const void *t,const unsigned char *p,size_t n,unsigned char *out){(void)t;(void)p;(void)n;memset(out,0,32);return crypto_error;}
void mbedtls_pk_init(mbedtls_pk_context *c){(void)c;}void mbedtls_pk_free(mbedtls_pk_context *c){(void)c;}
int mbedtls_pk_parse_public_key(mbedtls_pk_context *c,const unsigned char *p,size_t n){(void)c;assert(p&&n==sizeof("test public key")&&p[n-1]==0);return crypto_error;}
int mbedtls_pk_verify(mbedtls_pk_context *c,int t,const unsigned char *p,size_t n,const unsigned char *s,size_t sn){(void)c;(void)t;(void)p;(void)n;(void)s;(void)sn;return crypto_error;}
static void run(unsigned count){loops=0;stop_loop=count;if(!setjmp(done))worker(NULL);stop_loop=0;}
int main(void){
 net.connected=true;net.time_ready=true;strcpy(desc.version,"0.9.0");strcpy(desc.project_name,"waveform_p4_display");part.size=0x800000;
 snprintf(manifest_text,sizeof(manifest_text),"{\"schema\":1,\"version\":\"0.9.1\",\"target\":\"%s\",\"layout\":\"%s\",\"size\":1024,\"sha256\":\"%064d\",\"url\":\"%sv0.9.1/waveform-p4.bin\"}",WAVEFORM_UPDATE_TARGET,WAVEFORM_UPDATE_LAYOUT,0,WAVEFORM_UPDATE_BASE);
 assert(!waveform_update_request("check"));assert(!waveform_update_configure(720,true,180,240));
 open_error=-1;assert(waveform_update_start()!=0);open_error=0;read_error=-1;assert(waveform_update_start()!=0);read_error=ESP_ERR_NVS_NOT_FOUND;
 namespace_missing=true;active_pending=true;assert(waveform_update_start()==ESP_OK&&!namespace_missing&&persisted.schema==1);commands=NULL;active_pending=false;
 queue_fail=true;assert(waveform_update_start()!=0);queue_fail=false;task_fail=true;assert(waveform_update_start()!=0);task_fail=false;assert(waveform_update_start()==0);assert(waveform_update_start()==0);
 waveform_update_status_t s;waveform_update_status(&s);assert(s.interval_hours==720&&s.automatic);
 assert(!waveform_update_request(NULL));assert(!waveform_update_request("bad"));assert(!waveform_update_configure(2,true,180,240));assert(!waveform_update_configure(1,true,180,180));
 assert(waveform_update_request("check"));run(1);assert(status.state==UPDATE_AVAILABLE&&saved.next>test_clock);assert(candidate.size==1024);
 /* A successful scheduled check sets a future deadline. Manual checks bypass it. */
 saved.automatic=false;saved.next=0;
 unsigned checks=release_checks;run(32);assert(release_checks==checks+1&&saved.next>test_clock);
 checks=release_checks;run(1);assert(release_checks==checks);
 assert(waveform_update_request("check"));run(1);assert(release_checks==checks+1&&saved.next>test_clock);
 /* Settings queued after Check must not overwrite its immediate intent. */
 checks=release_checks;assert(waveform_update_request("check"));
 assert(waveform_update_configure(1,false,10,20));run(1);
 assert(release_checks==checks+1&&saved.hours==1&&!saved.automatic);
 /* Manual bypass must still honour the provider's rate limit. */
 checks=release_checks;rate_limited_until=test_clock+600;
 assert(waveform_update_request("check"));run(1);
 assert(release_checks==checks&&saved.next>=rate_limited_until);
 rate_limited_until=0;

 assert(waveform_update_request("postpone"));run(1);assert(saved.postponed>test_clock);
 net.connected=false;assert(waveform_update_request("check"));run(1);assert(status.state==UPDATE_ERROR);assert(waveform_update_request("install"));run(1);net.connected=true;
 http_code=404;assert(check_release());http_code=500;assert(!check_release());http_code=200;
 crypto_error=-1;assert(!check_release());crypto_error=0;
 /* Malformed or non-object release metadata fails cleanly, never dereferencing a failed parse. */
 static const char *const bad_latest[]={"{not json","null","[]","\"v0.9.1\"","{\"tag_name\":1,\"draft\":false,\"prerelease\":false}","{\"tag_name\":\"v0.9.1\",\"draft\":true,\"prerelease\":false}"};
 for(unsigned i=0;i<sizeof(bad_latest)/sizeof(bad_latest[0]);i++){latest_body=bad_latest[i];assert(!check_release()&&strstr(status.message,"Invalid release metadata"));}
 latest_body=latest;
 strcpy(saved.blocked,"0.9.1");assert(check_release());saved.blocked[0]=0;strcpy(status.installed,"0.9.1");assert(check_release());strcpy(status.installed,"0.9.0");
 truncated=true;assert(!check_release());truncated=false;read_failed=true;assert(!check_release());read_failed=false;
 init_failed=true;assert(!check_release());init_failed=false;connect_failed=true;assert(!check_release());connect_failed=false;assert(check_release());
 assert(!allowed_url("https://api.github.com.evil/test",true,0));assert(!allowed_url("https://github.com/other/repo",false,0));
 assert(allowed_url("https://objects.githubusercontent.com/asset",false,1));assert(!allowed_url("https://objects.githubusercontent.com/asset",false,0));
 /* Redirect hops: CDN only, never back to github.com, no metadata redirects, no ambiguous characters. */
 assert(!allowed_url(WAVEFORM_UPDATE_BASE "v0.9.1/waveform-p4.bin",false,1));assert(!allowed_url(LATEST,true,1));
 static const char *const bad_hops[]={"http://release-assets.githubusercontent.com/x","//release-assets.githubusercontent.com/x","https://release-assets.githubusercontent.com@evil.example/x",
   "https://release-assets.githubusercontent.com.evil.example/x","https://release-assets.githubusercontent.com/","https://release-assets.githubusercontent.com/a b","https://release-assets.githubusercontent.com/a\\b","https://evil.example/x"};
 for(unsigned i=0;i<sizeof(bad_hops)/sizeof(bad_hops[0]);i++)assert(!allowed_url(bad_hops[i],false,1));
 int http;assert(!open_url("http://github.com/evil",&http,NULL));has_redirect=true;assert(!open_url(LATEST,&http,NULL));has_redirect=true;void *h=open_url(WAVEFORM_UPDATE_BASE "v0.9.1/waveform-p4-manifest.json",&http,NULL);assert(h);esp_http_client_cleanup(h);
 redirect_to=WAVEFORM_UPDATE_BASE "v0.9.1/waveform-p4-manifest.json";has_redirect=true;assert(!open_url(WAVEFORM_UPDATE_BASE "v0.9.1/waveform-p4-manifest.json",&http,NULL));
 redirect_to="https://evil.example/waveform-p4-manifest.json";has_redirect=true;assert(!open_url(WAVEFORM_UPDATE_BASE "v0.9.1/waveform-p4-manifest.json",&http,NULL));
 redirect_to="https://release-assets.githubusercontent.com/test";
 char ver[32];strcpy(ver,candidate.version);candidate.version[0]=0;assert(!install());strcpy(candidate.version,ver);
 active_pending=true;assert(!install());active_pending=false;no_partition=true;assert(!install());no_partition=false;
 /* A busy background client prevents erasing or opening an OTA connection;
    a timeout always releases the pause so recognition can recover. */
 assert(waveform_network_acquire());
 {unsigned b=begins;assert(!install());assert(begins==b&&!waveform_network_paused());
  assert(strstr(status.message,"Background connection"));}
 waveform_network_release();
 /* Reject bad lengths before flash writes. */
 unsigned begin_count=begins,write_count=writes;
 const int64_t bad_lengths[]={1023,1025,0,-1};
 for(unsigned i=0;i<sizeof(bad_lengths)/sizeof(bad_lengths[0]);i++){
  declared_size=bad_lengths[i];assert(!install()&&begins==begin_count&&writes==write_count&&activated==0);
 }
 declared_size=-2;body_delta=1;assert(!install()&&begins==begin_count&&writes==write_count);body_delta=0;
 /* Reject dishonest/truncated bodies without activation. */
 declared_size=1024;
 for(int delta=-1;delta<=1;delta+=2){body_delta=delta;unsigned abort_count=aborted;assert(!install()&&aborted==abort_count+1&&activated==0);}
 body_delta=0;declared_size=-2;
 unsigned before=begins;http_code=500;assert(!install()&&begins==before);http_code=200;begin_error=-1;assert(!install());begin_error=0;
 ota_error=-1;assert(!install()&&aborted);ota_error=0;
 /* Every exit after the size check releases the HTTP client, hash and OTA handle exactly once. */
 {unsigned c=cleanup_count,f=md_frees,a=aborted;md_setup_error=-1;
  assert(!install()&&cleanup_count==c+1&&md_frees==f+1&&aborted==a+1&&activated==0);md_setup_error=0;
  c=cleanup_count;f=md_frees;a=aborted;begin_error=-1;
  assert(!install()&&cleanup_count==c+1&&md_frees==f+1&&aborted==a&&activated==0);begin_error=0;
  c=cleanup_count;f=md_frees;a=aborted;ota_error=-1;
  assert(!install()&&cleanup_count==c+1&&md_frees==f+1&&aborted==a+1&&activated==0);ota_error=0;}
 /* esp_ota_end() validation failure: boot selection is never touched; the running image stays selected. */
 {unsigned calls=boot_calls,abort_count=aborted;end_error=0x1503 /* ESP_ERR_OTA_VALIDATE_FAILED */;
  assert(!install()&&boot_calls==calls&&activated==0&&aborted==abort_count&&!persisted.pending[0]);end_error=0;}
{unsigned v=verifying_messages;read_failed=true;assert(!install());read_failed=false;assert(verifying_messages==v);}
truncated=true;assert(!install());truncated=false;
 candidate.sha256[0]='1';assert(!install());candidate.sha256[0]='0';commit_error=-1;assert(!install());commit_error=0;boot_error=-1;assert(!install()&&!persisted.pending[0]);boot_error=0;
 {unsigned v=verifying_messages;read_step_us=301000000;
  assert(!install()&&!waveform_network_paused());
  assert(strstr(status.message,"Download timed out")&&verifying_messages==v);read_step_us=0;}
 assert(activated==0);{unsigned yields=download_yields;assert(install()&&activated==1&&restarted==1);assert(download_yields>yields&&!waveform_network_paused());}
 /* Manual installation starts in this worker turn, outside the automatic window. */
 assert(waveform_update_request("install"));run(1);assert(restarted==2&&status.countdown_seconds==0);
 /* Automatic installs retain the cancellation countdown; Install now skips the remainder. */
 saved.automatic=true;saved.start=0;saved.end=1439;saved.postponed=0;
 run(1);assert(status.countdown_seconds==60&&restarted==2);
 assert(waveform_update_request("install"));run(1);assert(restarted==3&&status.countdown_seconds==0);
 saved.automatic=false;
 /* A failed install must report failure and keep the running version unchanged. */
 read_failed=true;assert(waveform_update_request("install"));run(1);
 assert(status.state==UPDATE_ERROR&&restarted==3&&!strcmp(status.installed,"0.9.0"));read_failed=false;
 assert(strstr(status.last_install_message,"Download interrupted"));
 assert(check_release());assert(strstr(status.last_install_message,"Download interrupted"));
 /* Failed trial survives reboot. */
 commands=NULL;read_error=0;strcpy(desc.version,"0.9.1");active_pending=true;assert(waveform_update_start()==0);run(2);assert(!strcmp(persisted.pending,"0.9.1"));
 commands=NULL;strcpy(desc.version,"0.9.0");active_pending=false;trial_aborted=true;assert(waveform_update_start()==0);trial_aborted=false;assert(!strcmp(persisted.blocked,"0.9.1")&&!persisted.pending[0]);assert(check_release()&&!candidate.version[0]);
 /* Acceptance requires durable state. */
 active_pending=true;strcpy(status.installed,"0.9.1");read_error=0;
 commit_error=-1;assert(!waveform_update_validate_trial());run(1);assert(!accepted&&active_pending);commit_error=0;
 read_error=-1;assert(!waveform_update_validate_trial());run(1);assert(!accepted&&active_pending);read_error=0;
 accept_error=-1;assert(!waveform_update_validate_trial());run(1);assert(!accepted&&active_pending);accept_error=0;
 cleanup_fails=true;assert(!waveform_update_validate_trial());run(1);assert(!trial_validated&&accepted==1&&!active_pending&&persisted.pending[0]);
 cleanup_fails=false;commit_error=0;assert(!waveform_update_validate_trial());run(1);assert(waveform_update_validate_trial()&&accepted==1&&!persisted.pending[0]);assert(strstr(status.last_install_message,"Version 0.9.1 installed"));
 /* Retry marker cleanup. */
 commands=NULL;strcpy(persisted.pending,"0.9.1");strcpy(desc.version,"0.9.1");active_pending=true;assert(waveform_update_start()==0);run(1);active_pending=false;commit_error=-1;run(1);assert(persisted.pending[0]);commit_error=0;run(1);assert(!persisted.pending[0]);strcpy(desc.version,"0.9.0");
 commands=NULL;strcpy(persisted.pending,"0.9.1");persisted.blocked[0]=0;assert(waveform_update_start()==0);assert(!persisted.blocked[0]&&!persisted.pending[0]);
 commands=NULL;persisted.schema=2;assert(waveform_update_start()!=0);persisted.schema=1;write_error=-1;strcpy(persisted.pending,"0.9.1");assert(waveform_update_start()!=0);write_error=0;
 puts("PASS: actual OTA worker HTTP limits, signature rejection, scheduling, persistent rollback block and inactive-slot activation only after validation");
}

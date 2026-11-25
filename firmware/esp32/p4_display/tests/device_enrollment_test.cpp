#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
static time_t current_time=1800000000;
static time_t fake_time(time_t *out){if(out)*out=current_time;return current_time;}
#include "cJSON.h"
/* Record the exact parse length: it must cover only received bytes, never the NUL terminator. */
static size_t parse_length,parse_calls;
static cJSON *checked_parse(const char *value,size_t length){parse_calls++;parse_length=length;assert(length==strlen(value));return cJSON_ParseWithLength(value,length);}
#define time fake_time
#define cJSON_ParseWithLength checked_parse
#include "../main/device_enrollment.cpp"
#undef cJSON_ParseWithLength
#undef time
const char *test_enrollment_url="https://tempo.example.com/v1/device";
static bool paused;
static unsigned leases;
bool waveform_network_acquire(){if(paused)return false;++leases;return true;}
void waveform_network_release(){assert(leases);--leases;}
bool waveform_network_paused(){return paused;}
static int64_t timer=1000000;
static bool stall_read, pause_read;
int64_t esp_timer_get_time(){return timer;}
static unsigned init_calls;
static bool install_notification;
static unsigned address=0xc0a80122,requests,cert_requests,installs;
static int init_error,sign_error,csr_error,install_error,http_error,http_status=200;
static bool fail_client,complete=true,iface_present=true,pending,have_certificate;
static const char *content_type="application/json";static esp_http_client_config_t *client_config;
static int64_t renew_time;
static char response[13000],request_body[7000];static size_t response_offset;
static void (*task)(void *);static int task_result=pdPASS;
static jmp_buf loop_exit;static unsigned delays;
static bool force_tick(){schedule.next=0;schedule.retry=0;return tick();}
esp_netif_t *esp_netif_get_handle_from_ifkey(const char *key){assert(!strcmp(key,"WIFI_STA_DEF"));return iface_present?(void*)1:NULL;}
int esp_netif_get_ip_info(esp_netif_t *iface,esp_netif_ip_info_t *out){assert(iface);out->ip.addr=address;return 0;}
void esp_fill_random(void *out,size_t length){memset(out,0x12,length);}
uint32_t esp_random(void){return 4;}
int waveform_identity_init(void){init_calls++;return init_error;}
bool waveform_identity_changed(void){assert(!"enrollment must not consume the phone reload notification");return false;}
const char *waveform_identity_id(void){return "abababababababababababababababab";}
int64_t waveform_identity_renew_after(void){return renew_time;}
int waveform_identity_csr(char *out,size_t size){assert(size>=4);strcpy(out,"csr");return csr_error;}
int waveform_identity_sign(const char *body,char *out,size_t size){
 cJSON *j=cJSON_Parse(body);assert(j&&size>=10);assert(!strcmp(cJSON_GetObjectItem(j,"product")->valuestring,"waveform"));assert(strlen(cJSON_GetObjectItem(j,"nonce")->valuestring)==64);
 assert(cJSON_GetObjectItem(j,"issued_at")->valuedouble==current_time);cJSON_Delete(j);strcpy(out,"signature");return sign_error;
}
int waveform_identity_install(const char *chain,int64_t *out){assert(!strcmp(chain,"chain"));installs++;if(install_error)return install_error;*out=renew_time=current_time+10000;install_notification=true;return 0;}
void esp_crt_bundle_attach(void){}
void *esp_http_client_init(esp_http_client_config_t *config){
 assert(!strcmp(config->url,test_enrollment_url)&&config->crt_bundle_attach==esp_crt_bundle_attach&&config->disable_auto_redirect);
 client_config=config;requests++;response_offset=0;request_body[0]=0;return fail_client?NULL:(void*)1;
}
int esp_http_client_set_method(void *h,int method){assert(h&&method==HTTP_METHOD_POST);return 0;}
int esp_http_client_set_header(void *h,const char *name,const char *value){assert(h&&name&&value);return 0;}
int esp_http_client_open(void *h,int length){assert(h&&length>0);return http_error;}
int esp_http_client_write(void *h,const char *data,int length){assert(h);if(http_error)return -1;memcpy(request_body,data,length);request_body[length]=0;return length;}
int64_t esp_http_client_fetch_headers(void *h){
 assert(h);cJSON *j=cJSON_Parse(request_body);assert(j);const char *action=cJSON_GetObjectItem(j,"action")->valuestring;
 if(!strcmp(action,"certificate"))cert_requests++;
 snprintf(response,sizeof(response),"{\"state\":\"%s\",\"address\":\"192.168.1.34\",\"certificate\":\"%s\"}",pending?"pending":"ready",have_certificate?"chain":"");
 if(content_type){esp_http_client_event_t header={};header.event_id=HTTP_EVENT_ON_HEADER;header.user_data=client_config->user_data;header.header_key="content-type";header.header_value=content_type;assert(client_config->event_handler(&header)==0);}
 cJSON_Delete(j);return http_error?-1:(int64_t)strlen(response);
}
int esp_http_client_get_status_code(void *h){assert(h);return http_status;}
int esp_http_client_read(void *h,char *out,int capacity){assert(h);if(stall_read)timer+=21000000;if(pause_read)paused=true;if(http_error)return -1;size_t length=strlen(response)-response_offset;if(length>(size_t)capacity)length=(size_t)capacity;memcpy(out,response+response_offset,length);response_offset+=length;return (int)length;}
bool esp_http_client_is_complete_data_received(void *h){assert(h);return complete;}
void esp_http_client_close(void *h){assert(h);}
void esp_http_client_cleanup(void *h){assert(h);}
int xTaskCreate(void (*fn)(void *),const char *name,unsigned stack,void *arg,unsigned priority,void *handle){assert(!strcmp(name,"enrollment")&&stack==12288&&!arg&&priority==3&&!handle);task=fn;return task_result;}
void vTaskDelay(unsigned ticks){assert(ticks>=5000);if(++delays>=2)longjmp(loop_exit,1);}
int main(void){
 test_enrollment_url="";
 init_error=-1;assert(waveform_enrollment_start()==init_error&&init_calls==1&&!task&&!requests);init_error=0;
 assert(waveform_enrollment_start()==0&&init_calls==2&&!task&&!started&&!requests);
 assert(waveform_enrollment_start()==0&&init_calls==3&&!task&&!started&&!requests);

 test_enrollment_url="http://tempo.example.com/v1/device";assert(waveform_enrollment_start()!=0);
 test_enrollment_url="https://tempo.example.com/wrong";assert(waveform_enrollment_start()!=0);
 test_enrollment_url="https://tempo.example.com/v1/device?bad";assert(waveform_enrollment_start()!=0);
 /* Query, fragment and userinfo are rejected anywhere, including before the path; host and path are required. */
 static const char *const rejected[]={"https://tempo.example.com?x/v1/device","https://tempo.example.com#x/v1/device",
  "https://tempo.example.com/v1/device#x","https://user@tempo.example.com/v1/device","https://user:pw@tempo.example.com/v1/device",
  "https:///v1/device","https://tempo.example.com","https://tempo.example.com/","https://tempo.example.com/v1/device/",
  "https://tempo.example.com/x/v1/device","HTTPS://tempo.example.com/v1/device"};
 for(size_t i=0;i<sizeof(rejected)/sizeof(*rejected);i++){test_enrollment_url=rejected[i];assert(waveform_enrollment_start()!=0&&!task&&!started);}
 assert(!requests);
 static const char *const malformed[]={"https://tempo.example.com/v1/device?","https://:443/v1/device","https://tempo example.com/v1/device",
  "https://tempo.example.com%40evil.example/v1/device","https://[::1]/v1/device","https://tempo.example.com\\@evil/v1/device"};
 for(size_t i=0;i<sizeof(malformed)/sizeof(*malformed);i++)assert(!enrollment::valid_url(malformed[i]));
 assert(enrollment::valid_url("https://tempo.example.com/v1/device")&&!enrollment::valid_url("https://tempo.example.com:443/v1/device"));
 test_enrollment_url="https://tempo.example.com/v1/device";
 init_error=-1;assert(waveform_enrollment_start()!=0);init_error=0;
 task_result=0;assert(waveform_enrollment_start()!=0);task_result=pdPASS;
 assert(waveform_enrollment_start()==0&&task);assert(waveform_enrollment_start()==0);
 paused=true;assert(force_tick()&&!requests);paused=false;
 current_time=1;assert(force_tick()&&!requests);current_time=1800000000;
 iface_present=false;assert(force_tick()&&!requests);iface_present=true;
 address=0;assert(force_tick()&&!requests);address=0xc0a80122;
 /* Slow or OTA-interrupted responses release their network lease without parsing. */
 stall_read=true;parse_calls=0;assert(!force_tick()&&!leases&&!parse_calls);stall_read=false;
 pause_read=true;parse_calls=0;assert(!force_tick()&&!leases&&!parse_calls);pause_read=false;paused=false;
 sign_error=-1;assert(!force_tick());sign_error=0;
 fail_client=true;assert(!force_tick());fail_client=false;
 http_error=-1;assert(!force_tick());http_error=0;
 http_status=302;assert(!force_tick());http_status=401;assert(!force_tick());http_status=200;
 complete=false;assert(!force_tick());complete=true;
 /* Only declared JSON replies are protocol data; HTML, lookalike and missing types fail with backoff. */
 static const char *const wrong[]={"text/html","application/jsonp","application/json-seq","text/json",""," "};
 for(size_t i=0;i<sizeof(wrong)/sizeof(*wrong);i++){content_type=wrong[i];parse_calls=0;assert(!force_tick()&&!parse_calls&&!waiting_certificate);}
 content_type=NULL;parse_calls=0;assert(!force_tick()&&!parse_calls&&!waiting_certificate);
 for(int status=200;status<=202;status+=2){http_status=status;content_type="text/html; charset=utf-8";assert(!force_tick()&&!waiting_certificate);}
 http_status=200;content_type="Application/JSON;charset=utf-8";
 parse_calls=0;assert(force_tick()&&waiting_certificate&&cert_requests==1&&!strcmp(last_address,"192.168.1.34"));
 assert(parse_calls>0&&parse_length==strlen(response)); /* Valid replies parse exactly the received JSON. */
 pending=true;assert(force_tick()&&installs==0&&cert_requests==1);pending=false;have_certificate=true;
 /* A 202 status body is non-final: it must not install, change DNS state or request a CSR. */
 last_address[0]=0;http_status=202;assert(!force_tick()&&waiting_certificate&&installs==0&&cert_requests==1&&!last_address[0]);http_status=200;
 /* Renewal is due (renew_time=0) while the offered chain fails to install: retry that chain, never submit another CSR. */
 install_error=-1;assert(!force_tick()&&!install_notification&&waiting_certificate&&installs==1&&cert_requests==1);
 assert(!force_tick()&&!install_notification&&waiting_certificate&&installs==2&&cert_requests==1);install_error=0;
 assert(force_tick()&&!waiting_certificate&&installs==3&&cert_requests==1&&install_notification);
 assert(force_tick()&&install_notification); /* Further polls leave notification for phone maintenance. */
 renew_time=current_time-1;csr_error=-1;assert(!force_tick());csr_error=0;
 assert(force_tick()&&cert_requests==2);
 if(!setjmp(loop_exit))task(NULL);
 assert(delays==2);delays=0;paused=true;
 if(!setjmp(loop_exit))task(NULL);
 assert(leases==0);
 puts("PASS: signed HTTPS enrollment, clock/network gating, renewal, redirects denied and retry backoff");
}

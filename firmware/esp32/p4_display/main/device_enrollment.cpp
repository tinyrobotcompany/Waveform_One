#include "device_enrollment.h"
#include "device_identity.h"
#include "network_activity.h"
#include "enrollment_policy.h"
#include <atomic>
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_netif.h"
#include "esp_random.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cJSON.h"
#include "sdkconfig.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <time.h>

static bool started;
static enrollment::Schedule schedule;
static unsigned server_retry;
static bool pending_work;
static char last_address[16];
static bool waiting_certificate;

/* Protocol replies must declare JSON: application/json with optional parameters, any case. */
static esp_err_t response_header(esp_http_client_event_t *event)
{
    if(event->event_id==HTTP_EVENT_ON_HEADER&&event->header_key&&event->header_value&&
       !strcasecmp(event->header_key,"Retry-After")){
        char *end=nullptr;unsigned long seconds=strtoul(event->header_value,&end,10);
        if(end!=event->header_value&&!*end)server_retry=seconds>86400?86400:static_cast<unsigned>(seconds);
    }
    if(event->event_id==HTTP_EVENT_ON_HEADER&&event->header_key&&event->header_value&&!strcasecmp(event->header_key,"Content-Type")){
        const char *type=event->header_value;while(*type==' '||*type=='\t')type++;
        size_t length=strlen("application/json");
        *static_cast<bool *>(event->user_data)=!strncasecmp(type,"application/json",length)&&(!type[length]||type[length]==';'||type[length]==' '||type[length]=='\t');
    }
    return ESP_OK;
}
/* Returns the parsed 200/202 reply; *http_status reports which, so callers can require a final 200. */
static cJSON *request(const char *action,const char *value,int *http_status)
{
    *http_status=0;
    const int64_t deadline=esp_timer_get_time()+20000000;
    if(!waveform_network_acquire())return nullptr;
    struct Lease { ~Lease(){waveform_network_release();} } lease;
    cJSON *json=cJSON_CreateObject();if(!json)return NULL;
    unsigned char random[32];char nonce[65];esp_fill_random(random,sizeof(random));
    for(unsigned i=0;i<32;i++)snprintf(nonce+2*i,3,"%02x",random[i]);
    cJSON_AddStringToObject(json,"product","waveform");
    cJSON_AddStringToObject(json,"device_id",waveform_identity_id());
    cJSON_AddStringToObject(json,"action",action);cJSON_AddStringToObject(json,"nonce",nonce);
    cJSON_AddNumberToObject(json,"issued_at",(double)time(NULL));
    if(value)cJSON_AddStringToObject(json,!strcmp(action,"certificate")?"csr":"address",value);
    char *body=cJSON_PrintUnformatted(json);cJSON_Delete(json);if(!body)return NULL;
    char signature[112];
    if(waveform_identity_sign(body,signature,sizeof(signature))!=ESP_OK){cJSON_free(body);return NULL;}
    bool json_reply=false;
    esp_http_client_config_t config={};
    config.url=CONFIG_WAVEFORM_ENROLLMENT_URL;config.timeout_ms=5000;
    config.event_handler=response_header;config.user_data=&json_reply;
    config.crt_bundle_attach=esp_crt_bundle_attach;config.disable_auto_redirect=true;
    config.buffer_size=2048;config.buffer_size_tx=2048;
    esp_http_client_handle_t client=esp_http_client_init(&config);
    if(!client){cJSON_free(body);return NULL;}
    esp_http_client_set_method(client,HTTP_METHOD_POST);
    esp_http_client_set_header(client,"Content-Type","application/json");
    esp_http_client_set_header(client,"X-Device-Signature",signature);
    size_t length=strlen(body);esp_err_t err=esp_http_client_open(client,(int)length);
    if(err==ESP_OK){
        size_t sent=0;
        while(sent<length&&!waveform_network_paused()&&esp_timer_get_time()<deadline){int n=esp_http_client_write(client,body+sent,(int)(length-sent));if(n<=0){err=ESP_FAIL;break;}sent+=(size_t)n;}
        if(sent!=length)err=ESP_FAIL;
    }
    cJSON_free(body);
    cJSON *reply=NULL;
    if(err==ESP_OK&&esp_http_client_fetch_headers(client)>=0){
        int status=esp_http_client_get_status_code(client);*http_status=status;
        if(status==200||status==202){
            char *response=static_cast<char *>(calloc(1,12289));size_t total=0;bool valid=response!=NULL;
            while(valid&&total<12288&&!waveform_network_paused()&&esp_timer_get_time()<deadline){
                int n=esp_http_client_read(client,response+total,12288-total);
                if(n<0){valid=false;break;}if(n==0)break;total+=(size_t)n;
            }
            if(valid&&esp_timer_get_time()<deadline&&!waveform_network_paused()&&json_reply&&total<12288&&esp_http_client_is_complete_data_received(client))reply=cJSON_ParseWithLength(response,total);
            free(response);
        }
    }
    esp_http_client_close(client);esp_http_client_cleanup(client);return reply;
}

static bool tick(void)
{
    // Waiting for connectivity/time is not a failed cloud request.
    if(time(NULL)<1700000000||waveform_network_paused())return true;
    esp_netif_t *iface=esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");esp_netif_ip_info_t info;
    if(!iface||esp_netif_get_ip_info(iface,&info)!=ESP_OK||!info.ip.addr)return true;
    char address[16];snprintf(address,sizeof(address),IPSTR,IP2STR(&info.ip));
    if(!schedule.due(time(NULL),address,waveform_identity_renew_after()))return true;
    if(strcmp(last_address,address)){
        int http_status;cJSON *reply=request("address",address,&http_status);if(!reply)return false;
        /* Mark only when status confirms DNS applied; POST acceptance isn't completion. */
        cJSON_Delete(reply);
    }
    /* Status is authoritative only as a final 200; the service never answers status with 202. */
    int http_status;cJSON *status=request("status",NULL,&http_status);if(!status)return false;
    if(http_status!=200){cJSON_Delete(status);return false;}
    const cJSON *dns=cJSON_GetObjectItemCaseSensitive(status,"address");
    if(cJSON_IsString(dns)&&!strcmp(dns->valuestring,address))snprintf(last_address,sizeof(last_address),"%s",address);
    const cJSON *chain=cJSON_GetObjectItemCaseSensitive(status,"certificate");
    const cJSON *state=cJSON_GetObjectItemCaseSensitive(status,"state");
    bool pending=cJSON_IsString(state)&&(!strcmp(state->valuestring,"pending")||!strcmp(state->valuestring,"running"));
    if(waiting_certificate&&!pending&&cJSON_IsString(chain)&&chain->valuestring[0]){
        int64_t renewal;
        /* An issued chain that fails local install is retried with worker backoff.
           Return before the renewal block: never request another CSR while one is on offer. */
        if(waveform_identity_install(chain->valuestring,&renewal)!=ESP_OK){cJSON_Delete(status);return false;}
        waiting_certificate=false;
    }
    pending_work=pending||strcmp(last_address,address);
    cJSON_Delete(status);
    if(time(NULL)>=waveform_identity_renew_after()&&!pending){
        char *csr=static_cast<char *>(calloc(1,2048));if(!csr)return false;
        esp_err_t err=waveform_identity_csr(csr,2048);
        cJSON *reply=err==ESP_OK?request("certificate",csr,&http_status):NULL;free(csr);
        if(!reply)return false;
        cJSON_Delete(reply);waiting_certificate=true;pending_work=true;
    }
    schedule.success(time(NULL),address,pending_work,waveform_identity_renew_after(),esp_random()%901);
    return true;
}
static void worker(void *unused)
{
    (void)unused;
    for(;;){
        if(!waveform_network_paused()){
            server_retry=0;
            if(!tick())schedule.failure(time(NULL),server_retry);
        }
        // Observe DHCP changes promptly, without creating a cloud request on each tick.
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
esp_err_t waveform_enrollment_start(void)
{
    if(started)return ESP_OK;
    /* Manufacturing identity must exist even before the service is configured. */
    esp_err_t err=waveform_identity_init();if(err!=ESP_OK)return err;
    const char *url=CONFIG_WAVEFORM_ENROLLMENT_URL;
    if(!url[0])return ESP_OK; /* Local identity ready; no network worker. */
    if(!enrollment::valid_url(url))return ESP_ERR_INVALID_ARG;
    if(xTaskCreate(worker,"enrollment",12288,NULL,3,NULL)!=pdPASS)return ESP_ERR_NO_MEM;
    started=true;return ESP_OK;
}

#include "phone_tls.h"
#include "sdkconfig.h"
#if CONFIG_WAVEFORM_MANAGED_IDENTITY
#include "identity_store.h"
#include "esp_key_config.h"
extern const char device_roots_start[] __asm__("_binary_device_roots_pem_start");
#endif
#include "nvs.h"
#include "nvs_flash.h"
#ifdef CONFIG_NVS_ENCRYPTION
#include "esp_partition.h"
#endif
#include "mbedtls/pk.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/version.h"
#include "esp_random.h"
#include <stdlib.h>
#include <string.h>

/* Separate from settings and OTA metadata; never erase on an initialization error. */
static char hostname[97];
static httpd_handle_t active_server;
static char *certificate, *private_key, *trust_bundle;
static struct { mbedtls_x509_time from, to; } validity[16];
static size_t validity_count;
bool waveform_phone_tls_time_valid(void)
{
    if(!validity_count)return false;
    for(size_t i=0;i<validity_count;++i)
        if(mbedtls_x509_time_is_future(&validity[i].from)||mbedtls_x509_time_is_past(&validity[i].to))return false;
    return true;
}
static bool remember_validity(const mbedtls_x509_crt *chain)
{
    for(const mbedtls_x509_crt *c=chain;c;c=c->next){
        if(validity_count==sizeof(validity)/sizeof(validity[0]))return false;
        validity[validity_count].from=c->valid_from;validity[validity_count++].to=c->valid_to;
    }
    return true;
}
static void clear_identity(void)
{
    validity_count=0;
    if(private_key){volatile char *p=private_key;size_t n=strlen(private_key);while(n--)*p++=0;}
    free(private_key);free(certificate);free(trust_bundle);private_key=certificate=trust_bundle=NULL;hostname[0]=0;
}
static bool valid_hostname(const char *name)
{
    size_t n=strlen(name),label=0;bool dot=false;
    if(!n||n>96)return false;
    for(size_t i=0;i<n;i++){
        char c=name[i];
        if(c=='.'){if(!label||name[i-1]=='-')return false;label=0;dot=true;continue;}
        if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-')||(!label&&c=='-')||++label>63)return false;
    }
    return dot&&label&&name[n-1]!='-';
}
static esp_err_t read_pem(nvs_handle_t h,const char *name,size_t maximum,char **out)
{
    size_t n=0;esp_err_t err=nvs_get_str(h,name,NULL,&n);
    if(err!=ESP_OK)return err;
    if(n<2||n>maximum)return ESP_ERR_INVALID_ARG;
    *out=static_cast<char *>(calloc(1,n));if(!*out)return ESP_ERR_NO_MEM;
    err=nvs_get_str(h,name,*out,&n);
    return err;
}
#if MBEDTLS_VERSION_MAJOR < 4
static int random_bytes(void *ctx,unsigned char *out,size_t n){(void)ctx;esp_fill_random(out,n);return 0;}
#endif
static bool matches_identity(void)
{
    mbedtls_x509_crt cert, trust;mbedtls_pk_context key;
    mbedtls_x509_crt_init(&cert);mbedtls_x509_crt_init(&trust);mbedtls_pk_init(&key);
    int result=mbedtls_x509_crt_parse(&cert,(const unsigned char *)certificate,strlen(certificate)+1);
    if(result==0)result=mbedtls_x509_crt_parse(&trust,(const unsigned char *)trust_bundle,strlen(trust_bundle)+1);
    /* A device leaf must be CA-issued, never a self-signed trust anchor. */
    if(result==0 && cert.issuer_raw.len==cert.subject_raw.len &&
       !memcmp(cert.issuer_raw.p,cert.subject_raw.p,cert.issuer_raw.len))result=MBEDTLS_ERR_X509_CERT_VERIFY_FAILED;
#if MBEDTLS_VERSION_MAJOR >= 4
    if(result==0 && mbedtls_x509_crt_get_ca_istrue(&cert))result=MBEDTLS_ERR_X509_CERT_VERIFY_FAILED;
#endif
    if(result==0){
#if CONFIG_WAVEFORM_MANAGED_IDENTITY
        result=mbedtls_pk_wrap_psa(&key,WAVEFORM_TLS_KEY_ID);
        if(result==0)result=mbedtls_pk_check_pair(&cert.pk,&key);
#elif MBEDTLS_VERSION_MAJOR >= 4
        result=mbedtls_pk_parse_key(&key,(const unsigned char *)private_key,strlen(private_key)+1,NULL,0);
        if(result==0)result=mbedtls_pk_check_pair(&cert.pk,&key);
#else
        result=mbedtls_pk_parse_key(&key,(const unsigned char *)private_key,strlen(private_key)+1,NULL,0,random_bytes,NULL);
        if(result==0)result=mbedtls_pk_check_pair(&cert.pk,&key,random_bytes,NULL);
#endif
    }
    if(result==0){
        uint32_t flags=0;
        /* Never waive validity dates. Startup is retried once network time is usable. */
        result=mbedtls_x509_crt_verify(&cert,&trust,NULL,hostname,&flags,NULL,NULL);
        if(result==0 && flags)result=MBEDTLS_ERR_X509_CERT_VERIFY_FAILED;
        if(result==0 && (!remember_validity(&cert)||!remember_validity(&trust)||!waveform_phone_tls_time_valid()))
            result=MBEDTLS_ERR_X509_CERT_VERIFY_FAILED;
    }
    mbedtls_pk_free(&key);mbedtls_x509_crt_free(&cert);mbedtls_x509_crt_free(&trust);return result==0;
}
esp_err_t waveform_phone_tls_start(httpd_handle_t *server,const httpd_config_t *config)
{
    if(!server||!config)return ESP_ERR_INVALID_ARG;
    if(active_server)return ESP_ERR_INVALID_STATE;
    *server=NULL;
    clear_identity();
    esp_err_t err;
#if CONFIG_WAVEFORM_MANAGED_IDENTITY
    err=waveform_identity_store_init();
#elif defined(CONFIG_NVS_ENCRYPTION)
    nvs_sec_cfg_t encryption;
    const esp_partition_t *keys=esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS_KEYS, "nvs_keys");
    if(!keys)return ESP_ERR_INVALID_STATE;
    err=nvs_flash_read_security_cfg(keys,&encryption);
    if(err==ESP_OK)err=nvs_flash_secure_init_partition("phone_tls",&encryption);
    volatile unsigned char *wipe=(volatile unsigned char *)&encryption;
    for(size_t i=0;i<sizeof(encryption);++i)wipe[i]=0;
#else
    err=nvs_flash_init_partition("phone_tls");
#endif
    if(err!=ESP_OK)return err;
    nvs_handle_t h;err=nvs_open_from_partition("phone_tls","identity",NVS_READONLY,&h);if(err!=ESP_OK)return err;
    size_t n=sizeof(hostname);err=nvs_get_str(h,"hostname",hostname,&n);
    if(err==ESP_OK&&!valid_hostname(hostname))err=ESP_ERR_INVALID_ARG;
    if(err==ESP_OK)err=read_pem(h,"cert",8192,&certificate);
#if CONFIG_WAVEFORM_MANAGED_IDENTITY
    if(err==ESP_OK){trust_bundle=strdup(device_roots_start);if(!trust_bundle)err=ESP_ERR_NO_MEM;}
#else
    if(err==ESP_OK)err=read_pem(h,"key",4096,&private_key);
    if(err==ESP_OK)err=read_pem(h,"trust",8192,&trust_bundle);
#endif
    nvs_close(h);
    if(err==ESP_OK&&!matches_identity())err=ESP_ERR_INVALID_ARG;
    free(trust_bundle);trust_bundle=NULL; // verification only; do not retain an extra CA buffer while serving
    if(err!=ESP_OK){clear_identity();return err;}
    httpd_ssl_config_t ssl=HTTPD_SSL_CONFIG_DEFAULT();ssl.httpd=*config;
    ssl.transport_mode=HTTPD_SSL_TRANSPORT_SECURE;ssl.port_secure=443;
    ssl.servercert=(const unsigned char *)certificate;ssl.servercert_len=strlen(certificate)+1;
#if CONFIG_WAVEFORM_MANAGED_IDENTITY
    static esp_key_config_t tls_key={};
    tls_key.source=ESP_KEY_SOURCE_PSA;tls_key.psa.key_id=WAVEFORM_TLS_KEY_ID;
    ssl.server_key=&tls_key;
#else
    ssl.prvtkey_pem=(const unsigned char *)private_key;ssl.prvtkey_len=strlen(private_key)+1;
#endif
    err=httpd_ssl_start(server,&ssl);
    if(err==ESP_OK&&!*server)err=ESP_FAIL;
    if(err!=ESP_OK){*server=NULL;clear_identity();}
    else active_server=*server;
    return err;
}
esp_err_t waveform_phone_tls_stop(httpd_handle_t server)
{
    if(server!=active_server)return ESP_ERR_INVALID_STATE;
    if(server){
        /* ESP-IDF v6.1 copies both PEM buffers in https_server.c. Its httpd_ssl_stop
         * delegates to httpd_stop, which waits for THREAD_STOPPED before releasing
         * server contexts. Retain our buffers AND handle if shutdown fails. */
        const esp_err_t err=httpd_ssl_stop(server);
        if(err!=ESP_OK)return err;
    }
    active_server=NULL;
    clear_identity();
    return ESP_OK;
}
const char *waveform_phone_tls_hostname(void){return hostname;}

#include "phone_tls.h"
#include "sdkconfig.h"
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
static char *certificate, *private_key;
static void clear_identity(void)
{
    if(private_key){volatile char *p=private_key;size_t n=strlen(private_key);while(n--)*p++=0;}
    free(private_key);free(certificate);private_key=certificate=NULL;hostname[0]=0;
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
    *out=calloc(1,n);if(!*out)return ESP_ERR_NO_MEM;
    err=nvs_get_str(h,name,*out,&n);
    return err;
}
#if MBEDTLS_VERSION_MAJOR < 4
static int random_bytes(void *ctx,unsigned char *out,size_t n){(void)ctx;esp_fill_random(out,n);return 0;}
#endif
static bool matches_identity(void)
{
    mbedtls_x509_crt cert;mbedtls_pk_context key;
    mbedtls_x509_crt_init(&cert);mbedtls_pk_init(&key);
    int result=mbedtls_x509_crt_parse(&cert,(const unsigned char *)certificate,strlen(certificate)+1);
    if(result==0){
#if MBEDTLS_VERSION_MAJOR >= 4
        result=mbedtls_pk_parse_key(&key,(const unsigned char *)private_key,strlen(private_key)+1,NULL,0);
        if(result==0)result=mbedtls_pk_check_pair(&cert.pk,&key);
#else
        result=mbedtls_pk_parse_key(&key,(const unsigned char *)private_key,strlen(private_key)+1,NULL,0,random_bytes,NULL);
        if(result==0)result=mbedtls_pk_check_pair(&cert.pk,&key,random_bytes,NULL);
#endif
    }
    if(result==0){
        uint32_t flags=0;
        /* The provisioner verifies the issuer chain. Here pin the provisioned leaf,
           check the name/key, and let the browser enforce validity dates even before SNTP. */
        result=mbedtls_x509_crt_verify(&cert,&cert,NULL,hostname,&flags,NULL,NULL);
        if(result==MBEDTLS_ERR_X509_CERT_VERIFY_FAILED &&
           !(flags & ~(MBEDTLS_X509_BADCERT_EXPIRED|MBEDTLS_X509_BADCERT_FUTURE)))result=0;
    }
    mbedtls_pk_free(&key);mbedtls_x509_crt_free(&cert);return result==0;
}
esp_err_t waveform_phone_tls_start(httpd_handle_t *server,const httpd_config_t *config)
{
    clear_identity();
    esp_err_t err;
#ifdef CONFIG_NVS_ENCRYPTION
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
    if(err==ESP_OK)err=read_pem(h,"key",4096,&private_key);
    nvs_close(h);
    if(err==ESP_OK&&!matches_identity())err=ESP_ERR_INVALID_ARG;
    if(err!=ESP_OK){clear_identity();return err;}
    httpd_ssl_config_t ssl=HTTPD_SSL_CONFIG_DEFAULT();ssl.httpd=*config;
    ssl.transport_mode=HTTPD_SSL_TRANSPORT_SECURE;ssl.port_secure=443;
    ssl.servercert=(const unsigned char *)certificate;ssl.servercert_len=strlen(certificate)+1;
    ssl.prvtkey_pem=(const unsigned char *)private_key;ssl.prvtkey_len=strlen(private_key)+1;
    err=httpd_ssl_start(server,&ssl);
    if(err!=ESP_OK)clear_identity();
    return err;
}
void waveform_phone_tls_stop(httpd_handle_t server){httpd_ssl_stop(server);clear_identity();}
const char *waveform_phone_tls_hostname(void){return hostname;}

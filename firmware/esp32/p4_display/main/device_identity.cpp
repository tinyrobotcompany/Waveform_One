#include "device_identity.h"
#include "identity_store.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_flash_encrypt.h"
#include "esp_secure_boot.h"
#include "sdkconfig.h"
#include "mbedtls/pk.h"
#include "mbedtls/x509_csr.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/base64.h"
#include "psa/crypto.h"
#include <atomic>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

enum { DEVICE_HOSTNAME_MAX = 96, SAN_SHORT_FORM_NAME_MAX = 125 };
/* One dNSName uses two bytes inside a SEQUENCE whose length must be <128. */
static_assert(DEVICE_HOSTNAME_MAX <= SAN_SHORT_FORM_NAME_MAX,
               "CSR SAN encoder requires short-form DER lengths");
static char identifier[33], device_hostname[DEVICE_HOSTNAME_MAX+1];
static std::atomic_bool changed;
static std::atomic<int64_t> renewal;
static bool public_identity_logged;
extern const char device_roots_start[] __asm__("_binary_device_roots_pem_start");
extern const char device_roots_end[] __asm__("_binary_device_roots_pem_end");
static bool dns_name_valid(const char *name)
{
    size_t total=strlen(name),label=0;
    if(!total||total>DEVICE_HOSTNAME_MAX)return false;
    for(size_t i=0;i<total;i++){
        unsigned char c=(unsigned char)name[i];
        if(c=='.'){
            if(!label||name[i-1]=='-')return false;
            label=0;continue;
        }
        bool alnum=(c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9');
        if(!alnum&&c!='-')return false;
        if(!label&&c=='-')return false;
        if(++label>63)return false;
    }
    return label&&name[total-1]!='-';
}

static esp_err_t key_open(const char *space,mbedtls_pk_context *key)
{
    uint32_t id=!strcmp(space,"factory")?WAVEFORM_FACTORY_KEY_ID:WAVEFORM_TLS_KEY_ID;
    return mbedtls_pk_wrap_psa(key,id)==0?ESP_OK:ESP_ERR_INVALID_STATE;
}
static esp_err_t ensure_key(uint32_t id)
{
    psa_key_attributes_t attributes=PSA_KEY_ATTRIBUTES_INIT;
    psa_status_t result=psa_get_key_attributes(id,&attributes);
    if(result==PSA_SUCCESS){
        bool valid=psa_get_key_type(&attributes)==PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_R1)&&
            psa_get_key_bits(&attributes)==256&&psa_get_key_lifetime(&attributes)==PSA_KEY_LIFETIME_PERSISTENT&&
            psa_get_key_algorithm(&attributes)==PSA_ALG_ECDSA(PSA_ALG_SHA_256)&&
            psa_get_key_usage_flags(&attributes)==(PSA_KEY_USAGE_SIGN_HASH|PSA_KEY_USAGE_VERIFY_HASH|PSA_KEY_USAGE_SIGN_MESSAGE|PSA_KEY_USAGE_VERIFY_MESSAGE);
        psa_reset_key_attributes(&attributes);return valid?ESP_OK:ESP_ERR_INVALID_STATE;
    }
    psa_reset_key_attributes(&attributes);
    /* get_key_attributes maps an absent persistent key to INVALID_HANDLE.
       These two IDs are fixed valid user IDs; storage/corruption errors stay fatal. */
    if(result!=PSA_ERROR_INVALID_HANDLE)return ESP_ERR_INVALID_STATE;
    psa_set_key_id(&attributes,id);
    psa_set_key_type(&attributes,PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_R1));
    psa_set_key_bits(&attributes,256);
    psa_set_key_usage_flags(&attributes,PSA_KEY_USAGE_SIGN_HASH|PSA_KEY_USAGE_VERIFY_HASH);
    psa_set_key_algorithm(&attributes,PSA_ALG_ECDSA(PSA_ALG_SHA_256));
    mbedtls_svc_key_id_t generated=0;
    result=psa_generate_key(&attributes,&generated);psa_reset_key_attributes(&attributes);
    return result==PSA_SUCCESS&&generated==id?ESP_OK:ESP_FAIL;
}

static int64_t epoch(const mbedtls_x509_time *value)
{
    /* X.509 dates are UTC. Convert directly; never consult process TZ or
       mutate it while other firmware tasks are running. */
    static const int before_month[]={0,31,59,90,120,151,181,212,243,273,304,334};
    static const int month_days[]={31,28,31,30,31,30,31,31,30,31,30,31};
    int y=value->year,m=value->mon;
    bool leap=y%4==0&&(y%100!=0||y%400==0);
    if(y<1||y>9999||m<1||m>12||value->day<1||
       value->day>month_days[m-1]+(m==2&&leap)||value->hour<0||value->hour>23||
       value->min<0||value->min>59||value->sec<0||value->sec>59)return -1;
    int previous=y-1;
    int64_t days=365LL*(y-1970)+(previous/4-1969/4)-(previous/100-1969/100)+
        (previous/400-1969/400)+before_month[m-1]+(m>2&&leap)+value->day-1;
    return ((days*24+value->hour)*60+value->min)*60+value->sec;
}

esp_err_t waveform_identity_init(void)
{
    /* Reject DN metacharacters and invalid DNS labels before generating keys. */
    if(!dns_name_valid(CONFIG_WAVEFORM_DEVICE_SUFFIX)||
       strlen(CONFIG_WAVEFORM_DEVICE_SUFFIX)>DEVICE_HOSTNAME_MAX-35)return ESP_ERR_INVALID_ARG;
#if CONFIG_WAVEFORM_PRODUCTION_SECURITY
    if(!esp_flash_encryption_enabled()||!esp_secure_boot_enabled())return ESP_ERR_INVALID_STATE;
#endif
    esp_err_t err=waveform_identity_store_init();if(err!=ESP_OK)return err;
    nvs_handle_t existing_handle;
    err=nvs_open_from_partition("phone_tls","identity",NVS_READONLY,&existing_handle);
    if(err==ESP_OK){
        size_t n=0;esp_err_t old_key=nvs_get_str(existing_handle,"key",nullptr,&n);
        nvs_close(existing_handle);
        // Never silently replace an operator-provisioned identity.
        if(old_key!=ESP_ERR_NVS_NOT_FOUND)return ESP_ERR_INVALID_STATE;
    }else if(err!=ESP_ERR_NVS_NOT_FOUND)return err;
    if(psa_crypto_init()!=PSA_SUCCESS)return ESP_FAIL;
    if((err=ensure_key(WAVEFORM_FACTORY_KEY_ID))!=ESP_OK||(err=ensure_key(WAVEFORM_TLS_KEY_ID))!=ESP_OK)return err;
    mbedtls_pk_context key;mbedtls_pk_init(&key);
    err=key_open("factory",&key);
    unsigned char der[256],digest[32];size_t digest_len=0;
    int length=err==ESP_OK?mbedtls_pk_write_pubkey_der(&key,der,sizeof(der)):-1;
    if(length<=0||psa_hash_compute(PSA_ALG_SHA_256,der+sizeof(der)-length,(size_t)length,digest,sizeof(digest),&digest_len)!=PSA_SUCCESS||digest_len!=32)err=ESP_FAIL;
    if(err==ESP_OK){
        for(unsigned i=0;i<16;i++)snprintf(identifier+2*i,3,"%02x",digest[i]);
        int written=snprintf(device_hostname,sizeof(device_hostname),"d-%s.%s",identifier,CONFIG_WAVEFORM_DEVICE_SUFFIX);
        if(written<0||(size_t)written>=sizeof(device_hostname))err=ESP_ERR_INVALID_ARG;
    }
    if(err==ESP_OK&&!public_identity_logged){
        char public_pem[256];
        if(mbedtls_pk_write_pubkey_pem(&key,(unsigned char *)public_pem,sizeof(public_pem))!=0)err=ESP_FAIL;
        else {ESP_LOGI("identity","Manufacturing PUBLIC identity for %s:\n%s",identifier,public_pem);public_identity_logged=true;}
    }
    mbedtls_pk_free(&key);
    if(err!=ESP_OK)return err;
    nvs_handle_t handle;err=nvs_open_from_partition("phone_tls","identity",NVS_READWRITE,&handle);
    if(err==ESP_OK){
        err=nvs_set_str(handle,"hostname",device_hostname);
        if(err==ESP_OK)err=nvs_commit(handle);
        size_t length=8192;char *pem=static_cast<char *>(malloc(length));
        if(pem&&nvs_get_str(handle,"cert",pem,&length)==ESP_OK){
            mbedtls_x509_crt cert;mbedtls_x509_crt_init(&cert);
            if(mbedtls_x509_crt_parse(&cert,(unsigned char *)pem,length)==0)
                renewal=epoch(&cert.valid_from)+(epoch(&cert.valid_to)-epoch(&cert.valid_from))/2;
            mbedtls_x509_crt_free(&cert);
        }
        free(pem);nvs_close(handle);
    }
    return err;
}
const char *waveform_identity_id(void){return identifier;}
const char *waveform_identity_hostname(void){return device_hostname;}
bool waveform_identity_changed(void){return changed.exchange(false);}
int64_t waveform_identity_renew_after(void){return renewal;}

esp_err_t waveform_identity_csr(char *output,size_t capacity)
{
    size_t n=strlen(device_hostname);
    unsigned char san[sizeof(device_hostname)+4];
    if(!output||!capacity||!dns_name_valid(device_hostname)||!n||n>sizeof(device_hostname)-1||n+4>sizeof(san)||n>SAN_SHORT_FORM_NAME_MAX)
        return ESP_ERR_INVALID_ARG;
    mbedtls_pk_context key;mbedtls_pk_init(&key);
    esp_err_t err=key_open("identity",&key);
    mbedtls_x509write_csr csr;mbedtls_x509write_csr_init(&csr);
    if(err==ESP_OK){
        char subject[104];snprintf(subject,sizeof(subject),"CN=%s",device_hostname);
        mbedtls_x509write_csr_set_key(&csr,&key);mbedtls_x509write_csr_set_md_alg(&csr,MBEDTLS_MD_SHA256);
        int result=mbedtls_x509write_csr_set_subject_name(&csr,subject);
        /* DER SEQUENCE containing exactly one dNSName (all names fit short lengths). */
        san[0]=0x30;san[1]=(unsigned char)(n+2);san[2]=0x82;san[3]=(unsigned char)n;memcpy(san+4,device_hostname,n);
        if(result==0)result=mbedtls_x509write_csr_set_extension(&csr,"\x55\x1d\x11",3,0,san,n+4);
        if(result==0)result=mbedtls_x509write_csr_pem(&csr,(unsigned char *)output,capacity);
        if(result!=0)err=ESP_FAIL;
    }
    mbedtls_x509write_csr_free(&csr);mbedtls_pk_free(&key);return err;
}

esp_err_t waveform_identity_sign(const char *body,char *signature,size_t capacity)
{
    if(!body||!signature||!capacity)return ESP_ERR_INVALID_ARG;
    static const char context[]="WAVEFORM-DEVICE-V1\nPOST\n/v1/device\n";
    size_t length=strlen(body);if(length>6144)return ESP_ERR_INVALID_ARG;
    unsigned char *message=static_cast<unsigned char *>(malloc(sizeof(context)-1+length));if(!message)return ESP_ERR_NO_MEM;
    memcpy(message,context,sizeof(context)-1);memcpy(message+sizeof(context)-1,body,length);
    unsigned char digest[32],der[80];size_t digest_len=0,der_len=0,encoded=0;
    esp_err_t err=psa_hash_compute(PSA_ALG_SHA_256,message,sizeof(context)-1+length,digest,sizeof(digest),&digest_len)==PSA_SUCCESS?ESP_OK:ESP_FAIL;
    free(message);
    mbedtls_pk_context key;mbedtls_pk_init(&key);
    if(err==ESP_OK)err=key_open("factory",&key);
    if(err==ESP_OK&&mbedtls_pk_sign(&key,MBEDTLS_MD_SHA256,digest,digest_len,der,sizeof(der),&der_len)!=0)err=ESP_FAIL;
    if(err==ESP_OK&&mbedtls_base64_encode((unsigned char *)signature,capacity,&encoded,der,der_len)!=0)err=ESP_FAIL;
    mbedtls_pk_free(&key);return err;
}

esp_err_t waveform_identity_install(const char *chain,int64_t *renew_after)
{
    if(!chain||!renew_after)return ESP_ERR_INVALID_ARG;
    size_t length=strlen(chain)+1;if(length>8192||length<2)return ESP_ERR_INVALID_ARG;
    mbedtls_x509_crt cert,roots;mbedtls_x509_crt_init(&cert);mbedtls_x509_crt_init(&roots);
    mbedtls_pk_context key;mbedtls_pk_init(&key);
    esp_err_t err=key_open("identity",&key);
    /* parse keeps the delivered intermediates in cert.next. verify walks that
       untrusted chain; only the separately embedded roots are trust anchors. */
    int result=mbedtls_x509_crt_parse(&cert,(const unsigned char *)chain,length);
    if(result==0)result=mbedtls_x509_crt_parse(&roots,(const unsigned char *)device_roots_start,device_roots_end-device_roots_start);
    if(result==0&&err==ESP_OK)result=mbedtls_pk_check_pair(&cert.pk,&key);
    uint32_t flags=0;
    if(result==0)result=mbedtls_x509_crt_verify(&cert,&roots,NULL,device_hostname,&flags,NULL,NULL);
    int64_t start=epoch(&cert.valid_from),end=epoch(&cert.valid_to),now=time(NULL);
    if(err==ESP_OK&&(result!=0||flags||start<0||end<=start||end-now<86400||start>now))err=ESP_ERR_INVALID_ARG;
    /* Reject before consulting stored bytes: equality cannot validate a cert. */
    if(err!=ESP_OK){
        mbedtls_pk_free(&key);mbedtls_x509_crt_free(&cert);mbedtls_x509_crt_free(&roots);return err;
    }
    if(err==ESP_OK){
        nvs_handle_t handle;err=nvs_open_from_partition("phone_tls","identity",NVS_READWRITE,&handle);
        if(err==ESP_OK){
            char *existing=static_cast<char *>(malloc(length));size_t previous_length=length;
            bool same=existing&&nvs_get_str(handle,"cert",existing,&previous_length)==ESP_OK&&!strcmp(existing,chain);
            free(existing);
            if(same){
                renewal=start+(end-start)/2;*renew_after=renewal;
                nvs_close(handle);mbedtls_pk_free(&key);mbedtls_x509_crt_free(&cert);mbedtls_x509_crt_free(&roots);
                return ESP_OK;
            }
            /* One NVS value + commit: interrupted renewal leaves old cert usable. */
            err=nvs_set_str(handle,"cert",chain);if(err==ESP_OK)err=nvs_commit(handle);nvs_close(handle);
        }
        if(err==ESP_OK){renewal=start+(end-start)/2;*renew_after=renewal;changed.store(true);}
    }
    mbedtls_pk_free(&key);mbedtls_x509_crt_free(&cert);mbedtls_x509_crt_free(&roots);return err;
}

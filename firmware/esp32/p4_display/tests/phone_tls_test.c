#include "../main/phone_tls.c"
#include <assert.h>
#include <stdio.h>
static int init_error,open_error,read_error,parse_error,key_error,pair_error,verify_error,start_error;
static unsigned starts,stops;static uint32_t verify_flags;
static const char *name="device-a7f3.waveform.tinyrobotcompany.io";
static size_t fake_size;static bool self_signed,ca_leaf,clock_expired;
int mbedtls_x509_time_is_past(const mbedtls_x509_time *t){(void)t;return clock_expired;}
int mbedtls_x509_time_is_future(const mbedtls_x509_time *t){(void)t;return 0;}
#ifdef CONFIG_NVS_ENCRYPTION
static int key_error_read, missing_keys;
static esp_partition_t keys;
const esp_partition_t *esp_partition_find_first(int type,int subtype,const char *label){assert(type==ESP_PARTITION_TYPE_DATA&&subtype==ESP_PARTITION_SUBTYPE_DATA_NVS_KEYS&&!strcmp(label,"nvs_keys"));return missing_keys?NULL:&keys;}
int nvs_flash_read_security_cfg(const esp_partition_t *p,nvs_sec_cfg_t *cfg){assert(p==&keys);memset(cfg,0x42,sizeof(*cfg));return key_error_read;}
int nvs_flash_secure_init_partition(const char *name,nvs_sec_cfg_t *cfg){assert(!strcmp(name,"phone_tls")&&cfg->eky[0]==0x42);return init_error;}
#endif
int nvs_flash_init_partition(const char *p){assert(!strcmp(p,"phone_tls"));
#ifdef CONFIG_NVS_ENCRYPTION
assert(0 && "Encrypted identities must never use plaintext init");
#endif
return init_error;}
int nvs_open_from_partition(const char *p,const char *ns,int mode,nvs_handle_t *h){assert(!strcmp(p,"phone_tls")&&!strcmp(ns,"identity")&&mode==NVS_READONLY);*h=1;return open_error;}
void nvs_close(nvs_handle_t h){assert(h==1);}
int nvs_get_str(nvs_handle_t h,const char *key,char *out,size_t *n){assert(h==1);if(read_error)return read_error;const char *value=!strcmp(key,"hostname")?name:!strcmp(key,"cert")?"certificate":"private key";size_t length=strlen(value)+1;if(!out){*n=fake_size?fake_size:length;return 0;}if(*n<length)return ESP_FAIL;memcpy(out,value,length);*n=length;return 0;}
void mbedtls_pk_init(mbedtls_pk_context *p){(void)p;}void mbedtls_pk_free(mbedtls_pk_context *p){(void)p;}
void mbedtls_x509_crt_init(mbedtls_x509_crt *p){memset(p,0,sizeof(*p));}
int mbedtls_x509_crt_get_ca_istrue(const mbedtls_x509_crt *p){(void)p;return ca_leaf;}void mbedtls_x509_crt_free(mbedtls_x509_crt *p){(void)p;}
int mbedtls_x509_crt_parse(mbedtls_x509_crt *p,const unsigned char *s,size_t n){p->issuer_raw=(mbedtls_x509_buf){(unsigned char*)"issuer",6};p->subject_raw=(mbedtls_x509_buf){(unsigned char*)(self_signed?"issuer":"device"),6};assert(n==strlen((const char*)s)+1);return parse_error;}
int mbedtls_pk_parse_key(mbedtls_pk_context *p,const unsigned char *s,size_t n,const unsigned char *pwd,size_t len){(void)p;assert(n==strlen((const char*)s)+1&&!pwd&&!len);return key_error;}
int mbedtls_pk_check_pair(const mbedtls_pk_context *a,const mbedtls_pk_context *b){(void)a;(void)b;return pair_error;}
int mbedtls_x509_crt_verify(mbedtls_x509_crt *c,mbedtls_x509_crt *ca,void *crl,const char *host,uint32_t *flags,void *cb,void *ctx){assert(c!=ca&&!crl&&!strcmp(host,name)&&!cb&&!ctx);*flags=verify_flags;return verify_error;}
int httpd_ssl_start(httpd_handle_t *server,const httpd_ssl_config_t *cfg){starts++;assert(cfg->transport_mode==HTTPD_SSL_TRANSPORT_SECURE&&cfg->port_secure==443);assert(cfg->servercert_len==strlen((const char*)cfg->servercert)+1&&cfg->prvtkey_len==strlen((const char*)cfg->prvtkey_pem)+1);*server=(void*)1;return start_error;}
int httpd_ssl_stop(httpd_handle_t server){assert(server==(void*)1);stops++;return 0;}
int main(void){httpd_handle_t server=NULL;httpd_config_t config=HTTPD_DEFAULT_CONFIG();
 assert(!valid_hostname("")&&!valid_hostname("single")&&!valid_hostname("-bad.example")&&!valid_hostname("bad-.example")&&!valid_hostname("bad..example")&&!valid_hostname("bad.example.")&&!valid_hostname("*.example")&&!valid_hostname("UPPER.example"));
 char long_name[120];memset(long_name,'a',sizeof(long_name));long_name[119]=0;assert(!valid_hostname(long_name));long_name[70]='.';long_name[72]=0;assert(!valid_hostname(long_name));
#ifdef CONFIG_NVS_ENCRYPTION
 missing_keys=1;assert(waveform_phone_tls_start(&server,&config)!=0);missing_keys=0;
 key_error_read=-1;assert(waveform_phone_tls_start(&server,&config)!=0);key_error_read=0;
#endif
 init_error=-1;assert(waveform_phone_tls_start(&server,&config)!=0);init_error=0;
 open_error=-1;assert(waveform_phone_tls_start(&server,&config)!=0);open_error=0;
 read_error=-1;assert(waveform_phone_tls_start(&server,&config)!=0);read_error=0;
 name="bad..example";assert(waveform_phone_tls_start(&server,&config)!=0);name="device-a7f3.waveform.tinyrobotcompany.io";
 fake_size=9000;assert(waveform_phone_tls_start(&server,&config)!=0);fake_size=1;assert(waveform_phone_tls_start(&server,&config)!=0);fake_size=0;
 self_signed=true;assert(waveform_phone_tls_start(&server,&config)!=0);self_signed=false;
 ca_leaf=true;assert(waveform_phone_tls_start(&server,&config)!=0);ca_leaf=false;
 parse_error=-1;assert(waveform_phone_tls_start(&server,&config)!=0);parse_error=0;
 key_error=-1;assert(waveform_phone_tls_start(&server,&config)!=0);key_error=0;
 pair_error=-1;assert(waveform_phone_tls_start(&server,&config)!=0);pair_error=0;
 verify_error=MBEDTLS_ERR_X509_CERT_VERIFY_FAILED;verify_flags=4;assert(waveform_phone_tls_start(&server,&config)!=0);assert(!starts);
 verify_flags=MBEDTLS_X509_BADCERT_FUTURE;assert(waveform_phone_tls_start(&server,&config)!=0);
 verify_flags=MBEDTLS_X509_BADCERT_EXPIRED;assert(waveform_phone_tls_start(&server,&config)!=0);
 verify_error=0;verify_flags=0;start_error=-1;assert(waveform_phone_tls_start(&server,&config)!=0&&!waveform_phone_tls_hostname()[0]);start_error=0;
 assert(waveform_phone_tls_start(&server,&config)==0&&!strcmp(waveform_phone_tls_hostname(),name));assert(waveform_phone_tls_time_valid());clock_expired=true;assert(!waveform_phone_tls_time_valid());clock_expired=false;waveform_phone_tls_stop(server);assert(!waveform_phone_tls_time_valid());assert(stops==1&&!private_key&&!certificate);
 puts("PASS: HTTPS identity failures fail closed; keys match, hostname checked, TLS-only startup");}

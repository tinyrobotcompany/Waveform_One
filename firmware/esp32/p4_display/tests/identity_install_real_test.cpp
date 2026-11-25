/* Production install implementation, real SDK crypto; only NVS/HW are host seams. */
#define ESP_ERR_INVALID_STATE 4
#ifndef CONFIG_WAVEFORM_DEVICE_SUFFIX
#define CONFIG_WAVEFORM_DEVICE_SUFFIX "tempo.test"
#endif
#include "../main/device_identity.cpp"
#include <assert.h>
static char stored[8192],pending[8192];
static bool pending_write,fail_commit;
static unsigned public_logs;
int waveform_identity_store_init(void){return ESP_OK;}
void test_log(const char *tag,const char *format,...){(void)tag;(void)format;public_logs++;}
int nvs_open_from_partition(const char *part,const char *space,int mode,nvs_handle_t *h){
 (void)mode;assert(!strcmp(part,"phone_tls")&&!strcmp(space,"identity"));*h=1;return ESP_OK;
}
void nvs_close(nvs_handle_t h){assert(h==1);pending_write=false;}
int nvs_get_str(nvs_handle_t h,const char *name,char *out,size_t *size){
 assert(h==1);if(!strcmp(name,"key"))return ESP_ERR_NVS_NOT_FOUND;assert(!strcmp(name,"cert"));if(!stored[0])return ESP_ERR_NVS_NOT_FOUND;
 size_t n=strlen(stored)+1;if(!out){*size=n;return ESP_OK;}if(*size<n)return ESP_FAIL;
 memcpy(out,stored,n);*size=n;return ESP_OK;
}
int nvs_set_str(nvs_handle_t h,const char *name,const char *value){
 assert(h==1);if(!strcmp(name,"hostname"))return ESP_OK;
 assert(!strcmp(name,"cert")&&strlen(value)<sizeof(pending));strcpy(pending,value);pending_write=true;return ESP_OK;
}
int nvs_commit(nvs_handle_t h){assert(h==1);if(fail_commit)return ESP_FAIL;if(pending_write)strcpy(stored,pending);pending_write=false;return ESP_OK;}
static char *read_file(const char *path){
 FILE *f=fopen(path,"rb");assert(f);assert(!fseek(f,0,SEEK_END));long n=ftell(f);assert(n>0&&n<8192);rewind(f);
 char *text=static_cast<char *>(calloc(1,(size_t)n+1));assert(text&&fread(text,1,n,f)==(size_t)n);fclose(f);return text;
}
static void reject(const char *path,const char *previous){
 char *chain=read_file(path);int64_t result=123;
 assert(waveform_identity_install(chain,&result)!=ESP_OK&&result==123);
 assert(!strcmp(stored,previous)&&!waveform_identity_changed());free(chain);
}
int main(int argc,char **argv){
 assert(argc==2&&psa_crypto_init()==PSA_SUCCESS);
 if(!strcmp(argv[1],"csr")){
  /* Isolated test process/file-backed PSA store; no device key export. */
  psa_destroy_key(WAVEFORM_FACTORY_KEY_ID);psa_destroy_key(WAVEFORM_TLS_KEY_ID);
  assert(waveform_identity_init()==ESP_OK);char csr[2048];assert(waveform_identity_csr(csr,sizeof(csr))==ESP_OK);
  FILE *f=fopen("device.csr","wb");assert(f);fputs(csr,f);fclose(f);
  f=fopen("hostname.txt","wb");assert(f);fputs(waveform_identity_hostname(),f);fclose(f);return 0;
 }
 assert(!strcmp(argv[1],"verify"));
 setenv("TZ","EST5EDT",1);tzset();assert(waveform_identity_init()==ESP_OK);
 char *chain=read_file("fullchain.pem");int64_t renewal=0;
 assert(waveform_identity_install(chain,&renewal)==ESP_OK&&renewal>time(NULL));
 assert(!strcmp(stored,chain)&&waveform_identity_changed()&&!waveform_identity_changed());
 int64_t expected=renewal;setenv("TZ","JST-9",1);tzset();
 assert(waveform_identity_install(chain,&renewal)==ESP_OK&&renewal==expected&&!waveform_identity_changed());
 assert(waveform_identity_init()==ESP_OK&&waveform_identity_renew_after()==expected&&public_logs==1);
 reject("leaf.pem",chain);reject("wrong-host.pem",chain);reject("wrong-key.pem",chain);
 reject("untrusted.pem",chain);reject("expired.pem",chain);reject("malformed.pem",chain);
 /* Even a byte-identical stored invalid certificate must be rejected. */
 const char *invalid[]={"malformed.pem","expired.pem","wrong-host.pem"};
 for(size_t i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++){
  char *corrupt=read_file(invalid[i]);strcpy(stored,corrupt);
  reject(invalid[i],corrupt);assert(waveform_identity_renew_after()==expected);free(corrupt);
 }
 strcpy(stored,chain);
 char *replacement=read_file("replacement.pem");assert(strcmp(replacement,chain));
 fail_commit=true;assert(waveform_identity_install(replacement,&renewal)!=ESP_OK);
 assert(!strcmp(stored,chain)&&!waveform_identity_changed());fail_commit=false;
 assert(waveform_identity_install(replacement,&renewal)==ESP_OK&&waveform_identity_changed());
 assert(!strcmp(stored,replacement));
 free(replacement);free(chain);psa_destroy_key(WAVEFORM_FACTORY_KEY_ID);psa_destroy_key(WAVEFORM_TLS_KEY_ID);
 puts("PASS: real install accepts leaf+intermediate with only root trusted; rejects missing issuer, wrong name/key/trust/validity; preserves old certificate on commit failure; timezone-independent renewal");
}

#include <assert.h>
#include <stdio.h>
#include "../main/identity_store.cpp"
static bool secure=true,encrypted=true,keys_present=true;
static int hardware_error,storage_error,registration_error,open_error,read_error,write_error,commit_error;
static unsigned secure_inits,key_reads,commits,writes;
static unsigned char blob[MAX_KEY_RECORD],tls_blob[MAX_KEY_RECORD];static size_t blob_size,tls_size;
static const esp_psa_its_custom_ops_t *ops;
bool esp_secure_boot_enabled(void){return secure;}
bool esp_flash_encryption_enabled(void){return encrypted;}
const esp_partition_t *esp_partition_find_first(int type,int subtype,const char *name){
 assert(type==ESP_PARTITION_TYPE_DATA&&subtype==ESP_PARTITION_SUBTYPE_DATA_NVS_KEYS&&!strcmp(name,"nvs_keys"));
 static esp_partition_t keys={};return keys_present?&keys:nullptr;
}
int nvs_flash_read_security_cfg(const esp_partition_t *keys,nvs_sec_cfg_t *cfg){
 assert(keys);key_reads++;memset(cfg->eky,1,32);memset(cfg->tky,2,32);return hardware_error;
}
int nvs_flash_secure_init_partition(const char *p,nvs_sec_cfg_t *cfg){assert(!strcmp(p,"phone_tls")&&cfg->eky[0]==1&&cfg->tky[0]==2);secure_inits++;return storage_error;}
int esp_psa_its_register_custom_backend(const esp_psa_its_custom_ops_t *o){ops=o;return registration_error;}
int nvs_open_from_partition(const char *p,const char *space,int mode,nvs_handle_t *h){(void)mode;assert(ready&&!strcmp(p,"phone_tls")&&!strcmp(space,"psa_keys"));*h=1;return open_error;}
void nvs_close(nvs_handle_t h){assert(h==1);}
int nvs_get_blob(nvs_handle_t h,const char *key,void *out,size_t *n){assert(h==1&&key);if(read_error)return read_error;size_t length=!strcmp(key,"tls")?tls_size:blob_size;
 if(!length)return ESP_ERR_NVS_NOT_FOUND;
 if(out){if(*n<length)return ESP_FAIL;memcpy(out,!strcmp(key,"tls")?tls_blob:blob,length);}*n=length;return 0;}
int nvs_set_blob(nvs_handle_t h,const char *key,const void *data,size_t n){assert(h==1&&key&&n<=sizeof(blob));writes++;if(write_error)return write_error;
 if(!strcmp(key,"tls")){memcpy(tls_blob,data,n);tls_size=n;}else{memcpy(blob,data,n);blob_size=n;}return 0;}
int nvs_commit(nvs_handle_t h){assert(h==1);commits++;return commit_error;}
int main(void){
 struct psa_storage_info_t info;unsigned char out[20];size_t written=999;
 assert(store_info(NULL,WAVEFORM_FACTORY_KEY_ID,&info)==PSA_ERROR_STORAGE_FAILURE);
 assert(store_set(NULL,WAVEFORM_FACTORY_KEY_ID,1,"x",0)==PSA_ERROR_STORAGE_FAILURE);
 secure=false;assert(waveform_identity_store_init()!=0);secure=true;
 encrypted=false;assert(waveform_identity_store_init()!=0);encrypted=true;
 keys_present=false;assert(waveform_identity_store_init()!=0&&!secure_inits&&!key_reads);keys_present=true;
 hardware_error=-1;assert(waveform_identity_store_init()!=0&&!secure_inits);hardware_error=0;
 storage_error=-1;assert(waveform_identity_store_init()!=0&&!ready);storage_error=0;
 registration_error=-1;assert(waveform_identity_store_init()!=0&&!ready);registration_error=0;
 assert(waveform_identity_store_init()==0&&ready&&ops);unsigned calls=key_reads;
 assert(waveform_identity_store_init()==0&&calls==key_reads);
 assert(ops->get_info(NULL,WAVEFORM_FACTORY_KEY_ID,&info)==PSA_ERROR_DOES_NOT_EXIST);
 assert(ops->set(NULL,123,1,"x",0)==PSA_ERROR_STORAGE_FAILURE);
 assert(ops->set(NULL,WAVEFORM_FACTORY_KEY_ID,1,"x",1)==PSA_ERROR_INVALID_ARGUMENT);
 assert(ops->set(NULL,WAVEFORM_FACTORY_KEY_ID,513,"x",0)==PSA_ERROR_INVALID_ARGUMENT);
 open_error=-1;assert(ops->set(NULL,WAVEFORM_FACTORY_KEY_ID,1,"x",0)==PSA_ERROR_STORAGE_FAILURE);
 assert(ops->get_info(NULL,WAVEFORM_FACTORY_KEY_ID,&info)==PSA_ERROR_STORAGE_FAILURE);open_error=0;
 write_error=-1;assert(ops->set(NULL,WAVEFORM_FACTORY_KEY_ID,1,"x",0)==PSA_ERROR_STORAGE_FAILURE&&!commits);write_error=0;
 commit_error=-1;assert(ops->set(NULL,WAVEFORM_FACTORY_KEY_ID,1,"x",0)==PSA_ERROR_STORAGE_FAILURE);commit_error=0;
 /* NVS may already contain a first write despite a reported commit failure.
    Never erase or replace it on retry; recovery must inspect that identity. */
 assert(ops->get(NULL,WAVEFORM_FACTORY_KEY_ID,0,20,out,&written)==PSA_SUCCESS&&written==1&&out[0]=='x');
 assert(ops->set(NULL,WAVEFORM_FACTORY_KEY_ID,6,"secret",0)==PSA_ERROR_NOT_PERMITTED);
 blob_size=0; /* Separate fresh-device scenario, not production recovery. */
 assert(ops->set(NULL,WAVEFORM_FACTORY_KEY_ID,6,"secret",0)==PSA_SUCCESS);
 unsigned before_writes=writes,before_commits=commits;
 commit_error=-1;
 assert(ops->set(NULL,WAVEFORM_FACTORY_KEY_ID,7,"changed",0)==PSA_ERROR_NOT_PERMITTED);
 assert(ops->set(NULL,WAVEFORM_FACTORY_KEY_ID,6,"secret",0)==PSA_SUCCESS);
 assert(writes==before_writes&&commits==before_commits);
 assert(ops->get(NULL,WAVEFORM_FACTORY_KEY_ID,0,20,out,&written)==PSA_SUCCESS&&written==6&&!memcmp(out,"secret",6));
 commit_error=0;
 read_error=-1;assert(ops->set(NULL,WAVEFORM_FACTORY_KEY_ID,7,"changed",0)==PSA_ERROR_STORAGE_FAILURE);
 assert(writes==before_writes);read_error=0;
 assert(ops->get_info(NULL,WAVEFORM_FACTORY_KEY_ID,&info)==PSA_SUCCESS&&info.size==6&&info.flags==0);
 assert(ops->get(NULL,WAVEFORM_FACTORY_KEY_ID,2,2,out,&written)==PSA_SUCCESS&&written==2&&!memcmp(out,"cr",2));
 assert(ops->get(NULL,WAVEFORM_FACTORY_KEY_ID,2,20,out,&written)==PSA_SUCCESS&&written==4);
 assert(ops->get(NULL,WAVEFORM_FACTORY_KEY_ID,7,20,out,&written)==PSA_ERROR_INVALID_ARGUMENT&&written==0);
 assert(ops->get(NULL,WAVEFORM_FACTORY_KEY_ID,6,0,NULL,&written)==PSA_SUCCESS&&written==0);
 assert(ops->get(NULL,WAVEFORM_FACTORY_KEY_ID,0,1,NULL,&written)==PSA_ERROR_INVALID_ARGUMENT);
 assert(ops->get_info(NULL,WAVEFORM_FACTORY_KEY_ID,NULL)==PSA_ERROR_INVALID_ARGUMENT);
 read_error=-1;assert(ops->get_info(NULL,WAVEFORM_FACTORY_KEY_ID,&info)==PSA_ERROR_STORAGE_FAILURE);read_error=0;
 blob_size=513;assert(ops->get_info(NULL,WAVEFORM_FACTORY_KEY_ID,&info)==PSA_ERROR_DATA_CORRUPT);blob_size=6;
 assert(ops->set(NULL,WAVEFORM_TLS_KEY_ID,3,"tls",0)==PSA_SUCCESS);
 assert(ops->remove(NULL,WAVEFORM_TLS_KEY_ID)==PSA_ERROR_NOT_PERMITTED);
 puts("PASS: locked hardware prerequisites, encrypted-only NVS, bounded PSA storage and failure propagation");
}

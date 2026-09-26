#include <cassert>
#include <cstring>
#include <string>
#include "update_flash.h"
#include "update_identity.h"
namespace {
esp_partition_t slot{3*1024*1024};
bool live=false;
unsigned next=0;
int end_result=0,boot_result=0,hash_result=0,aborts=0,boot_calls=0;
}
const esp_partition_t* esp_ota_get_next_update_partition(const esp_partition_t*){return &slot;}
int esp_ota_begin(const esp_partition_t*,unsigned,esp_ota_handle_t* handle){assert(!live);live=true;*handle=++next;return 0;}
int esp_ota_write(esp_ota_handle_t,const void*,unsigned){assert(live);return 0;}
// IDF 6.1 esp_ota_end frees the entry on success AND every valid-handle error.
int esp_ota_end(esp_ota_handle_t handle){assert(live&&handle==next);live=false;return end_result;}
int esp_ota_abort(esp_ota_handle_t handle){assert(live&&handle==next);live=false;++aborts;return 0;}
int esp_ota_set_boot_partition(const esp_partition_t*){assert(!live);++boot_calls;return boot_result;}
int psa_crypto_init(){return 0;}
int psa_hash_setup(psa_hash_operation_t* hash,int){assert(!hash->active);hash->active=true;return 0;}
int psa_hash_update(psa_hash_operation_t*,const unsigned char*,size_t){return 0;}
int psa_hash_finish(psa_hash_operation_t* hash,unsigned char* data,size_t,size_t* size){
    if(hash_result)return hash_result;
    hash->active=false;memset(data,0xaa,32);*size=32;return 0;
}
int psa_hash_abort(psa_hash_operation_t* hash){hash->active=false;return 0;}
int main(){
    unsigned char identity[32];
    for(unsigned i=0;i<32;i++)identity[i]=i;
    assert(update::identity_hex(identity)=="000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f");
    update::Flash flash;
    const auto digest=std::string(64,'a');
    unsigned char bytes[4]{};
    for(int failure=0;failure<3;failure++){
        end_result=failure==0?-1:0;boot_result=failure==1?-1:0;hash_result=failure==2?-1:0;
        aborts=0;boot_calls=0;
        assert(flash.begin(4,digest.c_str()));assert(flash.write(bytes,4));
        assert(!flash.finish());assert(!live);
        assert(aborts==(failure==2?1:0));
        assert(boot_calls==(failure==1?1:0));
        // Cleanup is idempotent and never aborts an already-consumed IDF handle.
        flash.abort();assert(aborts==(failure==2?1:0));
        end_result=boot_result=hash_result=0;
        assert(flash.begin(4,digest.c_str()));assert(flash.write(bytes,4));assert(flash.finish());
        assert(!live);flash.abort();
    }
    update::Transfer transfer(flash);
    assert(transfer.handle("WFU BEGIN 4 "+digest)=="WFU READY 0");
    assert(transfer.handle("WFU DATA 0 0000")=="WFU READY 2");
    assert(!transfer.expire(30000000));
    assert(transfer.active() && live);
    assert(transfer.expire(30000001));
    assert(!transfer.active() && !live);
    assert(!transfer.expire(60000000));
    assert(transfer.handle("WFU DATA 2 0000")=="WFU ERR INACTIVE");
    assert(transfer.handle("WFU BEGIN 4 "+digest)=="WFU READY 0");
    assert(transfer.handle("WFU DATA 0 00000000")=="WFU READY 4");
    assert(transfer.handle("WFU END")=="WFU STAGED");
    assert(!transfer.active() && !live);

}

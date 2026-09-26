#pragma once
#include "update_protocol.h"
#include "esp_ota_ops.h"
#include "psa/crypto.h"
#include <cstring>
namespace update {
class Flash : public update::Storage {
    esp_ota_handle_t handle=0;
    const esp_partition_t* partition=nullptr;
    psa_hash_operation_t hash=PSA_HASH_OPERATION_INIT;
    unsigned char expected[32]{};
public:
    bool begin(unsigned size,const char* digest) override {
        partition=esp_ota_get_next_update_partition(nullptr);
        if(!partition || size>partition->size)return false;
        for(int i=0;i<32;i++)expected[i]=(update::hex(digest[2*i])<<4)|update::hex(digest[2*i+1]);
        if(psa_crypto_init()!=PSA_SUCCESS || psa_hash_setup(&hash,PSA_ALG_SHA_256)!=PSA_SUCCESS)return false;
        if(esp_ota_begin(partition,size,&handle)!=ESP_OK){psa_hash_abort(&hash);handle=0;return false;}
        return true;
    }
    bool write(const unsigned char* bytes,unsigned size) override {
        return psa_hash_update(&hash,bytes,size)==PSA_SUCCESS && esp_ota_write(handle,bytes,size)==ESP_OK;
    }
    bool finish() override {
        unsigned char digest[32];size_t size=0;
        if(psa_hash_finish(&hash,digest,sizeof(digest),&size)!=PSA_SUCCESS || size!=32 || memcmp(digest,expected,32)){abort();return false;}
        // ESP-IDF 6.1 esp_ota_end consumes the handle on success AND failure.
        // See esp_ota_ops.c: cleanup removes/frees the entry before returning.
        // Never esp_ota_abort this handle after end(), including boot-selection errors.
        const auto result=esp_ota_end(handle);
        handle=0;
        if(result!=ESP_OK)return false;
        return esp_ota_set_boot_partition(partition)==ESP_OK;
    }
    void abort() override {if(handle)esp_ota_abort(handle);handle=0;psa_hash_abort(&hash);}
};
}

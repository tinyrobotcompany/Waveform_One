#pragma once
#include <stddef.h>
#include <stdint.h>
typedef int psa_status_t;
typedef uint64_t psa_storage_uid_t;
typedef uint32_t psa_storage_create_flags_t;
struct psa_storage_info_t {uint32_t size,flags;};
#define PSA_SUCCESS 0
#define PSA_ERROR_DOES_NOT_EXIST -1
#define PSA_ERROR_STORAGE_FAILURE -2
#define PSA_ERROR_DATA_CORRUPT -3
#define PSA_ERROR_INVALID_ARGUMENT -4
#define PSA_ERROR_NOT_PERMITTED -5
typedef struct {
 psa_status_t (*set)(void *,psa_storage_uid_t,uint32_t,const void *,psa_storage_create_flags_t);
 psa_status_t (*get)(void *,psa_storage_uid_t,uint32_t,uint32_t,void *,size_t *);
 psa_status_t (*get_info)(void *,psa_storage_uid_t,struct psa_storage_info_t *);
 psa_status_t (*remove)(void *,psa_storage_uid_t);
 void *ctx;
} esp_psa_its_custom_ops_t;
int esp_psa_its_register_custom_backend(const esp_psa_its_custom_ops_t *);

#pragma once
#include <cstddef>
struct psa_hash_operation_t { bool active=false; };
#define PSA_HASH_OPERATION_INIT {}
constexpr int PSA_SUCCESS=0,PSA_ALG_SHA_256=1;
int psa_crypto_init();
int psa_hash_setup(psa_hash_operation_t*,int);
int psa_hash_update(psa_hash_operation_t*,const unsigned char*,size_t);
int psa_hash_finish(psa_hash_operation_t*,unsigned char*,size_t,size_t*);
int psa_hash_abort(psa_hash_operation_t*);

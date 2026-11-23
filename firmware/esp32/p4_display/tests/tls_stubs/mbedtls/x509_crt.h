#pragma once
#include "pk.h"
#include <stdint.h>
typedef struct {mbedtls_pk_context pk;} mbedtls_x509_crt;
#define MBEDTLS_ERR_X509_CERT_VERIFY_FAILED -9984
#define MBEDTLS_X509_BADCERT_EXPIRED 1
#define MBEDTLS_X509_BADCERT_FUTURE 2
void mbedtls_x509_crt_init(mbedtls_x509_crt *);
void mbedtls_x509_crt_free(mbedtls_x509_crt *);
int mbedtls_x509_crt_parse(mbedtls_x509_crt *,const unsigned char *,size_t);
int mbedtls_x509_crt_verify(mbedtls_x509_crt *,mbedtls_x509_crt *,void *,const char *,uint32_t *,void *,void *);

#pragma once
#include "pk.h"
#include <stdint.h>
typedef struct {unsigned char *p;size_t len;} mbedtls_x509_buf;
typedef struct { int year,mon,day,hour,min,sec; } mbedtls_x509_time;
typedef struct mbedtls_x509_crt {mbedtls_pk_context pk;mbedtls_x509_buf issuer_raw,subject_raw;mbedtls_x509_time valid_from,valid_to;struct mbedtls_x509_crt *next;} mbedtls_x509_crt;
int mbedtls_x509_time_is_past(const mbedtls_x509_time *);
int mbedtls_x509_time_is_future(const mbedtls_x509_time *);
int mbedtls_x509_crt_get_ca_istrue(const mbedtls_x509_crt *);
#define MBEDTLS_ERR_X509_CERT_VERIFY_FAILED -9984
#define MBEDTLS_X509_BADCERT_EXPIRED 1
#define MBEDTLS_X509_BADCERT_FUTURE 2
void mbedtls_x509_crt_init(mbedtls_x509_crt *);
void mbedtls_x509_crt_free(mbedtls_x509_crt *);
int mbedtls_x509_crt_parse(mbedtls_x509_crt *,const unsigned char *,size_t);
int mbedtls_x509_crt_verify(mbedtls_x509_crt *,mbedtls_x509_crt *,void *,const char *,uint32_t *,void *,void *);

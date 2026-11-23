#pragma once
#include "esp_http_server.h"
typedef struct {httpd_config_t httpd;const unsigned char *servercert,*prvtkey_pem;size_t servercert_len,prvtkey_len;int transport_mode,port_secure;} httpd_ssl_config_t;
#define HTTPD_SSL_CONFIG_DEFAULT() httpd_ssl_config_t{}
#define HTTPD_SSL_TRANSPORT_SECURE 1
int httpd_ssl_start(httpd_handle_t *,const httpd_ssl_config_t *);
int httpd_ssl_stop(httpd_handle_t);

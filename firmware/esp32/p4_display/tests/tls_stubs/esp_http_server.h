#pragma once
#include "platform.h"
#include <stddef.h>
typedef struct {const char *uri; size_t content_len;const char *body,*auth,*type;size_t received;int code;char response[32768];size_t response_len;bool download;bool no_store,no_referrer;} httpd_req_t;
typedef void *httpd_handle_t;
typedef struct {const char *uri;int method;esp_err_t (*handler)(httpd_req_t *);} httpd_uri_t;
typedef struct {int stack_size,max_open_sockets,recv_wait_timeout,send_wait_timeout,max_uri_handlers;bool lru_purge_enable;} httpd_config_t;
#define HTTPD_DEFAULT_CONFIG() httpd_config_t{}
#define HTTP_GET 0
#define HTTP_POST 1
#define HTTPD_400_BAD_REQUEST 400
#define HTTPD_401_UNAUTHORIZED 401
#define HTTPD_500_INTERNAL_SERVER_ERROR 500
esp_err_t httpd_resp_set_hdr(httpd_req_t *,const char *,const char *);
esp_err_t httpd_resp_set_type(httpd_req_t *,const char *);
esp_err_t httpd_resp_set_status(httpd_req_t *,const char *);
esp_err_t httpd_resp_send_err(httpd_req_t *,int,const char *);
esp_err_t httpd_resp_send(httpd_req_t *,const char *,int);
esp_err_t httpd_resp_sendstr(httpd_req_t *,const char *);
esp_err_t httpd_req_get_hdr_value_str(httpd_req_t *,const char *,char *,size_t);
int httpd_req_recv(httpd_req_t *,char *,size_t);
esp_err_t httpd_start(httpd_handle_t *,const httpd_config_t *);
esp_err_t httpd_register_uri_handler(httpd_handle_t,const httpd_uri_t *);

esp_err_t httpd_stop(httpd_handle_t);

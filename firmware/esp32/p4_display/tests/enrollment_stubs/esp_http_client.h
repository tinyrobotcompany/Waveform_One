#pragma once
#include "platform.h"
#define HTTP_METHOD_POST 1
int esp_http_client_set_method(void *,int);
int esp_http_client_write(void *,const char *,int);

#pragma once
#include "platform.h"
typedef void esp_netif_t;
typedef struct {struct {unsigned addr;} ip;} esp_netif_ip_info_t;
#define IPSTR "%u.%u.%u.%u"
#define IP2STR(ip) ((ip)->addr>>24),(((ip)->addr>>16)&255),(((ip)->addr>>8)&255),((ip)->addr&255)
esp_netif_t *esp_netif_get_handle_from_ifkey(const char *);
esp_err_t esp_netif_get_ip_info(esp_netif_t *,esp_netif_ip_info_t *);

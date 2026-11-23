#include "phone_dns.h"
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
bool waveform_phone_dns_matches(const char *hostname,uint32_t station_address)
{
    if(!hostname||!hostname[0]||!station_address)return false;
    struct addrinfo hints={0},*addresses=NULL;
    hints.ai_family=AF_UNSPEC;hints.ai_socktype=SOCK_STREAM;
    if(getaddrinfo(hostname,NULL,&hints,&addresses)!=0)return false;
    bool found=false,match=true;
    for(struct addrinfo *a=addresses;a;a=a->ai_next){
        /* This prototype publishes only an A record. Reject mixed/stale answers,
           including AAAA, rather than letting clients choose a different endpoint. */
        if(a->ai_family!=AF_INET||!a->ai_addr||a->ai_addrlen<sizeof(struct sockaddr_in))match=false;
        else {found=true;if(((struct sockaddr_in *)a->ai_addr)->sin_addr.s_addr!=station_address)match=false;}
    }
    freeaddrinfo(addresses);return found&&match;
}

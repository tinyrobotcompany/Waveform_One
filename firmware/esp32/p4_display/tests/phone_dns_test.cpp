#define _POSIX_C_SOURCE 200112L
#include "../main/phone_dns.cpp"
#include <assert.h>
#include <stdio.h>
static int error;static unsigned freed;static struct addrinfo first,second;static struct sockaddr_in address1,address2;
int getaddrinfo(const char *host,const char *service,const struct addrinfo *hints,struct addrinfo **out){assert(!strcmp(host,"device.example.com")&&!service&&hints->ai_family==AF_UNSPEC);*out=&first;return error;}
void freeaddrinfo(struct addrinfo *a){assert(a==&first);freed++;}
int main(void){
 assert(!waveform_phone_dns_matches(NULL,1)&&!waveform_phone_dns_matches("",1)&&!waveform_phone_dns_matches("device.example.com",0));
 first={};first.ai_family=AF_INET;first.ai_addr=reinterpret_cast<struct sockaddr *>(&address1);first.ai_addrlen=sizeof(address1);address1.sin_addr.s_addr=1;
 error=1;assert(!waveform_phone_dns_matches("device.example.com",1)&&!freed);error=0;
 assert(waveform_phone_dns_matches("device.example.com",1)&&freed==1);
 assert(!waveform_phone_dns_matches("device.example.com",2)); /* DHCP changed */
 first.ai_next=&second;second=first;second.ai_next=NULL;second.ai_addr=(struct sockaddr*)&address2;address2.sin_addr.s_addr=2;
 assert(!waveform_phone_dns_matches("device.example.com",1));address2.sin_addr.s_addr=1;assert(waveform_phone_dns_matches("device.example.com",1));
 second.ai_family=AF_INET6;assert(!waveform_phone_dns_matches("device.example.com",1));second.ai_family=AF_INET;second.ai_addrlen=0;assert(!waveform_phone_dns_matches("device.example.com",1));
 first.ai_next=NULL;first.ai_addr=NULL;assert(!waveform_phone_dns_matches("device.example.com",1));
 puts("PASS: unresolved, matching, wrong, DHCP-changed, mixed and IPv6 DNS answers");}

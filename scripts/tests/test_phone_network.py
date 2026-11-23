"""Compile the production HTTPS startup/DNS functions with host boundary fakes.

The actual network.cpp functions are extracted so orchestration, ordering and
readiness publication are tested without duplicating them in a test model.
"""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'firmware/esp32/p4_display/main/network.cpp'


def function(source, name):
    start = source.index('void ' + name + '(')
    opening = source.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


class PhoneNetworkTests(unittest.TestCase):
    def test_startup_and_retry_refresh_dns_before_publishing_pairing(self):
        source = SOURCE.read_text()
        functions = ''
        if 'void refresh_phone_dns(' in source:
            functions += function(source, 'refresh_phone_dns') + '\n'
        functions += function(source, 'start_secure_phone_server') + '\n'
        functions += function(source, 'stop_unavailable_phone_server')
        harness = r'''
#include <atomic>
#include <mutex>
#include <cstdio>
#include <ctime>
#include <cstdint>
#include <cassert>
#include <arpa/inet.h>
using esp_err_t=int; using httpd_handle_t=void*;
struct httpd_config_t {int max_uri_handlers,max_open_sockets;bool lru_purge_enable;int recv_wait_timeout,send_wait_timeout;};
#define HTTPD_DEFAULT_CONFIG() httpd_config_t{}
#define ESP_OK 0
#define ESP_LOGE(...) ((void)0)
constexpr int HTTP_POST=1,HTTP_GET=0;
std::mutex auth_mutex;
struct {bool suspended=false;void suspend(){suspended=true;}} auth;
std::atomic_bool lan_connected{true},phone_tls_ready{false};
std::atomic<uint32_t> dns_address{0};std::atomic<int64_t> dns_valid_until{0};
char acquired_host[16]="192.168.1.26",verified_phone_host[97]{};
int64_t now=1000000;int64_t esp_timer_get_time(){return now;}
bool certificate_valid=true;int stop_error=0,stops=0;
bool waveform_phone_tls_time_valid(){return certificate_valid;}
int waveform_phone_tls_stop(httpd_handle_t h){assert(h);stops++;return stop_error;}
bool matching_dns=true,published=false;int starts=0,records=0,resolves=0,publications=0,start_error=0;
const char *waveform_phone_tls_hostname(){return "unit.waveform.example.com";}
int waveform_phone_tls_start(httpd_handle_t *h,const httpd_config_t *){starts++;*h=start_error?nullptr:(void*)1;return start_error;}
bool waveform_phone_dns_matches(const char *,uint32_t address){assert(address==inet_addr(acquired_host));resolves++;return matching_dns;}
void bind_remote(){auth.suspended=false;assert(records==13);assert(resolves>0);publications++;published=phone_tls_ready&&dns_address==inet_addr(acquired_host)&&dns_valid_until>now;}
void register_uri(httpd_handle_t,const char *,int,int){records++;}
constexpr int history_handler=0,page_handler=0,pair_handler=0,session_handler=0,state_handler=0,
 update_status_handler=0,update_action_handler=0,update_config_handler=0,style_handler=0,
 brightness_handler=0,name_handler=0,appearance_handler=0,time_handler=0;
'''
        harness += functions + r'''
int main(){
 httpd_handle_t server=nullptr;
 start_error=1;start_secure_phone_server(&server);
 assert(!published&&!phone_tls_ready&&resolves==0&&publications==0);
 start_error=0;start_secure_phone_server(&server);
 assert(server&&published&&phone_tls_ready&&resolves==1&&publications==1);
 // Successful retry must resolve/publish before returning, without a timer tick.
 server=nullptr;phone_tls_ready=false;records=0;matching_dns=false;published=false;start_secure_phone_server(&server);
 assert(!published&&dns_valid_until==0&&resolves==2);
 server=nullptr;phone_tls_ready=false;records=0;matching_dns=true;start_secure_phone_server(&server);
 assert(published&&resolves==3);
 certificate_valid=false;stop_error=1;
 stop_unavailable_phone_server(&server);
 assert(server&&!phone_tls_ready&&dns_valid_until==0&&auth.suspended&&stops==1);
 // Even if the clock recovers, a pending shutdown must finish before restart.
 certificate_valid=true;stop_error=0;
 stop_unavailable_phone_server(&server);
 assert(!server&&!phone_tls_ready&&auth.suspended&&stops==2);
 records=0;start_secure_phone_server(&server);
 assert(published&&phone_tls_ready&&!auth.suspended&&resolves==4);
}
'''
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / 'network.cpp'
            path.write_text(harness)
            binary = Path(temp) / 'network'
            subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++17', '-pthread',
                            '-Wall', '-Wextra', '-Werror', str(path), '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True, timeout=15)

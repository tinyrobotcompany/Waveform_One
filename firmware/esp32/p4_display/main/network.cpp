#include "network.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_random.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

namespace {

constexpr char kTag[] = "waveform_network";
RemoteCallbacks controls{};
EventGroupHandle_t wifi_events = nullptr;
constexpr EventBits_t kConnected = BIT0;
constexpr EventBits_t kFailed = BIT1;
int connection_attempts = 0;
char station_address[32]{};
char remote_address[96]{};
char remote_token[33]{};
std::atomic_bool wifi_ready{false};
std::atomic_bool scanning{false};
std::atomic_bool attempting_home{false};

constexpr char kPage[] = R"HTML(<!doctype html>
<html lang="en"><meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<title>Waveform One</title><style>
:root{color-scheme:dark;font-family:Arial,sans-serif;--bg:#090e0a;--panel:#141c15;--line:#354638;--ink:#edf3e8;--muted:#899489;--accent:#a8d57a}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--ink);min-height:100vh;padding:env(safe-area-inset-top) 22px env(safe-area-inset-bottom)}
header{height:68px;display:flex;align-items:center;border-bottom:1px solid var(--line);letter-spacing:2px;font-size:12px}header b{color:var(--accent)}#clock{margin-left:auto;font-size:18px;letter-spacing:0}
main{max-width:620px;margin:auto;padding:34px 0}.eyebrow{color:var(--accent);font-size:11px;letter-spacing:2px}.art{aspect-ratio:1;border:1px solid var(--line);border-radius:16px;background:linear-gradient(145deg,#263528,#111812);display:grid;place-items:center;margin:18px 0 26px;font-size:48px;color:var(--muted);text-align:center}
h1{font-size:42px;font-weight:400;line-height:1.05;margin:12px 0}p{color:var(--muted);line-height:1.5}.card{background:var(--panel);border:1px solid var(--line);border-radius:14px;padding:18px;margin-top:18px}h2{font-size:14px;letter-spacing:1.5px;color:var(--accent);font-weight:400;margin:0 0 16px}
button,input{font:inherit}button{min-height:48px;border:1px solid var(--line);border-radius:10px;background:#1c281d;color:var(--ink);padding:10px 16px}button.active{border-color:var(--accent);color:var(--accent)}.styles{display:grid;grid-template-columns:repeat(3,1fr);gap:8px}label{display:block;color:var(--muted);font-size:13px}input[type=text]{width:100%;margin:9px 0 12px;padding:13px;border:1px solid var(--line);border-radius:9px;background:#0c120d;color:var(--ink);font-size:18px}input[type=range]{width:100%;height:42px;accent-color:var(--accent)}#status{font-size:13px;color:var(--accent);min-height:20px}
</style><body><header><b>WAVEFORM ONE</b><span id="clock"></span></header><main>
<div class="eyebrow">NOW PLAYING</div><h1 id="welcome">Welcome, Simon</h1><div class="art">WAVEFORM<br>ONE</div><h1>Listening for music</h1><p>Track, artist and album artwork will appear here.</p>
<section class="card"><h2>PROFILE</h2><label>Your name<input id="name" type="text" maxlength="40" value="Simon"></label><button onclick="saveName()">Save name</button></section>
<section class="card"><h2>LED STYLE</h2><div class="styles"><button onclick="style('classic')">Classic</button><button onclick="style('mirrored')">Mirrored</button><button onclick="style('waterfall')">Waterfall</button></div></section>
<section class="card"><h2>DISPLAY</h2><label>Brightness <input id="brightness" type="range" min="10" max="100" value="100"></label></section><p id="status">Connected locally to Waveform One</p>
</main><script>
const status=document.querySelector('#status');
const token=location.hash.slice(1);
if(!token)status.textContent='Pairing token missing. Open this remote from the QR code.';
function post(path,data){if(!token){status.textContent='Pairing required. Scan the QR code again.';return Promise.resolve(false)}return fetch(path,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded','X-Waveform-Token':token},body:new URLSearchParams(data)}).then(r=>{if(!r.ok)throw Error('Request failed');status.textContent='Saved';return true}).catch(()=>{status.textContent='Could not save setting';return false})}
function style(value){status.textContent='Applying on LED…';post('/api/style',{style:value})}
function saveName(){const name=document.querySelector('#name').value.trim();if(name){document.querySelector('#welcome').textContent='Welcome, '+name;post('/api/name',{name})}}
let timer;document.querySelector('#brightness').addEventListener('input',e=>{clearTimeout(timer);timer=setTimeout(()=>post('/api/brightness',{value:e.target.value}),80)});
function tick(){document.querySelector('#clock').textContent=new Date().toLocaleTimeString([],{hour:'2-digit',minute:'2-digit'})}tick();setInterval(tick,1000);post('/api/time',{epoch:Math.floor(Date.now()/1000)});
</script></body></html>)HTML";

bool read_body(httpd_req_t *request, std::string &body)
{
    if (request->content_len <= 0 || request->content_len > 256) return false;
    body.resize(request->content_len);
    size_t received = 0;
    while (received < body.size()) {
        const int count = httpd_req_recv(request, body.data() + received, body.size() - received);
        if (count <= 0) return false;
        received += static_cast<size_t>(count);
    }
    return true;
}

int hex_value(char value)
{
    if (value >= '0' && value <= '9') return value - '0';
    value = static_cast<char>(std::tolower(static_cast<unsigned char>(value)));
    return value >= 'a' && value <= 'f' ? value - 'a' + 10 : -1;
}

std::string form_value(const std::string &body, const char *key)
{
    const std::string prefix = std::string(key) + "=";
    size_t start = body.find(prefix);
    if (start == std::string::npos || (start != 0 && body[start - 1] != '&')) return {};
    std::string result;
    for (size_t index = start + prefix.size(); index < body.size() && result.size() < 64; ++index) {
        char value = body[index];
        if (value == '&') break;
        if (value == '+') value = ' ';
        else if (value == '%' && index + 2 < body.size()) {
            const int high = hex_value(body[index + 1]);
            const int low = hex_value(body[index + 2]);
            if (high < 0 || low < 0) return {};
            value = static_cast<char>((high << 4) | low);
            index += 2;
        }
        if (static_cast<unsigned char>(value) < 32) return {};
        result.push_back(value);
    }
    return result;
}

esp_err_t reply(httpd_req_t *request, const char *body = "OK")
{
    httpd_resp_set_type(request, "text/plain");
    return httpd_resp_sendstr(request, body);
}

esp_err_t service_unavailable(httpd_req_t *request, const char *message)
{
    httpd_resp_set_status(request, "503 Service Unavailable");
    httpd_resp_set_type(request, "text/plain");
    return httpd_resp_sendstr(request, message);
}

esp_err_t page_handler(httpd_req_t *request)
{
    httpd_resp_set_type(request, "text/html");
    return httpd_resp_send(request, kPage, HTTPD_RESP_USE_STRLEN);
}

void restart_task(void *)
{
    vTaskDelay(pdMS_TO_TICKS(2500));
    esp_restart();
}

bool save_wifi(const std::string &ssid, const std::string &password)
{
    nvs_handle_t handle{};
    if (nvs_open("waveform", NVS_READWRITE, &handle) != ESP_OK) return false;
    esp_err_t result = nvs_set_str(handle, "wifi_ssid", ssid.c_str());
    if (result == ESP_OK) result = nvs_set_str(handle, "wifi_pass", password.c_str());
    if (result == ESP_OK) result = nvs_commit(handle);
    nvs_close(handle);
    return result == ESP_OK;
}

bool load_wifi(std::string &ssid, std::string &password)
{
    nvs_handle_t handle{};
    if (nvs_open("waveform", NVS_READONLY, &handle) != ESP_OK) return false;
    char ssid_buffer[33]{};
    char password_buffer[64]{};
    size_t ssid_size = sizeof(ssid_buffer);
    size_t password_size = sizeof(password_buffer);
    const esp_err_t ssid_result = nvs_get_str(handle, "wifi_ssid", ssid_buffer, &ssid_size);
    const esp_err_t password_result = nvs_get_str(handle, "wifi_pass", password_buffer, &password_size);
    nvs_close(handle);
    if (ssid_result != ESP_OK || password_result != ESP_OK || ssid_buffer[0] == '\0') return false;
    ssid = ssid_buffer;
    password = password_buffer;
    return true;
}

bool load_or_create_remote_token()
{
    nvs_handle_t handle{};
    if (nvs_open("waveform", NVS_READWRITE, &handle) != ESP_OK) return false;
    size_t size = sizeof(remote_token);
    esp_err_t result = nvs_get_str(handle, "remote_token", remote_token, &size);
    if (result == ESP_OK && std::strlen(remote_token) == 32) {
        nvs_close(handle);
        return true;
    }

    uint8_t random[16]{};
    esp_fill_random(random, sizeof(random));
    for (size_t index = 0; index < sizeof(random); ++index) {
        std::snprintf(remote_token + index * 2, 3, "%02x", random[index]);
    }
    result = nvs_set_str(handle, "remote_token", remote_token);
    if (result == ESP_OK) result = nvs_commit(handle);
    nvs_close(handle);
    if (result != ESP_OK) std::memset(remote_token, 0, sizeof(remote_token));
    return result == ESP_OK;
}

bool authorized(httpd_req_t *request)
{
    constexpr char kHeader[] = "X-Waveform-Token";
    const size_t length = httpd_req_get_hdr_value_len(request, kHeader);
    if (length == 0 || length > 64) return false;
    char presented[65]{};
    if (httpd_req_get_hdr_value_str(request, kHeader, presented, sizeof(presented)) != ESP_OK) {
        return false;
    }
    return control_policy::remote_token_matches(remote_token, presented);
}

bool require_authorized(httpd_req_t *request)
{
    if (authorized(request)) return true;
    httpd_resp_send_err(request, HTTPD_403_FORBIDDEN, "Pairing token required");
    return false;
}

esp_err_t style_handler(httpd_req_t *request)
{
    if (!require_authorized(request)) return ESP_OK;
    std::string body;
    if (!read_body(request, body)) return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid request");
    const std::string value = form_value(body, "style");
    LedStyle style;
    if (value == "classic") style = LedStyle::Classic;
    else if (value == "mirrored") style = LedStyle::Mirrored;
    else if (value == "waterfall") style = LedStyle::Waterfall;
    else return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid style");
    if (controls.set_style == nullptr || !controls.set_style(style)) {
        return service_unavailable(request, "LED controller unavailable");
    }
    return reply(request);
}

esp_err_t brightness_handler(httpd_req_t *request)
{
    if (!require_authorized(request)) return ESP_OK;
    std::string body;
    if (!read_body(request, body)) return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid request");
    const std::string value = form_value(body, "value");
    char *end = nullptr;
    const long percent = std::strtol(value.c_str(), &end, 10);
    if (value.empty() || *end != '\0' || percent < 10 || percent > 100) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid brightness");
    }
    if (controls.set_brightness == nullptr ||
        !controls.set_brightness(static_cast<int>(percent))) {
        return service_unavailable(request, "Brightness update failed");
    }
    return reply(request);
}

esp_err_t name_handler(httpd_req_t *request)
{
    if (!require_authorized(request)) return ESP_OK;
    std::string body;
    if (!read_body(request, body)) return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid request");
    const std::string name = form_value(body, "name");
    if (name.empty()) return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid name");
    if (controls.set_name != nullptr) controls.set_name(name.c_str());
    return reply(request);
}

esp_err_t time_handler(httpd_req_t *request)
{
    if (!require_authorized(request)) return ESP_OK;
    std::string body;
    if (!read_body(request, body)) return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid request");
    const std::string value = form_value(body, "epoch");
    char *end = nullptr;
    const long long epoch = std::strtoll(value.c_str(), &end, 10);
    if (value.empty() || *end != '\0' || epoch < 1700000000LL) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid time");
    }
    if (controls.set_time != nullptr) controls.set_time(static_cast<std::time_t>(epoch));
    return reply(request);
}

void register_uri(httpd_handle_t server, const char *uri, httpd_method_t method,
                  esp_err_t (*handler)(httpd_req_t *))
{
    httpd_uri_t route{};
    route.uri = uri;
    route.method = method;
    route.handler = handler;
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &route));
}

void wifi_event(void *, esp_event_base_t base, int32_t event_id, void *event_data)
{
    if (base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        if (attempting_home.load()) esp_wifi_connect();
    } else if (base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (attempting_home.load()) {
            const auto *event = static_cast<wifi_event_sta_disconnected_t *>(event_data);
            ESP_LOGW(kTag, "Home Wi-Fi attempt %d disconnected, reason=%d",
                     connection_attempts + 1, event->reason);
            if (++connection_attempts < 5) esp_wifi_connect();
            else xEventGroupSetBits(wifi_events, kFailed);
        }
    } else if (base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        const auto *event = static_cast<ip_event_got_ip_t *>(event_data);
        std::snprintf(station_address, sizeof(station_address), "http://" IPSTR,
                      IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(wifi_events, kConnected);
    }
}

bool connect_home_wifi(const std::string &ssid, const std::string &password)
{
    wifi_config_t station{};
    std::memcpy(station.sta.ssid, ssid.data(), ssid.size());
    std::memcpy(station.sta.password, password.data(), password.size());
    station.sta.threshold.authmode = WIFI_AUTH_OPEN;
    station.sta.pmf_cfg.capable = true;
    station.sta.pmf_cfg.required = false;
    connection_attempts = 0;
    attempting_home.store(true);
    xEventGroupClearBits(wifi_events, kConnected | kFailed);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &station));
    ESP_ERROR_CHECK(esp_wifi_start());
    const EventBits_t result = xEventGroupWaitBits(
        wifi_events, kConnected | kFailed, pdFALSE, pdFALSE, pdMS_TO_TICKS(20000));
    attempting_home.store(false);
    if ((result & kConnected) != 0) return true;
    ESP_LOGW(kTag, "Could not join saved home Wi-Fi; opening touchscreen setup");
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_stop());
    return false;
}

void start_unconfigured_wifi()
{
    attempting_home.store(false);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void start_time_sync()
{
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();
    esp_sntp_config_t time_config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    ESP_ERROR_CHECK(esp_netif_sntp_init(&time_config));
}

void scan_task(void *)
{
    wifi_scan_config_t config{};
    control_policy::WifiNetworkList networks{};
    if (esp_wifi_scan_start(&config, true) == ESP_OK) {
        uint16_t count = 0;
        esp_wifi_scan_get_ap_num(&count);
        count = std::min<uint16_t>(count, 16);
        wifi_ap_record_t records[16]{};
        if (count > 0 && esp_wifi_scan_get_ap_records(&count, records) == ESP_OK) {
            for (uint16_t index = 0; index < count; ++index) {
                size_t size = 0;
                if (!control_policy::scanned_ssid_size(records[index].ssid, 32, size)) {
                    continue;
                }
                control_policy::add_wifi_network(networks, records[index].ssid, size);
            }
        }
    }
    if (controls.set_networks != nullptr) controls.set_networks(networks);
    scanning.store(false);
    vTaskDelete(nullptr);
}

void network_task(void *)
{
    esp_err_t result = nvs_flash_init();
    if (result == ESP_ERR_NVS_NO_FREE_PAGES || result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        result = nvs_flash_init();
    }
    ESP_ERROR_CHECK(result);
    ESP_ERROR_CHECK(load_or_create_remote_token() ? ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(esp_netif_init());
    result = esp_event_loop_create_default();
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) ESP_ERROR_CHECK(result);
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&config));
    wifi_events = xEventGroupCreate();
    ESP_ERROR_CHECK(wifi_events != nullptr ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event, nullptr));

    std::string home_ssid;
    std::string home_password;
    const bool connected = load_wifi(home_ssid, home_password) &&
                           connect_home_wifi(home_ssid, home_password);
    if (!connected) {
        start_unconfigured_wifi();
        if (controls.set_network != nullptr) controls.set_network(NetworkMode::Unconfigured, "");
    } else if (controls.set_network != nullptr) {
        start_time_sync();
        std::snprintf(remote_address, sizeof(remote_address), "%s/#%s",
                      station_address, remote_token);
        controls.set_network(NetworkMode::HomeWifi, remote_address);
    }
    wifi_ready.store(true);
    network_request_scan();

    httpd_config_t server_config = HTTPD_DEFAULT_CONFIG();
    server_config.max_uri_handlers = 10;
    httpd_handle_t server = nullptr;
    ESP_ERROR_CHECK(httpd_start(&server, &server_config));
    register_uri(server, "/", HTTP_GET, page_handler);
    register_uri(server, "/api/style", HTTP_POST, style_handler);
    register_uri(server, "/api/brightness", HTTP_POST, brightness_handler);
    register_uri(server, "/api/name", HTTP_POST, name_handler);
    register_uri(server, "/api/time", HTTP_POST, time_handler);
    if (connected) {
        ESP_LOGI(kTag, "Phone remote ready on home Wi-Fi: %s", station_address);
    } else {
        ESP_LOGI(kTag, "Home Wi-Fi requires touchscreen setup");
    }
    vTaskDelete(nullptr);
}

} // namespace

void network_start(const RemoteCallbacks &callbacks)
{
    controls = callbacks;
    ESP_ERROR_CHECK(xTaskCreate(network_task, "network", 8192, nullptr, 8, nullptr) == pdPASS
                        ? ESP_OK : ESP_ERR_NO_MEM);
}

void network_request_scan()
{
    if (!wifi_ready.load() || scanning.exchange(true)) return;
    if (xTaskCreate(scan_task, "wifi_scan", 4096, nullptr, 6, nullptr) != pdPASS) {
        scanning.store(false);
    }
}

bool network_configure_home(const control_policy::WifiNetwork &selected, const char *password)
{
    if (!wifi_ready.load() || password == nullptr || selected.ssid[0] == '\0') return false;
    const std::string network(selected.ssid);
    const std::string secret(password);
    if (network.size() > 32 || secret.size() > 63 || !save_wifi(network, secret)) return false;
    return xTaskCreate(restart_task, "wifi_restart", 2048, nullptr, 5, nullptr) == pdPASS;
}

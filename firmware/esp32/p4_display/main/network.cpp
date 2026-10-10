#include "network.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <memory>
#include <mutex>
#include <new>
#include <string>

#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "remote_auth.h"

namespace {

constexpr char kTag[] = "waveform_network";
RemoteCallbacks controls{};
EventGroupHandle_t wifi_events = nullptr;
constexpr EventBits_t kConnected = BIT0;
constexpr EventBits_t kFailed = BIT1;
constexpr TickType_t kMaintenanceInterval = pdMS_TO_TICKS(15000);
int connection_attempts = 0;
std::atomic_bool credentials_rejected{false};
std::mutex auth_mutex;
char acquired_host[16]{}; // guarded by auth_mutex
remote_auth::RemoteAuth auth{esp_fill_random};
std::atomic_bool wifi_ready{false};
std::atomic_bool scanning{false};
std::atomic_bool attempting_home{false};
// True while the device should stay on its saved home network and rejoin it
// after an access-point drop.
std::atomic_bool maintaining_home{false};
std::atomic_bool configuring_home{false};
std::atomic_bool time_sync_started{false};

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
let csrf='';
const sessionReady=fetch('/api/session',{credentials:'same-origin'}).then(r=>{if(!r.ok)throw Error('Pairing required');return r.text()}).then(value=>{csrf=value;status.textContent='Connected locally to Waveform One';return true}).catch(()=>{status.textContent='Pairing required. Scan the QR code on Waveform One.';return false});
function post(path,data){if(!csrf){status.textContent='Pairing required. Scan the QR code again.';return Promise.resolve(false)}return fetch(path,{method:'POST',credentials:'same-origin',headers:{'Content-Type':'application/x-www-form-urlencoded','X-Waveform-CSRF':csrf},body:new URLSearchParams(data)}).then(r=>{if(!r.ok)throw Error('Request failed');status.textContent='Saved';return true}).catch(()=>{status.textContent='Could not save setting';return false})}
function style(value){status.textContent='Applying on LED…';post('/api/style',{style:value})}
function saveName(){const name=document.querySelector('#name').value.trim();if(name){document.querySelector('#welcome').textContent='Welcome, '+name;post('/api/name',{name})}}
let timer;document.querySelector('#brightness').addEventListener('input',e=>{clearTimeout(timer);timer=setTimeout(()=>post('/api/brightness',{value:e.target.value}),80)});
function tick(){document.querySelector('#clock').textContent=new Date().toLocaleTimeString([],{hour:'2-digit',minute:'2-digit'})}tick();setInterval(tick,1000);sessionReady.then(ok=>{if(ok)post('/api/time',{epoch:Math.floor(Date.now()/1000)})});
</script></body></html>)HTML";

bool read_body(httpd_req_t *request, std::string &body)
{
    if (request->content_len <= 0 || request->content_len > 512) return false;
    body.resize(request->content_len);
    size_t received = 0;
    while (received < body.size()) {
        const int count = httpd_req_recv(request, body.data() + received, body.size() - received);
        if (count <= 0) return false;
        received += static_cast<size_t>(count);
    }
    return true;
}

bool read_form_field(httpd_req_t *request, const char *key, std::string &value,
                     std::size_t max_size = 32)
{
    std::string body;
    return read_body(request, body) && control_policy::form_field(body, key, value, max_size);
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

bool request_header(httpd_req_t *request, const char *name, char *value, size_t capacity)
{
    const size_t length = httpd_req_get_hdr_value_len(request, name);
    return length > 0 && length < capacity &&
           httpd_req_get_hdr_value_str(request, name, value, capacity) == ESP_OK;
}

struct RequestHeaders {
    char host[32]{};
    char origin[48]{};
    char cookies[513]{};
    char csrf[65]{};
};

void read_headers(httpd_req_t *request, RequestHeaders &headers)
{
    request_header(request, "Host", headers.host, sizeof(headers.host));
    request_header(request, "Origin", headers.origin, sizeof(headers.origin));
    request_header(request, "Cookie", headers.cookies, sizeof(headers.cookies));
    request_header(request, "X-Waveform-CSRF", headers.csrf, sizeof(headers.csrf));
}

void set_page_security_headers(httpd_req_t *request)
{
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    httpd_resp_set_hdr(request, "Content-Security-Policy",
                       "default-src 'none'; script-src 'unsafe-inline'; "
                       "style-src 'unsafe-inline'; connect-src 'self'; "
                       "frame-ancestors 'none'; base-uri 'none'; form-action 'none'");
    httpd_resp_set_hdr(request, "X-Frame-Options", "DENY");
    httpd_resp_set_hdr(request, "Referrer-Policy", "no-referrer");
    httpd_resp_set_hdr(request, "X-Content-Type-Options", "nosniff");
}

bool request_host_valid(httpd_req_t *request)
{
    RequestHeaders headers;
    read_headers(request, headers);
    const std::lock_guard<std::mutex> lock(auth_mutex);
    return auth.host_valid(headers.host);
}

esp_err_t page_handler(httpd_req_t *request)
{
    if (!request_host_valid(request)) {
        return httpd_resp_send_err(request, HTTPD_403_FORBIDDEN, "Invalid host");
    }
    set_page_security_headers(request);
    httpd_resp_set_type(request, "text/html");
    return httpd_resp_send(request, kPage, HTTPD_RESP_USE_STRLEN);
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

void clear_wifi()
{
    nvs_handle_t handle{};
    if (nvs_open("waveform", NVS_READWRITE, &handle) != ESP_OK) return;
    esp_err_t result = nvs_erase_key(handle, "wifi_ssid");
    if (result == ESP_OK || result == ESP_ERR_NVS_NOT_FOUND) {
        result = nvs_erase_key(handle, "wifi_pass");
    }
    if (result == ESP_OK || result == ESP_ERR_NVS_NOT_FOUND) nvs_commit(handle);
    nvs_close(handle);
}

bool load_wifi(std::string &ssid, std::string &password)
{
    nvs_handle_t handle{};
    if (nvs_open("waveform", NVS_READONLY, &handle) != ESP_OK) return false;
    char ssid_buffer[33]{};
    char password_buffer[65]{};
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

// Serializes reading the remote state with showing it, so a slower caller can
// never put an outdated QR back on screen.
std::mutex publish_mutex;

void publish_remote()
{
    const std::lock_guard<std::mutex> publishing(publish_mutex);
    char address[96]{};
    bool available = false;
    {
        const std::lock_guard<std::mutex> lock(auth_mutex);
        available = auth.pairing_url(address, sizeof(address));
    }
    if (controls.set_network == nullptr) return;
    if (available) {
        controls.set_network(NetworkMode::HomeWifi, address);
    } else if (maintaining_home.load()) {
        controls.set_network(NetworkMode::Reconnecting, "");
    } else {
        controls.set_network(NetworkMode::Unconfigured, "");
    }
}

void bind_remote()
{
    {
        const std::lock_guard<std::mutex> lock(auth_mutex);
        auth.connect(acquired_host, esp_timer_get_time());
    }
    publish_remote();
}

void revoke_remote()
{
    const std::lock_guard<std::mutex> lock(auth_mutex);
    auth.disconnect();
}

bool require_authorized(httpd_req_t *request)
{
    RequestHeaders headers;
    read_headers(request, headers);
    bool valid = false;
    {
        const std::lock_guard<std::mutex> lock(auth_mutex);
        valid = auth.authorize(headers.host, headers.origin, headers.cookies, headers.csrf,
                               esp_timer_get_time());
    }
    if (valid) return true;
    httpd_resp_send_err(request, HTTPD_403_FORBIDDEN, "Pairing session required");
    return false;
}

esp_err_t session_handler(httpd_req_t *request)
{
    RequestHeaders headers;
    read_headers(request, headers);
    char csrf[remote_auth::kTokenSize]{};
    bool valid = false;
    {
        const std::lock_guard<std::mutex> lock(auth_mutex);
        valid = auth.session_csrf(headers.host, headers.cookies, esp_timer_get_time(), csrf);
    }
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    if (!valid) return httpd_resp_send_err(request, HTTPD_401_UNAUTHORIZED, "Pairing required");
    return reply(request, csrf);
}

esp_err_t pair_handler(httpd_req_t *request)
{
    RequestHeaders headers;
    read_headers(request, headers);
    const size_t query_length = httpd_req_get_url_query_len(request);
    char query[97]{};
    char presented[65]{};
    char session_id[remote_auth::kTokenSize]{};
    bool paired = false;
    if (query_length > 0 && query_length < sizeof(query) &&
        httpd_req_get_url_query_str(request, query, sizeof(query)) == ESP_OK &&
        httpd_query_key_value(query, "code", presented, sizeof(presented)) == ESP_OK) {
        const std::lock_guard<std::mutex> lock(auth_mutex);
        paired = auth.pair(headers.host, presented, esp_timer_get_time(), session_id);
    }
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    httpd_resp_set_hdr(request, "Referrer-Policy", "no-referrer");
    if (!paired) {
        return httpd_resp_send_err(request, HTTPD_403_FORBIDDEN,
                                   "This pairing code has expired. Scan the QR code on "
                                   "Waveform One again.");
    }
    char cookie[112]{};
    std::snprintf(cookie, sizeof(cookie),
                  "wf1_session=%s; Path=/; Max-Age=3600; HttpOnly; SameSite=Strict", session_id);
    httpd_resp_set_hdr(request, "Set-Cookie", cookie);
    httpd_resp_set_hdr(request, "Location", "/");
    httpd_resp_set_status(request, "303 See Other");
    // The pairing code just rotated; show the replacement QR.
    publish_remote();
    return httpd_resp_send(request, nullptr, 0);
}

esp_err_t style_handler(httpd_req_t *request)
{
    if (!require_authorized(request)) return ESP_OK;
    std::string value;
    if (!read_form_field(request, "style", value)) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid request");
    }
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
    std::string value;
    if (!read_form_field(request, "value", value)) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid request");
    }
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
    std::string name;
    if (!read_form_field(request, "name", name, control_policy::kMaxDisplayNameBytes) ||
        !control_policy::safe_display_name(name)) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid name");
    }
    if (controls.set_name != nullptr) controls.set_name(name.c_str());
    return reply(request);
}

esp_err_t time_handler(httpd_req_t *request)
{
    if (!require_authorized(request)) return ESP_OK;
    std::string value;
    if (!read_form_field(request, "epoch", value)) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid request");
    }
    char *end = nullptr;
    const long long epoch = std::strtoll(value.c_str(), &end, 10);
    if (value.empty() || *end != '\0' ||
        !control_policy::valid_browser_time(epoch, std::time(nullptr))) {
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
        if (attempting_home.load() || maintaining_home.load()) esp_wifi_connect();
    } else if (base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (maintaining_home.load()) {
            // Sessions are bound to the address just lost; the maintenance loop rejoins.
            revoke_remote();
            publish_remote();
        } else if (attempting_home.load()) {
            const auto *event = static_cast<wifi_event_sta_disconnected_t *>(event_data);
            ESP_LOGW(kTag, "Home Wi-Fi attempt %d disconnected, reason=%d",
                     connection_attempts + 1, event->reason);
            // Only an explicit rejection proves the password wrong; an absent or
            // slow-booting router must not cost the user their saved network.
            if (event->reason == WIFI_REASON_AUTH_FAIL ||
                event->reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT ||
                event->reason == WIFI_REASON_HANDSHAKE_TIMEOUT) {
                credentials_rejected.store(true);
            }
            if (++connection_attempts < 5) esp_wifi_connect();
            else xEventGroupSetBits(wifi_events, kFailed);
        }
    } else if (base == IP_EVENT && event_id == IP_EVENT_STA_LOST_IP) {
        if (maintaining_home.load()) {
            revoke_remote();
            publish_remote();
        }
    } else if (base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        const auto *event = static_cast<ip_event_got_ip_t *>(event_data);
        {
            const std::lock_guard<std::mutex> lock(auth_mutex);
            std::snprintf(acquired_host, sizeof(acquired_host), IPSTR,
                          IP2STR(&event->ip_info.ip));
        }
        if (maintaining_home.load()) bind_remote();
        xEventGroupSetBits(wifi_events, kConnected);
    }
}

enum class JoinResult {
    Joined,
    Rejected,
    Unreachable,
};

wifi_config_t station_config(const std::string &ssid, const std::string &password)
{
    wifi_config_t station{};
    std::memcpy(station.sta.ssid, ssid.data(), ssid.size());
    std::memcpy(station.sta.password, password.data(), password.size());
    station.sta.threshold.authmode = WIFI_AUTH_OPEN;
    station.sta.pmf_cfg.capable = true;
    station.sta.pmf_cfg.required = false;
    return station;
}

JoinResult connect_home_wifi(const std::string &ssid, const std::string &password)
{
    wifi_config_t station = station_config(ssid, password);
    connection_attempts = 0;
    credentials_rejected.store(false);
    attempting_home.store(true);
    xEventGroupClearBits(wifi_events, kConnected | kFailed);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &station));
    ESP_ERROR_CHECK(esp_wifi_start());
    const EventBits_t result = xEventGroupWaitBits(
        wifi_events, kConnected | kFailed, pdFALSE, pdFALSE, pdMS_TO_TICKS(20000));
    attempting_home.store(false);
    if ((result & kConnected) != 0) return JoinResult::Joined;
    ESP_LOGW(kTag, "Could not join home Wi-Fi");
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_stop());
    return credentials_rejected.load() ? JoinResult::Rejected : JoinResult::Unreachable;
}

void start_unconfigured_wifi()
{
    attempting_home.store(false);
    maintaining_home.store(false);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    publish_remote();
}

// Keeps retrying the saved network in the background while the touchscreen
// stays free to choose a different one.
void resume_saved_wifi(const std::string &ssid, const std::string &password)
{
    wifi_config_t station = station_config(ssid, password);
    attempting_home.store(false);
    maintaining_home.store(true);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &station));
    ESP_ERROR_CHECK(esp_wifi_start());
    publish_remote();
}

// After a failed join, falls back to the saved network if one exists.
void fall_back_to_saved_wifi()
{
    std::string ssid;
    std::string password;
    if (load_wifi(ssid, password)) resume_saved_wifi(ssid, password);
    else start_unconfigured_wifi();
}

void start_time_sync()
{
    if (time_sync_started.exchange(true)) return;
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
        // Filter the full result set: hidden, malformed and duplicate SSIDs must not
        // crowd valid networks out of the list.
        uint16_t count = 0;
        esp_wifi_scan_get_ap_num(&count);
        count = std::min<uint16_t>(count, 64);
        std::unique_ptr<wifi_ap_record_t[]> records(new (std::nothrow) wifi_ap_record_t[count]);
        if (count > 0 && records != nullptr &&
            esp_wifi_scan_get_ap_records(&count, records.get()) == ESP_OK) {
            for (uint16_t index = 0; index < count; ++index) {
                const size_t size = control_policy::scanned_ssid_size(records[index].ssid, 32);
                control_policy::add_wifi_network(networks, records[index].ssid, size);
            }
        } else {
            esp_wifi_clear_ap_list();
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
        ESP_LOGE(kTag, "NVS recovery required (%s); refusing to erase saved credentials",
                 esp_err_to_name(result));
        if (controls.set_wifi_configuration != nullptr) {
            controls.set_wifi_configuration(WifiConfigurationState::StorageRecoveryRequired);
        }
        vTaskDelete(nullptr);
        return;
    }
    ESP_ERROR_CHECK(result);
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
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_LOST_IP, wifi_event, nullptr));

    std::string home_ssid;
    std::string home_password;
    const bool had_saved_wifi = load_wifi(home_ssid, home_password);
    const JoinResult join = had_saved_wifi ? connect_home_wifi(home_ssid, home_password)
                                           : JoinResult::Unreachable;
    const bool connected = join == JoinResult::Joined;
    if (connected) {
        start_time_sync();
        maintaining_home.store(true);
        bind_remote();
    } else if (had_saved_wifi && join == JoinResult::Unreachable) {
        ESP_LOGW(kTag, "Saved Wi-Fi unreachable; retrying in the background");
        resume_saved_wifi(home_ssid, home_password);
    } else {
        if (had_saved_wifi) {
            ESP_LOGW(kTag, "Saved Wi-Fi password was rejected; removing it");
            clear_wifi();
        }
        start_unconfigured_wifi();
    }
    wifi_ready.store(true);
    network_request_scan();

    httpd_config_t server_config = HTTPD_DEFAULT_CONFIG();
    server_config.max_uri_handlers = 10;
    httpd_handle_t server = nullptr;
    ESP_ERROR_CHECK(httpd_start(&server, &server_config));
    register_uri(server, "/", HTTP_GET, page_handler);
    register_uri(server, "/pair", HTTP_GET, pair_handler);
    register_uri(server, "/api/session", HTTP_GET, session_handler);
    register_uri(server, "/api/style", HTTP_POST, style_handler);
    register_uri(server, "/api/brightness", HTTP_POST, brightness_handler);
    register_uri(server, "/api/name", HTTP_POST, name_handler);
    register_uri(server, "/api/time", HTTP_POST, time_handler);
    if (connected) {
        ESP_LOGI(kTag, "Phone remote ready on home Wi-Fi: http://%s", acquired_host);
    } else {
        ESP_LOGI(kTag, "Home Wi-Fi requires touchscreen setup");
    }

    // Keep the on-screen pairing code valid and rejoin home Wi-Fi after a drop.
    for (;;) {
        vTaskDelay(kMaintenanceInterval);
        bool rotated = false;
        bool bound = false;
        {
            const std::lock_guard<std::mutex> lock(auth_mutex);
            rotated = auth.refresh_pairing(esp_timer_get_time());
            bound = auth.connected();
        }
        if (rotated) publish_remote();
        if (!bound && maintaining_home.load() && !configuring_home.load()) {
            const esp_err_t reconnect = esp_wifi_connect();
            if (reconnect != ESP_OK) {
                ESP_LOGW(kTag, "Home Wi-Fi rejoin deferred: %s", esp_err_to_name(reconnect));
            }
        }
    }
}

struct CandidateWifi {
    std::string ssid;
    std::string password;
};

void configure_home_task(void *context)
{
    auto *candidate = static_cast<CandidateWifi *>(context);
    maintaining_home.store(false);
    revoke_remote();
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_stop());
    const bool connected =
        connect_home_wifi(candidate->ssid, candidate->password) == JoinResult::Joined;
    const bool saved = connected && save_wifi(candidate->ssid, candidate->password);
    if (saved) {
        start_time_sync();
        maintaining_home.store(true);
        bind_remote();
        if (controls.set_wifi_configuration != nullptr) {
            controls.set_wifi_configuration(WifiConfigurationState::Connected);
        }
    } else {
        if (connected) ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_stop());
        fall_back_to_saved_wifi();
        if (controls.set_wifi_configuration != nullptr) {
            controls.set_wifi_configuration(WifiConfigurationState::Failed);
        }
    }
    delete candidate;
    configuring_home.store(false);
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
    if (!wifi_ready.load() || password == nullptr || selected.ssid[0] == '\0' ||
        configuring_home.exchange(true)) return false;
    const std::string network(selected.ssid);
    const std::string secret(password);
    if (network.size() > 32 || !control_policy::valid_wifi_password(secret)) {
        configuring_home.store(false);
        return false;
    }
    auto *candidate = new (std::nothrow) CandidateWifi{network, secret};
    if (candidate == nullptr ||
        xTaskCreate(configure_home_task, "wifi_configure", 4096, candidate, 7, nullptr) != pdPASS) {
        delete candidate;
        configuring_home.store(false);
        return false;
    }
    return true;
}

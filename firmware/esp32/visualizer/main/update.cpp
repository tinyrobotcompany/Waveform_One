#include "update.h"
#include "update_protocol.h"
#include "protocol_write.h"
#include "capture.h"
#include "esp_app_desc.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "update_flash.h"
#include "freertos/task.h"
#include <atomic>
#include <cstdio>
namespace {
std::atomic<bool> healthy{false};
update::Flash flash;
update::Transfer transfer(flash);
int64_t activity=0;
bool pending(){esp_ota_img_states_t state;return esp_ota_get_state_partition(esp_ota_get_running_partition(),&state)==ESP_OK && state==ESP_OTA_IMG_PENDING_VERIFY;}
void reply(const std::string& text){const auto line="\n"+text+"\n";control::write_reply(line.data(),line.size());}
}
bool update_active(){return transfer.active();}
void update_healthy(){healthy.store(true);}
void update_tick(){
    static const int64_t boot=esp_timer_get_time();
    const int64_t now=esp_timer_get_time();
    if(transfer.active() && now-activity>30000000)transfer.abort();
    // A wedged or unconfirmed new release must not become permanent.
    if(now-boot>120000000 && pending())esp_restart();
}
bool update_handle(std::string_view line){
    if(line.substr(0,4)!="WFU ")return false;
    activity=esp_timer_get_time();
    if(line=="WFU STATUS"){
        char digest[65];esp_app_get_elf_sha256(digest,sizeof(digest));
        reply(std::string("WFU STATUS ")+digest+" "+(pending()?"pending":"valid")+" "+(healthy.load()?"healthy":"starting")+" wf1-esp32s3-16mb");
    }else if(line=="WFU CONFIRM"){
        reply(healthy.load() && esp_ota_get_boot_partition()==esp_ota_get_running_partition() && esp_ota_mark_app_valid_cancel_rollback()==ESP_OK?"WFU CONFIRMED":"WFU ERR HEALTH");
    }else if(line=="WFU CANCEL"){
        transfer.abort();
        reply(esp_ota_set_boot_partition(esp_ota_get_running_partition())==ESP_OK && esp_ota_mark_app_valid_cancel_rollback()==ESP_OK?"WFU CANCELLED":"WFU ERR CANCEL");
    }else if(line=="WFU REBOOT"){
        transfer.abort();reply("WFU REBOOTING");vTaskDelay(pdMS_TO_TICKS(100));esp_restart();
    }else if(line=="WFU ROLLBACK"){
        transfer.abort();
        if(esp_ota_check_rollback_is_possible()){
            reply("WFU ROLLING_BACK");vTaskDelay(pdMS_TO_TICKS(100));esp_ota_mark_app_invalid_rollback_and_reboot();
        }else reply("WFU ERR NO_ROLLBACK");
    }else if(line.substr(0,10)=="WFU BEGIN " && capture_active())reply("WFU ERR BUSY");
    else reply(transfer.handle(line));
    return true;
}

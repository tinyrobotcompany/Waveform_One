#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "control_protocol.h"
#include "panel.h"

static void control_task(void*) {
    // Existing secondary USB console VFS: bounded nonblocking reads, no new
    // driver/ISR or changes to the working GPIO and console configuration.
    const int fd = open("/dev/secondary", O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        ESP_LOGE("waveform_control", "USB secondary console unavailable");
        vTaskDelete(nullptr);
        return;
    }
    control::Lines lines;
    while (true) {
        char buffer[64];
        const auto count = read(fd, buffer, sizeof(buffer));
        for (int i = 0; i < count; ++i) lines.push(buffer[i], [](std::string_view line) {
            control::Command command{};
            if (!control::parse(line, command)) {
                printf("WF1 0 ERR BAD_COMMAND\n");
                return;
            }
            if (command.changeMode) panel_set_mode(command.mode);
            printf("WF1 %u OK MODE %s\n", command.id, visual::name(panel_mode()));
        });
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
void control_start() {
    ESP_ERROR_CHECK(xTaskCreatePinnedToCore(control_task, "usb_control", 4096,
        nullptr, 1, nullptr, 0) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}

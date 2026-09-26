#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "control_protocol.h"
#include "panel.h"
#include "capture.h"
#include "protocol_write.h"

static void control_handle_line(std::string_view line) {
    control::Command command{};
    char reply[64];
    int size;
    bool started = false;
    if (!control::parse(line, command)) {
        size = snprintf(reply, sizeof(reply), "\nWF1 0 ERR BAD_COMMAND\n");
    } else if (command.capture) {
        started = capture_start(command.id);
        size = snprintf(reply, sizeof(reply), started ? "\nWF1 %u AUDIO 16000 128000\n" : "\nWF1 %u ERR BUSY\n", command.id);
    } else {
        if (command.changeMode) panel_set_mode(command.mode);
        size = snprintf(reply, sizeof(reply), "\nWF1 %u OK MODE %s\n",
                        command.id, visual::name(panel_mode()));
    }
    if (!control::write_reply(reply, size)) {
        if (started) capture_abort(command.id);
        ESP_LOGE("waveform_control", "WF1 reply could not be queued");
    }
}

static void control_task(void*) {
    // ESP-IDF v6.1's nonblocking VFS read checks the driver's RX ring buffer.
    // Without a driver it always reports zero available bytes, even when the
    // hardware FIFO contains commands. Route both VFS RX and TX through it.
    usb_serial_jtag_driver_config_t config = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    config.rx_buffer_size = 512;
    config.tx_buffer_size = 4096;
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&config));
    usb_serial_jtag_vfs_use_driver();
    const int fd = open("/dev/secondary", O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        ESP_LOGE("waveform_control", "USB secondary console unavailable");
        vTaskDelete(nullptr);
        return;
    }
    control::Lines lines;
    ESP_LOGI("waveform_control", "WF1 USB controls ready");
    while (true) {
        char buffer[64];
        const auto count = read(fd, buffer, sizeof(buffer));
        for (int i = 0; i < count; ++i) lines.push(buffer[i], [](std::string_view line) {
            control_handle_line(line);
        });
        capture_send();
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}
void control_start() {
    capture_init();
    ESP_ERROR_CHECK(xTaskCreatePinnedToCore(control_task, "usb_control", 4096,
        nullptr, 1, nullptr, 0) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}

#pragma once

typedef enum {
    ESP_BLUEDROID_STATUS_UNINITIALIZED = 0,
    ESP_BLUEDROID_STATUS_INITIALIZED = 1,
    ESP_BLUEDROID_STATUS_ENABLED = 2
} esp_bluedroid_status_t;

inline esp_bluedroid_status_t& fakeBluedroidStatus() {
    static esp_bluedroid_status_t status = ESP_BLUEDROID_STATUS_UNINITIALIZED;
    return status;
}

inline esp_bluedroid_status_t esp_bluedroid_get_status() {
    return fakeBluedroidStatus();
}

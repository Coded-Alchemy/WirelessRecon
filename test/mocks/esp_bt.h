#pragma once

typedef enum {
    ESP_BT_CONTROLLER_STATUS_IDLE = 0,
    ESP_BT_CONTROLLER_STATUS_INITED = 1,
    ESP_BT_CONTROLLER_STATUS_ENABLED = 2
} esp_bt_controller_status_t;

inline esp_bt_controller_status_t& fakeControllerStatus() {
    static esp_bt_controller_status_t status = ESP_BT_CONTROLLER_STATUS_IDLE;
    return status;
}

inline esp_bt_controller_status_t esp_bt_controller_get_status() {
    return fakeControllerStatus();
}

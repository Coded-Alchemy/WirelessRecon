//
// Created by dedlee on 11/15/26.
//


/**
 * @file DashboardScreen.cpp
 * @brief Implements the WirelessRecon dashboard presentation.
 *
 * @details
 * This module translates application state into LVGL widgets.
 *
 * Data flow:
 *
 *   User presses scan button
 *           |
 *           v
 *   DashboardScreen
 *           |
 *           v
 *   WirelessCoordinator::requestScan()
 *           |
 *           v
 *   Existing radio scanner
 *
 * Metric data follows the reverse presentation path:
 *
 *   DeviceInventory
 *           |
 *           v
 *   DashboardScreen::refresh()
 *           |
 *           v
 *   LVGL labels
 *
 * Important:
 * - This file must not include Arduino.h.
 * - This file must not access Wi-Fi or Bluetooth hardware.
 * - All LVGL operations run on the application/UI task.
 */

#include "ui/screens/DashboardScreen.h"

#include <cstdio>

#include "ui/theme/UITheme.h"

namespace wirelessrecon::ui::screens {

using namespace wirelessrecon::ui::theme;

void DashboardScreen::bind(
    core::WirelessCoordinator& coordinator,
    core::DeviceInventory& inventory
) {
    // Store non-owning references to the application's existing services.
    // The composition root is responsible for their lifetime.
    coordinator_ = &coordinator;
    inventory_ = &inventory;
}

bool DashboardScreen::create(lv_obj_t* parent) {
    if (parent == nullptr ||
        coordinator_ == nullptr ||
        inventory_ == nullptr) {
        return false;
    }

    root_ = lv_obj_create(parent);

    if (root_ == nullptr) {
        return false;
    }

    // Reset retained widget references before creating a new screen.
    // LVGL parent ownership remains unchanged.
    wifiCountLabel_ = nullptr;
    bleCountLabel_ = nullptr;
    classicCountLabel_ = nullptr;
    totalCountLabel_ = nullptr;
    scanStatusLabel_ = nullptr;

    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(
        parent,
        LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_START
    );

    lv_obj_set_style_pad_all(
        parent, Spacing::screenMargin, LV_PART_MAIN);

    lv_obj_set_style_pad_row(
        parent, Spacing::section, LV_PART_MAIN);

    // Allow vertical scrolling on compact displays such as 240x320.
    // This keeps the scan buttons reachable when the dashboard is taller
    // than the available screen area.
    lv_obj_set_scroll_dir(root_, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(root_, LV_SCROLLBAR_MODE_AUTO);

    // --------------------------------------------------------
    // Dashboard heading
    // --------------------------------------------------------

    lv_obj_t* heading = lv_label_create(parent);
    lv_label_set_text(heading, "WIRELESS OVERVIEW");

    lv_obj_set_style_text_color(
        heading,
        lv_color_hex(Colors::primary),
        LV_PART_MAIN
    );

    lv_obj_set_style_text_font(
        heading,
        &lv_font_montserrat_14,
        LV_PART_MAIN
    );

    // --------------------------------------------------------
    // Inventory metrics
    // --------------------------------------------------------

    lv_obj_t* metricsPanel = lv_obj_create(parent);

    lv_obj_set_width(metricsPanel, lv_pct(100));
    lv_obj_set_height(metricsPanel, LV_SIZE_CONTENT);

    applyPanel(metricsPanel);

    lv_obj_set_style_pad_all(
        metricsPanel, Spacing::standard, LV_PART_MAIN);

    lv_obj_set_style_pad_row(
        metricsPanel, Spacing::compact, LV_PART_MAIN);

    lv_obj_set_flex_flow(
        metricsPanel, LV_FLEX_FLOW_COLUMN);

    lv_obj_t* metricsHeading = lv_label_create(metricsPanel);
    lv_label_set_text(metricsHeading, "DISCOVERED DEVICES");

    lv_obj_set_style_text_color(
        metricsHeading,
        lv_color_hex(Colors::textSecondary),
        LV_PART_MAIN
    );

    createMetricRow(
        metricsPanel, "Wi-Fi access points", "0");

    createMetricRow(
        metricsPanel, "BLE devices", "0");

    createMetricRow(
        metricsPanel, "Bluetooth Classic", "0");

    createMetricRow(
        metricsPanel, "Total inventory", "0");

    // Retain the value labels in creation order.
    lv_obj_t* firstRow = lv_obj_get_child(metricsPanel, 1);
    lv_obj_t* secondRow = lv_obj_get_child(metricsPanel, 2);
    lv_obj_t* thirdRow = lv_obj_get_child(metricsPanel, 3);
    lv_obj_t* fourthRow = lv_obj_get_child(metricsPanel, 4);

    if (firstRow) wifiCountLabel_ =
        lv_obj_get_child(firstRow, 1);

    if (secondRow) bleCountLabel_ =
        lv_obj_get_child(secondRow, 1);

    if (thirdRow) classicCountLabel_ =
        lv_obj_get_child(thirdRow, 1);

    if (fourthRow) totalCountLabel_ =
        lv_obj_get_child(fourthRow, 1);

    // --------------------------------------------------------
    // Scanner controls
    // --------------------------------------------------------

    lv_obj_t* scanPanel = lv_obj_create(parent);

    lv_obj_set_width(scanPanel, lv_pct(100));
    lv_obj_set_height(scanPanel, LV_SIZE_CONTENT);

    applyPanel(scanPanel);

    lv_obj_set_style_pad_all(
        scanPanel, Spacing::standard, LV_PART_MAIN);

    lv_obj_set_style_pad_row(
        scanPanel, Spacing::standard, LV_PART_MAIN);

    lv_obj_set_flex_flow(
        scanPanel, LV_FLEX_FLOW_COLUMN);

    lv_obj_t* scanHeading = lv_label_create(scanPanel);
    lv_label_set_text(scanHeading, "SCANNER CONTROLS");

    lv_obj_set_style_text_color(
        scanHeading,
        lv_color_hex(Colors::textSecondary),
        LV_PART_MAIN
    );

    createScanButton(
        scanPanel,
        "Start Wi-Fi scan",
        core::RadioType::Wifi
    );

    createScanButton(
        scanPanel,
        "Start BLE scan",
        core::RadioType::Ble
    );

    // Classic Bluetooth is deliberately not exposed as a scan button yet.
    // The current modular composition registers Wi-Fi and BLE scanners.
    // The dashboard must not advertise a capability without an adapter.

    scanStatusLabel_ = lv_label_create(scanPanel);
    lv_label_set_text(scanStatusLabel_, "Scanner ready");

    lv_obj_set_width(scanStatusLabel_, lv_pct(100));

    lv_obj_set_style_text_color(
        scanStatusLabel_,
        lv_color_hex(Colors::warning),
        LV_PART_MAIN
    );

    // Display current inventory values immediately.
    refresh();

    return true;
}

lv_obj_t* DashboardScreen::createMetricRow(
    lv_obj_t* parent,
    const char* title,
    const char* value
) {
    lv_obj_t* row = lv_obj_create(parent);

    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);

    lv_obj_set_style_bg_opa(
        row, LV_OPA_TRANSP, LV_PART_MAIN);

    lv_obj_set_style_border_width(
        row, 0, LV_PART_MAIN);

    lv_obj_set_style_pad_all(
        row, 0, LV_PART_MAIN);

    lv_obj_set_flex_flow(
        row, LV_FLEX_FLOW_ROW);

    lv_obj_set_flex_align(
        row,
        LV_FLEX_ALIGN_SPACE_BETWEEN,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER
    );

    lv_obj_t* titleLabel = lv_label_create(row);
    lv_label_set_text(titleLabel, title);

    lv_obj_set_style_text_color(
        titleLabel,
        lv_color_hex(Colors::textSecondary),
        LV_PART_MAIN
    );

    lv_obj_t* valueLabel = lv_label_create(row);
    lv_label_set_text(valueLabel, value);

    lv_obj_set_style_text_color(
        valueLabel,
        lv_color_hex(Colors::primary),
        LV_PART_MAIN
    );

    return valueLabel;
}

lv_obj_t* DashboardScreen::createScanButton(
    lv_obj_t* parent,
    const char* text,
    core::RadioType radio
) {
    lv_obj_t* button = lv_btn_create(parent);

    lv_obj_set_width(button, lv_pct(100));

    // Forty-four logical pixels provides a practical initial touch target.
    // Physical usability will be verified on each supported display.
    lv_obj_set_height(button, 44);

    lv_obj_set_style_bg_color(
        button,
        lv_color_hex(Colors::surfaceRaised),
        LV_PART_MAIN
    );

    lv_obj_set_style_border_color(
        button,
        lv_color_hex(Colors::primary),
        LV_PART_MAIN
    );

    lv_obj_set_style_border_width(
        button,
        Spacing::borderWidth,
        LV_PART_MAIN
    );

    // LVGL user data carries the radio identifier for this button.
    lv_obj_set_user_data(
        button,
        reinterpret_cast<void*>(
            static_cast<uintptr_t>(radio)
        )
    );

    lv_obj_add_event_cb(
        button,
        &DashboardScreen::scanButtonEventCallback,
        LV_EVENT_CLICKED,
        this
    );

    lv_obj_t* label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_center(label);

    lv_obj_set_style_text_color(
        label,
        lv_color_hex(Colors::textPrimary),
        LV_PART_MAIN
    );

    return button;
}

void DashboardScreen::scanButtonEventCallback(lv_event_t* event) {
    if (event == nullptr) {
        return;
    }

    auto* self = static_cast<DashboardScreen*>(
        lv_event_get_user_data(event)
    );

    lv_obj_t* button = static_cast<lv_obj_t*>(
        lv_event_get_target(event)
    );

    if (self == nullptr || button == nullptr) {
        return;
    }

    const uintptr_t radioValue =
        reinterpret_cast<uintptr_t>(
            lv_obj_get_user_data(button)
        );

    const auto radio =
        static_cast<core::RadioType>(radioValue);

    self->requestScan(radio);
}

void DashboardScreen::requestScan(core::RadioType radio) {
    if (coordinator_ == nullptr) {
        return;
    }

    // All scanner availability, capability, and conflict checks happen
    // inside WirelessCoordinator. The UI only submits the request.
    const core::ScanRequestResult result =
        coordinator_->requestScan(radio);

    if (scanStatusLabel_ != nullptr) {
        lv_label_set_text(
            scanStatusLabel_,
            scanResultMessage(result)
        );
    }

    refresh();
}

const char* DashboardScreen::scanResultMessage(
    core::ScanRequestResult result
) {
    switch (result) {
        case core::ScanRequestResult::Started:
            return "Scan started";

        case core::ScanRequestResult::UnsupportedRadio:
            return "Radio not supported";

        case core::ScanRequestResult::ScannerUnavailable:
            return "Scanner unavailable";

        case core::ScanRequestResult::ScannerBusy:
            return "Scanner is busy";

        case core::ScanRequestResult::RadioConflict:
            return "Bluetooth controller busy";

        case core::ScanRequestResult::StartFailed:
            return "Unable to start scan";

        default:
            return "Unknown scan result";
    }
}

void DashboardScreen::refresh() {
    if (inventory_ == nullptr) {
        return;
    }

    // Inventory counts include historical records, not only devices
    // currently visible in the most recently completed scan.
    const size_t wifi =
        inventory_->count(core::RadioType::Wifi);

    const size_t ble =
        inventory_->count(core::RadioType::Ble);

    const size_t classic =
        inventory_->count(core::RadioType::BluetoothClassic);

    const size_t total = inventory_->size();

    char value[16];

    auto updateCount = [&value](
        lv_obj_t* label,
        size_t count
    ) {
        if (label == nullptr) {
            return;
        }

        std::snprintf(
            value,
            sizeof(value),
            "%u",
            static_cast<unsigned>(count)
        );

        lv_label_set_text(label, value);
    };

    updateCount(wifiCountLabel_, wifi);
    updateCount(bleCountLabel_, ble);
    updateCount(classicCountLabel_, classic);
    updateCount(totalCountLabel_, total);

    if (scanStatusLabel_ != nullptr &&
        coordinator_ != nullptr) {

        const bool wifiScanning =
            coordinator_->scanInProgress(core::RadioType::Wifi);

        const bool bleScanning =
            coordinator_->scanInProgress(core::RadioType::Ble);

        if (wifiScanning && bleScanning) {
            lv_label_set_text(
                scanStatusLabel_,
                "Wi-Fi and BLE scanning"
            );
        }
        else if (wifiScanning) {
            lv_label_set_text(
                scanStatusLabel_,
                "Wi-Fi scan running"
            );
        }
        else if (bleScanning) {
            lv_label_set_text(
                scanStatusLabel_,
                "BLE scan running"
            );
        }
        else {
            lv_label_set_text(
                scanStatusLabel_,
                "Scanner ready"
            );
        }
    }
}

} // namespace wirelessrecon::ui::screens

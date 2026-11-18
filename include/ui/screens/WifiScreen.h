/**
 * @file WifiScreen.h
 * @brief Public interface for the paginated Wi-Fi inventory screen.
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <lvgl.h>

#include "core/DeviceInventory.h"
#include "core/WirelessCoordinator.h"
#include "ui/theme/UITheme.h"

namespace wirelessrecon::ui::screens {

/**
 * @class WifiScreen
 * @brief Displays a bounded, paginated snapshot of Wi-Fi inventory records.
 *
 * @details
 * DeviceInventory remains authoritative. This screen copies a bounded number
 * of records into a local snapshot and rebuilds its visible cards only when
 * the inventory generation changes or the user changes pages.
 *
 * The screen does not own the coordinator or inventory. All LVGL operations
 * must run on the application/UI task.
 */
class WifiScreen {
public:
    using BackCallback = void (*)(void* context);

    using NavigationCallback = void (*)(
        void* context,
        theme::NavigationDestination destination
    );

    /** @brief Maximum number of Wi-Fi records represented on one page. */
    static constexpr std::size_t kItemsPerPage = 4;

    /**
     * @brief Binds non-owning application service references.
     */
    void bind(
        core::WirelessCoordinator& coordinator,
        core::DeviceInventory& inventory
    );

    /**
     * @brief Registers the Back-button owner callback.
     */
    void setBackCallback(
        BackCallback callback,
        void* context
    );

    /**
     * @brief Registers the persistent bottom-navigation callback.
     */
    void setNavigationCallback(
        NavigationCallback callback,
        void* context
    );

    /**
     * @brief Creates the Wi-Fi screen widget tree.
     * @param parent Active LVGL screen.
     * @return true if all required widgets were created.
     */
    bool create(lv_obj_t* parent);

    /**
     * @brief Refreshes inventory and scan-state presentation.
     */
    void refresh();

private:
    core::WirelessCoordinator* coordinator_ = nullptr;
    core::DeviceInventory* inventory_ = nullptr;

    BackCallback backCallback_ = nullptr;
    void* backContext_ = nullptr;

    NavigationCallback navigationCallback_ = nullptr;
    void* navigationContext_ = nullptr;

    lv_obj_t* root_ = nullptr;
    lv_obj_t* headerContainer_ = nullptr;
    lv_obj_t* contentContainer_ = nullptr;
    lv_obj_t* statusLabel_ = nullptr;
    lv_obj_t* listContainer_ = nullptr;
    lv_obj_t* controlsContainer_ = nullptr;
    lv_obj_t* previousButton_ = nullptr;
    lv_obj_t* pageLabel_ = nullptr;
    lv_obj_t* nextButton_ = nullptr;
    lv_obj_t* navigationContainer_ = nullptr;

    std::array<core::Device, kItemsPerPage> devices_{};

    std::size_t deviceCount_ = 0;
    std::size_t pageIndex_ = 0;
    uint32_t renderedGeneration_ = 0;

    /**
     * @brief Creates the fixed global navigation bar.
     */
    bool createNavigationBar(lv_obj_t* parent);

    /**
     * @brief Creates one bottom-navigation destination.
     */
    lv_obj_t* createNavigationButton(
        lv_obj_t* parent,
        const char* text,
        theme::NavigationDestination destination,
        bool enabled,
        bool selected
    );

    /**
     * @brief Copies the current Wi-Fi inventory into the bounded snapshot.
     */
    void reloadInventory();

    /**
     * @brief Rebuilds the cards for the current page.
     */
    void renderPage();

    /**
     * @brief Updates the page indicator and Previous/Next availability.
     */
    void updatePageControls();

    /**
     * @brief Moves one page backward or forward.
     */
    void changePage(int direction);

    /**
     * @brief Dispatches the Back action to the registered owner.
     */
    void goBack();

    /** @brief LVGL event adapter for the Back button. */
    static void backEventCallback(lv_event_t* event);

    /** @brief LVGL event adapter for Previous. */
    static void previousEventCallback(lv_event_t* event);

    /** @brief LVGL event adapter for Next. */
    static void nextEventCallback(lv_event_t* event);

    /** @brief LVGL event adapter for global bottom navigation. */
    static void navigationEventCallback(lv_event_t* event);
};

} // namespace wirelessrecon::ui::screens
#include "hardware/display/DisplayConfigFactory.h"

#include "hardware/BoardProfile.h"

namespace wirelessrecon::hardware::display {

    bool makeDisplayConfig(DisplayConfig& config) {

        // Reset to default values before applying the board profile.
        config = DisplayConfig{};

        // ============================================================
        // ThingPulse Color Kit Grande / ESP32-WROVER
        // ============================================================

#if defined(WR_BOARD_THINGPULSE_WROVER)

        if (!kHasDisplay ||
            kDisplayController != DisplayController::ILI9488 ||
            kDisplayBus != DisplayBus::SPI) {
            return false;
            }

        // Controller and bus
        config.controller = kDisplayController;
        config.bus = kDisplayBus;

        // Display dimensions
        config.width = kDisplayWidth;
        config.height = kDisplayHeight;

        config.memoryWidth = kTftMemoryWidth;
        config.memoryHeight = kTftMemoryHeight;

        config.panelWidth = kTftPanelWidth;
        config.panelHeight = kTftPanelHeight;

        config.offsetX = kTftOffsetX;
        config.offsetY = kTftOffsetY;

        // SPI GPIO
        config.pinSclk = kTftSclk;
        config.pinMosi = kTftMosi;
        config.pinMiso = kTftMiso;
        config.pinCs = kTftCs;
        config.pinDc = kTftDc;
        config.pinReset = kTftReset;
        config.pinBacklight = kTftBacklight;

        // SPI settings
        config.spiMode = kTftSpiMode;
        config.writeFrequency = kTftWriteFrequency;
        config.readFrequency = kTftReadFrequency;

        // Panel settings
        config.dummyReadPixel = kTftDummyReadPixel;
        config.dummyReadBits = kTftDummyReadBits;

        config.readable = kTftReadable;
        config.invertColors = kTftInvertColors;
        config.rgbOrder = kTftRgbOrder;
        config.busShared = kTftBusShared;

        config.rotation = 0;
        config.useDma = true;

        return true;

#else

        // No display configuration has been implemented for this
        // board profile yet.
        return false;

#endif
    }

} // namespace wirelessrecon::hardware::display
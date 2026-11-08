/*
 * ============================================================
 * Wireless Recon Console  -  Phase 7 (BLE Proximity Tracking)
 * ============================================================
 * Sketch : Arduino IDE / arduino-cli (.ino)
 *   Board  : ESP32 Wrover Module, PSRAM Enabled, ESP32 core 2.0.17
 *   Library: LovyanGFX (lovyan03)
 * Target : ThingPulse ESP32 Color Kit Grande
 *          (ePulse Feather ESP32, ILI9488 320x480, FT6236 touch)
 *
 * Phase 7.1: adds shared BLE and Bluetooth Classic target tracking with RSSI history.
 *            Tap a BLE or Classic device to track its signal while moving closer/farther away.
 *            RSSI is relative and noisy; it is not a reliable distance meter.
 *            RF channel dashboard and existing inventory retained.
 * ============================================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <LovyanGFX.hpp>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <esp_bt.h>
#include <esp_bt_main.h>
#include <esp_gap_bt_api.h>
#include <esp_bt_device.h>
#include <esp_err.h>
#include <algorithm>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// ============================================================
// Forward declarations
// ============================================================

void setFont2();

void drawSignalBars(int x, int y, int rssi);

void drawHeader(const char *title, bool showBack);
void drawHeader(const char *title);

void drawBottomNav();

// ============================================================
// PIN MAP  (display pins match ThingPulse's reference project)
// ============================================================

namespace Pins
{
    constexpr int TFT_SCLK = 5;
    constexpr int TFT_MOSI = 18;
    constexpr int TFT_MISO = 19;
    constexpr int TFT_CS   = 15;
    constexpr int TFT_DC   = 2;
    constexpr int TFT_RST  = 4;
    constexpr int TFT_BL   = 32;   // backlight, driven via digitalWrite in setup()

    // FT6236 capacitive touch (I2C). Verify against ThingPulse settings.h
    // if the boot log says "touch controller NOT found".
    constexpr int TOUCH_SDA = 23;
    constexpr int TOUCH_SCL = 22;
}

constexpr uint8_t TOUCH_ADDR = 0x38;

// Touch orientation tweaks - flip if taps land in the wrong place.
constexpr bool TOUCH_SWAP_XY  = false;
constexpr bool TOUCH_INVERT_X = false;
constexpr bool TOUCH_INVERT_Y = false;
constexpr bool TOUCH_DEBUG    = true;   // log raw press coordinates

// ============================================================
// DISPLAY
// ============================================================

class LGFX : public lgfx::LGFX_Device
{
    lgfx::Panel_ILI9488 _panel;
    lgfx::Bus_SPI       _bus;

public:
    LGFX()
    {
        {
            auto cfg = _bus.config();

            cfg.spi_host    = VSPI_HOST;
            cfg.spi_mode    = 0;
            cfg.freq_write  = 20000000;   // 20 MHz - verified working with DisplayTest
            cfg.freq_read   = 16000000;
            cfg.spi_3wire   = false;
            cfg.use_lock    = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;

            cfg.pin_sclk = Pins::TFT_SCLK;
            cfg.pin_mosi = Pins::TFT_MOSI;
            cfg.pin_miso = Pins::TFT_MISO;
            cfg.pin_dc   = Pins::TFT_DC;

            _bus.config(cfg);
            _panel.setBus(&_bus);
        }

        {
            auto cfg = _panel.config();

            cfg.pin_cs   = Pins::TFT_CS;
            cfg.pin_rst  = Pins::TFT_RST;
            cfg.pin_busy = -1;

            cfg.memory_width  = 320;
            cfg.memory_height = 480;
            cfg.panel_width   = 320;
            cfg.panel_height  = 480;
            cfg.offset_x = 0;
            cfg.offset_y = 0;
            cfg.offset_rotation = 0;

            cfg.dummy_read_pixel = 8;
            cfg.dummy_read_bits  = 1;

            cfg.readable   = false;   // no need to read pixels back
            cfg.invert     = false;   // try true if colors look inverted
            cfg.rgb_order  = false;   // try true if red/blue are swapped
            cfg.dlen_16bit = false;
            cfg.bus_shared = false;   // SPI is display-only; touch is I2C

            _panel.config(cfg);
        }

        setPanel(&_panel);
    }
};

LGFX lcd;

constexpr int SCREEN_W = 320;
constexpr int SCREEN_H = 480;

// ============================================================
// COLORS (RGB565)
// ============================================================

constexpr uint16_t COLOR_BG     = 0x0000;
constexpr uint16_t COLOR_PANEL  = 0x1082;
constexpr uint16_t COLOR_ACTIVE = 0x041F;
constexpr uint16_t COLOR_TEXT   = 0xFFFF;
constexpr uint16_t COLOR_DIM    = 0x8410;
constexpr uint16_t COLOR_GREEN  = 0x07E0;
constexpr uint16_t COLOR_YELLOW = 0xFFE0;
constexpr uint16_t COLOR_RED    = 0xF800;
constexpr uint16_t COLOR_CYAN   = 0x07FF;
constexpr uint16_t COLOR_ORANGE = 0xFD20;

// ============================================================
// LAYOUT
// ============================================================

constexpr int HEADER_H = 48;
constexpr int STATUS_Y = 48;
constexpr int LIST_TOP = 68;
constexpr int ROW_PITCH = 70;
constexpr int ROW_H = 64;
constexpr int ROWS_PER_PAGE = 5;
constexpr int NAV_Y = 430;
constexpr int NAV_H = 50;
constexpr int TAB_COUNT = 6;

constexpr int SWIPE_PX = 40;
constexpr uint32_t SCAN_INTERVAL_MS = 20000;
constexpr uint32_t SCAN_TIMEOUT_MS  = 15000;

// ============================================================
// DATA MODEL
// ============================================================

constexpr int MAX_WIFI_NETWORKS = 50;
constexpr int MAX_BLE_DEVICES = 50;
constexpr uint32_t BLE_SCAN_INTERVAL_MS = 15000;
constexpr uint32_t BLE_TRACK_SCAN_INTERVAL_MS = 1000; // short idle between scans while tracking
constexpr uint32_t CLASSIC_TRACK_SCAN_INTERVAL_MS = 1000; // delay after each Classic inquiry completes
constexpr uint32_t BLE_SCAN_DURATION_SECONDS = 5;
constexpr uint32_t BT_CLASSIC_SCAN_INTERVAL_MS = 30000;
constexpr uint8_t BT_CLASSIC_INQUIRY_LENGTH = 10; // 10 x 1.28 seconds, approx. 12.8 seconds
constexpr int MAX_CLASSIC_DEVICES = 50;

struct BLEDeviceInfo
{
    String address;
    String name;
    int32_t rssi = -100;
    uint32_t firstSeen = 0;
    uint32_t lastSeen = 0;
    uint32_t sightings = 0;
    bool seenThisScan = false;
};

BLEDeviceInfo bleDevices[MAX_BLE_DEVICES];
int bleDeviceCount = 0;
int bleScrollOffset = 0;
BLEScan *bleScanner = nullptr;
volatile bool bleScanRunning = false;
uint32_t lastBLEScanDone = 0;
bool bleInitialised = false;

struct ClassicDeviceInfo
{
    String address;
    String name;
    int32_t rssi = -100;
    uint32_t firstSeen = 0;
    uint32_t lastSeen = 0;
    uint32_t sightings = 0;
    bool seenThisScan = false;
    bool hasRSSI = false; // Classic inquiry RSSI is optional on some devices.
};

ClassicDeviceInfo classicDevices[MAX_CLASSIC_DEVICES];
int classicDeviceCount = 0;
int classicScrollOffset = 0;
volatile bool classicScanRunning = false;
bool classicInitialised = false;
uint32_t lastClassicScanDone = 0;

struct NetworkInfo
{
    String ssid;
    String bssid;

    int32_t rssi = -100;
    int32_t previousRSSI = -100;

    int32_t channel = 0;
    wifi_auth_mode_t encryption = WIFI_AUTH_OPEN;

    bool hidden = false;
    bool duplicateSSID = false;

    uint32_t firstSeen = 0;
    uint32_t lastSeen = 0;

    uint32_t sightings = 0;

    // Signal history for this AP.
    int32_t minRSSI = -100;
    int32_t maxRSSI = -100;
    int32_t avgRSSI = -100;
    int64_t rssiTotal = 0;

    // Lifecycle state.
    bool seenThisScan = false;
    bool currentlyVisible = false;
    bool isNew = false;
    bool returned = false;
};

NetworkInfo networks[MAX_WIFI_NETWORKS];
int networkCount = 0;

// Historical inventory is separate from the current visible list.
// An AP remains here after disappearing from a scan.
NetworkInfo inventory[MAX_WIFI_NETWORKS];
int inventoryCount = 0;

// The unified view references the source arrays rather than copying Strings.
// This keeps heap use lower on the ESP32 and allows up to 150 tracked records.
enum UnifiedKind : uint8_t { UNIFIED_WIFI, UNIFIED_BLE, UNIFIED_CLASSIC };
struct UnifiedEntry
{
    UnifiedKind kind;
    uint8_t index;
    uint32_t lastSeen;
};
constexpr int MAX_UNIFIED_ENTRIES = MAX_WIFI_NETWORKS + MAX_BLE_DEVICES + MAX_CLASSIC_DEVICES;
UnifiedEntry unifiedEntries[MAX_UNIFIED_ENTRIES];
int unifiedEntryCount = 0;
int unifiedScrollOffset = 0;

int newCount = 0;
int missingCount = 0;
int returnedCount = 0;

// ============================================================
// UI STATE
// ============================================================

enum Screen
{
    SCR_WIFI,
    SCR_DETAILS,
    SCR_STATS,
    SCR_BLE,
    SCR_BLE_TRACK,
    SCR_BT,
    SCR_RF,
    SCR_PRIVACY
};

struct Tab
{
    const char *label;
    Screen screen;
};

const Tab TABS[TAB_COUNT] = {
    {"WIFI", SCR_WIFI},
    {"RF",   SCR_STATS},
    {"BLE",  SCR_BLE},
    {"BT",   SCR_BT},
    {"ALL",  SCR_RF},
    {"PRIV", SCR_PRIVACY},
};

Screen currentScreen = SCR_WIFI;
String selectedBssid;       // selection survives rescans / re-sorting
String selectedBleAddress;  // stable BLE tracking target across RSSI sorting
String selectedClassicAddress; // stable Classic target across RSSI sorting
enum TrackProtocol { TRACK_BLE, TRACK_CLASSIC };
TrackProtocol selectedTrackProtocol = TRACK_BLE;
constexpr int BLE_RSSI_HISTORY = 30;
int16_t bleRssiHistory[BLE_RSSI_HISTORY];
int bleRssiHistoryCount = 0;
int bleRssiHistoryHead = 0;
uint32_t lastTrackSampleMs = 0;
int scrollOffset = 0;
volatile bool needRedraw = true;

// GAP/BLE callbacks run on Bluetooth task context while the UI runs in loop().
// Protect shared device arrays and RSSI history so String objects are not read
// while callbacks are reordering or updating them.
SemaphoreHandle_t radioDataMutex = nullptr;

class RadioDataGuard
{
public:
    RadioDataGuard()
    {
        if (radioDataMutex != nullptr)
        {
            xSemaphoreTake(radioDataMutex, portMAX_DELAY);
            locked = true;
        }
    }
    ~RadioDataGuard()
    {
        if (locked)
            xSemaphoreGive(radioDataMutex);
    }
    RadioDataGuard(const RadioDataGuard &) = delete;
    RadioDataGuard &operator=(const RadioDataGuard &) = delete;
private:
    bool locked = false;
};

bool bleScanRequested = false;
bool classicScanRequested = false;

// ============================================================
// PRIVACY
//   OFF          : everything visible
//   IDENTIFIERS  : BSSIDs masked
//   RECORDING    : SSIDs + BSSIDs masked (safe for screen capture)
//   MAX          : SSIDs, BSSIDs and signal levels masked
// ============================================================

enum PrivacyMode
{
    PRIVACY_OFF,
    PRIVACY_IDENTIFIERS,
    PRIVACY_RECORDING,
    PRIVACY_MAX
};

PrivacyMode privacyMode = PRIVACY_OFF;

// ============================================================
// STABLE PRIVACY IDENTIFIERS
// ============================================================

struct PrivacyIdentity
{
    String bssid;
    uint16_t number = 0;
};

constexpr int MAX_PRIVACY_IDENTITIES = MAX_UNIFIED_ENTRIES;

PrivacyIdentity privacyIdentities[MAX_PRIVACY_IDENTITIES];
int privacyIdentityCount = 0;

uint16_t getPrivacyNumber(const String &bssid)
{
    if (bssid.length() == 0)
        return 0;

    // Existing mapping
    for (int i = 0; i < privacyIdentityCount; i++)
    {
        if (privacyIdentities[i].bssid == bssid)
            return privacyIdentities[i].number;
    }

    // Create a new mapping
    if (privacyIdentityCount < MAX_PRIVACY_IDENTITIES)
    {
        uint16_t number = privacyIdentityCount + 1;

        privacyIdentities[privacyIdentityCount].bssid = bssid;
        privacyIdentities[privacyIdentityCount].number = number;

        privacyIdentityCount++;

        return number;
    }

    return 0;
}

String shownSSID(int i)
{
    if (networks[i].hidden)
        return "<hidden>";

    if (privacyMode >= PRIVACY_RECORDING)
    {
        uint16_t id = getPrivacyNumber(networks[i].bssid);

        if (id > 0)
            return "WiFi-" + String(id);

        return "WiFi";
    }

    return networks[i].ssid;
}

String shownBSSID(int i)
{
    if (privacyMode >= PRIVACY_IDENTIFIERS)
    {
        uint16_t id = getPrivacyNumber(networks[i].bssid);

        if (id > 0)
            return "AP-" + String(id);

        return "AP";
    }

    return networks[i].bssid;
}

String shownRSSI(int i)
{
    if (privacyMode >= PRIVACY_MAX)
        return "--";

    return String(networks[i].rssi) + " dBm";
}

String networkStatus(int i)
{
    if (networks[i].isNew)
        return "NEW";

    if (networks[i].returned)
        return "RETURNED";

    return "ACTIVE";
}

String shownBSSIDFromValue(const String &bssid)
{
    if (privacyMode >= PRIVACY_IDENTIFIERS)
    {
        uint16_t id = getPrivacyNumber(bssid);

        if (id > 0)
            return "AP-" + String(id);

        return "AP";
    }

    return bssid;
}

String fit(const String &s, size_t maxLen)
{
    if (s.length() <= maxLen)
        return s;

    return s.substring(0, maxLen - 2) + "..";
}

// ============================================================
// TOUCH (FT6236)
// ============================================================

bool readTouch(int &x, int &y)
{
    Wire.beginTransmission(TOUCH_ADDR);
    Wire.write(0x02);

    if (Wire.endTransmission(false) != 0)
        return false;

    if (Wire.requestFrom((uint8_t)TOUCH_ADDR, (size_t)5, true) != 5)
        return false;

    uint8_t touches = Wire.read() & 0x0F;
    uint8_t xh = Wire.read();
    uint8_t xl = Wire.read();
    uint8_t yh = Wire.read();
    uint8_t yl = Wire.read();

    if (touches == 0 || touches > 2)
        return false;

    int rx = ((xh & 0x0F) << 8) | xl;
    int ry = ((yh & 0x0F) << 8) | yl;

    if (TOUCH_SWAP_XY)
        std::swap(rx, ry);
    if (TOUCH_INVERT_X)
        rx = SCREEN_W - 1 - rx;
    if (TOUCH_INVERT_Y)
        ry = SCREEN_H - 1 - ry;

    x = constrain(rx, 0, SCREEN_W - 1);
    y = constrain(ry, 0, SCREEN_H - 1);

    return true;
}

// ============================================================
// WIFI SCANNING (asynchronous - UI never blocks)
// ============================================================

bool scanRunning = false;
uint32_t scanStarted = 0;
uint32_t lastScanDone = 0;

void drawStatusLine();

void startScan()
{
    if (scanRunning)
        return;

    WiFi.scanDelete();
    // async, show hidden, active scan, 300 ms per channel
    WiFi.scanNetworks(true, true, false, 300);

    scanRunning = true;
    scanStarted = millis();

    if (currentScreen == SCR_WIFI)
        drawStatusLine();
}

void collectScan(int found)
{
    if (found > MAX_WIFI_NETWORKS)
        found = MAX_WIFI_NETWORKS;

    const uint32_t now = millis();

    // Mark the entire historical inventory as missing until
    // the current scan proves that an AP is visible again.
    for (int i = 0; i < inventoryCount; i++)
    {
        inventory[i].seenThisScan = false;
        inventory[i].currentlyVisible = false;
        inventory[i].isNew = false;
        inventory[i].returned = false;
    }

    newCount = 0;
    returnedCount = 0;

    // Merge the current scan into the historical inventory.
    // BSSID is the stable AP identity.
    for (int i = 0; i < found; i++)
    {
        String bssid = WiFi.BSSIDstr(i);
        int match = -1;

        for (int j = 0; j < inventoryCount; j++)
        {
            if (inventory[j].bssid == bssid)
            {
                match = j;
                break;
            }
        }

        if (match >= 0)
        {
            NetworkInfo &n = inventory[match];

            bool wasVisible = n.currentlyVisible;

            n.previousRSSI = n.rssi;
            n.rssi = WiFi.RSSI(i);

            n.ssid       = WiFi.SSID(i);
            n.channel    = WiFi.channel(i);
            n.encryption = WiFi.encryptionType(i);
            n.hidden     = n.ssid.length() == 0;

            n.lastSeen = now;
            n.sightings++;

            n.minRSSI = min(n.minRSSI, n.rssi);
            n.maxRSSI = max(n.maxRSSI, n.rssi);
            n.rssiTotal += n.rssi;
            n.avgRSSI = (int32_t)(n.rssiTotal / (int64_t)n.sightings);

            n.seenThisScan = true;
            n.currentlyVisible = true;
            n.isNew = false;
            n.returned = !wasVisible;

            if (n.returned)
                returnedCount++;
        }
        else
        {
            if (inventoryCount >= MAX_WIFI_NETWORKS)
                continue;

            NetworkInfo &n = inventory[inventoryCount];

            n.ssid = WiFi.SSID(i);
            n.bssid = bssid;
            n.rssi = WiFi.RSSI(i);
            n.previousRSSI = n.rssi;

            n.channel = WiFi.channel(i);
            n.encryption = WiFi.encryptionType(i);
            n.hidden = n.ssid.length() == 0;
            n.duplicateSSID = false;

            n.firstSeen = now;
            n.lastSeen = now;
            n.sightings = 1;

            n.minRSSI = n.rssi;
            n.maxRSSI = n.rssi;
            n.avgRSSI = n.rssi;
            n.rssiTotal = n.rssi;

            n.seenThisScan = true;
            n.currentlyVisible = true;
            n.isNew = true;
            n.returned = false;

            inventoryCount++;
            newCount++;
        }
    }

    // Count APs that were previously tracked but are absent
    // from this completed scan.
    missingCount = 0;

    for (int i = 0; i < inventoryCount; i++)
    {
        if (!inventory[i].currentlyVisible)
            missingCount++;
    }

    // Build the current visible list from the historical inventory.
    networkCount = 0;

    for (int i = 0; i < inventoryCount; i++)
    {
        if (!inventory[i].currentlyVisible)
            continue;

        if (networkCount >= MAX_WIFI_NETWORKS)
            break;

        networks[networkCount] = inventory[i];
        networkCount++;
    }

    // Duplicate SSID detection applies only to currently visible APs.
    for (int i = 0; i < networkCount; i++)
        networks[i].duplicateSSID = false;

    for (int i = 0; i < networkCount; i++)
    {
        if (networks[i].hidden)
            continue;

        for (int j = i + 1; j < networkCount; j++)
        {
            if (!networks[j].hidden &&
                networks[i].ssid == networks[j].ssid)
            {
                networks[i].duplicateSSID = true;
                networks[j].duplicateSSID = true;
            }
        }
    }

    // Push duplicate state back into historical inventory.
    for (int i = 0; i < networkCount; i++)
    {
        for (int j = 0; j < inventoryCount; j++)
        {
            if (inventory[j].bssid == networks[i].bssid)
            {
                inventory[j].duplicateSSID = networks[i].duplicateSSID;
                break;
            }
        }
    }

    // Sort only the visible list. Historical inventory order remains stable.
    std::sort(networks, networks + networkCount,
              [](const NetworkInfo &a, const NetworkInfo &b)
              {
                  return a.rssi > b.rssi;
              });

    // Register stable privacy identities.
    for (int i = 0; i < networkCount; i++)
        getPrivacyNumber(networks[i].bssid);

    // Keep the list aligned to full pages of five rows. The final page
    // may contain fewer than five networks, but still starts on a page edge.
    int maxOffset = (networkCount > 0)
                        ? ((networkCount - 1) / ROWS_PER_PAGE) * ROWS_PER_PAGE
                        : 0;

    scrollOffset = constrain(scrollOffset, 0, maxOffset);
}

void pollScan()
{
    if (!scanRunning)
        return;

    int n = WiFi.scanComplete();

    if (n == WIFI_SCAN_RUNNING)
    {
        if (millis() - scanStarted > SCAN_TIMEOUT_MS)
        {
            Serial.println("Wi-Fi scan timed out, resetting.");
            WiFi.scanDelete();
            scanRunning = false;
            lastScanDone = millis();
        }
        return;
    }

    scanRunning = false;
    lastScanDone = millis();

    if (n < 0)
    {
        Serial.println("Wi-Fi scan failed.");
        needRedraw = true;
        return;
    }

    collectScan(n);
    WiFi.scanDelete();

    // Counts only - identifiers are never logged (Recording Mode safe)
    Serial.printf("Scan complete: %d active, %d tracked, %d new, %d missing, %d returned\n",
                  networkCount,
                  inventoryCount,
                  newCount,
                  missingCount,
                  returnedCount);

    needRedraw = true;
}

void maybeAutoScan()
{
    if (scanRunning)
        return;

    if (currentScreen != SCR_WIFI && currentScreen != SCR_STATS)
        return;

    if (millis() - lastScanDone > SCAN_INTERVAL_MS)
        startScan();
}

// ============================================================
// PASSIVE BLE DISCOVERY
// ============================================================

class PassiveBLECallbacks : public BLEAdvertisedDeviceCallbacks
{
    void onResult(BLEAdvertisedDevice advertisedDevice) override
    {
        String address = advertisedDevice.getAddress().toString().c_str();
        String name = advertisedDevice.haveName()
                          ? String(advertisedDevice.getName().c_str())
                          : String("<unnamed>");
        int rssi = advertisedDevice.getRSSI();
        uint32_t now = millis();
        RadioDataGuard dataGuard;
        int match = -1;

        for (int i = 0; i < bleDeviceCount; i++)
        {
            if (bleDevices[i].address == address)
            {
                match = i;
                break;
            }
        }

        if (match < 0)
        {
            if (bleDeviceCount >= MAX_BLE_DEVICES)
                return;

            match = bleDeviceCount++;
            bleDevices[match].address = address;
            bleDevices[match].name = name;
            bleDevices[match].firstSeen = now;
            bleDevices[match].sightings = 0;
        }

        BLEDeviceInfo &device = bleDevices[match];
        if (name != "<unnamed>" && device.name != name)
            device.name = name;
        device.rssi = rssi;
        device.lastSeen = now;
        device.sightings++;
        device.seenThisScan = true;

        // Keep a one-sample-per-second RSSI history for the selected target.
        // RSSI is noisy; the trend is more useful than treating one reading as distance.
        if (selectedTrackProtocol == TRACK_BLE && currentScreen == SCR_BLE_TRACK &&
            selectedBleAddress.length() && address == selectedBleAddress &&
            rssi > -127 && rssi < 20 &&
            (lastTrackSampleMs == 0 || now - lastTrackSampleMs >= 1000))
        {
            bleRssiHistory[bleRssiHistoryHead] = (int16_t)rssi;
            bleRssiHistoryHead = (bleRssiHistoryHead + 1) % BLE_RSSI_HISTORY;
            if (bleRssiHistoryCount < BLE_RSSI_HISTORY) bleRssiHistoryCount++;
            lastTrackSampleMs = now;
            needRedraw = true;
        }

    }
};

void onBLEScanComplete(BLEScanResults results)
{
    (void)results;
    RadioDataGuard dataGuard;
    bleScanRunning = false;
    lastBLEScanDone = millis();
    // Sort once per scan, rather than on every advertisement. This avoids
    // repeatedly swapping String objects while the radio callback is busy.
    std::sort(bleDevices, bleDevices + bleDeviceCount,
              [](const BLEDeviceInfo &a, const BLEDeviceInfo &b)
              {
                  return a.rssi > b.rssi;
              });
    int maxOffset = (bleDeviceCount > 0)
                        ? ((bleDeviceCount - 1) / ROWS_PER_PAGE) * ROWS_PER_PAGE
                        : 0;
    bleScrollOffset = constrain(bleScrollOffset, 0, maxOffset);
    needRedraw = true;
    Serial.printf("BLE scan complete: %d tracked devices\n", bleDeviceCount);
}

void startBLEScan()
{
    if (!bleInitialised || !bleScanner)
        return;

    {
        RadioDataGuard dataGuard;
        if (bleScanRunning)
            return;
        if (classicScanRunning)
        {
            bleScanRequested = true;
            return; // Classic inquiry owns the shared controller for now.
        }
        bleScanRequested = false;
        for (int i = 0; i < bleDeviceCount; i++)
            bleDevices[i].seenThisScan = false;
        // Reserve the scan state before releasing the lock so the scheduler
        // cannot launch a second scan while the library call is starting.
        bleScanRunning = true;
    }

    bleScanner->clearResults();
    bool started = bleScanner->start(BLE_SCAN_DURATION_SECONDS,
                                     onBLEScanComplete, false);
    if (!started)
    {
        RadioDataGuard dataGuard;
        bleScanRunning = false;
        lastBLEScanDone = millis();
        Serial.println("BLE scan could not start.");
    }
    needRedraw = true;
}

void initBLE()
{
    if (bleInitialised)
        return;

    BLEDevice::init("");
    bleScanner = BLEDevice::getScan();
    bleScanner->setAdvertisedDeviceCallbacks(new PassiveBLECallbacks(), false);
    bleScanner->setActiveScan(false); // passive only: do not send scan requests
    bleScanner->setInterval(100);
    bleScanner->setWindow(80);
    bleInitialised = true;
    Serial.println("Passive BLE discovery initialised.");
}

void maybeAutoBLEScan()
{
    const bool trackingBLE = (currentScreen == SCR_BLE_TRACK && selectedTrackProtocol == TRACK_BLE);
    if (currentScreen != SCR_BLE && !trackingBLE)
    {
        bleScanRequested = false;
        return;
    }

    bool bleActive, classicActive, requested;
    uint32_t lastDone;
    {
        RadioDataGuard dataGuard;
        bleActive = bleScanRunning;
        classicActive = classicScanRunning;
        requested = bleScanRequested;
        lastDone = lastBLEScanDone;
    }
    if (bleActive || classicActive)
        return;

    uint32_t interval = trackingBLE ? BLE_TRACK_SCAN_INTERVAL_MS : BLE_SCAN_INTERVAL_MS;
    if (requested || lastDone == 0 || millis() - lastDone >= interval)
        startBLEScan();
}

String shownBLEName(int i)
{
    if (privacyMode >= PRIVACY_RECORDING)
    {
        uint16_t id = getPrivacyNumber(bleDevices[i].address);
        return id > 0 ? "BLE-" + String(id) : String("BLE");
    }
    return bleDevices[i].name;
}

String shownBLEAddress(int i)
{
    if (privacyMode >= PRIVACY_IDENTIFIERS)
    {
        uint16_t id = getPrivacyNumber(bleDevices[i].address);
        return id > 0 ? "BLE-" + String(id) : String("BLE");
    }
    return bleDevices[i].address;
}

// ============================================================
// BLE PROXIMITY TRACKING
// ============================================================

int findSelectedBLE()
{
    if (!selectedBleAddress.length()) return -1;
    for (int i = 0; i < bleDeviceCount; i++)
        if (bleDevices[i].address == selectedBleAddress) return i;
    return -1;
}

int findSelectedClassic()
{
    if (!selectedClassicAddress.length()) return -1;
    for (int i = 0; i < classicDeviceCount; i++)
        if (classicDevices[i].address == selectedClassicAddress) return i;
    return -1;
}

int findSelectedTrackTarget()
{
    return selectedTrackProtocol == TRACK_BLE ? findSelectedBLE() : findSelectedClassic();
}

void resetTrackHistory(int rssi)
{
    bleRssiHistoryCount = 0;
    bleRssiHistoryHead = 0;
    lastTrackSampleMs = millis();
    if (rssi > -127 && rssi < 20)
    {
        bleRssiHistory[bleRssiHistoryHead] = (int16_t)rssi;
        bleRssiHistoryHead = (bleRssiHistoryHead + 1) % BLE_RSSI_HISTORY;
        bleRssiHistoryCount = 1;
    }
}

String bleSignalLevel(int rssi)
{
    if (rssi >= -50) return "VERY STRONG";
    if (rssi >= -60) return "STRONG";
    if (rssi >= -70) return "MODERATE";
    if (rssi >= -82) return "WEAK";
    return "VERY WEAK";
}

String bleTrendLabel()
{
    if (bleRssiHistoryCount < 4) return "COLLECTING TREND";
    int olderN = bleRssiHistoryCount / 2;
    int newerN = bleRssiHistoryCount - olderN;
    int olderSum = 0, newerSum = 0;
    int start = (bleRssiHistoryHead - bleRssiHistoryCount + BLE_RSSI_HISTORY) % BLE_RSSI_HISTORY;
    for (int i = 0; i < bleRssiHistoryCount; i++)
    {
        int idx = (start + i) % BLE_RSSI_HISTORY;
        if (i < olderN) olderSum += bleRssiHistory[idx];
        else newerSum += bleRssiHistory[idx];
    }
    float delta = (float)newerSum / newerN - (float)olderSum / olderN;
    if (delta >= 4.0f) return "SIGNAL GETTING STRONGER";
    if (delta <= -4.0f) return "SIGNAL GETTING WEAKER";
    return "SIGNAL FAIRLY STEADY";
}

// Forward declarations: the Classic display helpers are defined with the
// Classic discovery implementation below this shared tracking screen.
String shownClassicName(int i);
String shownClassicAddress(int i);

void drawBLETrackScreen()
{
    const bool trackingBLE = selectedTrackProtocol == TRACK_BLE;
    lcd.fillScreen(COLOR_BG);
    drawHeader(trackingBLE ? "BLE TRACK" : "BT TRACK", true);

    int i = findSelectedTrackTarget();
    if (i < 0)
    {
        setFont2();
        lcd.setTextColor(COLOR_YELLOW, COLOR_BG);
        lcd.setCursor(12, 90);
        lcd.print(trackingBLE ? "Select a device from BLE list." : "Select a device from BT list.");
        lcd.setCursor(12, 118);
        lcd.print("Tap BACK to return.");
        drawBottomNav();
        return;
    }

    const String targetName = trackingBLE ? shownBLEName(i) : shownClassicName(i);
    const String targetAddress = trackingBLE ? shownBLEAddress(i) : shownClassicAddress(i);
    const bool hasTargetRSSI = trackingBLE || classicDevices[i].hasRSSI;
    const int targetRSSI = trackingBLE ? bleDevices[i].rssi
                                      : (classicDevices[i].hasRSSI ? classicDevices[i].rssi : -127);
    const bool seenNow = trackingBLE ? bleDevices[i].seenThisScan : classicDevices[i].seenThisScan;

    setFont2();
    lcd.setTextColor(COLOR_TEXT, COLOR_BG);
    lcd.setCursor(10, 62);
    lcd.print(fit(targetName, 28));
    lcd.setTextColor(COLOR_DIM, COLOR_BG);
    lcd.setCursor(10, 82);
    lcd.print(fit(targetAddress, 28));

    if (privacyMode >= PRIVACY_MAX)
    {
        lcd.setTextColor(COLOR_YELLOW, COLOR_BG);
        lcd.setCursor(10, 125);
        lcd.print("Signal values hidden by MAX privacy");
    }
    else
    {
        lcd.setFont(&lgfx::fonts::Font4);
        lcd.setTextSize(1);
        lcd.setTextColor(hasTargetRSSI ? COLOR_CYAN : COLOR_YELLOW, COLOR_BG);
        lcd.setCursor(10, 112);
        lcd.print(hasTargetRSSI ? String(targetRSSI) + " dBm" : String("RSSI unavailable"));
        if (hasTargetRSSI)
            drawSignalBars(250, 104, targetRSSI);
        setFont2();
        lcd.setTextColor(COLOR_TEXT, COLOR_BG);
        lcd.setCursor(10, 151);
        lcd.print(hasTargetRSSI ? "LEVEL: " + bleSignalLevel(targetRSSI) : "LEVEL: NO RSSI DATA");
        lcd.setTextColor(COLOR_GREEN, COLOR_BG);
        lcd.setCursor(10, 174);
        lcd.print(bleRssiHistoryCount >= 4 ? bleTrendLabel() : "COLLECTING TREND");
    }

    // One history graph is shared by both radio types. Classic samples arrive
    // only when inquiry returns the target, so its graph updates more slowly.
    lcd.drawRoundRect(8, 207, 304, 143, 5, COLOR_DIM);
    setFont2();
    lcd.setTextColor(COLOR_DIM, COLOR_BG);
    lcd.setCursor(16, 212);
    lcd.print(trackingBLE ? "BLE RSSI HISTORY (stronger = taller)" : "CLASSIC RSSI HISTORY");
    lcd.drawFastHLine(18, 325, 284, COLOR_PANEL);
    if (privacyMode < PRIVACY_MAX && bleRssiHistoryCount > 0)
    {
        int start = (bleRssiHistoryHead - bleRssiHistoryCount + BLE_RSSI_HISTORY) % BLE_RSSI_HISTORY;
        int slotW = 278 / BLE_RSSI_HISTORY;
        for (int n = 0; n < bleRssiHistoryCount; n++)
        {
            int idx = (start + n) % BLE_RSSI_HISTORY;
            int rssi = bleRssiHistory[idx];
            int h = constrain((rssi + 100) * 105 / 70, 3, 105);
            uint16_t c = rssi >= -60 ? COLOR_GREEN : (rssi >= -75 ? COLOR_YELLOW : COLOR_ORANGE);
            lcd.fillRect(20 + n * slotW, 324 - h, max(3, slotW - 2), h, c);
        }
    }
    setFont2();
    lcd.setTextColor(COLOR_DIM, COLOR_BG);
    lcd.setCursor(14, 359);
    lcd.print("-100 dBm");
    lcd.setCursor(245, 359);
    lcd.print("-30 dBm");

    lcd.setTextColor(COLOR_CYAN, COLOR_BG);
    lcd.setCursor(10, 383);
    if (trackingBLE)
        lcd.print(seenNow ? "Receiving BLE advertisements" : "Waiting for BLE advertisement");
    else
        lcd.print(seenNow ? "Seen in current Classic inquiry" : "Waiting for next Classic inquiry");
    lcd.setTextColor(COLOR_DIM, COLOR_BG);
    lcd.setCursor(10, 402);
    lcd.print(trackingBLE ? "Samples update as ads arrive" : "Classic RSSI updates per inquiry");
    drawBottomNav();
}

// ============================================================
// BLUETOOTH CLASSIC DISCOVERY (inquiry only; no connections)
// ============================================================

String classicAddressToString(const uint8_t *bda)
{
    char address[18];
    snprintf(address, sizeof(address), "%02X:%02X:%02X:%02X:%02X:%02X",
             bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
    return String(address);
}

void recordClassicDevice(const String &address, const String &name, int rssi)
{
    if (address.length() == 0)
        return;

    uint32_t now = millis();
    int match = -1;
    for (int i = 0; i < classicDeviceCount; i++)
    {
        if (classicDevices[i].address == address)
        {
            match = i;
            break;
        }
    }

    if (match < 0)
    {
        if (classicDeviceCount >= MAX_CLASSIC_DEVICES)
            return;

        match = classicDeviceCount++;
        classicDevices[match].address = address;
        classicDevices[match].name = "<unnamed>";
        classicDevices[match].firstSeen = now;
        classicDevices[match].sightings = 0;
    }

    ClassicDeviceInfo &device = classicDevices[match];
    if (name.length() > 0 && name != "<unnamed>" && device.name != name)
        device.name = name;
    if (rssi > -127 && rssi < 20)
    {
        device.rssi = rssi;
        device.hasRSSI = true;
    }
    device.lastSeen = now;
    device.sightings++;
    device.seenThisScan = true;

    // Classic RSSI is reported during inquiry results. Add a history point for
    // the selected target, throttled so repeated result events do not over-sample.
    if (selectedTrackProtocol == TRACK_CLASSIC && currentScreen == SCR_BLE_TRACK &&
        selectedClassicAddress.length() && address == selectedClassicAddress &&
        rssi > -127 && rssi < 20 &&
        (lastTrackSampleMs == 0 || now - lastTrackSampleMs >= 1000))
    {
        bleRssiHistory[bleRssiHistoryHead] = (int16_t)rssi;
        bleRssiHistoryHead = (bleRssiHistoryHead + 1) % BLE_RSSI_HISTORY;
        if (bleRssiHistoryCount < BLE_RSSI_HISTORY) bleRssiHistoryCount++;
        lastTrackSampleMs = now;
    }

    // Valid RSSI devices first, strongest to weakest. Missing RSSI is not -100 dBm.
    std::sort(classicDevices, classicDevices + classicDeviceCount,
              [](const ClassicDeviceInfo &a, const ClassicDeviceInfo &b)
              {
                  if (a.hasRSSI != b.hasRSSI)
                      return a.hasRSSI;
                  return a.hasRSSI && b.hasRSSI ? a.rssi > b.rssi : a.lastSeen > b.lastSeen;
              });
}

void classicGapCallback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    RadioDataGuard dataGuard;
    if (event == ESP_BT_GAP_DISC_RES_EVT)
    {
        String address = classicAddressToString(param->disc_res.bda);
        String name = "<unnamed>";
        int rssi = -127; // sentinel: RSSI is optional in Classic inquiry properties

        for (int i = 0; i < param->disc_res.num_prop; i++)
        {
            esp_bt_gap_dev_prop_t *prop = param->disc_res.prop + i;
            if (prop->type == ESP_BT_GAP_DEV_PROP_BDNAME && prop->val && prop->len > 0)
            {
                const char *rawName = static_cast<const char *>(prop->val);
                if (rawName[0] != '\0')
                    name = String(rawName).substring(0, 31);
            }
            else if (prop->type == ESP_BT_GAP_DEV_PROP_RSSI && prop->val && prop->len >= sizeof(int8_t))
            {
                rssi = *static_cast<int8_t *>(prop->val);
            }
        }

        recordClassicDevice(address, name, rssi);
        needRedraw = true;
    }
    else if (event == ESP_BT_GAP_DISC_STATE_CHANGED_EVT)
    {
        if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED)
        {
            classicScanRunning = false;
            lastClassicScanDone = millis();
            needRedraw = true;
            Serial.printf("Bluetooth Classic inquiry complete: %d tracked devices\n", classicDeviceCount);
        }
        else if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STARTED)
        {
            classicScanRunning = true;
            needRedraw = true;
        }
    }
}

void initClassicBluetooth()
{
    if (classicInitialised)
        return;

    // BLEDevice::init() already initializes the ESP32 Bluetooth controller
    // and Bluedroid stack. Register the Classic GAP inquiry callback on that stack.
    esp_err_t result = esp_bt_gap_register_callback(classicGapCallback);
    if (result == ESP_OK)
    {
        classicInitialised = true;
        Serial.println("Bluetooth Classic GAP discovery initialised.");
    }
    else
    {
        Serial.printf("Bluetooth Classic GAP init failed: %s\n", esp_err_to_name(result));
    }
}

void startClassicDiscovery()
{
    if (!classicInitialised)
        return;

    bool stopBLE = false;
    {
        RadioDataGuard dataGuard;
        if (classicScanRunning)
            return;
        if (bleScanRunning && bleScanner)
        {
            classicScanRequested = true;
            stopBLE = true;
        }
        else
        {
            classicScanRequested = false;
            for (int i = 0; i < classicDeviceCount; i++)
                classicDevices[i].seenThisScan = false;
            // Reserve the radio while the GAP inquiry start call is made.
            classicScanRunning = true;
        }
    }

    if (stopBLE)
    {
        bleScanner->stop();
        return; // BLE completion callback releases the scan before retry.
    }

    esp_err_t result = esp_bt_gap_start_discovery(
        ESP_BT_INQ_MODE_GENERAL_INQUIRY, BT_CLASSIC_INQUIRY_LENGTH, 0);
    if (result != ESP_OK)
    {
        RadioDataGuard dataGuard;
        classicScanRunning = false;
        lastClassicScanDone = millis();
        Serial.printf("Bluetooth Classic inquiry could not start: %s\n", esp_err_to_name(result));
    }
    else
    {
        Serial.println("Bluetooth Classic inquiry started (discovery only; no connections).");
    }
    needRedraw = true;
}

void maybeAutoClassicScan()
{
    const bool trackingClassic = (currentScreen == SCR_BLE_TRACK && selectedTrackProtocol == TRACK_CLASSIC);
    if (currentScreen != SCR_BT && !trackingClassic)
    {
        classicScanRequested = false;
        return;
    }

    bool bleActive, classicActive, requested;
    uint32_t lastDone;
    {
        RadioDataGuard dataGuard;
        bleActive = bleScanRunning;
        classicActive = classicScanRunning;
        requested = classicScanRequested;
        lastDone = lastClassicScanDone;
    }
    if (classicActive || bleActive)
        return;

    uint32_t interval = trackingClassic ? CLASSIC_TRACK_SCAN_INTERVAL_MS : BT_CLASSIC_SCAN_INTERVAL_MS;
    if (requested || lastDone == 0 || millis() - lastDone >= interval)
        startClassicDiscovery();
}

String shownClassicName(int i)
{
    if (privacyMode >= PRIVACY_RECORDING)
    {
        uint16_t id = getPrivacyNumber(classicDevices[i].address);
        return id > 0 ? "BT-" + String(id) : String("BT");
    }
    return classicDevices[i].name;
}

String shownClassicAddress(int i)
{
    if (privacyMode >= PRIVACY_IDENTIFIERS)
    {
        uint16_t id = getPrivacyNumber(classicDevices[i].address);
        return id > 0 ? "BT-" + String(id) : String("BT");
    }
    return classicDevices[i].address;
}

// ============================================================
// HELPERS
// ============================================================

String securityName(wifi_auth_mode_t mode)
{
    switch (mode)
    {
        case WIFI_AUTH_OPEN:            return "OPEN";
        case WIFI_AUTH_WEP:             return "WEP";
        case WIFI_AUTH_WPA_PSK:         return "WPA";
        case WIFI_AUTH_WPA2_PSK:        return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK:    return "WPA/WPA2";
        case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-ENT";
        case WIFI_AUTH_WPA3_PSK:        return "WPA3";
        case WIFI_AUTH_WPA2_WPA3_PSK:   return "WPA2/WPA3";
        default:                        return "OTHER";
    }
}

bool isWeakSecurity(wifi_auth_mode_t mode)
{
    return mode == WIFI_AUTH_OPEN || mode == WIFI_AUTH_WEP;
}

int findSelected()
{
    for (int i = 0; i < networkCount; i++)
        if (networks[i].bssid == selectedBssid)
            return i;

    return -1;
}

void setFont2()
{
    lcd.setFont(&lgfx::fonts::Font2);
    lcd.setTextSize(1);
}

// ============================================================
// DRAWING PRIMITIVES
// ============================================================

void drawSignalBars(int x, int y, int rssi)
{
    int level = 0;

    if (rssi >= -50)      level = 4;
    else if (rssi >= -60) level = 3;
    else if (rssi >= -70) level = 2;
    else if (rssi >= -80) level = 1;

    uint16_t on = level >= 3 ? COLOR_GREEN
                : level == 2 ? COLOR_YELLOW
                             : COLOR_ORANGE;

    for (int i = 0; i < 4; i++)
    {
        int h = 6 + i * 5;

        lcd.fillRect(x + i * 7, y + 25 - h, 5, h,
                     i < level ? on : COLOR_PANEL);
    }
}

void drawHeader(const char *title, bool showBack)
{
    lcd.fillRect(0, 0, SCREEN_W, HEADER_H, COLOR_PANEL);

    lcd.setFont(&lgfx::fonts::Font4);
    lcd.setTextSize(1);
    lcd.setTextColor(COLOR_TEXT, COLOR_PANEL);
    lcd.setCursor(8, 11);
    lcd.print(title);

    setFont2();

    if (showBack)
    {
        lcd.setTextColor(COLOR_CYAN, COLOR_PANEL);
        lcd.setCursor(246, 15);
        lcd.print("< BACK");
    }
    else if (privacyMode != PRIVACY_OFF)
    {
        lcd.setTextColor(COLOR_YELLOW, COLOR_PANEL);
        lcd.setCursor(250, 15);
        lcd.print("PRIVACY");
    }
}

void drawHeader(const char *title)
{
    drawHeader(title, false);
}

void drawBottomNav()
{
    lcd.fillRect(0, NAV_Y, SCREEN_W, NAV_H, COLOR_PANEL);

    Screen active = (currentScreen == SCR_DETAILS) ? SCR_WIFI
                  : (currentScreen == SCR_BLE_TRACK) ? SCR_BLE
                  : currentScreen;
    int tabW = SCREEN_W / TAB_COUNT;

    setFont2();

    for (int i = 0; i < TAB_COUNT; i++)
    {
        uint16_t bg = (TABS[i].screen == active) ? COLOR_ACTIVE : COLOR_PANEL;

        lcd.fillRect(i * tabW, NAV_Y, tabW, NAV_H, bg);
        lcd.setTextColor(COLOR_TEXT, bg);

        int tw = lcd.textWidth(TABS[i].label);
        lcd.setCursor(i * tabW + (tabW - tw) / 2, NAV_Y + 17);
        lcd.print(TABS[i].label);
    }
}

void drawStatusLine()
{
    lcd.fillRect(0, STATUS_Y, SCREEN_W, 18, COLOR_BG);

    setFont2();
    lcd.setTextColor(COLOR_DIM, COLOR_BG);
    lcd.setCursor(8, STATUS_Y + 1);
    lcd.print(String(networkCount) + " active / " +
              String(inventoryCount) + " tracked");

    if (scanRunning)
    {
        lcd.setTextColor(COLOR_YELLOW, COLOR_BG);
        lcd.print("   scanning...");
    }
    else
    {
        lcd.setTextColor(COLOR_DIM, COLOR_BG);
        lcd.print("   tap to rescan");
    }
}

void drawField(const char *label, const String &value, int y,
               uint16_t color)
{
    setFont2();

    lcd.setTextColor(COLOR_DIM, COLOR_BG);
    lcd.setCursor(10, y);
    lcd.print(label);

    lcd.setTextColor(color, COLOR_BG);
    lcd.setCursor(120, y);
    lcd.print(value);
}

void drawField(const char *label, const String &value, int y)
{
    drawField(label, value, y, COLOR_TEXT);
}

// ============================================================
// UNIFIED CROSS-RADIO INVENTORY
// ============================================================

void buildUnifiedEntries()
{
    unifiedEntryCount = 0;

    for (int i = 0; i < inventoryCount && unifiedEntryCount < MAX_UNIFIED_ENTRIES; i++)
    {
        unifiedEntries[unifiedEntryCount++] = {UNIFIED_WIFI, (uint8_t)i, inventory[i].lastSeen};
    }
    for (int i = 0; i < bleDeviceCount && unifiedEntryCount < MAX_UNIFIED_ENTRIES; i++)
    {
        unifiedEntries[unifiedEntryCount++] = {UNIFIED_BLE, (uint8_t)i, bleDevices[i].lastSeen};
    }
    for (int i = 0; i < classicDeviceCount && unifiedEntryCount < MAX_UNIFIED_ENTRIES; i++)
    {
        unifiedEntries[unifiedEntryCount++] = {UNIFIED_CLASSIC, (uint8_t)i, classicDevices[i].lastSeen};
    }

    // Newest observations first. Age-based comparison remains safe across millis() rollover.
    const uint32_t now = millis();
    std::sort(unifiedEntries, unifiedEntries + unifiedEntryCount,
              [now](const UnifiedEntry &a, const UnifiedEntry &b)
              {
                  return (uint32_t)(now - a.lastSeen) < (uint32_t)(now - b.lastSeen);
              });

    int maxOffset = unifiedEntryCount > 0
                        ? ((unifiedEntryCount - 1) / ROWS_PER_PAGE) * ROWS_PER_PAGE
                        : 0;
    unifiedScrollOffset = constrain(unifiedScrollOffset, 0, maxOffset);
}

String unifiedName(const UnifiedEntry &entry)
{
    if (entry.kind == UNIFIED_WIFI)
    {
        const NetworkInfo &n = inventory[entry.index];
        if (n.hidden) return "<hidden SSID>";
        if (privacyMode >= PRIVACY_RECORDING)
        {
            uint16_t id = getPrivacyNumber(n.bssid);
            return id ? "WiFi-" + String(id) : "WiFi";
        }
        return n.ssid.length() ? n.ssid : String("<hidden SSID>");
    }
    if (entry.kind == UNIFIED_BLE) return shownBLEName(entry.index);
    return shownClassicName(entry.index);
}

String unifiedAddress(const UnifiedEntry &entry)
{
    if (entry.kind == UNIFIED_WIFI) return shownBSSIDFromValue(inventory[entry.index].bssid);
    if (entry.kind == UNIFIED_BLE) return shownBLEAddress(entry.index);
    return shownClassicAddress(entry.index);
}

int unifiedRSSI(const UnifiedEntry &entry)
{
    if (entry.kind == UNIFIED_WIFI) return inventory[entry.index].rssi;
    if (entry.kind == UNIFIED_BLE) return bleDevices[entry.index].rssi;
    return classicDevices[entry.index].hasRSSI ? classicDevices[entry.index].rssi : -127;
}

bool unifiedHasRSSI(const UnifiedEntry &entry)
{
    return entry.kind != UNIFIED_CLASSIC || classicDevices[entry.index].hasRSSI;
}

bool unifiedSeenNow(const UnifiedEntry &entry)
{
    if (entry.kind == UNIFIED_WIFI) return inventory[entry.index].currentlyVisible;
    if (entry.kind == UNIFIED_BLE) return bleDevices[entry.index].seenThisScan;
    return classicDevices[entry.index].seenThisScan;
}

const char *unifiedProtocol(const UnifiedEntry &entry)
{
    if (entry.kind == UNIFIED_WIFI) return "WIFI";
    if (entry.kind == UNIFIED_BLE) return "BLE";
    return "BT CLASSIC";
}

// ============================================================
// SCREENS
// ============================================================

void drawWiFiScreen()
{
    lcd.fillScreen(COLOR_BG);
    drawHeader("WIFI RECON");
    drawStatusLine();

    setFont2();

    for (int r = 0; r < ROWS_PER_PAGE; r++)
    {
        int i = scrollOffset + r;

        if (i >= networkCount)
            break;

        int y = LIST_TOP + r * ROW_PITCH;

        lcd.drawRect(5, y, 310, ROW_H, COLOR_PANEL);

        lcd.setTextColor(COLOR_TEXT, COLOR_BG);
        lcd.setCursor(12, y + 5);
        lcd.print(fit(shownSSID(i), 22));

        lcd.setTextColor(isWeakSecurity(networks[i].encryption)
                             ? COLOR_RED : COLOR_DIM,
                         COLOR_BG);
        lcd.setCursor(12, y + 24);
        lcd.print("CH " + String(networks[i].channel) + "  " +
                  securityName(networks[i].encryption));

        lcd.setTextColor(COLOR_DIM, COLOR_BG);
        lcd.setCursor(12, y + 43);
        lcd.print(shownBSSID(i));

        drawSignalBars(282, y + 8, networks[i].rssi);

        String rssi = shownRSSI(i);
        lcd.setCursor(310 - lcd.textWidth(rssi), y + 43);
        lcd.print(rssi);
    }

    // Scroll indicator
    if (networkCount > ROWS_PER_PAGE)
    {
        int trackH = ROWS_PER_PAGE * ROW_PITCH - 6;
        int thumbH = max(16, trackH * ROWS_PER_PAGE / networkCount);
        int maxOff = ((networkCount - 1) / ROWS_PER_PAGE) * ROWS_PER_PAGE;
        int thumbY = LIST_TOP + (trackH - thumbH) * scrollOffset / max(1, maxOff);

        lcd.fillRect(316, thumbY, 3, thumbH, COLOR_DIM);
    }

    drawBottomNav();
}

void drawDetailsScreen()
{
    lcd.fillScreen(COLOR_BG);
    drawHeader("NETWORK", true);

    int i = findSelected();

    if (i < 0)
    {
        setFont2();
        lcd.setTextColor(COLOR_DIM, COLOR_BG);
        lcd.setCursor(10, 70);
        lcd.print("Network no longer visible");
        drawBottomNav();
        return;
    }

    const NetworkInfo &n = networks[i];

    lcd.setFont(&lgfx::fonts::Font4);
    lcd.setTextSize(1);
    lcd.setTextColor(COLOR_TEXT, COLOR_BG);
    lcd.setCursor(10, 62);
    lcd.print(fit(shownSSID(i), 20));

    int y = 110;

    drawField("BSSID", shownBSSID(i), y);               y += 28;
    drawField("RSSI", shownRSSI(i), y);                 y += 28;
    drawField("CHANNEL",
              String(n.channel) + " (2.4 GHz)",
              y);                                       y += 28;
    drawField("SECURITY", securityName(n.encryption), y,
              isWeakSecurity(n.encryption) ? COLOR_RED : COLOR_TEXT);
                                                        y += 28;
    drawField("SSID REUSE", n.duplicateSSID ? "DUPLICATE" : "UNIQUE", y,
              n.duplicateSSID ? COLOR_YELLOW : COLOR_TEXT);
                                                        y += 28;
    drawField("FIRST SEEN",
              String((millis() - n.firstSeen) / 1000) + " s ago", y);
                                                        y += 28;
    drawField("LAST SEEN",
              String((millis() - n.lastSeen) / 1000) + " s ago", y);
                                                        y += 40;

    setFont2();

    if (n.encryption == WIFI_AUTH_OPEN)
    {
        lcd.setTextColor(COLOR_RED, COLOR_BG);
        lcd.setCursor(10, y);
        lcd.print("WARNING: OPEN NETWORK");
    }
    else if (n.encryption == WIFI_AUTH_WEP)
    {
        lcd.setTextColor(COLOR_RED, COLOR_BG);
        lcd.setCursor(10, y);
        lcd.print("WARNING: WEP IS BROKEN");
    }

    drawBottomNav();
}

void drawStatsScreen()
{
    lcd.fillScreen(COLOR_BG);
    drawHeader("RF ANALYSIS");

    constexpr int CH_COUNT = 13;
    int perChannel[CH_COUNT + 1] = {0};
    int64_t rssiTotal[CH_COUNT + 1] = {0};
    int strongest[CH_COUNT + 1];
    for (int ch = 0; ch <= CH_COUNT; ch++) strongest[ch] = -100;

    int open = 0, hidden = 0, weak = 0, strong = 0;
    int64_t overallRssiTotal = 0;

    for (int i = 0; i < networkCount; i++)
    {
        const int ch = networks[i].channel;
        const int rssi = networks[i].rssi;

        if (ch >= 1 && ch <= CH_COUNT)
        {
            perChannel[ch]++;
            rssiTotal[ch] += rssi;
            if (rssi > strongest[ch]) strongest[ch] = rssi;
        }

        overallRssiTotal += rssi;
        if (rssi >= -60) strong++;
        if (rssi <= -75) weak++;
        if (networks[i].encryption == WIFI_AUTH_OPEN) open++;
        if (networks[i].hidden) hidden++;
    }

    int busiest = 1;
    for (int ch = 1; ch <= CH_COUNT; ch++)
        if (perChannel[ch] > perChannel[busiest]) busiest = ch;

    const int averageRSSI = networkCount > 0
        ? (int)(overallRssiTotal / networkCount) : -100;

    setFont2();
    lcd.setTextColor(COLOR_TEXT, COLOR_BG);
    lcd.setCursor(8, 58);
    lcd.print("APs " + String(networkCount) + "  Open " + String(open) +
              "  Hidden " + String(hidden));

    lcd.setCursor(8, 77);
    if (privacyMode >= PRIVACY_MAX)
    {
        lcd.setTextColor(COLOR_DIM, COLOR_BG);
        lcd.print("RSSI hidden by MAX privacy");
    }
    else
    {
        lcd.setTextColor(COLOR_CYAN, COLOR_BG);
        lcd.print("Avg RSSI " + String(networkCount ? averageRSSI : 0) +
                  " dBm  Strong " + String(strong));
        lcd.setTextColor(COLOR_YELLOW, COLOR_BG);
        lcd.setCursor(8, 95);
        lcd.print("Weak (<= -75 dBm): " + String(weak));
    }

    lcd.setTextColor(COLOR_DIM, COLOR_BG);
    lcd.setCursor(8, 119);
    lcd.print("Observed APs by 2.4 GHz channel");

    const int baseY = 333;
    const int maxH = 165;
    const int barW = 15;
    const int pitch = 23;
    const int x0 = 9;
    int maxCount = max(1, perChannel[busiest]);

    lcd.drawFastHLine(x0 - 2, baseY, pitch * CH_COUNT + 2, COLOR_DIM);

    for (int ch = 1; ch <= CH_COUNT; ch++)
    {
        const int x = x0 + (ch - 1) * pitch;
        const int h = perChannel[ch] * maxH / maxCount;
        const bool commonNonOverlapping = (ch == 1 || ch == 6 || ch == 11);

        if (perChannel[ch] > 0)
        {
            uint16_t barColor = (ch == busiest) ? COLOR_ORANGE
                              : (commonNonOverlapping ? COLOR_GREEN : COLOR_CYAN);
            lcd.fillRect(x, baseY - h, barW, h, barColor);
            lcd.setTextColor(COLOR_TEXT, COLOR_BG);
            lcd.setCursor(x + (perChannel[ch] > 9 ? 0 : 4), baseY - h - 16);
            lcd.print(perChannel[ch]);
        }

        lcd.setTextColor(commonNonOverlapping ? COLOR_GREEN : COLOR_DIM, COLOR_BG);
        lcd.setCursor(x + (ch > 9 ? 0 : 4), baseY + 4);
        lcd.print(ch);

        if (privacyMode >= PRIVACY_MAX)
        {
            lcd.setTextColor(COLOR_DIM, COLOR_BG);
            lcd.setCursor(x - 1, baseY + 20);
            lcd.print("--");
        }
        else if (perChannel[ch] > 0)
        {
            const int avgCh = (int)(rssiTotal[ch] / perChannel[ch]);
            lcd.setTextColor(COLOR_CYAN, COLOR_BG);
            lcd.setCursor(x - 3, baseY + 20);
            lcd.print(String(avgCh));
        }
        else
        {
            lcd.setTextColor(COLOR_DIM, COLOR_BG);
            lcd.setCursor(x - 1, baseY + 20);
            lcd.print("-");
        }
    }

    lcd.setTextColor(COLOR_DIM, COLOR_BG);
    lcd.setCursor(8, 386);
    lcd.print("CH: AP count; below: avg RSSI dBm");
    lcd.setCursor(8, 402);
    lcd.print("Green: 1/6/11  Orange: busiest");
    drawBottomNav();
}

void drawBLEScreen()
{
    lcd.fillScreen(COLOR_BG);
    drawHeader("BLE RECON");

    setFont2();
    lcd.setTextColor(COLOR_DIM, COLOR_BG);
    lcd.setCursor(8, STATUS_Y + 1);
    lcd.print(String(bleDeviceCount) + " tracked / passive scan");

    lcd.setCursor(8, 68);
    if (bleScanRunning)
    {
        lcd.setTextColor(COLOR_YELLOW, COLOR_BG);
        lcd.print("Scanning... tap a row to track");
    }
    else
    {
        lcd.setTextColor(COLOR_CYAN, COLOR_BG);
        lcd.print("Tap row to track / tap here to scan");
    }

    for (int row = 0; row < ROWS_PER_PAGE; row++)
    {
        int i = bleScrollOffset + row;
        int y = 91 + row * ROW_PITCH;
        if (i >= bleDeviceCount)
            break;

        uint16_t bg = (row % 2 == 0) ? COLOR_PANEL : COLOR_BG;
        lcd.fillRoundRect(6, y, 308, ROW_H - 4, 5, bg);

        lcd.setFont(&lgfx::fonts::Font4);
        lcd.setTextSize(1);
        lcd.setTextColor(COLOR_TEXT, bg);
        lcd.setCursor(12, y + 6);
        lcd.print(fit(shownBLEName(i), 22));

        setFont2();
        lcd.setTextColor(COLOR_DIM, bg);
        lcd.setCursor(12, y + 29);
        lcd.print(fit(shownBLEAddress(i), 20));

        lcd.setTextColor(bleDevices[i].seenThisScan ? COLOR_GREEN : COLOR_DIM, bg);
        lcd.setCursor(12, y + 46);
        if (bleDevices[i].seenThisScan)
            lcd.print("SEEN NOW");
        else
            lcd.print(String((millis() - bleDevices[i].lastSeen) / 1000) + "s ago");

        String rssi = privacyMode >= PRIVACY_MAX
                          ? String("--")
                          : String(bleDevices[i].rssi) + " dBm";
        lcd.setTextColor(privacyMode >= PRIVACY_MAX ? COLOR_DIM : COLOR_CYAN, bg);
        lcd.setCursor(310 - lcd.textWidth(rssi), y + 46);
        lcd.print(rssi);
        drawSignalBars(270, y + 10, bleDevices[i].rssi);
    }

    if (bleDeviceCount > ROWS_PER_PAGE)
    {
        int trackH = ROWS_PER_PAGE * ROW_PITCH - 6;
        int thumbH = max(16, trackH * ROWS_PER_PAGE / bleDeviceCount);
        int maxOff = ((bleDeviceCount - 1) / ROWS_PER_PAGE) * ROWS_PER_PAGE;
        int thumbY = 91 + (trackH - thumbH) * bleScrollOffset / max(1, maxOff);
        lcd.fillRect(316, thumbY, 3, thumbH, COLOR_DIM);
    }

    drawBottomNav();
}

void drawClassicScreen()
{
    lcd.fillScreen(COLOR_BG);
    drawHeader("BT CLASSIC");

    setFont2();
    lcd.setTextColor(COLOR_DIM, COLOR_BG);
    lcd.setCursor(8, STATUS_Y + 1);
    lcd.print(String(classicDeviceCount) + " tracked / inquiry scan");

    lcd.setCursor(8, 68);
    if (classicScanRunning)
    {
        lcd.setTextColor(COLOR_YELLOW, COLOR_BG);
        lcd.print("Searching Classic devices...");
    }
    else
    {
        lcd.setTextColor(COLOR_CYAN, COLOR_BG);
        lcd.print("Tap row to track / tap here to scan");
    }

    if (!classicInitialised)
    {
        lcd.setTextColor(COLOR_RED, COLOR_BG);
        lcd.setCursor(8, 95);
        lcd.print("Classic Bluetooth unavailable");
    }
    else if (classicDeviceCount == 0 && !classicScanRunning)
    {
        lcd.setTextColor(COLOR_DIM, COLOR_BG);
        lcd.setCursor(8, 105);
        lcd.print("No devices found yet");
        lcd.setCursor(8, 126);
        lcd.print("Some devices hide their name");
    }

    for (int row = 0; row < ROWS_PER_PAGE; row++)
    {
        int i = classicScrollOffset + row;
        int y = 91 + row * ROW_PITCH;
        if (i >= classicDeviceCount)
            break;

        uint16_t bg = (row % 2 == 0) ? COLOR_PANEL : COLOR_BG;
        lcd.fillRoundRect(6, y, 308, ROW_H - 4, 5, bg);

        lcd.setFont(&lgfx::fonts::Font4);
        lcd.setTextSize(1);
        lcd.setTextColor(COLOR_TEXT, bg);
        lcd.setCursor(12, y + 6);
        lcd.print(fit(shownClassicName(i), 22));

        setFont2();
        lcd.setTextColor(COLOR_DIM, bg);
        lcd.setCursor(12, y + 29);
        lcd.print(fit(shownClassicAddress(i), 20));

        lcd.setTextColor(classicDevices[i].seenThisScan ? COLOR_GREEN : COLOR_DIM, bg);
        lcd.setCursor(12, y + 46);
        if (classicDevices[i].seenThisScan)
            lcd.print("SEEN NOW");
        else
            lcd.print(String((millis() - classicDevices[i].lastSeen) / 1000) + "s ago");

        String rssi = (privacyMode >= PRIVACY_MAX)
                          ? String("--")
                          : (classicDevices[i].hasRSSI ? String(classicDevices[i].rssi) + " dBm" : String("n/a"));
        lcd.setTextColor((privacyMode >= PRIVACY_MAX || !classicDevices[i].hasRSSI) ? COLOR_DIM : COLOR_CYAN, bg);
        lcd.setCursor(310 - lcd.textWidth(rssi), y + 46);
        lcd.print(rssi);
        if (classicDevices[i].hasRSSI)
            drawSignalBars(270, y + 10, classicDevices[i].rssi);
    }

    if (classicDeviceCount > ROWS_PER_PAGE)
    {
        int trackH = ROWS_PER_PAGE * ROW_PITCH - 6;
        int thumbH = max(16, trackH * ROWS_PER_PAGE / classicDeviceCount);
        int maxOff = ((classicDeviceCount - 1) / ROWS_PER_PAGE) * ROWS_PER_PAGE;
        int thumbY = 91 + (trackH - thumbH) * classicScrollOffset / max(1, maxOff);
        lcd.fillRect(316, thumbY, 3, thumbH, COLOR_DIM);
    }

    drawBottomNav();
}

void drawUnifiedScreen()
{
    buildUnifiedEntries();
    lcd.fillScreen(COLOR_BG);
    drawHeader("ALL DEVICES");

    setFont2();
    lcd.setTextColor(COLOR_DIM, COLOR_BG);
    lcd.setCursor(8, STATUS_Y + 1);
    lcd.print("Tracked " + String(unifiedEntryCount) +
              "  WiFi " + String(inventoryCount) +
              " BLE " + String(bleDeviceCount) +
              " BT " + String(classicDeviceCount));

    lcd.setCursor(8, 68);
    lcd.setTextColor(COLOR_CYAN, COLOR_BG);
    lcd.print("All radios / newest observations first");

    if (unifiedEntryCount == 0)
    {
        lcd.setTextColor(COLOR_DIM, COLOR_BG);
        lcd.setCursor(12, 115);
        lcd.print("No devices tracked yet");
        lcd.setCursor(12, 139);
        lcd.print("Visit WiFi, BLE, and BT tabs");
    }

    for (int row = 0; row < ROWS_PER_PAGE; row++)
    {
        int pos = unifiedScrollOffset + row;
        if (pos >= unifiedEntryCount) break;

        const UnifiedEntry &entry = unifiedEntries[pos];
        int y = 91 + row * ROW_PITCH;
        uint16_t bg = (row % 2 == 0) ? COLOR_PANEL : COLOR_BG;
        int rssiValue = unifiedRSSI(entry);

        lcd.fillRoundRect(6, y, 308, ROW_H - 4, 5, bg);

        lcd.setFont(&lgfx::fonts::Font4);
        lcd.setTextSize(1);
        lcd.setTextColor(COLOR_TEXT, bg);
        lcd.setCursor(12, y + 5);
        lcd.print(fit(unifiedName(entry), 21));

        setFont2();
        lcd.setTextColor(COLOR_DIM, bg);
        lcd.setCursor(12, y + 28);
        lcd.print(String(unifiedProtocol(entry)) + "  " + fit(unifiedAddress(entry), 18));

        lcd.setTextColor(unifiedSeenNow(entry) ? COLOR_GREEN : COLOR_DIM, bg);
        lcd.setCursor(12, y + 46);
        if (unifiedSeenNow(entry))
            lcd.print("SEEN NOW");
        else
            lcd.print(String((millis() - entry.lastSeen) / 1000) + "s ago");

        if (privacyMode >= PRIVACY_MAX)
        {
            lcd.setTextColor(COLOR_DIM, bg);
            lcd.setCursor(270, y + 46);
            lcd.print("-- dBm");
        }
        else if (!unifiedHasRSSI(entry))
        {
            lcd.setTextColor(COLOR_DIM, bg);
            lcd.setCursor(278, y + 46);
            lcd.print("n/a");
        }
        else
        {
            String rssi = String(rssiValue) + " dBm";
            lcd.setTextColor(COLOR_CYAN, bg);
            lcd.setCursor(310 - lcd.textWidth(rssi), y + 46);
            lcd.print(rssi);
            drawSignalBars(270, y + 7, rssiValue);
        }
    }

    if (unifiedEntryCount > ROWS_PER_PAGE)
    {
        int trackH = ROWS_PER_PAGE * ROW_PITCH - 6;
        int thumbH = max(16, trackH * ROWS_PER_PAGE / unifiedEntryCount);
        int maxOff = ((unifiedEntryCount - 1) / ROWS_PER_PAGE) * ROWS_PER_PAGE;
        int thumbY = 91 + (trackH - thumbH) * unifiedScrollOffset / max(1, maxOff);
        lcd.fillRect(316, thumbY, 3, thumbH, COLOR_DIM);
    }

    drawBottomNav();
}

void drawPlaceholder(const char *title, const char *message)
{
    lcd.fillScreen(COLOR_BG);
    drawHeader(title);

    lcd.setFont(&lgfx::fonts::Font4);
    lcd.setTextSize(1);
    lcd.setTextColor(COLOR_DIM, COLOR_BG);

    int tw = lcd.textWidth(message);
    lcd.setCursor((SCREEN_W - tw) / 2, 200);
    lcd.print(message);

    drawBottomNav();
}

void drawPrivacyScreen()
{
    lcd.fillScreen(COLOR_BG);
    drawHeader("PRIVACY");

    static const char *OPTIONS[] = {
        "PRIVACY OFF", "IDENTIFIERS", "RECORDING MODE", "MAX PRIVACY"};

    static const char *DESCRIPTIONS[] = {
        "Everything visible.",
        "BSSIDs are masked.",
        "SSIDs and BSSIDs masked. Safe to screen-record.",
        "SSIDs, BSSIDs and signal levels masked."};

    setFont2();
    lcd.setTextColor(COLOR_DIM, COLOR_BG);
    lcd.setCursor(10, 62);
    lcd.print("Hide sensitive identifiers on screen");

    for (int i = 0; i < 4; i++)
    {
        int y = 100 + i * 60;
        uint16_t bg = (privacyMode == i) ? COLOR_ACTIVE : COLOR_PANEL;

        lcd.fillRect(10, y, 300, 48, bg);

        lcd.setFont(&lgfx::fonts::Font4);
        lcd.setTextSize(1);
        lcd.setTextColor(COLOR_TEXT, bg);
        lcd.setCursor(24, y + 11);
        lcd.print(OPTIONS[i]);
    }

    setFont2();
    lcd.setTextColor(COLOR_YELLOW, COLOR_BG);
    lcd.setCursor(10, 350);
    lcd.print(DESCRIPTIONS[privacyMode]);

    drawBottomNav();
}

void drawScreen()
{
    RadioDataGuard dataGuard;
    switch (currentScreen)
    {
        case SCR_WIFI:    drawWiFiScreen();    break;
        case SCR_DETAILS: drawDetailsScreen(); break;
        case SCR_STATS:   drawStatsScreen();   break;
        case SCR_BLE:       drawBLEScreen(); break;
        case SCR_BLE_TRACK: drawBLETrackScreen(); break;
        case SCR_BT:        drawClassicScreen(); break;
        case SCR_RF:      drawUnifiedScreen(); break;
        case SCR_PRIVACY: drawPrivacyScreen(); break;
    }
}

// ============================================================
// INPUT HANDLING
// ============================================================

void goTo(int s)
{
    Screen previous;
    TrackProtocol protocol;
    {
        RadioDataGuard dataGuard;
        if (currentScreen == (Screen)s)
            return;
        previous = currentScreen;
        protocol = selectedTrackProtocol;
        currentScreen = (Screen)s;
        needRedraw = true;
    }

    // Do not hold the data mutex while stopping/starting Bluetooth: callbacks
    // need that same mutex to report scan completion.
    if ((previous == SCR_BT ||
         (previous == SCR_BLE_TRACK && protocol == TRACK_CLASSIC)) && classicScanRunning)
        esp_bt_gap_cancel_discovery();

    if (currentScreen == SCR_BLE ||
        (currentScreen == SCR_BLE_TRACK && selectedTrackProtocol == TRACK_BLE))
        startBLEScan();
    else if (currentScreen == SCR_BT ||
             (currentScreen == SCR_BLE_TRACK && selectedTrackProtocol == TRACK_CLASSIC))
    {
        initClassicBluetooth();
        startClassicDiscovery();
    }
}

void handleTap(int x, int y)
{
    // Bottom navigation is available everywhere
    if (y >= NAV_Y)
    {
        int tab = constrain(x / (SCREEN_W / TAB_COUNT), 0, TAB_COUNT - 1);
        goTo(TABS[tab].screen);
        return;
    }

    switch (currentScreen)
    {
        case SCR_WIFI:
        {
            if (y >= STATUS_Y && y < LIST_TOP)
            {
                startScan();
                return;
            }

            if (y >= LIST_TOP)
            {
                int r = (y - LIST_TOP) / ROW_PITCH;
                int within = (y - LIST_TOP) % ROW_PITCH;
                int i = scrollOffset + r;

                if (r < ROWS_PER_PAGE && within < ROW_H && i < networkCount)
                {
                    selectedBssid = networks[i].bssid;
                    goTo(SCR_DETAILS);
                }
            }
            break;
        }

        case SCR_BLE:
            if (y >= STATUS_Y && y < 90)
            {
                startBLEScan();
                break;
            }
            if (y >= 91 && y < NAV_Y)
            {
                int row = (y - 91) / ROW_PITCH;
                int within = (y - 91) % ROW_PITCH;
                bool selected = false;
                {
                    RadioDataGuard dataGuard;
                    int index = bleScrollOffset + row;
                    if (row < ROWS_PER_PAGE && within < ROW_H - 4 && index < bleDeviceCount)
                    {
                        selectedTrackProtocol = TRACK_BLE;
                        if (selectedBleAddress != bleDevices[index].address || bleRssiHistoryCount == 0)
                        {
                            selectedBleAddress = bleDevices[index].address;
                            resetTrackHistory(bleDevices[index].rssi);
                        }
                        selected = true;
                    }
                }
                if (selected)
                    goTo(SCR_BLE_TRACK);
            }
            break;

        case SCR_BLE_TRACK:
            if (y < HEADER_H && x > 220)
                goTo(selectedTrackProtocol == TRACK_BLE ? SCR_BLE : SCR_BT);
            break;

        case SCR_BT:
            if (y >= STATUS_Y && y < 90)
            {
                startClassicDiscovery();
                break;
            }
            if (y >= 91 && y < NAV_Y)
            {
                int row = (y - 91) / ROW_PITCH;
                int within = (y - 91) % ROW_PITCH;
                bool selected = false;
                {
                    RadioDataGuard dataGuard;
                    int index = classicScrollOffset + row;
                    if (row < ROWS_PER_PAGE && within < ROW_H - 4 && index < classicDeviceCount)
                    {
                        selectedTrackProtocol = TRACK_CLASSIC;
                        if (selectedClassicAddress != classicDevices[index].address || bleRssiHistoryCount == 0)
                        {
                            selectedClassicAddress = classicDevices[index].address;
                            resetTrackHistory(classicDevices[index].hasRSSI ? classicDevices[index].rssi : -127);
                        }
                        selected = true;
                    }
                }
                if (selected)
                    goTo(SCR_BLE_TRACK);
            }
            break;

        case SCR_DETAILS:
            if (y < HEADER_H && x > 230)
                goTo(SCR_WIFI);
            break;

        case SCR_PRIVACY:
        {
            if (y >= 100 && y < 340)
            {
                int i = (y - 100) / 60;
                int within = (y - 100) % 60;

                if (i < 4 && within < 48)
                {
                    privacyMode = (PrivacyMode)i;
                    needRedraw = true;
                }
            }
            break;
        }

        default:
            break;
    }
}

void handleSwipe(int dy)
{
    if (currentScreen == SCR_BLE_TRACK)
    {
        // Vertical swipes on the tracker reset its history, useful when beginning a new approach.
        if (dy < 0)
        {
            RadioDataGuard dataGuard;
            int i = findSelectedTrackTarget();
            if (i >= 0)
            {
                int rssi = selectedTrackProtocol == TRACK_BLE
                               ? bleDevices[i].rssi
                               : (classicDevices[i].hasRSSI ? classicDevices[i].rssi : -127);
                resetTrackHistory(rssi);
            }
            needRedraw = true;
        }
        return;
    }

    if (currentScreen == SCR_BLE)
    {
        RadioDataGuard dataGuard;
        int maxOffset = (bleDeviceCount > 0)
                            ? ((bleDeviceCount - 1) / ROWS_PER_PAGE) * ROWS_PER_PAGE
                            : 0;
        bleScrollOffset += (dy < 0) ? ROWS_PER_PAGE : -ROWS_PER_PAGE;
        bleScrollOffset = constrain(bleScrollOffset, 0, maxOffset);
        needRedraw = true;
        return;
    }

    if (currentScreen == SCR_BT)
    {
        RadioDataGuard dataGuard;
        int maxOffset = (classicDeviceCount > 0)
                            ? ((classicDeviceCount - 1) / ROWS_PER_PAGE) * ROWS_PER_PAGE
                            : 0;
        classicScrollOffset += (dy < 0) ? ROWS_PER_PAGE : -ROWS_PER_PAGE;
        classicScrollOffset = constrain(classicScrollOffset, 0, maxOffset);
        needRedraw = true;
        return;
    }

    if (currentScreen == SCR_RF)
    {
        buildUnifiedEntries();
        int maxOffset = unifiedEntryCount > 0
                            ? ((unifiedEntryCount - 1) / ROWS_PER_PAGE) * ROWS_PER_PAGE
                            : 0;
        unifiedScrollOffset += (dy < 0) ? ROWS_PER_PAGE : -ROWS_PER_PAGE;
        unifiedScrollOffset = constrain(unifiedScrollOffset, 0, maxOffset);
        needRedraw = true;
        return;
    }

    if (currentScreen != SCR_WIFI)
        return;

    // One completed swipe advances exactly one page (five networks),
    // regardless of swipe distance. Never stop partway through a page.
    scrollOffset += (dy < 0) ? ROWS_PER_PAGE : -ROWS_PER_PAGE;

    int maxOffset = (networkCount > 0)
                        ? ((networkCount - 1) / ROWS_PER_PAGE) * ROWS_PER_PAGE
                        : 0;
    scrollOffset = constrain(scrollOffset, 0, maxOffset);

    needRedraw = true;
}

// Press / release state machine. Releases are confirmed after two
// consecutive empty reads so I2C glitches don't cause phantom taps.
void pollTouch()
{
    static bool down = false;
    static int startX = 0, startY = 0, lastX = 0, lastY = 0;
    static uint8_t misses = 0;

    int x, y;

    if (readTouch(x, y))
    {
        misses = 0;

        if (!down)
        {
            down = true;
            startX = x;
            startY = y;

            if (TOUCH_DEBUG)
                Serial.printf("Touch down: x=%d y=%d\n", x, y);
        }

        lastX = x;
        lastY = y;
        return;
    }

    if (down && ++misses >= 2)
    {
        down = false;
        misses = 0;

        int dx = lastX - startX;
        int dy = lastY - startY;

        if (abs(dy) > SWIPE_PX && abs(dy) > abs(dx))
            handleSwipe(dy);
        else
            handleTap(startX, startY);
    }
}

// ============================================================
// SETUP / LOOP
// ============================================================

void splash(const char *line1, const char *line2)
{
    lcd.fillScreen(COLOR_BG);

    lcd.setFont(&lgfx::fonts::Font4);
    lcd.setTextSize(1);
    lcd.setTextColor(COLOR_GREEN, COLOR_BG);
    lcd.setCursor(20, 180);
    lcd.print("WIRELESS RECON");

    setFont2();
    lcd.setTextColor(COLOR_TEXT, COLOR_BG);
    lcd.setCursor(20, 220);
    lcd.print(line1);

    if (line2)
    {
        lcd.setTextColor(COLOR_DIM, COLOR_BG);
        lcd.setCursor(20, 242);
        lcd.print(line2);
    }
}

void splash(const char *line1)
{
    splash(line1, nullptr);
}

void setup()
{
    Serial.begin(115200);
    delay(300);

    Serial.println();
    Serial.println("================================");
    Serial.println(" Wireless Recon Console - Phase 7.2 Stability Update");
    Serial.println("================================");

    // Display
    // Backlight: plain GPIO, active-high (verified working in DisplayTest)
    pinMode(Pins::TFT_BL, OUTPUT);
    digitalWrite(Pins::TFT_BL, HIGH);

    lcd.init();
    lcd.setRotation(0);
    splash("Display OK", "Starting touch + Wi-Fi...");
    Serial.println("Display initialised.");

    // Touch
    Wire.begin(Pins::TOUCH_SDA, Pins::TOUCH_SCL);
    Wire.setClock(400000);

    Wire.beginTransmission(TOUCH_ADDR);
    bool touchFound = (Wire.endTransmission() == 0);

    Serial.println(touchFound
                       ? "Touch controller found at 0x38."
                       : "Touch controller NOT found - check I2C pins.");

    if (!touchFound)
        splash("Display OK", "Touch controller not found (I2C)");

    // Create callback/UI synchronization before starting either Bluetooth stack.
    radioDataMutex = xSemaphoreCreateMutex();
    if (radioDataMutex == nullptr)
    {
        Serial.println("ERROR: could not create radio data mutex.");
        splash("Display OK", "Radio lock init failed");
        while (true) delay(1000);
    }

    // Wi-Fi: passive scanning only, never associate
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false, false);
    delay(100);

    initBLE();
    initClassicBluetooth();
    startScan();
    drawScreen();

    Serial.println("Ready.");
}

void loop()
{
    pollTouch();
    pollScan();
    maybeAutoScan();
    maybeAutoBLEScan();
    maybeAutoClassicScan();

    if (needRedraw)
    {
        needRedraw = false;
        drawScreen();
    }

    delay(10);
}

#include "core/DeviceInventory.h"
#include <cassert>
using namespace wirelessrecon::core;
int main() {
    DeviceInventory inventory;
    assert(inventory.beginScan(RadioType::Ble));
    Device d; d.radio = RadioType::Ble; d.address = "AA:BB"; d.name = "sensor"; d.rssi = -55; d.hasRssi = true; d.lastSeenMs = 100;
    assert(inventory.upsert(d));
    inventory.completeScan(RadioType::Ble);
    assert(inventory.size() == 1 && inventory.count(RadioType::Ble) == 1);
    assert(inventory.find(RadioType::Ble, "AA:BB")->currentlyVisible);
    assert(inventory.beginScan(RadioType::Ble));
    inventory.completeScan(RadioType::Ble);
    assert(!inventory.find(RadioType::Ble, "AA:BB")->currentlyVisible);
    assert(inventory.beginScan(RadioType::Ble));
    d.rssi = -42; d.lastSeenMs = 300;
    assert(inventory.upsert(d));
    inventory.completeScan(RadioType::Ble);
    assert(inventory.size() == 1 && inventory.find(RadioType::Ble, "AA:BB")->sightings == 2);
    assert(inventory.find(RadioType::Ble, "AA:BB")->currentlyVisible);
}

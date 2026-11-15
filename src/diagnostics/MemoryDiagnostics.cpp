//
// Created by dedlee on 11/14/26.
//

#include "diagnostics/MemoryDiagnostics.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp32-hal-psram.h>

namespace wirelessrecon::diagnostics {

namespace {

constexpr uint32_t kInternalMemoryCaps =
    MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;

constexpr uint32_t kPsramMemoryCaps =
    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;

void printMemoryPool(
    const char* name,
    uint32_t capabilities
) {
    const size_t freeBytes =
        heap_caps_get_free_size(capabilities);

    const size_t minimumFreeBytes =
        heap_caps_get_minimum_free_size(capabilities);

    const size_t largestBlock =
        heap_caps_get_largest_free_block(capabilities);

    Serial.printf(
        "[MEMORY] %-12s free: %7u bytes | minimum-ever-free: %7u bytes | largest block: %7u bytes\n",
        name,
        static_cast<unsigned>(freeBytes),
        static_cast<unsigned>(minimumFreeBytes),
        static_cast<unsigned>(largestBlock)
    );
}

} // namespace

void MemoryDiagnostics::printSnapshot(const char* label) {
    Serial.println();
    Serial.println(
        "------------------------------------------------------------"
    );

    Serial.printf(
        "[MEMORY] Snapshot: %s\n",
        label != nullptr ? label : "Unnamed"
    );

    Serial.println(
        "------------------------------------------------------------"
    );

    // Aggregate 8-bit heap. This is useful as an overall view,
    // but the internal and PSRAM pools below are the key figures.
    Serial.printf(
        "[MEMORY] Total free 8-bit heap: %u bytes\n",
        static_cast<unsigned>(
            heap_caps_get_free_size(MALLOC_CAP_8BIT)
        )
    );

    printMemoryPool("Internal DRAM", kInternalMemoryCaps);

    if (psramFound()) {
        Serial.printf(
            "[MEMORY] PSRAM total: %u bytes\n",
            static_cast<unsigned>(ESP.getPsramSize())
        );

        printMemoryPool("External PSRAM", kPsramMemoryCaps);
    } else {
        Serial.println("[MEMORY] External PSRAM: not available");
    }

    Serial.printf(
        "[MEMORY] ESP.getFreeHeap(): %u bytes\n",
        static_cast<unsigned>(ESP.getFreeHeap())
    );

    Serial.printf(
        "[MEMORY] ESP.getMinFreeHeap(): %u bytes\n",
        static_cast<unsigned>(ESP.getMinFreeHeap())
    );

    Serial.println(
        "------------------------------------------------------------"
    );
}

} // namespace wirelessrecon::diagnostics
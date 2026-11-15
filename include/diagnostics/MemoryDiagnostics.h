//
// Created by dedlee on 11/14/26.
//

#pragma once

namespace wirelessrecon::diagnostics {

    class MemoryDiagnostics {
    public:
        /**
         * Print a memory snapshot with a caller-provided lifecycle label.
         *
         * Safe to call on boards with or without external PSRAM.
         */
        static void printSnapshot(const char* label);
    };

} // namespace wirelessrecon::diagnostics
#pragma once

#include <cstdint>
#include <string>

class String {
public:
    String() = default;
    String(const char* value) : value_(value == nullptr ? "" : value) {}
    String(const std::string& value) : value_(value) {}

    unsigned int length() const { return static_cast<unsigned int>(value_.length()); }
    const char* c_str() const { return value_.c_str(); }

    bool operator==(const String& other) const { return value_ == other.value_; }
    bool operator!=(const String& other) const { return !(*this == other); }

private:
    std::string value_;
};

namespace fake_platform {
inline uint32_t currentMillis = 123456;
inline uint32_t delayTotalMs = 0;
inline void resetClock() {
    currentMillis = 123456;
    delayTotalMs = 0;
}
}

inline uint32_t millis() {
    return fake_platform::currentMillis;
}

inline void delay(uint32_t milliseconds) {
    fake_platform::delayTotalMs += milliseconds;
    fake_platform::currentMillis += milliseconds;
}

#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
using std::max;
using std::min;
extern unsigned long testMillis;
constexpr int OUTPUT = 1, HIGH = 1, LOW = 0;
inline int testPinLevels[256] = {};
inline void pinMode(uint8_t, int) {}
inline void digitalWrite(uint8_t pin, int value) { testPinLevels[pin] = value; }
inline unsigned long millis() { return testMillis; }
template<class T, class L, class H> T constrain(T v, L low, H high) {
    return v < low ? static_cast<T>(low) : v > high ? static_cast<T>(high) : v;
}
#define DEBUGLOG_HPP
#define LOG_INFO(...) ((void)0)
#define LOG_INFOF(...) ((void)0)
#define LOG_WARN(...) ((void)0)
#define LOG_WARNF(...) ((void)0)
#define LOG_ERROR(...) ((void)0)
#define LOG_ERRORF(...) ((void)0)

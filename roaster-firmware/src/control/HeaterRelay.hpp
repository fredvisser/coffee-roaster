#ifndef HEATER_RELAY_HPP
#define HEATER_RELAY_HPP

#include <Arduino.h>

class HeaterRelay {
public:
    explicit HeaterRelay(uint8_t pin) : pin(pin) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, LOW);
    }

    void setPWM(uint8_t duty) {
        requestedDuty = duty;
        if (duty == 0) activeDuty = 0;
        tick();
    }

    void tick() {
        uint32_t now = millis();
        uint32_t elapsed = now - windowStartedAt;
        if (elapsed >= PERIOD_MS) {
            windowStartedAt += (elapsed / PERIOD_MS) * PERIOD_MS;
            elapsed %= PERIOD_MS;
            activeDuty = requestedDuty;
        }
        bool on = activeDuty == 255 || elapsed < activeDuty * PERIOD_MS / 255;
        digitalWrite(pin, on ? HIGH : LOW);
    }

private:
    // Latch each one-second window; control updates must not restart relay timing.
    static constexpr uint32_t PERIOD_MS = 1000;
    uint8_t pin;
    uint8_t requestedDuty = 0, activeDuty = 0;
    uint32_t windowStartedAt = 0;
};

#endif

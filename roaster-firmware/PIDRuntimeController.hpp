#ifndef PID_RUNTIME_CONTROLLER_HPP
#define PID_RUNTIME_CONTROLLER_HPP

#include <Arduino.h>
#include <Preferences.h>
#include <math.h>
#include "CalibrationTypes.hpp"

class PIDRuntimeController {
public:
    struct BandModel {
        bool valid = false;
        double targetTemp = 0.0;
        double minTemp = 0.0;
        double maxTemp = 0.0;
        double drift = 0.0;
        double coolingCoeff = 0.0;
        double heaterCoeff = 0.0;
        double deadTime = 0.0;
        double kp = 0.0;
        double ki = 0.0;
        double kd = 0.0;
    };

    struct ControlDecision {
        bool scheduleActive = false;
        int8_t bandIndex = -1;
        double kp = 0.0;
        double ki = 0.0;
        double kd = 0.0;
        double feedforward = 0.0;
    };

    void clear() {
        enabled = false;
        validBandCount = 0;
        activeBandIndex = -1;
        lastSetpointValid = false;
        lastSetpointTemp = 0.0;
        lastSetpointMs = 0;
        lastFeedforward = 0.0;
        lastDecision = {};
        for (uint8_t index = 0; index < Calibration::BAND_COUNT; index++) {
            bands[index] = {};
        }
    }

    void resetForRoast() {
        activeBandIndex = -1;
        lastSetpointValid = false;
        lastSetpointTemp = 0.0;
        lastSetpointMs = 0;
        lastFeedforward = 0.0;
        lastDecision = {};
    }

    void setFallbackGains(double newKp, double newKi, double newKd) {
        fallbackKp = newKp;
        fallbackKi = newKi;
        fallbackKd = newKd;
    }

    bool loadFromSummary(const Calibration::CharacterizationSummary &summary) {
        clear();

        for (uint8_t index = 0; index < Calibration::BAND_COUNT; index++) {
            const Calibration::BandCharacterization &source = summary.bands[index];
            if (!source.valid) {
                continue;
            }

            BandModel &band = bands[index];
            band.valid = true;
            band.targetTemp = source.targetTemp;
            band.minTemp = source.minTemp;
            band.maxTemp = source.maxTemp;
            band.drift = source.drift;
            band.coolingCoeff = source.coolingCoeff;
            band.heaterCoeff = source.heaterCoeff;
            band.deadTime = source.deadTime;
            band.kp = source.kp;
            band.ki = source.ki;
            band.kd = source.kd;
            validBandCount++;
        }

        enabled = validBandCount >= 2;
        return enabled;
    }

    bool loadFromPreferences(Preferences &prefs) {
        clear();

        enabled = prefs.getBool("pid_sched", false);
        if (!enabled) {
            return false;
        }

        for (uint8_t index = 0; index < Calibration::BAND_COUNT; index++) {
            BandModel &band = bands[index];
            band.valid = prefs.getBool(makeBandKey(index, "v"), false);
            if (!band.valid) {
                continue;
            }

            band.targetTemp = prefs.getDouble(makeBandKey(index, "tg"), 0.0);
            band.minTemp = prefs.getDouble(makeBandKey(index, "mn"), 0.0);
            band.maxTemp = prefs.getDouble(makeBandKey(index, "mx"), 0.0);
            band.drift = prefs.getDouble(makeBandKey(index, "dr"), 0.0);
            band.coolingCoeff = prefs.getDouble(makeBandKey(index, "cc"), 0.0);
            band.heaterCoeff = prefs.getDouble(makeBandKey(index, "hc"), 0.0);
            band.deadTime = prefs.getDouble(makeBandKey(index, "dt"), 0.0);
            band.kp = prefs.getDouble(makeBandKey(index, "kp"), 0.0);
            band.ki = prefs.getDouble(makeBandKey(index, "ki"), 0.0);
            band.kd = prefs.getDouble(makeBandKey(index, "kd"), 0.0);
            validBandCount++;
        }

        enabled = validBandCount >= 2;
        if (!enabled) {
            prefs.putBool("pid_sched", false);
        }
        return enabled;
    }

    void saveToPreferences(Preferences &prefs) const {
        prefs.putBool("pid_sched", enabled);
        for (uint8_t index = 0; index < Calibration::BAND_COUNT; index++) {
            const BandModel &band = bands[index];
            prefs.putBool(makeBandKey(index, "v"), band.valid);
            if (!band.valid) {
                continue;
            }

            prefs.putDouble(makeBandKey(index, "tg"), band.targetTemp);
            prefs.putDouble(makeBandKey(index, "mn"), band.minTemp);
            prefs.putDouble(makeBandKey(index, "mx"), band.maxTemp);
            prefs.putDouble(makeBandKey(index, "dr"), band.drift);
            prefs.putDouble(makeBandKey(index, "cc"), band.coolingCoeff);
            prefs.putDouble(makeBandKey(index, "hc"), band.heaterCoeff);
            prefs.putDouble(makeBandKey(index, "dt"), band.deadTime);
            prefs.putDouble(makeBandKey(index, "kp"), band.kp);
            prefs.putDouble(makeBandKey(index, "ki"), band.ki);
            prefs.putDouble(makeBandKey(index, "kd"), band.kd);
        }
    }

    void clearPreferences(Preferences &prefs) {
        clear();
        prefs.putBool("pid_sched", false);
        for (uint8_t index = 0; index < Calibration::BAND_COUNT; index++) {
            const char *suffixes[] = {"v", "tg", "mn", "mx", "dr", "cc", "hc", "dt", "kp", "ki", "kd"};
            for (size_t suffixIndex = 0; suffixIndex < sizeof(suffixes) / sizeof(suffixes[0]); suffixIndex++) {
                prefs.remove(makeBandKey(index, suffixes[suffixIndex]));
            }
        }
    }

    ControlDecision decide(unsigned long now, double currentTemp, double setpointTemp, double ambientTemp) {
        ControlDecision decision;
        decision.kp = fallbackKp;
        decision.ki = fallbackKi;
        decision.kd = fallbackKd;

        if (!enabled) {
            lastDecision = decision;
            return decision;
        }

        int8_t bandIndex = selectBand(setpointTemp);
        if (bandIndex < 0) {
            lastDecision = decision;
            return decision;
        }

        const BandModel &band = bands[bandIndex];
        double desiredRate = 0.0;
        if (lastSetpointValid && now > lastSetpointMs) {
            double dtSeconds = static_cast<double>(now - lastSetpointMs) / 1000.0;
            if (dtSeconds > 0.0) {
                desiredRate = (setpointTemp - lastSetpointTemp) / dtSeconds;
            }
        }

        desiredRate = constrain(desiredRate, -0.75, 1.5);
        double effectiveAmbient = ambientTemp > 20.0 ? ambientTemp : currentTemp;
        double leadTarget = setpointTemp + desiredRate * min(band.deadTime, 12.0);
        double rawFeedforward = (desiredRate - band.drift - band.coolingCoeff * (leadTarget - effectiveAmbient)) / max(band.heaterCoeff, 1e-6);
        rawFeedforward = constrain(rawFeedforward * FEEDFORWARD_SCALE, 0.0, 255.0);

        double smoothedFeedforward = lastSetpointValid
            ? lastFeedforward + FEEDFORWARD_FILTER * (rawFeedforward - lastFeedforward)
            : rawFeedforward;

        lastSetpointValid = true;
        lastSetpointTemp = setpointTemp;
        lastSetpointMs = now;
        lastFeedforward = smoothedFeedforward;
        activeBandIndex = bandIndex;

        decision.scheduleActive = true;
        decision.bandIndex = bandIndex;
        decision.kp = band.kp;
        decision.ki = band.ki;
        decision.kd = band.kd;
        decision.feedforward = smoothedFeedforward;
        lastDecision = decision;
        return decision;
    }

    bool isEnabled() const { return enabled; }
    uint8_t getValidBandCount() const { return validBandCount; }
    int8_t getActiveBandIndex() const { return activeBandIndex; }
    ControlDecision getLastDecision() const { return lastDecision; }
    const BandModel &getBand(uint8_t index) const { return bands[index]; }

private:
    static constexpr double FEEDFORWARD_SCALE = 0.85;
    static constexpr double FEEDFORWARD_FILTER = 0.35;
    static constexpr double BAND_HYSTERESIS_F = 6.0;

    double fallbackKp = 0.0;
    double fallbackKi = 0.0;
    double fallbackKd = 0.0;
    bool enabled = false;
    uint8_t validBandCount = 0;
    int8_t activeBandIndex = -1;
    bool lastSetpointValid = false;
    double lastSetpointTemp = 0.0;
    unsigned long lastSetpointMs = 0;
    double lastFeedforward = 0.0;
    BandModel bands[Calibration::BAND_COUNT];
    ControlDecision lastDecision;

    int8_t selectBand(double setpointTemp) {
        if (activeBandIndex >= 0 && bands[activeBandIndex].valid) {
            const BandModel &activeBand = bands[activeBandIndex];
            if (setpointTemp >= activeBand.minTemp - BAND_HYSTERESIS_F &&
                setpointTemp <= activeBand.maxTemp + BAND_HYSTERESIS_F) {
                return activeBandIndex;
            }
        }

        for (uint8_t index = 0; index < Calibration::BAND_COUNT; index++) {
            if (!bands[index].valid) {
                continue;
            }
            if (setpointTemp >= bands[index].minTemp && setpointTemp <= bands[index].maxTemp) {
                return static_cast<int8_t>(index);
            }
        }

        int8_t nearestBand = -1;
        double nearestDelta = 1e9;
        for (uint8_t index = 0; index < Calibration::BAND_COUNT; index++) {
            if (!bands[index].valid) {
                continue;
            }
            double delta = fabs(setpointTemp - bands[index].targetTemp);
            if (delta < nearestDelta) {
                nearestDelta = delta;
                nearestBand = static_cast<int8_t>(index);
            }
        }
        return nearestBand;
    }

    const char *makeBandKey(uint8_t bandIndex, const char *suffix) const {
        static char key[16];
        snprintf(key, sizeof(key), "pb%u%s", bandIndex, suffix);
        return key;
    }
};

#endif
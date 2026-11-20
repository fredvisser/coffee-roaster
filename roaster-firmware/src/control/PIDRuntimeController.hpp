#ifndef PID_RUNTIME_CONTROLLER_HPP
#define PID_RUNTIME_CONTROLLER_HPP

#include <Arduino.h>
#include <Preferences.h>
#include <math.h>
#include "../platform/CalibrationTypes.hpp"
#include "../support/DebugLog.hpp"

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
        double targetRate = 0.0;
        bool outsideRange = false;
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
            if (!source.valid) continue;
            if (!validModel(source.heaterCoeff, source.coolingCoeff,
                                             source.timeConstant, source.kp, source.ki, source.kd)) {
                LOG_WARN("Invalid PID model ignored");
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
            if (!validBand(band)) {
                band.valid = false;
                LOG_WARN("Invalid PID model range or dynamics ignored");
                continue;
            }
            validBandCount++;
        }

        enabled = validBandCount >= 1;
        return enabled;
    }

    bool loadFromPreferences(Preferences &prefs) {
        clear();

        enabled = prefs.getBool("pid_sched", false);
        if (!enabled) {
            return false;
        }
        if (prefs.getUInt("pid_model_ver", 0) != MODEL_VERSION) {
            LOG_WARN("Legacy PID model disabled; retune before enabling feedforward");
            enabled = false;
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
            band.valid = validModel(band.heaterCoeff, band.coolingCoeff, 1.0,
                                    band.kp, band.ki, band.kd) && validBand(band);
            if (band.valid) validBandCount++;
            else LOG_WARN("Invalid stored PID model ignored");
        }

        enabled = validBandCount >= 1;
        if (!enabled) {
            prefs.putBool("pid_sched", false);
        }
        return enabled;
    }

    void saveToPreferences(Preferences &prefs) const {
        prefs.putBool("pid_sched", enabled);
        prefs.putUInt("pid_model_ver", MODEL_VERSION);
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

    ControlDecision decide(unsigned long now, double currentTemp, double setpointTemp, double ambientTemp,
                           double profileRate = NAN) {
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

        if (isfinite(profileRate)) desiredRate = profileRate;
        desiredRate = constrain(desiredRate, -0.75, 1.5);
        double effectiveAmbient = ambientTemp;
        if (!isfinite(effectiveAmbient) || effectiveAmbient < 20.0 || !isfinite(setpointTemp) || !isfinite(currentTemp)) {
            LOG_ERROR("Invalid inlet temperature or setpoint; PID feedforward disabled");
            lastDecision = decision;
            return decision;
        }
        double leadTarget = setpointTemp + desiredRate * min(band.deadTime, 12.0);
        // Model inverse: u = (rate*tau + T - offset) / K, with drift = offset/tau learned at tuning time.
        double rawFeedforward = (desiredRate * rampScale - band.drift +
                                 band.coolingCoeff * leadTarget) / band.heaterCoeff;
        rawFeedforward = constrain(rawFeedforward, 0.0, 255.0);

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
        decision.targetRate = desiredRate;
        decision.outsideRange = setpointTemp < band.minTemp || setpointTemp > band.maxTemp;
        lastDecision = decision;
        return decision;
    }

    bool isEnabled() const { return enabled; }
    uint8_t getValidBandCount() const { return validBandCount; }
    int8_t getActiveBandIndex() const { return activeBandIndex; }
    ControlDecision getLastDecision() const { return lastDecision; }
    const BandModel &getBand(uint8_t index) const { return bands[index]; }
    double getRampScale() const { return rampScale; }
    void setRampScale(double value) { rampScale = constrain(value, 0.5, 1.5); }
    static constexpr uint32_t MODEL_VERSION = 3;

private:
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
    double rampScale = 1.0;

    static bool validBand(const BandModel &band) {
        return isfinite(band.minTemp) && isfinite(band.maxTemp) && band.minTemp <= band.maxTemp &&
               isfinite(band.targetTemp) && isfinite(band.drift) &&
               isfinite(band.deadTime) && band.deadTime >= 0;
    }

    static bool validModel(double heater, double cooling, double tau, double p, double i, double d) {
        return isfinite(heater) && heater > 1e-6 && isfinite(cooling) && cooling > 0.0 &&
               isfinite(tau) && tau > 0.0 && isfinite(p) && p > 0.0 && p <= 200.0 &&
               isfinite(i) && i >= 0.0 && i <= 20.0 && isfinite(d) && d >= 0.0 && d <= 500.0;
    }

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
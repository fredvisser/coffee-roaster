#ifndef PID_TUNING_STORE_HPP
#define PID_TUNING_STORE_HPP

#include "PIDRuntimeController.hpp"
#include <stddef.h>

class PIDTuningStore {
public:
    static constexpr uint8_t HISTORY_SIZE = 10;
    struct Band {
        uint32_t valid = 0;
        float target = 0, minTemp = 0, maxTemp = 0, drift = 0, cooling = 0, heater = 0;
        float delay = 0, kp = 0, ki = 0, kd = 0;
    };
    struct Configuration {
        float kp = 8, ki = 0.46f, kd = 0, rampScale = 1;
        Band bands[Calibration::BAND_COUNT];
    };
    struct Record {
        uint32_t version = STORAGE_VERSION, id = 0, parentId = 0, bootId = 0, uptimeMs = 0, unixTime = 0;
        uint32_t durationMs = 0, fan = 0;
        char firmware[16] = "", load[32] = "", outcome[24] = "", reason[48] = "";
        float tauFactor = 1, fitRmse = 0, predictionRmse = 0, noise = 0;
        float rampMae = 0, holdMae = 0, overshoot = 0, saturatedPercent = 0, lagSeconds = 0, rampWithin3Percent = 0;
        Configuration before, proposed;
        uint32_t checksum = 0;
    };

    explicit PIDTuningStore(Preferences &storage) : prefs(storage) {}
    const char *getError() const { return lastError; }
    uint32_t latestId() {
        uint32_t latest = 0;
        for (uint8_t slot = 0; slot < HISTORY_SIZE; ++slot) {
            Record record;
            if (readSlot(slot, record)) latest = max(latest, record.id);
        }
        return latest;
    }
    bool readRecord(uint32_t id, Record &record) {
        return id != 0 && readSlot(id % HISTORY_SIZE, record) && record.id == id;
    }
    bool append(Record &record) {
        record.id = latestId() + 1;
        return update(record);
    }
    bool update(Record &record) {
        record.version = STORAGE_VERSION;
        record.checksum = hash(&record, offsetof(Record, checksum));
        char key[16];
        snprintf(key, sizeof(key), "pid_run_%u", static_cast<unsigned>(record.id % HISTORY_SIZE));
        return write(key, &record, sizeof(record));
    }
    bool readActive(Configuration &config) { return readConfig("pid_active", config); }
    bool readPrevious(Configuration &config) { return readConfig("pid_previous", config); }
    bool readKnownGood(Configuration &config) { return readConfig("pid_good", config); }
    bool saveKnownGood(const Configuration &config) {
        if (!valid(config)) return storageError("invalid_pid_configuration");
        SavedConfiguration saved;
        saved.config = config;
        saved.checksum = hash(&saved, offsetof(SavedConfiguration, checksum));
        return write("pid_good", &saved, sizeof(saved));
    }
    bool saveActive(const Configuration &config, const Configuration &previous) {
        if (!valid(config)) return storageError("invalid_pid_configuration");
        SavedConfiguration old;
        old.config = previous;
        old.checksum = hash(&old, offsetof(SavedConfiguration, checksum));
        if (!write("pid_previous", &old, sizeof(old))) return false;
        SavedConfiguration next;
        next.config = config;
        next.checksum = hash(&next, offsetof(SavedConfiguration, checksum));
        return write("pid_active", &next, sizeof(next));
    }
    static Configuration capture(double p, double i, double d, const PIDRuntimeController &runtime) {
        Configuration config;
        config.kp = p; config.ki = i; config.kd = d; config.rampScale = runtime.getRampScale();
        for (uint8_t j = 0; j < Calibration::BAND_COUNT; ++j) {
            const auto &source = runtime.getBand(j);
            auto &out = config.bands[j];
            out.valid = source.valid;
            out.target = source.targetTemp; out.minTemp = source.minTemp; out.maxTemp = source.maxTemp;
            out.drift = source.drift; out.cooling = source.coolingCoeff; out.heater = source.heaterCoeff;
            out.delay = source.deadTime; out.kp = source.kp; out.ki = source.ki; out.kd = source.kd;
        }
        return config;
    }
    static bool validGains(double p, double i, double d) {
        return isfinite(p) && p > 0 && p <= 200 && isfinite(i) && i >= 0 && i <= 20 &&
               isfinite(d) && d >= 0 && d <= 500;
    }
    static bool valid(const Configuration &config) {
        if (!validGains(config.kp, config.ki, config.kd) ||
            !isfinite(config.rampScale) || config.rampScale < 0.5 || config.rampScale > 1.5) return false;
        for (const auto &band : config.bands) {
            if (!band.valid) continue;
            if (!validGains(band.kp, band.ki, band.kd) || !isfinite(band.heater) || band.heater <= 1e-6 ||
                !isfinite(band.cooling) || band.cooling <= 0 || !isfinite(band.drift) ||
                !isfinite(band.delay) || band.delay < 0 || band.delay > 60 ||
                !isfinite(band.minTemp) || !isfinite(band.maxTemp) || !isfinite(band.target) ||
                band.minTemp < 20 || band.maxTemp > 450 || band.maxTemp <= band.minTemp) return false;
        }
        return true;
    }
    static void loadRuntime(const Configuration &config, PIDRuntimeController &runtime) {
        Calibration::CharacterizationSummary summary = {};
        for (uint8_t j = 0; j < Calibration::BAND_COUNT; ++j) {
            const auto &source = config.bands[j];
            auto &out = summary.bands[j];
            out.valid = source.valid;
            out.targetTemp = source.target; out.minTemp = source.minTemp; out.maxTemp = source.maxTemp;
            out.drift = source.drift; out.coolingCoeff = source.cooling; out.heaterCoeff = source.heater;
            out.deadTime = source.delay; out.timeConstant = source.cooling > 0 ? 1.0 / source.cooling : 0;
            out.kp = source.kp; out.ki = source.ki; out.kd = source.kd;
        }
        runtime.setFallbackGains(config.kp, config.ki, config.kd);
        runtime.loadFromSummary(summary);
        runtime.setRampScale(config.rampScale);
    }

private:
    static constexpr uint32_t STORAGE_VERSION = 3;
    // v2 bands stored drift relative to the live inlet; keep their gains but drop the incompatible feedforward model.
    static constexpr uint32_t LEGACY_VERSION = 2;
    static void dropBands(Configuration &config) { for (auto &band : config.bands) band.valid = 0; }
    struct SavedConfiguration {
        uint32_t version = STORAGE_VERSION;
        Configuration config;
        uint32_t checksum = 0;
    };
    Preferences &prefs;
    const char *lastError = "none";
    static uint32_t hash(const void *data, size_t count) {
        auto bytes = static_cast<const uint8_t *>(data);
        uint32_t value = 2166136261U;
        for (size_t j = 0; j < count; ++j) value = (value ^ bytes[j]) * 16777619U;
        return value;
    }
    bool storageError(const char *error) {
        lastError = error;
        LOG_ERRORF("PID persistence: %s", error);
        return false;
    }
    bool write(const char *key, const void *value, size_t count) {
        if (prefs.putBytes(key, value, count) != count) return storageError("storage_write_failed");
        lastError = "none";
        return true;
    }
    bool readConfig(const char *key, Configuration &config) {
        size_t count = prefs.getBytesLength(key);
        if (count == 0) return false;
        SavedConfiguration saved;
        if (count != sizeof(saved) || prefs.getBytes(key, &saved, sizeof(saved)) != sizeof(saved) ||
            (saved.version != STORAGE_VERSION && saved.version != LEGACY_VERSION) ||
            saved.checksum != hash(&saved, offsetof(SavedConfiguration, checksum))) return storageError("stored_configuration_invalid");
        if (saved.version == LEGACY_VERSION) dropBands(saved.config);
        if (!valid(saved.config)) return storageError("stored_configuration_invalid");
        config = saved.config;
        return true;
    }
    bool readSlot(uint8_t slot, Record &record) {
        char key[16];
        snprintf(key, sizeof(key), "pid_run_%u", slot);
        size_t count = prefs.getBytesLength(key);
        if (count == 0) return false;
        if (count != sizeof(record) || prefs.getBytes(key, &record, sizeof(record)) != sizeof(record) ||
            (record.version != STORAGE_VERSION && record.version != LEGACY_VERSION) ||
            record.checksum != hash(&record, offsetof(Record, checksum)))
            return storageError("history_record_invalid");
        if (record.version == LEGACY_VERSION) { dropBands(record.before); dropBands(record.proposed); }
        return true;
    }
};
#endif

#ifndef STEP_RESPONSE_TUNER_HPP
#define STEP_RESPONSE_TUNER_HPP

#include <Arduino.h>
#include <math.h>
#include <string.h>

#include "DebugLog.hpp"
#include "CalibrationTypes.hpp"

class StepResponseTuner {
public:
    static constexpr uint8_t MAX_BANDS = 3;
    static constexpr uint16_t MAX_SAMPLES = 500;

    struct FOPDTModel {
        bool valid = false;
        double processGain = 0.0;
        double timeConstant = 0.0;
        double deadTime = 0.0;       // FOPDT-fitted dead time (may be inflated for higher-order systems)
        double onsetDeadTime = 0.0;  // Measured response onset time (baseline + 3σ crossing)
        double fitRmse = 0.0;
        double baselineTemp = 0.0;
        double finalTemp = 0.0;
        double baselineOutput = 0.0;
        double stepDelta = 0.0;
        double noiseStdDev = 0.0;    // Temperature noise σ measured during stabilization
    };

    struct BandResult {
        bool valid = false;
        FOPDTModel model;
        double kp = 0.0;
        double ki = 0.0;
        double kd = 0.0;
        double minTemp = 0.0;
        double maxTemp = 0.0;
        double targetTemp = 0.0;
        uint16_t sampleCount = 0;
    };

    struct TraceSample {
        uint32_t elapsedMs = 0;
        float actualTempF = 0.0f;
        float setpointTempF = 0.0f;
        float heaterOutput = 0.0f;
        uint8_t phaseId = 0;
    };

    struct Summary {
        bool valid = false;
        bool passed = false;
        double recommendedKp = 0.0;
        double recommendedKi = 0.0;
        double recommendedKd = 0.0;
        double tauCFactor = 0.0;
        uint8_t completedBands = 0;
        uint8_t totalBands = 0;
        uint8_t primaryBandIndex = 0;
        double currentSetpoint = 0.0;
        double maxTemp = 0.0;
        uint16_t totalSamples = 0;
        BandResult bands[MAX_BANDS];

        // Compatibility fields for existing API
        double targetTemp = 0.0;
        double ambientTemp = 0.0;
        double meanFitRmse = 0.0;
        double worstFitRmse = 0.0;
        double observedPeakRate = 0.0;
        uint8_t validBandCount = 0;
        double bestScore = 0.0;
        uint8_t bestCycleIndex = 255;
        uint8_t completedCycles = 0;
        uint8_t maxCycles = 0;
        double rampRateFps = 0.0;

        // Cycle compatibility
        Calibration::CycleSummary cycles[Calibration::MAX_CYCLES];
    };

    StepResponseTuner() {
        resetState();
    }

    void start(double currentTemp, double seedKp, double seedKi, double seedKd,
               double fanSpeed = 255.0, double tauCFactor = 0.5) {
        resetState();

        if (currentTemp > MAX_START_TEMP_F) {
            fail("start_temp_too_high");
            return;
        }

        initialTemp = currentTemp;
        configuredFanSpeed = fanSpeed;
        configTauCFactor = constrain(tauCFactor, 0.3, 3.0);
        seedGains[0] = seedKp;
        seedGains[1] = seedKi;
        seedGains[2] = seedKd;

        configureBands(currentTemp);

        running = true;
        complete = false;
        startTimeMs = millis();
        activeBandIndex = 0;
        setPhase(STABILIZE, startTimeMs);
        setHeaterCommand(bandConfigs[0].baselineOutput);

        rebuildSummary();
        LOG_INFOF("Step-response tuning started: temp=%.1fF, bands=%u, tauC_factor=%.2f",
                  currentTemp, totalBands, configTauCFactor);
    }

    void stop() {
        if (phase != COMPLETE && phase != FAILED) {
            fail("stopped");
        }
    }

    void cancel() {
        if (phase != COMPLETE && phase != FAILED) {
            fail("cancelled");
        }
    }

    bool isRunning() const { return running; }
    bool isComplete() const { return complete; }
    bool isRecoveryCooling() const { return phase == COOL_DOWN; }
    double getSetpoint() const { return currentSetpoint; }
    uint16_t getSampleCount() const { return totalSampleCount; }
    uint8_t getActiveBandIndex() const { return activeBandIndex; }
    const char *getLastError() const { return lastError; }
    double getHeaterCommand() const { return heaterCommand; }

    Summary getSummary() const { return summary; }

    void getPID(double &outKp, double &outKi, double &outKd) const {
        outKp = recommendedKp;
        outKi = recommendedKi;
        outKd = recommendedKd;
    }

    // Compatibility: populate a CharacterizationSummary for existing API code
    Calibration::CharacterizationSummary getCharacterizationSummary() const {
        Calibration::CharacterizationSummary cs = {};
        cs.valid = summary.valid;
        cs.passed = summary.passed;
        cs.recommendedKp = summary.recommendedKp;
        cs.recommendedKi = summary.recommendedKi;
        cs.recommendedKd = summary.recommendedKd;
        cs.targetTemp = summary.targetTemp;
        cs.ambientTemp = summary.ambientTemp;
        cs.meanFitRmse = summary.meanFitRmse;
        cs.worstFitRmse = summary.worstFitRmse;
        cs.observedPeakRate = summary.observedPeakRate;
        cs.maxTemp = summary.maxTemp;
        cs.totalSamples = summary.totalSamples;
        cs.validBandCount = summary.validBandCount;
        cs.completedCycles = summary.completedBands;
        cs.maxCycles = summary.totalBands;
        cs.bestCycleIndex = summary.primaryBandIndex;
        cs.bestScore = summary.bestScore;
        cs.rampRateFps = 0.0;
        cs.currentSetpoint = summary.currentSetpoint;

        for (uint8_t i = 0; i < Calibration::BAND_COUNT && i < summary.completedBands; i++) {
            const BandResult &br = summary.bands[i];
            Calibration::BandCharacterization &bc = cs.bands[i];
            bc.valid = br.valid;
            bc.targetTemp = br.targetTemp;
            bc.minTemp = br.minTemp;
            bc.maxTemp = br.maxTemp;
            bc.processGain = br.model.processGain;
            bc.timeConstant = br.model.timeConstant;
            bc.deadTime = br.model.deadTime;
            bc.fitRmse = br.model.fitRmse;
            bc.kp = br.kp;
            bc.ki = br.ki;
            bc.kd = br.kd;
            bc.sampleCount = br.sampleCount;

            // Derive feedforward model parameters from FOPDT model.
            // heaterCoeff = dT/dt per unit heater output = processGain / timeConstant
            double tau = max(br.model.timeConstant, 1.0);
            bc.heaterCoeff = fabs(br.model.processGain) / tau;

            // coolingCoeff from steady-state energy balance at baseline:
            // heaterCoeff * u_baseline = coolingCoeff * (T_baseline - T_ambient)
            double tempDiff = br.model.baselineTemp - lastAmbientTemp;
            if (tempDiff > 10.0 && bc.heaterCoeff > 1e-6) {
                bc.coolingCoeff = bc.heaterCoeff * br.model.baselineOutput / tempDiff;
            } else {
                bc.coolingCoeff = 0.0;
            }

            // drift is ~0 at steady state
            bc.drift = 0.0;
        }

        for (uint8_t i = 0; i < Calibration::MAX_CYCLES && i < summary.completedBands; i++) {
            Calibration::CycleSummary &cyc = cs.cycles[i];
            const BandResult &br = summary.bands[i];
            cyc.valid = br.valid;
            cyc.passed = br.valid;
            cyc.modelValid = br.model.valid;
            cyc.cycleIndex = i + 1;
            cyc.kp = br.kp;
            cyc.ki = br.ki;
            cyc.kd = br.kd;
            cyc.processGain = br.model.processGain;
            cyc.timeConstant = br.model.timeConstant;
            cyc.deadTime = br.model.deadTime;
            cyc.fitRmse = br.model.fitRmse;
            cyc.sampleCount = br.sampleCount;
        }

        return cs;
    }

    uint16_t getTraceSampleCount() const {
        return activeSampleCount > 0 ? activeSampleCount : lastBandSampleCount;
    }

    TraceSample getTraceSample(uint16_t index) const {
        if (activeSampleCount > 0 && index < activeSampleCount) {
            return samples[index];
        }
        if (index < lastBandSampleCount) {
            return lastBandSamples[index];
        }
        return {};
    }

    const char *getPhaseName() const {
        switch (phase) {
            case STABILIZE:
                snprintf(phaseLabel, sizeof(phaseLabel), "STABILIZE_BAND_%u", activeBandIndex + 1);
                return phaseLabel;
            case STEP_UP:
                snprintf(phaseLabel, sizeof(phaseLabel), "STEP_UP_BAND_%u", activeBandIndex + 1);
                return phaseLabel;
            case RECORD:
                snprintf(phaseLabel, sizeof(phaseLabel), "RECORD_BAND_%u", activeBandIndex + 1);
                return phaseLabel;
            case COOL_DOWN:
                snprintf(phaseLabel, sizeof(phaseLabel), "COOL_DOWN_BAND_%u", activeBandIndex + 1);
                return phaseLabel;
            case COMPUTE:   return "COMPUTE";
            case COMPLETE:  return "COMPLETE";
            case FAILED:    return "FAILED";
            case IDLE:
            default:        return "IDLE";
        }
    }

    double getProgressPercent() const {
        if (!running && phase == IDLE) return 0.0;
        if (!running && (phase == COMPLETE || phase == FAILED)) return 100.0;

        double bandWeight = totalBands > 0 ? 90.0 / static_cast<double>(totalBands) : 30.0;
        double base = static_cast<double>(activeBandIndex) * bandWeight;

        switch (phase) {
            case STABILIZE:  return min(99.0, base + bandWeight * 0.2);
            case STEP_UP:    return min(99.0, base + bandWeight * 0.3);
            case RECORD:     return min(99.0, base + bandWeight * 0.7);
            case COOL_DOWN:  return min(99.0, base + bandWeight * 0.95);
            case COMPUTE:    return 95.0;
            case COMPLETE:
            case FAILED:     return 100.0;
            default:         return base;
        }
    }

    // Called every control interval (250ms) — returns heater output 0–255
    double getOutput(double chamberTemp, double ambientTemp, double fanCommand) {
        (void)fanCommand;
        if (!running) return 0.0;

        unsigned long now = millis();
        if (now - startTimeMs > MAX_TOTAL_MS) {
            fail("timeout");
            return 0.0;
        }

        if (chamberTemp > MAX_SAFE_TEMP_F) {
            fail("over_temp");
            return 0.0;
        }

        observedMaxTemp = max(observedMaxTemp, chamberTemp);

        updatePhase(now, chamberTemp, ambientTemp);

        if (!running) {
            currentSetpoint = 0.0;
            return 0.0;
        }

        captureSample(now, chamberTemp, ambientTemp);
        rebuildSummary();
        return heaterCommand;
    }

private:
    enum Phase {
        IDLE,
        STABILIZE,
        STEP_UP,
        RECORD,
        COOL_DOWN,
        COMPUTE,
        COMPLETE,
        FAILED
    };

    struct BandConfig {
        double baselineOutput;
        double stepDelta;
        double targetTemp;
        double minTemp;
        double maxTemp;
    };

    // Configuration constants
    static constexpr double MAX_START_TEMP_F = 140.0;
    static constexpr double MAX_SAFE_TEMP_F = 450.0;
    static constexpr unsigned long MAX_TOTAL_MS = 2400000UL;
    static constexpr unsigned long SAMPLE_INTERVAL_MS = 250UL;
    static constexpr unsigned long STABILIZE_TIMEOUT_MS = 300000UL;
    static constexpr unsigned long STABILIZE_SETTLED_MS = 30000UL;
    static constexpr unsigned long RECORD_TIMEOUT_MS = 180000UL;
    static constexpr unsigned long RECORD_SETTLED_MS = 20000UL;
    static constexpr unsigned long COOL_DOWN_TIMEOUT_MS = 300000UL;
    static constexpr double SETTLED_RATE_THRESHOLD = 0.05;
    static constexpr uint16_t RATE_WINDOW_SAMPLES = 30;
    static constexpr double STEP_DELTA_DEFAULT = 50.0;
    static constexpr double KP_MIN = 0.1;
    static constexpr double KP_MAX = 200.0;
    static constexpr double KI_MIN = 0.0001;
    static constexpr double KI_MAX = 20.0;
    static constexpr double KD_MIN = 0.0;
    static constexpr double KD_MAX = 500.0;

    // State
    bool running = false;
    bool complete = false;
    Phase phase = IDLE;
    unsigned long startTimeMs = 0;
    unsigned long phaseStartMs = 0;
    unsigned long lastSampleMs = 0;
    unsigned long settledSinceMs = 0;
    double initialTemp = 0.0;
    double currentSetpoint = 0.0;
    double heaterCommand = 0.0;
    double observedMaxTemp = 0.0;
    double configuredFanSpeed = 255.0;
    double configTauCFactor = 0.8;
    double seedGains[3] = {};
    double recommendedKp = 0.0;
    double recommendedKi = 0.0;
    double recommendedKd = 0.0;

    uint8_t activeBandIndex = 0;
    uint8_t totalBands = 0;
    BandConfig bandConfigs[MAX_BANDS];
    BandResult bandResults[MAX_BANDS];

    // Sample buffers
    uint16_t activeSampleCount = 0;
    uint16_t lastBandSampleCount = 0;
    uint16_t totalSampleCount = 0;
    TraceSample samples[MAX_SAMPLES];
    TraceSample lastBandSamples[MAX_SAMPLES];

    // For stabilization rate detection
    double recentTemps[RATE_WINDOW_SAMPLES];
    uint16_t recentTempIndex = 0;
    uint16_t recentTempCount = 0;
    double baselineTemp = 0.0;
    double noiseStdDev = 0.0;       // Temperature noise σ from stabilization
    double lastAmbientTemp = 70.0;  // Most recent ambient reading for feedforward

    Summary summary;
    char lastError[32] = "none";
    mutable char phaseLabel[32] = "IDLE";

    void resetState() {
        running = false;
        complete = false;
        phase = IDLE;
        startTimeMs = 0;
        phaseStartMs = 0;
        lastSampleMs = 0;
        settledSinceMs = 0;
        initialTemp = 0.0;
        currentSetpoint = 0.0;
        heaterCommand = 0.0;
        observedMaxTemp = 0.0;
        configuredFanSpeed = 255.0;
        configTauCFactor = 0.8;
        activeBandIndex = 0;
        totalBands = 0;
        activeSampleCount = 0;
        lastBandSampleCount = 0;
        totalSampleCount = 0;
        recommendedKp = 0.0;
        recommendedKi = 0.0;
        recommendedKd = 0.0;
        recentTempIndex = 0;
        recentTempCount = 0;
        baselineTemp = 0.0;
        noiseStdDev = 0.0;
        lastAmbientTemp = 70.0;
        memset(bandConfigs, 0, sizeof(bandConfigs));
        memset(bandResults, 0, sizeof(bandResults));
        memset(samples, 0, sizeof(samples));
        memset(lastBandSamples, 0, sizeof(lastBandSamples));
        memset(recentTemps, 0, sizeof(recentTemps));
        memset(&summary, 0, sizeof(summary));
        memset(seedGains, 0, sizeof(seedGains));
        copyError("none");
    }

    void configureBands(double currentTemp) {
        // Configure step-test bands based on starting temperature
        // Band 0: Low (~175°F) — baseline 80 PWM, step +50
        // Band 1: Mid (~275°F) — baseline 140 PWM, step +50
        totalBands = 2;

        bandConfigs[0] = { 80.0,  STEP_DELTA_DEFAULT, 175.0, 120.0, 225.0 };
        bandConfigs[1] = { 140.0, STEP_DELTA_DEFAULT, 275.0, 225.0, 325.0 };
    }

    void copyError(const char *value) {
        strlcpy(lastError, value ? value : "unknown", sizeof(lastError));
    }

    void setPhase(Phase nextPhase, unsigned long now) {
        phase = nextPhase;
        phaseStartMs = now;
        settledSinceMs = 0;
        recentTempIndex = 0;
        recentTempCount = 0;
        LOG_INFOF("Step-response phase -> %s", getPhaseName());
    }

    void setHeaterCommand(double output) {
        heaterCommand = constrain(output, 0.0, 255.0);
    }

    void clearActiveSamples() {
        activeSampleCount = 0;
        lastSampleMs = 0;
        memset(samples, 0, sizeof(samples));
    }

    void copyBandTrace() {
        lastBandSampleCount = activeSampleCount;
        if (lastBandSampleCount > 0) {
            memcpy(lastBandSamples, samples, sizeof(TraceSample) * lastBandSampleCount);
        }
    }

    void captureSample(unsigned long now, double chamberTemp, double ambientTemp) {
        if (phase == IDLE || phase == COMPUTE || phase == COMPLETE || phase == FAILED) return;
        if (lastSampleMs != 0 && now - lastSampleMs < SAMPLE_INTERVAL_MS) return;
        lastSampleMs = now;

        // Update rate detection ring buffer
        recentTemps[recentTempIndex] = chamberTemp;
        recentTempIndex = (recentTempIndex + 1) % RATE_WINDOW_SAMPLES;
        if (recentTempCount < RATE_WINDOW_SAMPLES) recentTempCount++;

        // Store trace sample
        if (activeSampleCount < MAX_SAMPLES) {
            TraceSample &s = samples[activeSampleCount++];
            s.elapsedMs = now - phaseStartMs;
            s.actualTempF = static_cast<float>(chamberTemp);
            s.setpointTempF = static_cast<float>(currentSetpoint);
            s.heaterOutput = static_cast<float>(heaterCommand);
            s.phaseId = static_cast<uint8_t>(phase);
        }
    }

    double computeRate() const {
        if (recentTempCount < 4) return 999.0;

        // Linear regression on recent temperature samples to get dT/dt
        uint16_t n = min(recentTempCount, RATE_WINDOW_SAMPLES);
        double sumX = 0.0, sumY = 0.0, sumXY = 0.0, sumX2 = 0.0;

        for (uint16_t i = 0; i < n; i++) {
            uint16_t idx = (recentTempIndex + RATE_WINDOW_SAMPLES - n + i) % RATE_WINDOW_SAMPLES;
            double x = static_cast<double>(i) * (SAMPLE_INTERVAL_MS / 1000.0);
            double y = recentTemps[idx];
            sumX += x;
            sumY += y;
            sumXY += x * y;
            sumX2 += x * x;
        }

        double denom = n * sumX2 - sumX * sumX;
        if (fabs(denom) < 1e-12) return 999.0;
        double slope = (n * sumXY - sumX * sumY) / denom;
        return fabs(slope);
    }

    double computeAverageTemp() const {
        if (recentTempCount == 0) return 0.0;
        uint16_t n = min(recentTempCount, RATE_WINDOW_SAMPLES);
        double sum = 0.0;
        for (uint16_t i = 0; i < n; i++) {
            uint16_t idx = (recentTempIndex + RATE_WINDOW_SAMPLES - n + i) % RATE_WINDOW_SAMPLES;
            sum += recentTemps[idx];
        }
        return sum / static_cast<double>(n);
    }

    double computeNoiseStdDev() const {
        if (recentTempCount < 4) return 0.5;  // conservative default
        uint16_t n = min(recentTempCount, RATE_WINDOW_SAMPLES);
        double mean = computeAverageTemp();
        double sumSq = 0.0;
        for (uint16_t i = 0; i < n; i++) {
            uint16_t idx = (recentTempIndex + RATE_WINDOW_SAMPLES - n + i) % RATE_WINDOW_SAMPLES;
            double diff = recentTemps[idx] - mean;
            sumSq += diff * diff;
        }
        double sigma = sqrt(sumSq / static_cast<double>(n));
        return max(sigma, 0.1);  // floor at 0.1°F to avoid zero threshold
    }

    void updatePhase(unsigned long now, double chamberTemp, double ambientTemp) {
        lastAmbientTemp = ambientTemp;
        switch (phase) {
            case STABILIZE:
                handleStabilize(now, chamberTemp);
                break;
            case STEP_UP:
                handleStepUp(now, chamberTemp);
                break;
            case RECORD:
                handleRecord(now, chamberTemp, ambientTemp);
                break;
            case COOL_DOWN:
                handleCoolDown(now, chamberTemp);
                break;
            case COMPUTE:
                handleCompute();
                break;
            default:
                break;
        }
    }

    void handleStabilize(unsigned long now, double chamberTemp) {
        const BandConfig &band = bandConfigs[activeBandIndex];
        setHeaterCommand(band.baselineOutput);
        currentSetpoint = band.targetTemp;

        double rate = computeRate();

        if (rate < SETTLED_RATE_THRESHOLD) {
            if (settledSinceMs == 0) {
                settledSinceMs = now;
            }
            if (now - settledSinceMs >= STABILIZE_SETTLED_MS) {
                baselineTemp = computeAverageTemp();
                noiseStdDev = computeNoiseStdDev();
                LOG_INFOF("Band %u stabilized at %.1fF (baseline output=%.0f, noise sigma=%.3fF)",
                          activeBandIndex + 1, baselineTemp, band.baselineOutput, noiseStdDev);
                copyBandTrace();
                clearActiveSamples();
                setPhase(STEP_UP, now);
                return;
            }
        } else {
            settledSinceMs = 0;
        }

        if (now - phaseStartMs > STABILIZE_TIMEOUT_MS) {
            baselineTemp = computeAverageTemp();
            noiseStdDev = computeNoiseStdDev();
            LOG_WARNF("Band %u stabilize timeout, using T=%.1fF (noise sigma=%.3fF)",
                      activeBandIndex + 1, baselineTemp, noiseStdDev);
            copyBandTrace();
            clearActiveSamples();
            setPhase(STEP_UP, now);
        }
    }

    void handleStepUp(unsigned long now, double chamberTemp) {
        const BandConfig &band = bandConfigs[activeBandIndex];
        double steppedOutput = band.baselineOutput + band.stepDelta;
        setHeaterCommand(steppedOutput);
        currentSetpoint = baselineTemp + 30.0;  // visual hint on chart

        LOG_INFOF("Band %u step applied: heater %.0f -> %.0f",
                  activeBandIndex + 1, band.baselineOutput, steppedOutput);
        clearActiveSamples();
        setPhase(RECORD, now);
    }

    void handleRecord(unsigned long now, double chamberTemp, double ambientTemp) {
        const BandConfig &band = bandConfigs[activeBandIndex];
        double steppedOutput = band.baselineOutput + band.stepDelta;
        setHeaterCommand(steppedOutput);
        currentSetpoint = baselineTemp + 30.0;

        double rate = computeRate();

        // Check if new steady state reached
        if (now - phaseStartMs > 30000UL && rate < SETTLED_RATE_THRESHOLD) {
            if (settledSinceMs == 0) {
                settledSinceMs = now;
            }
            if (now - settledSinceMs >= RECORD_SETTLED_MS) {
                finishRecording(now, chamberTemp, ambientTemp);
                return;
            }
        } else if (rate >= SETTLED_RATE_THRESHOLD) {
            settledSinceMs = 0;
        }

        if (now - phaseStartMs > RECORD_TIMEOUT_MS) {
            LOG_WARNF("Band %u record timeout, fitting with available data", activeBandIndex + 1);
            finishRecording(now, chamberTemp, ambientTemp);
        }
    }

    void finishRecording(unsigned long now, double chamberTemp, double ambientTemp) {
        const BandConfig &band = bandConfigs[activeBandIndex];
        double finalTemp = computeAverageTemp();

        LOG_INFOF("Band %u recording complete: baseline=%.1fF, final=%.1fF, samples=%u",
                  activeBandIndex + 1, baselineTemp, finalTemp, activeSampleCount);

        BandResult &result = bandResults[activeBandIndex];
        result.sampleCount = activeSampleCount;
        result.minTemp = band.minTemp;
        result.maxTemp = band.maxTemp;
        result.targetTemp = band.targetTemp;

        fitFOPDT(result, band);

        totalSampleCount += activeSampleCount;
        copyBandTrace();

        // Transition to cool-down or next band
        setHeaterCommand(0.0);
        setPhase(COOL_DOWN, now);
    }

    void handleCoolDown(unsigned long now, double chamberTemp) {
        setHeaterCommand(0.0);
        currentSetpoint = 0.0;

        // Cool down to near the next band's baseline or to a safe starting point
        double coolTarget;
        if (activeBandIndex + 1 < totalBands) {
            coolTarget = bandConfigs[activeBandIndex + 1].targetTemp - 50.0;
            coolTarget = max(coolTarget, baselineTemp - 10.0);
        } else {
            coolTarget = 150.0;
        }

        // For the ascending band approach, we don't need to cool between bands
        // if the next band is at a higher temperature. Just advance directly.
        if (activeBandIndex + 1 < totalBands) {
            // Next band is higher temp — skip cool-down, go to stabilize
            activeBandIndex++;
            clearActiveSamples();
            setPhase(STABILIZE, now);
            return;
        }

        // After last band, brief cool then compute
        if (chamberTemp <= 200.0 || now - phaseStartMs > COOL_DOWN_TIMEOUT_MS) {
            setPhase(COMPUTE, now);
            return;
        }
    }

    void handleCompute() {
        computeAllGains();
        finish();
    }

    // =====================================================================
    // FOPDT Model Fitting
    // =====================================================================

    void fitFOPDT(BandResult &result, const BandConfig &band) {
        FOPDTModel &model = result.model;
        model.baselineTemp = baselineTemp;
        model.baselineOutput = band.baselineOutput;
        model.stepDelta = band.stepDelta;
        model.noiseStdDev = noiseStdDev;

        if (activeSampleCount < 20) {
            LOG_WARNF("Band %u insufficient samples (%u) for FOPDT fit",
                      activeBandIndex + 1, activeSampleCount);
            return;
        }

        // Determine final temperature from last 10% of samples
        uint16_t tailStart = activeSampleCount - max(static_cast<uint16_t>(10),
                             static_cast<uint16_t>(activeSampleCount / 10));
        double tailSum = 0.0;
        uint16_t tailCount = 0;
        for (uint16_t i = tailStart; i < activeSampleCount; i++) {
            tailSum += samples[i].actualTempF;
            tailCount++;
        }
        double finalTemp = tailCount > 0 ? tailSum / static_cast<double>(tailCount) : baselineTemp;
        model.finalTemp = finalTemp;

        double deltaT = finalTemp - baselineTemp;
        if (fabs(deltaT) < 1.0) {
            LOG_WARNF("Band %u temperature change too small (%.2fF)", activeBandIndex + 1, deltaT);
            return;
        }

        model.processGain = deltaT / band.stepDelta;

        // Detect response onset: first time T exceeds baseline + 3σ
        model.onsetDeadTime = detectOnsetDeadTime(deltaT);
        LOG_INFOF("Band %u onset dead time: %.2fs (noise sigma=%.3fF, threshold=%.2fF)",
                  activeBandIndex + 1, model.onsetDeadTime, noiseStdDev,
                  3.0 * noiseStdDev);

        // Method 1: Sundaresan-Krishnaswamy two-point method
        double target35 = baselineTemp + 0.353 * deltaT;
        double target85 = baselineTemp + 0.853 * deltaT;

        double t35 = -1.0, t85 = -1.0;
        for (uint16_t i = 1; i < activeSampleCount; i++) {
            double t = static_cast<double>(samples[i].elapsedMs) / 1000.0;
            double temp = samples[i].actualTempF;
            double prevTemp = samples[i - 1].actualTempF;

            if (t35 < 0.0 && deltaT > 0 ? (prevTemp < target35 && temp >= target35)
                                          : (prevTemp > target35 && temp <= target35)) {
                // Linear interpolation
                double frac = fabs(target35 - prevTemp) / max(fabs(temp - prevTemp), 1e-6);
                double prevT = static_cast<double>(samples[i - 1].elapsedMs) / 1000.0;
                t35 = prevT + frac * (t - prevT);
            }

            if (t85 < 0.0 && deltaT > 0 ? (prevTemp < target85 && temp >= target85)
                                          : (prevTemp > target85 && temp <= target85)) {
                double frac = fabs(target85 - prevTemp) / max(fabs(temp - prevTemp), 1e-6);
                double prevT = static_cast<double>(samples[i - 1].elapsedMs) / 1000.0;
                t85 = prevT + frac * (t - prevT);
            }
        }

        if (t35 < 0.0 || t85 < 0.0 || t85 <= t35) {
            LOG_WARNF("Band %u: S-K method failed (t35=%.1f, t85=%.1f), trying LS",
                      activeBandIndex + 1, t35, t85);
            fitFOPDT_LeastSquares(result, band, deltaT, finalTemp);
            return;
        }

        double skTau = 0.67 * (t85 - t35);
        double skTheta = 1.3 * t35 - 0.29 * t85;

        if (skTheta < 0.0) skTheta = 0.5;
        if (skTau < 1.0) skTau = 1.0;

        // Validate via RMSE
        double rmse = computeFOPDT_RMSE(baselineTemp, deltaT, skTau, skTheta);

        model.timeConstant = skTau;
        model.deadTime = skTheta;
        model.fitRmse = rmse;
        model.valid = true;

        LOG_INFOF("Band %u FOPDT (S-K): Kp=%.4f, tau=%.1fs, theta=%.1fs, RMSE=%.2fF",
                  activeBandIndex + 1, model.processGain, model.timeConstant,
                  model.deadTime, model.fitRmse);

        // Try least-squares as well, use whichever gives better RMSE
        FOPDTModel lsModel = model;
        BandResult lsResult = result;
        lsResult.model = lsModel;
        fitFOPDT_LeastSquares(lsResult, band, deltaT, finalTemp);

        if (lsResult.model.valid && lsResult.model.fitRmse < model.fitRmse * 0.9) {
            model.timeConstant = lsResult.model.timeConstant;
            model.deadTime = lsResult.model.deadTime;
            model.fitRmse = lsResult.model.fitRmse;
            LOG_INFOF("Band %u using LS fit (better RMSE): tau=%.1fs, theta=%.1fs, RMSE=%.2fF",
                      activeBandIndex + 1, model.timeConstant, model.deadTime, model.fitRmse);
        }

        result.valid = model.valid;
    }

    void fitFOPDT_LeastSquares(BandResult &result, const BandConfig &band,
                                double deltaT, double finalTemp) {
        FOPDTModel &model = result.model;

        double bestRmse = 1e9;
        double bestTau = 0.0;
        double bestTheta = 0.0;
        bool found = false;

        // Sweep dead time from 0 to 15s in 0.5s steps
        for (int thetaIdx = 0; thetaIdx <= 30; thetaIdx++) {
            double candidateTheta = static_cast<double>(thetaIdx) * 0.5;

            // For each candidate theta, fit tau via linear regression on:
            // ln((deltaT_final - (T(t) - T_baseline)) / deltaT_final) = -(t - theta) / tau
            double sumX = 0.0, sumY = 0.0, sumXY = 0.0, sumX2 = 0.0;
            uint16_t n = 0;

            for (uint16_t i = 0; i < activeSampleCount; i++) {
                double t = static_cast<double>(samples[i].elapsedMs) / 1000.0;
                if (t <= candidateTheta) continue;

                double tempRise = samples[i].actualTempF - baselineTemp;
                double residual = deltaT - tempRise;

                // Avoid log of non-positive numbers
                if ((deltaT > 0 && residual <= 0.01 * fabs(deltaT)) ||
                    (deltaT < 0 && residual >= -0.01 * fabs(deltaT))) continue;

                double ratio = residual / deltaT;
                if (ratio <= 0.01 || ratio >= 0.99) continue;

                double y = log(ratio);
                double x = t - candidateTheta;

                sumX += x;
                sumY += y;
                sumXY += x * y;
                sumX2 += x * x;
                n++;
            }

            if (n < 10) continue;

            double denom = n * sumX2 - sumX * sumX;
            if (fabs(denom) < 1e-12) continue;

            double slope = (n * sumXY - sumX * sumY) / denom;
            if (slope >= -1e-6) continue;  // slope should be negative (-1/tau)

            double candidateTau = -1.0 / slope;
            if (candidateTau < 1.0 || candidateTau > 300.0) continue;

            double rmse = computeFOPDT_RMSE(baselineTemp, deltaT, candidateTau, candidateTheta);

            if (rmse < bestRmse) {
                bestRmse = rmse;
                bestTau = candidateTau;
                bestTheta = candidateTheta;
                found = true;
            }
        }

        if (!found) {
            LOG_WARNF("Band %u LS FOPDT fit failed", activeBandIndex + 1);
            return;
        }

        if (bestTheta < 0.5) bestTheta = 0.5;

        model.timeConstant = bestTau;
        model.deadTime = bestTheta;
        model.fitRmse = bestRmse;
        model.valid = true;

        LOG_INFOF("Band %u FOPDT (LS): Kp=%.4f, tau=%.1fs, theta=%.1fs, RMSE=%.2fF",
                  activeBandIndex + 1, model.processGain, model.timeConstant,
                  model.deadTime, model.fitRmse);
    }

    double detectOnsetDeadTime(double deltaT) const {
        // Find the first time the temperature exceeds baseline + 3σ (for rising)
        // or drops below baseline - 3σ (for falling).
        // Uses 3σ threshold for 99.7% confidence above noise floor.
        double threshold = 3.0 * max(noiseStdDev, 0.1);
        bool rising = deltaT > 0;

        // Require 3 consecutive samples above threshold to filter noise spikes
        uint8_t consecutiveCount = 0;
        static constexpr uint8_t CONSECUTIVE_REQUIRED = 3;

        for (uint16_t i = 0; i < activeSampleCount; i++) {
            double deviation = samples[i].actualTempF - baselineTemp;
            bool crossed = rising ? (deviation > threshold) : (deviation < -threshold);

            if (crossed) {
                consecutiveCount++;
                if (consecutiveCount >= CONSECUTIVE_REQUIRED) {
                    // Onset is at the first of the consecutive samples
                    uint16_t onsetIdx = i - (CONSECUTIVE_REQUIRED - 1);
                    double onsetTime = static_cast<double>(samples[onsetIdx].elapsedMs) / 1000.0;
                    return max(onsetTime, 1.0);  // floor at 1 second
                }
            } else {
                consecutiveCount = 0;
            }
        }

        // If no clear onset found, fall back to a conservative estimate
        return max(noiseStdDev > 0.5 ? 5.0 : 2.0, 1.0);
    }

    double computeFOPDT_RMSE(double baseline, double deltaT,
                              double tau, double theta) const {
        double errorSum = 0.0;
        uint16_t count = 0;

        for (uint16_t i = 0; i < activeSampleCount; i++) {
            double t = static_cast<double>(samples[i].elapsedMs) / 1000.0;
            double predicted;
            if (t <= theta) {
                predicted = baseline;
            } else {
                predicted = baseline + deltaT * (1.0 - exp(-(t - theta) / tau));
            }
            double err = samples[i].actualTempF - predicted;
            errorSum += err * err;
            count++;
        }

        return count > 0 ? sqrt(errorSum / static_cast<double>(count)) : 1e9;
    }

    // =====================================================================
    // SIMC Gain Computation
    // =====================================================================

    void computeAllGains() {
        uint8_t validCount = 0;
        uint8_t primaryBand = 0;
        double bestBandDistance = 1e9;

        for (uint8_t i = 0; i < totalBands; i++) {
            BandResult &result = bandResults[i];
            if (!result.model.valid) continue;

            computeSIMC(result);
            validCount++;

            // Prefer the band closest to typical roasting midpoint (350°F)
            double dist = fabs(result.targetTemp - 350.0);
            if (dist < bestBandDistance) {
                bestBandDistance = dist;
                primaryBand = i;
            }
        }

        if (validCount == 0) {
            LOG_WARN("No valid FOPDT models — using seed gains");
            recommendedKp = seedGains[0];
            recommendedKi = seedGains[1];
            recommendedKd = seedGains[2];
            return;
        }

        const BandResult &primary = bandResults[primaryBand];
        recommendedKp = primary.kp;
        recommendedKi = primary.ki;
        recommendedKd = primary.kd;

        LOG_INFOF("Primary band %u (%.0fF): Kp=%.4f, Ki=%.6f, Kd=%.4f",
                  primaryBand + 1, primary.targetTemp,
                  recommendedKp, recommendedKi, recommendedKd);

        for (uint8_t i = 0; i < totalBands; i++) {
            const BandResult &br = bandResults[i];
            if (!br.model.valid) continue;
            LOG_INFOF("  Band %u: Kp_proc=%.4f tau=%.1fs theta_model=%.1fs theta_onset=%.1fs -> PID Kc=%.4f Ki=%.6f Kd=%.4f",
                      i + 1, br.model.processGain, br.model.timeConstant,
                      br.model.deadTime, br.model.onsetDeadTime,
                      br.kp, br.ki, br.kd);
        }
    }

    void computeSIMC(BandResult &result) const {
        const FOPDTModel &m = result.model;
        if (!m.valid || m.processGain == 0.0 || m.timeConstant <= 0.0) return;

        // Use the measured onset dead time for SIMC gain computation.
        // The FOPDT-fitted deadTime overestimates θ for higher-order thermal
        // systems, producing overly conservative gains and large ramp lag.
        double thetaOnset = max(m.onsetDeadTime, 1.0);
        double thetaModel = max(m.deadTime, 0.5);
        double tau = m.timeConstant;
        double kpProcess = fabs(m.processGain);
        double tauC = configTauCFactor * thetaOnset;

        // SIMC PI rules using onset dead time
        double Kc = tau / (kpProcess * (tauC + thetaOnset));
        double tauI = min(tau, 4.0 * (tauC + thetaOnset));
        double Ki = Kc / max(tauI, 0.1);

        // SIMC PID extension: use the larger of onset and model θ for
        // derivative term (conservative D avoids noise amplification)
        double thetaD = max(thetaOnset, thetaModel / 3.0);
        double tauD = thetaD / 3.0;
        double Kd = Kc * tauD;

        result.kp = constrain(Kc, KP_MIN, KP_MAX);
        result.ki = constrain(Ki, KI_MIN, KI_MAX);
        result.kd = constrain(Kd, KD_MIN, KD_MAX);

        LOG_INFOF("Band SIMC: theta_onset=%.1fs, theta_model=%.1fs, tau_c=%.1fs, Kc=%.3f, Ki=%.5f, Kd=%.3f",
                  thetaOnset, thetaModel, tauC, Kc, Ki, Kd);
    }

    // =====================================================================
    // Summary / Finish
    // =====================================================================

    void rebuildSummary() {
        uint8_t completed = 0;
        double fitRmseSum = 0.0;
        double worstRmse = 0.0;
        uint8_t validFits = 0;

        for (uint8_t i = 0; i < totalBands; i++) {
            if (bandResults[i].valid || bandResults[i].sampleCount > 0) {
                completed++;
            }
            if (bandResults[i].model.valid) {
                fitRmseSum += bandResults[i].model.fitRmse;
                worstRmse = max(worstRmse, bandResults[i].model.fitRmse);
                validFits++;
            }
        }

        summary.valid = recommendedKp > 0.0 || recommendedKi > 0.0;
        summary.passed = complete && validFits > 0;
        summary.recommendedKp = recommendedKp > 0 ? recommendedKp : seedGains[0];
        summary.recommendedKi = recommendedKi > 0 ? recommendedKi : seedGains[1];
        summary.recommendedKd = recommendedKd > 0 ? recommendedKd : seedGains[2];
        summary.tauCFactor = configTauCFactor;
        summary.completedBands = completed;
        summary.totalBands = totalBands;
        summary.currentSetpoint = currentSetpoint;
        summary.maxTemp = observedMaxTemp;
        summary.totalSamples = totalSampleCount + activeSampleCount;

        summary.meanFitRmse = validFits > 0 ? fitRmseSum / static_cast<double>(validFits) : 0.0;
        summary.worstFitRmse = worstRmse;
        summary.validBandCount = validFits;
        summary.completedCycles = completed;
        summary.maxCycles = totalBands;
        summary.targetTemp = totalBands > 0 ? bandConfigs[totalBands - 1].targetTemp : 0.0;

        for (uint8_t i = 0; i < totalBands && i < MAX_BANDS; i++) {
            summary.bands[i] = bandResults[i];
        }
    }

    void finish() {
        running = false;
        complete = true;
        phase = COMPLETE;
        copyError("none");

        rebuildSummary();
        summary.passed = summary.validBandCount > 0;
        summary.primaryBandIndex = 0;

        // Set primary band index
        double bestDist = 1e9;
        for (uint8_t i = 0; i < totalBands; i++) {
            if (!bandResults[i].valid) continue;
            double dist = fabs(bandResults[i].targetTemp - 350.0);
            if (dist < bestDist) {
                bestDist = dist;
                summary.primaryBandIndex = i;
            }
        }
        summary.bestCycleIndex = summary.primaryBandIndex;
        summary.bestScore = summary.worstFitRmse;

        LOG_INFOF("Step-response tuning complete: bands=%u/%u, valid=%u, Kp=%.4f Ki=%.6f Kd=%.4f",
                  summary.completedBands, summary.totalBands, summary.validBandCount,
                  recommendedKp, recommendedKi, recommendedKd);
    }

    void fail(const char *reason) {
        running = false;
        complete = false;
        phase = FAILED;
        copyError(reason);
        rebuildSummary();
        LOG_ERRORF("Step-response tuning failed: %s", reason);
    }
};

#endif // STEP_RESPONSE_TUNER_HPP

#ifndef STEP_RESPONSE_TUNER_HPP
#define STEP_RESPONSE_TUNER_HPP

#include <Arduino.h>
#include <math.h>
#include "PIDController.hpp"
#include "PIDRuntimeController.hpp"
#include "PIDValidation.hpp"
#include "../support/DebugLog.hpp"

class StepResponseTuner {
public:
    static constexpr uint8_t MAX_BANDS = 3;
    static constexpr uint16_t MAX_SAMPLES = 300;
    static constexpr unsigned long DEADLINE_MS = 295000;
    using Clock = unsigned long (*)();

    struct FOPDTModel {
        bool valid = false;
        double processGain = 0, timeConstant = 0, deadTime = 0, onsetDeadTime = 0;
        double fitRmse = 0, baselineTemp = 0, finalTemp = 0, baselineOutput = 80;
        double stepDelta = 50, noiseStdDev = 0, offset = 0, predictionRmse = 0;
    };
    struct BandResult {
        bool valid = false;
        FOPDTModel model;
        double kp = 0, ki = 0, kd = 0, minTemp = 0, maxTemp = 0, targetTemp = 0;
        uint16_t sampleCount = 0;
    };
    struct TraceSample {
        uint32_t elapsedMs = 0;
        float actualTempF = 0, setpointTempF = 0, heaterOutput = 0, ambientTempF = 0;
        float feedforward = 0, proportional = 0, integral = 0, derivative = 0;
        float kp = 0, ki = 0, kd = 0, targetRate = 0, fan = 0;
        uint8_t phaseId = 0;
    };
    struct Summary {
        bool valid = false, passed = false;
        double recommendedKp = 0, recommendedKi = 0, recommendedKd = 0, tauCFactor = 1;
        uint8_t completedBands = 0, totalBands = 1, primaryBandIndex = 0;
        double currentSetpoint = 0, maxTemp = 0;
        uint16_t totalSamples = 0;
        BandResult bands[MAX_BANDS];
        double targetTemp = 0, ambientTemp = 0, meanFitRmse = 0, worstFitRmse = 0;
        double observedPeakRate = 0, bestScore = 0, rampRateFps = 0;
        uint8_t validBandCount = 0, bestCycleIndex = 0, completedCycles = 0, maxCycles = 1;
        Calibration::CycleSummary cycles[Calibration::MAX_CYCLES] = {};
        double elapsedSeconds = 0;
        PIDValidationSession::Summary verification = {};
    };

    explicit StepResponseTuner(Clock timeSource = millis)
        : clock(timeSource), feedback(&actual, &target, &trim, -255, 255, 0, 0, 0) {
        feedback.setTimeStep(250);
    }

    void start(double temp, double, double, double, double fanSpeed = 255, double factor = 1.0) {
        summary = {};
        sampleCount = fitCount = 0;
        fitting = false;
        nextFitElapsed = 80000;
        previousFit = {};
        running = complete = false;
        cancelRequested = false;
        validation.reset();
        phase = IDLE;
        error = "none";
        heater = trim = feedforward = 0;
        lastSampleMs = lastMetricMs = 0;
        startMs = clock();
        fan = fanSpeed;
        tauFactor = factor;
        target = actual = temp;
        minAmbient = 1e9;
        maxAmbient = -1e9;
        noiseSum = noiseSq = 0;
        noiseCount = 0;
        if (!isfinite(temp) || temp < 32 || temp > 140 || !isfinite(factor) ||
            factor < 0.3 || factor > 3 || !isfinite(fan) || fan < 80 || fan > 255) {
            fail("invalid_start_conditions");
            return;
        }
        running = true;
        phase = PREPARE;
        summary.tauCFactor = factor;
        feedback.stop();
        LOG_INFO("Bounded PID tuning started; current settings will not be overwritten");
    }
    void stop() { if (running) fail("stopped"); }
    void cancel() { cancelRequested = true; }
    void abort(const char *reason) { if (running) fail(reason); }
    bool isRunning() const { return running; }
    bool isComplete() const { return complete; }
    bool isRecoveryCooling() const { return !running; }
    bool isVerifying() const { return phase == VERIFY; }
    void setRunId(uint32_t id) { runId = id; }
    uint32_t getRunId() const { return runId; }
    double getSetpoint() const { return target; }
    double getHeaterCommand() const { return heater; }
    uint16_t getSampleCount() const { return sampleCount; }
    uint8_t getActiveBandIndex() const { return 0; }
    const char *getLastError() const { return error; }
    Summary getSummary() const { return summary; }
    uint16_t getTraceSampleCount() const { return sampleCount; }
    TraceSample getTraceSample(uint16_t index) const { return index < sampleCount ? samples[index] : TraceSample{}; }
    double getProgressPercent() const { return running ? min(99.0, summary.elapsedSeconds / 2.95) : (phase == IDLE ? 0 : 100); }
    void getPID(double &p, double &i, double &d) const {
        p = summary.recommendedKp; i = summary.recommendedKi; d = summary.recommendedKd;
    }
    const char *getPhaseName() const {
        switch (phase) {
            case PREPARE: return "PREFLIGHT";
            case ACQUIRE: return "IDENTIFY";
            case FIT: return "FIT_MODEL";
            case VERIFY: return "VERIFY_RAMP_HOLD";
            case COMPLETE: return "VERIFIED_CANDIDATE";
            case FAILED: return "INCONCLUSIVE";
            default: return "IDLE";
        }
    }
    Calibration::CharacterizationSummary getCharacterizationSummary() const {
        Calibration::CharacterizationSummary result = {};
        result.valid = summary.valid;
        result.passed = summary.passed;
        result.recommendedKp = summary.recommendedKp;
        result.recommendedKi = summary.recommendedKi;
        result.recommendedKd = summary.recommendedKd;
        result.ambientTemp = summary.ambientTemp;
        result.maxTemp = summary.maxTemp;
        result.validBandCount = summary.validBandCount;
        result.totalSamples = sampleCount;
        result.completedCycles = summary.completedBands;
        result.maxCycles = 1;
        result.meanFitRmse = result.worstFitRmse = summary.meanFitRmse;
        const BandResult &b = summary.bands[0];
        auto &out = result.bands[0];
        out.valid = b.valid;
        out.targetTemp = b.targetTemp; out.minTemp = b.minTemp; out.maxTemp = b.maxTemp;
        out.processGain = b.model.processGain; out.timeConstant = b.model.timeConstant;
        out.deadTime = b.model.deadTime; out.fitRmse = b.model.fitRmse;
        out.kp = b.kp; out.ki = b.ki; out.kd = b.kd; out.sampleCount = b.sampleCount;
        if (b.valid) {
            out.coolingCoeff = 1.0 / b.model.timeConstant;
            out.heaterCoeff = b.model.processGain / b.model.timeConstant;
            // ponytail: drift holds offset/tau (absolute); feedforward no longer uses the live inlet, which warms with the heater.
            out.drift = b.model.offset / b.model.timeConstant;
        }
        return result;
    }

    double getOutput(double temp, double ambient, double fanCommand) {
        if (!running) return 0;
        unsigned long now = clock();
        summary.elapsedSeconds = (now - startMs) / 1000.0;
        if (cancelRequested) { fail("cancelled"); return 0; }
        if (now - startMs >= DEADLINE_MS) { fail("deadline_insufficient_data"); return 0; }
        if (!isfinite(temp) || !isfinite(ambient) || !isfinite(fanCommand) || temp < 32 || ambient < 20 ||
            temp >= 440 || fabs(fanCommand - fan) > 1) {
            fail("sensor_temperature_or_fan"); return 0;
        }
        if (ambient > temp + 30) { fail("sensor_pair_inconsistent"); return 0; }
        actual = temp;
        summary.maxTemp = max(summary.maxTemp, temp);
        summary.currentSetpoint = target;
        if (phase == PREPARE) {
            heater = 0;
            // ponytail: the inlet sensor warms with the heater, so ambient must only be steady before the step.
            minAmbient = min(minAmbient, ambient);
            maxAmbient = max(maxAmbient, ambient);
            if (now - startMs >= 10000) {
                if (maxAmbient - minAmbient > 5) { fail("ambient_unsteady_before_step"); return 0; }
                phase = ACQUIRE;
                acquireMs = now;
                summary.ambientTemp = ambient;
            }
        } else if (phase == ACQUIRE) {
            unsigned long elapsed = now - acquireMs;
            heater = elapsed < 30000 ? 80 : elapsed < 90000 ? 130 : 180;
            target = temp;
            if (elapsed >= 175000) {
                heater = 0;
                if (sampleCount < 150) {
                    fail("insufficient_data"); return 0;
                }
                phase = FIT;
                beginFit();
            } else {
                if (!fitting && elapsed >= nextFitElapsed) beginFit();
                if (fitting && advanceFit()) {
                    // Early finish only once the window spans ~2.5 time constants; slow plants need more data.
                    if (stableFit() && samples[fitCount - 1].elapsedMs >= 2500 * best.timeConstant)
                        finishFit(now, false);
                    previousFit = best;
                    previousFit.valid = best.fitRmse < 1e8;
                    fitting = false;
                    nextFitElapsed += 10000;
                }
            }
        } else if (phase == FIT) {
            if (advanceFit()) {
                if (previousFit.valid && !stableFit()) fail("unstable_model");
                else finishFit(now, true);
            }
        } else if (phase == VERIFY) {
            double elapsed = (now - verifyMs) / 1000.0;
            double rate = elapsed < 50 ? 0.25 : 0;
            target = verifyStart + min(elapsed, 50.0) * 0.25;
            auto decision = candidateRuntime.decide(now, actual, target, ambient, rate);
            feedforward = decision.feedforward;
            feedback.setOutputRange(-feedforward, 255 - feedforward);
            feedback.run(feedforward, now);
            heater = constrain(trim + feedforward, 0.0, 255.0);
            if (now - lastMetricMs >= 1000) {
                lastMetricMs = now;
                if ((elapsed >= 15 && elapsed < 50) || elapsed >= 65)
                    validation.recordSample(actual, target, rate, heater);
            }
            if (elapsed >= 85) {
                validation.finish(true, elapsed, "verification_complete");
                summary.verification = validation.getSummary();
                if (!summary.verification.passed) { fail("verification_failed"); return 0; }
                summary.passed = complete = true;
                running = false;
                phase = COMPLETE;
                summary.bands[0].maxTemp = max(summary.bands[0].maxTemp, target);
                heater = 0;
                LOG_INFO("PID candidate verified; explicit Apply required");
            }
        }
        if (now - lastSampleMs >= 1000) capture(now, ambient);
        summary.totalSamples = sampleCount;
        summary.verification = validation.getSummary();
        return heater;
    }

private:
    enum Phase { IDLE, PREPARE, ACQUIRE, FIT, VERIFY, COMPLETE, FAILED };
    Clock clock;
    Phase phase = IDLE;
    bool running = false, complete = false, fitting = false;
    volatile bool cancelRequested = false;
    const char *error = "none";
    unsigned long startMs = 0, acquireMs = 0, verifyMs = 0, lastSampleMs = 0, lastMetricMs = 0;
    unsigned long nextFitElapsed = 80000;
    double actual = 0, target = 0, heater = 0, trim = 0, feedforward = 0;
    double fan = 255, tauFactor = 1, fitTau = 5, verifyStart = 0;
    double minAmbient = 0, maxAmbient = 0, noiseSum = 0, noiseSq = 0;
    uint16_t noiseCount = 0, sampleCount = 0, fitCount = 0;
    uint32_t runId = 0;
    TraceSample samples[MAX_SAMPLES];
    Summary summary;
    FOPDTModel best, previousFit;
    PIDController feedback;
    PIDRuntimeController candidateRuntime;
    PIDValidationSession validation;

    void fail(const char *reason) {
        running = complete = false;
        heater = 0;
        phase = FAILED;
        error = reason;
        if (validation.isActive()) validation.finish(false, summary.elapsedSeconds, reason);
        summary.verification = validation.getSummary();
        summary.passed = false;
        LOG_WARNF("PID tuning not applied: %s", reason);
    }
    void capture(unsigned long now, double ambient) {
        lastSampleMs = now;
        if (sampleCount >= MAX_SAMPLES) { fail("trace_capacity"); return; }
        auto &s = samples[sampleCount++];
        s = {};
        s.elapsedMs = now - startMs; s.actualTempF = actual; s.setpointTempF = target;
        s.heaterOutput = heater; s.ambientTempF = ambient; s.phaseId = phase; s.fan = fan;
        s.feedforward = feedforward; s.proportional = feedback.getProportional();
        s.integral = feedback.getIntegralTerm(); s.derivative = feedback.getDerivative();
        s.kp = summary.recommendedKp; s.ki = summary.recommendedKi; s.kd = summary.recommendedKd;
        s.targetRate = phase == VERIFY && now - verifyMs < 50000 ? 0.25 : 0;
        if (phase == PREPARE) { noiseSum += actual; noiseSq += actual * actual; noiseCount++; }
        LOG_INFOF("PIDTRACE %lu,%lu,%.2f,%.2f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.0f,%.2f,%.3f,%.4f,%.2f,%u",
                  static_cast<unsigned long>(runId), static_cast<unsigned long>(s.elapsedMs), actual, target, heater, feedforward,
                  static_cast<double>(s.proportional), static_cast<double>(s.integral),
                  static_cast<double>(s.derivative), ambient, fan, static_cast<double>(s.targetRate),
                  summary.recommendedKp, summary.recommendedKi, summary.recommendedKd, s.phaseId);
    }

    bool evaluate(double tau, double theta, FOPDTModel &model) const {
        const uint16_t trainCount = fitCount * 4 / 5;
        double aa = 0, ab = 0, bb = 0, ay = 0, by = 0;
        double decay = 1, filteredOutput = 0;
        double initial = samples[0].actualTempF;
        uint16_t delayedIndex = 0;
        for (uint16_t j = 1; j < trainCount; ++j) {
            double dt = (samples[j].elapsedMs - samples[j - 1].elapsedMs) / 1000.0;
            double a = exp(-dt / tau);
            double delayedMs = samples[j - 1].elapsedMs - theta * 1000;
            while (delayedIndex + 1 < j && samples[delayedIndex + 1].elapsedMs <= delayedMs) delayedIndex++;
            double u = delayedMs >= samples[0].elapsedMs ? samples[delayedIndex].heaterOutput : 0;
            filteredOutput = a * filteredOutput + (1 - a) * u;
            decay *= a;
            double x = 1 - decay, z = filteredOutput;
            double y = samples[j].actualTempF - initial * decay;
            aa += x * x; ab += x * z; bb += z * z; ay += x * y; by += z * y;
        }
        double determinant = aa * bb - ab * ab;
        if (determinant < 1e-5) return false;
        model.offset = (ay * bb - by * ab) / determinant;
        model.processGain = (by * aa - ay * ab) / determinant;
        if (!isfinite(model.processGain) || model.processGain < 0.01 || model.processGain > 10 ||
            fabs(model.offset - summary.ambientTemp) > 20) return false;
        model.timeConstant = tau; model.deadTime = theta;
        double trainSq = 0, testSq = 0;
        decay = 1; filteredOutput = 0; delayedIndex = 0;
        for (uint16_t j = 1; j < fitCount; ++j) {
            double a = exp(-static_cast<double>(samples[j].elapsedMs - samples[j - 1].elapsedMs) / (1000.0 * tau));
            double delayedMs = samples[j - 1].elapsedMs - theta * 1000;
            while (delayedIndex + 1 < j && samples[delayedIndex + 1].elapsedMs <= delayedMs) delayedIndex++;
            double u = delayedMs >= samples[0].elapsedMs ? samples[delayedIndex].heaterOutput : 0;
            filteredOutput = a * filteredOutput + (1 - a) * u;
            decay *= a;
            double predicted = initial * decay + model.offset * (1 - decay) + model.processGain * filteredOutput;
            double residual = samples[j].actualTempF - predicted;
            if (j < trainCount) trainSq += residual * residual;
            else testSq += residual * residual;
        }
        model.fitRmse = sqrt(trainSq / (trainCount - 1));
        model.predictionRmse = sqrt(testSq / (fitCount - trainCount));
        return true;
    }
    void beginFit() {
        fitting = true;
        fitCount = sampleCount;
        fitTau = 5;
        best = {};
        best.fitRmse = 1e9;
    }
    bool advanceFit() {
        // ponytail: one time-constant slice per tick keeps sensors and safety responsive.
        for (double theta = 0; theta <= 20; theta += 0.5) {
            FOPDTModel candidate;
            if (evaluate(fitTau, theta, candidate) && candidate.fitRmse < best.fitRmse) best = candidate;
        }
        fitTau *= 1.12;
        return fitTau > 240;
    }
    bool stableFit() const {
        return previousFit.valid && best.fitRmse < 1e8 &&
               fabs(best.processGain / previousFit.processGain - 1) <= 0.1 &&
               fabs(best.timeConstant / previousFit.timeConstant - 1) <= 0.1 &&
               fabs(best.deadTime - previousFit.deadTime) <= 1;
    }
    void computeGains(BandResult &b) {
        double theta = max(b.model.deadTime, 1.0);
        double tauC = max(2.0, tauFactor * theta);
        b.kp = b.model.timeConstant / (b.model.processGain * (tauC + theta));
        b.ki = b.kp / min(b.model.timeConstant, 4 * (tauC + theta));
        // ponytail: SIMC PI avoids inventing derivative action from a first-order fit.
        b.kd = 0;
    }
    void finishFit(unsigned long now, bool finalAttempt) {
        if (best.fitRmse >= 1e8) { if (finalAttempt) fail("unidentifiable_model"); return; }
        // Refine the logarithmic grid locally without another unbounded search.
        FOPDTModel coarse = best;
        for (int i = -3; i <= 3; ++i) {
            for (int j = -2; j <= 2; ++j) {
                FOPDTModel refined;
                if (evaluate(coarse.timeConstant * (1 + i * 0.02), max(0.0, coarse.deadTime + j * 0.25), refined) &&
                    refined.fitRmse < best.fitRmse) best = refined;
            }
        }
        double sigma = noiseCount > 3 ? sqrt(max(0.0, noiseSq / noiseCount - pow(noiseSum / noiseCount, 2))) : 0.5;
        sigma = max(0.25, sigma);
        FOPDTModel slower, faster;
        bool ambiguous = (evaluate(best.timeConstant * 1.3, best.deadTime, slower) &&
                          slower.fitRmse <= best.fitRmse + max(0.03, sigma * 0.05)) ||
                         (evaluate(best.timeConstant * 0.7, best.deadTime, faster) &&
                          faster.fitRmse <= best.fitRmse + max(0.03, sigma * 0.05));
        if (ambiguous || best.fitRmse > max(1.0, 2 * sigma) || best.predictionRmse > max(1.5, 3 * sigma) ||
            best.timeConstant <= 5.1 || best.timeConstant >= 235 || best.deadTime >= 20 ||
            50 * best.processGain < max(5.0, 6 * sigma)) {
            if (finalAttempt) fail("low_confidence_model");
            return;
        }
        BandResult b;
        b.model = best;
        b.valid = b.model.valid = true;
        b.model.noiseStdDev = sigma;
        b.model.baselineTemp = samples[0].actualTempF;
        b.model.finalTemp = best.offset + best.processGain * 180;
        b.minTemp = 1e9; b.maxTemp = -1e9;
        for (uint16_t i = 0; i < fitCount; ++i) {
            b.minTemp = min(b.minTemp, static_cast<double>(samples[i].actualTempF));
            b.maxTemp = max(b.maxTemp, static_cast<double>(samples[i].actualTempF));
        }
        b.targetTemp = (b.minTemp + b.maxTemp) / 2;
        b.sampleCount = fitCount;
        computeGains(b);
        if (b.kp > 200 || b.ki > 20 || !isfinite(b.kp) || !isfinite(b.ki)) {
            if (finalAttempt) fail("unsafe_gain_range");
            return;
        }
        if (actual + 12.5 >= 435) { if (finalAttempt) fail("verification_temperature"); return; }
        summary.bands[0] = b;
        summary.valid = true; summary.completedBands = summary.validBandCount = summary.completedCycles = 1;
        summary.meanFitRmse = summary.worstFitRmse = best.fitRmse;
        summary.recommendedKp = b.kp; summary.recommendedKi = b.ki; summary.recommendedKd = 0;
        summary.tauCFactor = tauFactor;
        candidateRuntime.loadFromSummary(getCharacterizationSummary());
        feedback.setGains(b.kp, b.ki, b.kd);
        feedback.reset(now);
        verifyStart = actual;
        validation.begin(actual, actual + 12.5);
        verifyMs = now;
        phase = VERIFY;
    }
};
#endif

#ifndef PID_VALIDATION_HPP
#define PID_VALIDATION_HPP

#include <Arduino.h>
#include <math.h>
#include <string.h>
#include "../profiles/RoastProfile.hpp"

class PIDValidationSession {
public:
    static constexpr uint32_t VALIDATION_HOLD_TEMP_F = 120;
    static constexpr uint32_t VALIDATION_RAMP_START_TEMP_F = 150;
    static constexpr uint32_t VALIDATION_RAMP_END_TEMP_F = 200;
    static constexpr uint32_t VALIDATION_HOLD_MS = 15000;
    static constexpr uint32_t VALIDATION_STEP_MS = 16000;
    static constexpr uint32_t VALIDATION_RAMP_END_MS = 116000;
    static constexpr uint32_t VALIDATION_FINAL_HOLD_MS = 131000;

    struct Summary {
        bool active;
        bool complete;
        bool passed;
        bool cancelled;
        double startTemp;
        double finalTargetTemp;
        double durationSeconds;
        double meanAbsError;
        double rmse;
        double maxAbsError;
        double holdMeanAbsError;
        double holdMaxAbsError;
        double withinOneDegreePercent;
        double withinTwoDegreesPercent;
        uint16_t sampleCount;
        uint16_t holdSampleCount;
        uint16_t rampSampleCount;
        double rampMeanAbsError;
        double rampWithinThreeDegreesPercent;
        double meanSignedError;
        double meanLagSeconds;
        double saturatedPercent;
        double maxOvershoot;
        char lastError[32];
    };

    PIDValidationSession() {
        reset();
    }

    void reset() {
        summary = {};
        strlcpy(summary.lastError, "none", sizeof(summary.lastError));
        absErrorSum = 0.0;
        squaredErrorSum = 0.0;
        holdAbsErrorSum = 0.0;
        rampAbsErrorSum = signedErrorSum = lagSum = 0.0;
        rampWithinThree = saturatedCount = 0;
        withinOneDegreeCount = 0;
        withinTwoDegreesCount = 0;
    }

    void start(RoastProfile &profileToBuild, double currentTemp, double finalTargetTemp = 300.0, uint32_t fanPercent = 70) {
        (void)finalTargetTemp;
        reset();
        summary.active = true;
        summary.complete = false;
        summary.cancelled = false;
        summary.startTemp = currentTemp;
        summary.finalTargetTemp = VALIDATION_RAMP_END_TEMP_F;
        buildValidationProfile(profileToBuild, constrain(fanPercent, 20U, 100U));
    }

    bool isActive() const { return summary.active; }
    bool isComplete() const { return summary.complete; }
    Summary getSummary() const { return summary; }

    void begin(double currentTemp, double targetTemp) {
        reset();
        summary.active = true;
        summary.startTemp = currentTemp;
        summary.finalTargetTemp = targetTemp;
    }

    void recordSample(double actualTemp, double targetTemp, double targetRate = 0.0,
                      double heaterOutput = 0.0) {
        if (!summary.active || summary.complete) {
            return;
        }

        double error = actualTemp - targetTemp;
        double absError = fabs(error);
        summary.sampleCount++;
        absErrorSum += absError;
        squaredErrorSum += error * error;
        signedErrorSum += error;
        summary.maxOvershoot = max(summary.maxOvershoot, error);
        if (heaterOutput >= 254.0) saturatedCount++;
        summary.maxAbsError = max(summary.maxAbsError, absError);
        if (absError <= 1.0) {
            withinOneDegreeCount++;
        }
        if (absError <= 2.0) {
            withinTwoDegreesCount++;
        }

        if (fabs(targetRate) < 1e-6) {
            summary.holdSampleCount++;
            holdAbsErrorSum += absError;
            summary.holdMaxAbsError = max(summary.holdMaxAbsError, absError);
        } else {
            summary.rampSampleCount++;
            rampAbsErrorSum += absError;
            lagSum += -error / targetRate;
            if (absError <= 3.0) rampWithinThree++;
        }
    }

    void finish(bool completed, double durationSeconds, const char *reason) {
        if (!summary.active && summary.complete) {
            return;
        }

        summary.active = false;
        summary.complete = true;
        summary.cancelled = !completed;
        summary.durationSeconds = durationSeconds;
        strlcpy(summary.lastError, reason ? reason : "unknown", sizeof(summary.lastError));

        if (summary.sampleCount > 0) {
            summary.meanAbsError = absErrorSum / static_cast<double>(summary.sampleCount);
            summary.rmse = sqrt(squaredErrorSum / static_cast<double>(summary.sampleCount));
            summary.withinOneDegreePercent = 100.0 * static_cast<double>(withinOneDegreeCount) / static_cast<double>(summary.sampleCount);
            summary.withinTwoDegreesPercent = 100.0 * static_cast<double>(withinTwoDegreesCount) / static_cast<double>(summary.sampleCount);
            summary.meanSignedError = signedErrorSum / summary.sampleCount;
            summary.saturatedPercent = 100.0 * saturatedCount / summary.sampleCount;
        }

        if (summary.holdSampleCount > 0) {
            summary.holdMeanAbsError = holdAbsErrorSum / static_cast<double>(summary.holdSampleCount);
        }
        if (summary.rampSampleCount > 0) {
            summary.rampMeanAbsError = rampAbsErrorSum / summary.rampSampleCount;
            summary.rampWithinThreeDegreesPercent = 100.0 * rampWithinThree / summary.rampSampleCount;
            summary.meanLagSeconds = lagSum / summary.rampSampleCount;
        }

        summary.passed = completed &&
                         summary.sampleCount >= 30 &&
                         summary.rampSampleCount >= 15 && summary.holdSampleCount >= 10 &&
                         summary.rampMeanAbsError <= 2.0 &&
                         summary.rampWithinThreeDegreesPercent >= 90.0 &&
                         summary.maxOvershoot <= 4.0 &&
                         summary.holdMeanAbsError <= 1.0 && summary.saturatedPercent < 20.0;
    }

    static void buildValidationProfile(RoastProfile &profileToBuild, uint32_t fanPercent = 70) {
        profileToBuild.clearSetpoints();
        profileToBuild.addSetpoint(0, VALIDATION_HOLD_TEMP_F, fanPercent);
        profileToBuild.addSetpoint(VALIDATION_HOLD_MS, VALIDATION_HOLD_TEMP_F, fanPercent);
        profileToBuild.addSetpoint(VALIDATION_STEP_MS, VALIDATION_RAMP_START_TEMP_F, fanPercent);
        profileToBuild.addSetpoint(VALIDATION_RAMP_END_MS, VALIDATION_RAMP_END_TEMP_F, fanPercent);
        profileToBuild.addSetpoint(VALIDATION_FINAL_HOLD_MS, VALIDATION_RAMP_END_TEMP_F, fanPercent);
    }

private:
    Summary summary = {};
    double absErrorSum = 0.0;
    double squaredErrorSum = 0.0;
    double holdAbsErrorSum = 0.0;
    double rampAbsErrorSum = 0.0;
    double signedErrorSum = 0.0;
    double lagSum = 0.0;
    uint16_t rampWithinThree = 0;
    uint16_t saturatedCount = 0;
    uint16_t withinOneDegreeCount = 0;
    uint16_t withinTwoDegreesCount = 0;
};

#endif
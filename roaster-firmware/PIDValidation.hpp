#ifndef PID_VALIDATION_HPP
#define PID_VALIDATION_HPP

#include <Arduino.h>
#include <math.h>
#include <string.h>
#include "Profiles.hpp"

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
        lastTargetTemp = 0.0;
        hasLastTargetTemp = false;
        withinOneDegreeCount = 0;
        withinTwoDegreesCount = 0;
    }

    void start(Profiles &profileToBuild, double currentTemp, double finalTargetTemp = 300.0, uint32_t fanPercent = 70) {
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

    void recordSample(double actualTemp, double targetTemp) {
        if (!summary.active || summary.complete) {
            return;
        }

        double error = actualTemp - targetTemp;
        double absError = fabs(error);
        summary.sampleCount++;
        absErrorSum += absError;
        squaredErrorSum += error * error;
        summary.maxAbsError = max(summary.maxAbsError, absError);
        if (absError <= 1.0) {
            withinOneDegreeCount++;
        }
        if (absError <= 2.0) {
            withinTwoDegreesCount++;
        }

        if (hasLastTargetTemp && fabs(targetTemp - lastTargetTemp) <= 0.25) {
            summary.holdSampleCount++;
            holdAbsErrorSum += absError;
            summary.holdMaxAbsError = max(summary.holdMaxAbsError, absError);
        }

        lastTargetTemp = targetTemp;
        hasLastTargetTemp = true;
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
        }

        if (summary.holdSampleCount > 0) {
            summary.holdMeanAbsError = holdAbsErrorSum / static_cast<double>(summary.holdSampleCount);
        }

        summary.passed = completed &&
                         summary.sampleCount >= 30 &&
                         summary.meanAbsError <= 1.5 &&
                         summary.withinTwoDegreesPercent >= 90.0 &&
                         summary.maxAbsError <= 4.0 &&
                         (summary.holdSampleCount == 0 || (summary.holdMeanAbsError <= 1.0 && summary.holdMaxAbsError <= 2.0));
    }

    static void buildValidationProfile(Profiles &profileToBuild, uint32_t fanPercent = 70) {
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
    double lastTargetTemp = 0.0;
    bool hasLastTargetTemp = false;
    uint16_t withinOneDegreeCount = 0;
    uint16_t withinTwoDegreesCount = 0;
};

#endif
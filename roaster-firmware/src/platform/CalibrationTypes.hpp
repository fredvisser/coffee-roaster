#ifndef CALIBRATION_TYPES_HPP
#define CALIBRATION_TYPES_HPP

#include <Arduino.h>

// Shared calibration data structures used by the step-response tuner,
// PID runtime controller, and network API.

namespace Calibration {

static constexpr uint8_t BAND_COUNT = 3;
static constexpr uint8_t MAX_CYCLES = 5;

struct BandCharacterization {
    bool valid;
    double targetTemp;
    double minTemp;
    double maxTemp;
    double drift;
    double coolingCoeff;
    double heaterCoeff;
    double processGain;
    double timeConstant;
    double deadTime;
    double fitRmse;
    double avgRampRate;
    double maxRampRate;
    double kp;
    double ki;
    double kd;
    uint16_t sampleCount;
};

struct CycleSummary {
    bool valid;
    bool passed;
    bool modelValid;
    uint8_t cycleIndex;
    double ambientTemp;
    double startTemp;
    double endTemp;
    double kp;
    double ki;
    double kd;
    double meanAbsError;
    double rmse;
    double maxOvershoot;
    double maxUndershoot;
    double fitRmse;
    double processGain;
    double timeConstant;
    double deadTime;
    double peakRampRate;
    double controlVariation;
    double score;
    uint16_t oscillationCount;
    uint16_t sampleCount;
};

struct CharacterizationSummary {
    bool valid;
    bool passed;
    double targetTemp;
    double ambientTemp;
    double recommendedKp;
    double recommendedKi;
    double recommendedKd;
    double meanFitRmse;
    double worstFitRmse;
    double observedPeakRate;
    double maxTemp;
    uint16_t totalSamples;
    uint8_t validBandCount;
    BandCharacterization bands[BAND_COUNT];
    uint8_t completedCycles;
    uint8_t maxCycles;
    uint8_t bestCycleIndex;
    double bestScore;
    double rampRateFps;
    double currentSetpoint;
    CycleSummary cycles[MAX_CYCLES];
};

} // namespace Calibration

#endif

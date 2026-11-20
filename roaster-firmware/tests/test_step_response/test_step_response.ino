#include "../../src/control/StepResponseTuner.hpp"
#include <AUnit.h>

using namespace aunit;
static unsigned long virtualMs = 1000;
static unsigned long virtualClock() { return virtualMs; }

void setup() {
    Serial.begin(115200);
    TestRunner::setTimeout(60);
}
void loop() { TestRunner::run(); }

test(BoundedTuning_IdentifiesAndVerifiesDelayedPlant) {
    StepResponseTuner tuner(virtualClock);
    tuner.start(70, 8, 0.46, 0, 180);
    double temp = 70;
    double delayLine[20] = {};
    for (unsigned j = 0; j < 1250 && tuner.isRunning(); ++j) {
        double command = tuner.getOutput(temp, 70, 180);
        temp += (70 + delayLine[j % 20] - temp) * 0.25 / 60;
        delayLine[j % 20] = command;
        virtualMs += 250;
    }
    auto summary = tuner.getSummary();
    assertTrue(tuner.isComplete());
    assertTrue(summary.passed);
    assertTrue(summary.elapsedSeconds < 295);
    assertNear(summary.bands[0].model.processGain, 1.0, 0.05);
    assertNear(summary.bands[0].model.timeConstant, 60.0, 6.0);
    assertNear(summary.bands[0].model.deadTime, 5.0, 1.0);
    assertTrue(summary.verification.rampMeanAbsError <= 2);
    assertTrue(summary.verification.holdMeanAbsError <= 1);
    assertTrue(tuner.getTraceSampleCount() >= floor(summary.elapsedSeconds));

    PIDRuntimeController runtime;
    assertTrue(runtime.loadFromSummary(tuner.getCharacterizationSummary()));
    auto hold = runtime.decide(virtualMs, 175, 175, 70, 0);
    assertNear(hold.feedforward, 105.0, 6.0);
}

test(BoundedTuning_RejectsNoResponse) {
    StepResponseTuner tuner(virtualClock);
    tuner.start(70, 8, 0.46, 0, 180);
    for (unsigned j = 0; j < 1250 && tuner.isRunning(); ++j) {
        tuner.getOutput(70, 70, 180);
        virtualMs += 250;
    }
    assertFalse(tuner.isRunning());
    assertFalse(tuner.getSummary().passed);
    assertEqual(tuner.getHeaterCommand(), 0.0);
}

test(BoundedTuning_CancelDeadlineAndSensorFaultTurnHeatOff) {
    StepResponseTuner tuner(virtualClock);
    tuner.start(70, 8, 0.46, 0, 180);
    tuner.cancel();
    assertEqual(tuner.getOutput(70, 70, 180), 0.0);
    assertFalse(tuner.isRunning());
    tuner.start(70, 8, 0.46, 0, 180);
    virtualMs += 295000;
    assertEqual(tuner.getOutput(70, 70, 180), 0.0);
    assertFalse(tuner.getSummary().passed);
    tuner.start(70, 8, 0.46, 0, 180);
    assertEqual(tuner.getOutput(NAN, 70, 180), 0.0);
    assertFalse(tuner.isRunning());
}

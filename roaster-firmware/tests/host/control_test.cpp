#include <cassert>
#include "../../src/control/StepResponseTuner.hpp"
#include "../../src/control/PIDTuningStore.hpp"
#include "../../src/control/HeaterRelay.hpp"

unsigned long testMillis = 1000;

static void checkHeaterRelay() {
    HeaterRelay relay(1);
    for (unsigned duty : {80, 130, 180}) {
        relay.setPWM(duty);
        for (unsigned i = 0; i < 1000; ++i) {
            ++testMillis;
            relay.tick();
        }
        unsigned onMs = 0;
        for (unsigned i = 0; i < 30000; ++i) {
            ++testMillis;
            if (i % 250 == 0) relay.setPWM(duty);
            relay.tick();
            if (testPinLevels[1] == HIGH) ++onMs;
        }
        assert(fabs(onMs / 30000.0 - duty / 255.0) < 0.002);
        printf("Heater PWM %u: measured %.2f%% GPIO duty\n", duty, onMs * 100.0 / 30000);
    }
    relay.setPWM(255);
    testMillis += 1000;
    relay.tick();
    assert(testPinLevels[1] == HIGH);
    relay.setPWM(0);
    assert(testPinLevels[1] == LOW);
    testMillis += 1000;
    relay.tick();
    assert(testPinLevels[1] == LOW);
    testMillis = UINT32_MAX - 100;
    relay.setPWM(180);
    unsigned wrappedOnMs = 0;
    for (unsigned i = 0; i < 2000; ++i) {
        ++testMillis;
        relay.tick();
        if (testPinLevels[1] == HIGH) ++wrappedOnMs;
    }
    assert(wrappedOnMs >= 1400 && wrappedOnMs <= 1420);
    relay.setPWM(0);
    testMillis += 1000;
    relay.tick();
    assert(testPinLevels[1] == LOW);
}

static void checkTuning(double gain = 1, double tau = 60, double noise = 0, bool disturbed = false,
                        bool switchedHeater = false, bool warmingInlet = false) {
    StepResponseTuner tuner;
    tuner.start(70, 8, 0.46, 0, 180, 1);
    double temp = 70;
    double delayed[20] = {};
    HeaterRelay plantHeater(2);
    double delayedGpio[1000] = {};
    for (unsigned index = 0; tuner.isRunning() && index < 1300; ++index) {
        // The real inlet thermocouple warms ~13% of the bean rise; that must not reject the run.
        double inlet = warmingInlet ? 70 + 0.13 * (temp - 70) : 70;
        double command = tuner.getOutput(temp + noise * sin(index * 2.1), inlet, 180);
        double effectiveGain = disturbed && tuner.isVerifying() ? gain * 0.5 : gain;
        if (switchedHeater) {
            plantHeater.setPWM(command);
            for (unsigned j = 0; j < 50; ++j) {
                plantHeater.tick();
                unsigned slot = (index * 50 + j) % 1000;
                temp += (70 + effectiveGain * delayedGpio[slot] - temp) * 0.005 / tau;
                delayedGpio[slot] = testPinLevels[2] == HIGH ? 255 : 0;
                testMillis += 5;
            }
        } else {
            temp += (70 + effectiveGain * delayed[index % 20] - temp) * 0.25 / tau;
            delayed[index % 20] = command;
            testMillis += 250;
        }
    }
    auto summary = tuner.getSummary();
    printf("tuning: %s, %.2fs, K=%.4f tau=%.3f delay=%.2f fit=%.3f prediction=%.3f ramp=%.3f hold=%.3f\n",
           tuner.getLastError(), summary.elapsedSeconds, summary.bands[0].model.processGain,
           summary.bands[0].model.timeConstant, summary.bands[0].model.deadTime,
           summary.bands[0].model.fitRmse, summary.bands[0].model.predictionRmse,
           summary.verification.rampMeanAbsError, summary.verification.holdMeanAbsError);
    assert(summary.valid);
    assert(fabs(summary.bands[0].model.processGain - gain) < 0.05 * gain);
    assert(fabs(summary.bands[0].model.timeConstant - tau) < 0.1 * tau);
    assert(fabs(summary.bands[0].model.deadTime - 5) <= 1);
    assert(summary.elapsedSeconds < 295);
    if (disturbed) {
        assert(!tuner.isComplete() && !summary.passed && tuner.getHeaterCommand() == 0);
        return;
    }
    assert(tuner.isComplete() && summary.passed);
    assert(summary.elapsedSeconds < 270);
    PIDRuntimeController runtime;
    assert(runtime.loadFromSummary(tuner.getCharacterizationSummary()));
    auto hold = runtime.decide(testMillis, 175, 175, 70, 0);
    assert(fabs(hold.feedforward - 105 / gain) < 6);
    auto invalidModel = tuner.getCharacterizationSummary();
    invalidModel.bands[0].drift = NAN;
    assert(!runtime.loadFromSummary(invalidModel));
    assert(tuner.getTraceSampleCount() >= floor(summary.elapsedSeconds));
    assert(tuner.getTraceSample(0).elapsedMs <= 1000);
    assert(summary.elapsedSeconds * 1000 - tuner.getTraceSample(tuner.getTraceSampleCount() - 1).elapsedMs < 1000);

    StepResponseTuner noHeat;
    noHeat.start(70, 8, 0.46, 0, 180);
    for (unsigned i = 0; i < 1250 && noHeat.isRunning(); ++i) {
        noHeat.getOutput(70, 70, 180);
        testMillis += 250;
    }
    assert(!noHeat.isRunning() && !noHeat.isComplete() && noHeat.getHeaterCommand() == 0);
    assert(!noHeat.getSummary().passed);
}

static void checkPersistence() {
    Preferences prefs;
    PIDTuningStore store(prefs);
    PIDTuningStore::Configuration original, changed;
    changed.kp = 10;
    assert(store.saveActive(changed, original));
    assert(store.saveKnownGood(changed));
    PIDTuningStore rebooted(prefs);
    PIDTuningStore::Configuration saved;
    assert(rebooted.readActive(saved) && saved.kp == 10);
    assert(rebooted.readPrevious(saved) && saved.kp == 8);
    assert(rebooted.readKnownGood(saved) && saved.kp == 10);
    prefs.failWrites = true;
    changed.kp = 20;
    assert(!rebooted.saveActive(changed, original));
    assert(!rebooted.saveKnownGood(changed));
    assert(rebooted.readActive(saved) && saved.kp == 10);
    prefs.failWrites = false;
    assert(rebooted.saveActive(changed, original));
    assert(rebooted.readKnownGood(saved) && saved.kp == 10);
    PIDTuningStore::Record record;
    for (unsigned j = 0; j < 12; ++j) assert(store.append(record));
    assert(store.latestId() == 12);
    assert(rebooted.readKnownGood(saved) && saved.kp == 10);
    assert(!store.readRecord(1, record) && !store.readRecord(2, record));
    assert(store.readRecord(3, record));
    assert(store.readRecord(12, record));
    record.proposed.kp = 199;
    assert(prefs.putBytes("pid_run_2", &record, sizeof(record)) == sizeof(record));
    assert(!store.readRecord(12, record));
    assert(!PIDTuningStore::validGains(NAN, 1, 0));
    // v2 records keep their gains but lose the old inlet-relative feedforward bands.
    PIDTuningStore::Record legacy;
    legacy.version = 2; legacy.id = 13; legacy.proposed.kp = 9; legacy.proposed.bands[0].valid = 1;
    uint32_t fnv = 2166136261U;
    for (size_t j = 0; j < offsetof(PIDTuningStore::Record, checksum); ++j)
        fnv = (fnv ^ reinterpret_cast<const uint8_t *>(&legacy)[j]) * 16777619U;
    legacy.checksum = fnv;
    assert(prefs.putBytes("pid_run_3", &legacy, sizeof(legacy)) == sizeof(legacy));
    assert(store.readRecord(13, record) && record.proposed.kp == 9 && !record.proposed.bands[0].valid);
    static_assert(sizeof(PIDTuningStore::Record) <= 600, "Keep ten history summaries compact");
}

static void checkInconclusivePlants() {
    for (unsigned scenario = 0; scenario < 3; ++scenario) {
        StepResponseTuner tuner;
        tuner.start(70, 8, 0.46, 0, 180);
        double temp = 70, intermediate = 70;
        double delayed[20] = {};
        for (unsigned index = 0; tuner.isRunning() && index < 1300; ++index) {
            double ambient = scenario == 1 ? 70 + std::min(index, 40u) * 0.2 : 70;
            double gain = scenario == 2 ? 0.02 : 1;
            double command = tuner.getOutput(temp, ambient, 180);
            intermediate += (ambient + gain * delayed[index % 20] - intermediate) * 0.25 / 60;
            temp = scenario == 0 ? temp + (intermediate - temp) * 0.25 / 60 : intermediate;
            delayed[index % 20] = command;
            testMillis += 250;
        }
        assert(!tuner.isRunning() && !tuner.getSummary().passed && tuner.getHeaterCommand() == 0);
        assert(tuner.getSummary().elapsedSeconds <= 295);
    }
}

int main() {
    checkHeaterRelay();
    checkTuning();
    checkTuning(0.9, 50, 0.2);
    checkTuning(1, 60, 0, true);
    checkTuning(1, 60, 0, false, true);
    checkTuning(1, 60, 0, false, false, true);
    checkPersistence();
    checkInconclusivePlants();
    RoastProfile profile;
    profile.clearSetpoints();
    profile.addSetpoint(0, 150, 70);
    profile.addSetpoint(100000, 200, 70);
    profile.startProfile(150, testMillis);
    assert(fabs(profile.getControlTarget(testMillis + 250) - 150.125) < 1e-6);
    assert(fabs(profile.getTargetRate(testMillis + 250) - 0.5) < 1e-6);
    PIDValidationSession validation;
    validation.begin(150, 200);
    for (int i = 0; i < 50; ++i) validation.recordSample(150 + i * 0.5, 150 + i * 0.5, 0.5);
    assert(validation.getSummary().holdSampleCount == 0);

    double actual = 180, target = 175, trim = 0;
    PIDController pid(&actual, &target, &trim, -100, 155, 4, 0.1, 0);
    pid.setTimeStep(250);
    pid.run(100);
    testMillis += 250;
    pid.run(100);
    assert(trim < 0 && trim + 100 >= 0);
    actual = 0;
    target = 400;
    pid.stop();
    pid.setOutputRange(-200, 55);
    pid.run(200);
    testMillis += 250;
    pid.run(200);
    assert(pid.getIntegral() == 0);
    pid.stop();
    actual = 70;
    target = 72;
    pid.setOutputRange(-100, 155);
    pid.setGains(4, 0.4, 0);
    pid.run(100);
    testMillis += 250;
    pid.run(100);
    double beforeGainChange = trim;
    pid.setGains(5, 0.6, 0);
    testMillis += 250;
    pid.run(100);
    assert(fabs(trim - beforeGainChange - 0.3) < 1e-6);
    pid.stop();
    assert(pid.getProportional() == 0 && pid.getIntegralTerm() == 0 && pid.getDerivative() == 0);
    StepResponseTuner cancelled;
    cancelled.start(70, 8, 0.46, 0, 180);
    cancelled.cancel();
    assert(cancelled.getOutput(70, 70, 180) == 0 && !cancelled.isRunning());
    StepResponseTuner timedOut;
    timedOut.start(70, 8, 0.46, 0, 180);
    testMillis += 295000;
    assert(timedOut.getOutput(70, 70, 180) == 0 && !timedOut.isComplete());
    StepResponseTuner invalid;
    invalid.start(70, 8, 0.46, 0, 180);
    assert(invalid.getOutput(NAN, 70, 180) == 0 && !invalid.isRunning());
    invalid.start(70, 8, 0.46, 0, 180);
    assert(invalid.getOutput(70, 70, NAN) == 0 && !invalid.isRunning());
    invalid.start(80, 8, 0.46, 0, 255);
    testMillis += 15000;
    assert(invalid.getOutput(32, 77.9, 255) == 0 && !invalid.isRunning());
    assert(strcmp(invalid.getLastError(), "sensor_pair_inconsistent") == 0);
    puts("Control regressions passed");
}

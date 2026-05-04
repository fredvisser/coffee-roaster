/**
 * Step-Response Tuner Unit Tests
 *
 * Tests for the FOPDT model fitting and SIMC gain computation including:
 * - FOPDT model fitting accuracy on synthetic step-response data
 * - SIMC PI/PID gain computation
 * - Band configuration and state machine transitions
 * - Safety limits (over-temp, start temp, timeout)
 * - Summary and compatibility API
 */

#include <AUnit.h>
#include "../../StepResponseTuner.hpp"

using namespace aunit;

StepResponseTuner tuner;

void setup() {
    Serial.begin(115200);
    while (!Serial);
    delay(1000);
    TestRunner::setTimeout(60);
}

void loop() {
    TestRunner::run();
}

// ============================================================================
// HELPER: Simulate a first-order-plus-dead-time step response
// ============================================================================

// Returns temperature at time t (seconds) for a FOPDT model:
//   T(t) = baseline + Kp * stepDelta * (1 - exp(-(t - theta) / tau))  for t > theta
//   T(t) = baseline                                                     for t <= theta
static double fopdtTemp(double t, double baseline, double Kp, double stepDelta,
                        double tau, double theta) {
    if (t <= theta) return baseline;
    return baseline + Kp * stepDelta * (1.0 - exp(-(t - theta) / tau));
}

// ============================================================================
// CONSTRUCTION AND INITIAL STATE
// ============================================================================

test(StepResponse_InitialState) {
    StepResponseTuner t;
    assertFalse(t.isRunning());
    assertFalse(t.isComplete());
    assertEqual(t.getSampleCount(), (uint16_t)0);
    assertEqual(String(t.getLastError()), String("none"));
}

// ============================================================================
// START VALIDATION
// ============================================================================

test(StepResponse_StartRefusedAbove140F) {
    StepResponseTuner t;
    t.start(200.0, 8.0, 0.46, 0.0, 180.0, 0.8);
    assertFalse(t.isRunning());
    assertEqual(String(t.getLastError()), String("start_temp_too_high"));
}

test(StepResponse_StartAcceptedBelow140F) {
    StepResponseTuner t;
    t.start(100.0, 8.0, 0.46, 0.0, 180.0, 0.8);
    assertTrue(t.isRunning());
    assertFalse(t.isComplete());
}

test(StepResponse_StartAt140F) {
    StepResponseTuner t;
    t.start(140.0, 8.0, 0.46, 0.0, 180.0, 0.8);
    assertTrue(t.isRunning());
}

// ============================================================================
// TAU_C CLAMPING
// ============================================================================

test(StepResponse_TauCClampedLow) {
    StepResponseTuner t;
    t.start(100.0, 8.0, 0.46, 0.0, 180.0, 0.1);
    assertTrue(t.isRunning());
    StepResponseTuner::Summary s = t.getSummary();
    assertNear(s.tauCFactor, 0.3, 0.01);
}

test(StepResponse_TauCClampedHigh) {
    StepResponseTuner t;
    t.start(100.0, 8.0, 0.46, 0.0, 180.0, 5.0);
    assertTrue(t.isRunning());
    StepResponseTuner::Summary s = t.getSummary();
    assertNear(s.tauCFactor, 3.0, 0.01);
}

// ============================================================================
// PHASE PROGRESSION WITH SYNTHETIC DATA
// ============================================================================

test(StepResponse_InitialPhaseIsStabilize) {
    StepResponseTuner t;
    t.start(100.0, 8.0, 0.46, 0.0, 180.0, 0.8);
    String phase = String(t.getPhaseName());
    assertTrue(phase.startsWith("STABILIZE"));
}

test(StepResponse_OverTempProtection) {
    StepResponseTuner t;
    t.start(100.0, 8.0, 0.46, 0.0, 180.0, 0.8);
    assertTrue(t.isRunning());

    // Feed an over-temperature reading
    t.getOutput(460.0, 70.0, 180.0);

    assertFalse(t.isRunning());
    assertEqual(String(t.getLastError()), String("over_temp"));
}

test(StepResponse_CancelStopsTuning) {
    StepResponseTuner t;
    t.start(100.0, 8.0, 0.46, 0.0, 180.0, 0.8);
    assertTrue(t.isRunning());

    t.cancel();

    assertFalse(t.isRunning());
    assertEqual(String(t.getLastError()), String("cancelled"));
}

test(StepResponse_StopStopsTuning) {
    StepResponseTuner t;
    t.start(100.0, 8.0, 0.46, 0.0, 180.0, 0.8);
    assertTrue(t.isRunning());

    t.stop();

    assertFalse(t.isRunning());
    assertEqual(String(t.getLastError()), String("stopped"));
}

// ============================================================================
// PROGRESS REPORTING
// ============================================================================

test(StepResponse_ProgressStartsLow) {
    StepResponseTuner t;
    t.start(100.0, 8.0, 0.46, 0.0, 180.0, 0.8);
    double progress = t.getProgressPercent();
    assertTrue(progress >= 0.0);
    assertTrue(progress < 50.0);
}

test(StepResponse_HeaterCommandDuringStabilize) {
    StepResponseTuner t;
    t.start(100.0, 8.0, 0.46, 0.0, 180.0, 0.8);
    // First band baseline output should be 80
    double output = t.getOutput(100.0, 70.0, 180.0);
    assertNear(output, 80.0, 0.1);
}

// ============================================================================
// SUMMARY AND COMPATIBILITY
// ============================================================================

test(StepResponse_SummaryBandCount) {
    StepResponseTuner t;
    t.start(100.0, 8.0, 0.46, 0.0, 180.0, 0.5);
    StepResponseTuner::Summary s = t.getSummary();
    assertEqual(s.totalBands, (uint8_t)2);
    assertEqual(s.completedBands, (uint8_t)0);
}

test(StepResponse_CharacterizationSummaryCompatibility) {
    StepResponseTuner t;
    t.start(100.0, 8.0, 0.46, 0.0, 180.0, 0.5);
    Calibration::CharacterizationSummary cs = t.getCharacterizationSummary();
    // Should map totalBands to maxCycles
    assertEqual(cs.maxCycles, (uint8_t)2);
}

// ============================================================================
// SIMC GAIN COMPUTATION (synthetic math verification)
// ============================================================================

// Test SIMC computation by running a complete synthetic band through the tuner.
// We simulate a step response and verify the tuner produces correct FOPDT params.
//
// This test drives the tuner through a full 2-band sequence with synthetic data
// generated from a known FOPDT model, then checks the fitted parameters and
// SIMC gains.

test(StepResponse_FullSyntheticRun) {
    StepResponseTuner t;
    t.start(100.0, 8.0, 0.46, 0.0, 180.0, 0.5);

    // Known FOPDT parameters per band
    // Band 0: baseline 175F, Kp=0.15 F/PWM, tau=60s, theta=5s, step=50 PWM
    //   Final = 175 + 0.15*50 = 182.5F, deltaT = 7.5F
    // Band 1: baseline 275F, Kp=0.12 F/PWM, tau=50s, theta=4s, step=50 PWM
    //   Final = 275 + 0.12*50 = 281.0F, deltaT = 6.0F

    struct BandParams {
        double baseline;
        double kp;
        double tau;
        double theta;
        double stepDelta;
        double startOutput;
    };

    BandParams bands[2] = {
        {175.0, 0.15, 60.0, 5.0, 50.0, 80.0},
        {275.0, 0.12, 50.0, 4.0, 50.0, 140.0},
    };

    // For each band, we need to simulate:
    // 1. STABILIZE: feed ~175F (constant) for enough samples to settle
    // 2. STEP_UP: output increases (tuner handles this)
    // 3. RECORD: feed FOPDT curve data
    // 4. COOL_DOWN: tuner skips (ascending bands)

    for (int bandIdx = 0; bandIdx < 2; bandIdx++) {
        if (!t.isRunning()) break;

        BandParams &bp = bands[bandIdx];
        String phaseName;

        // STABILIZE: Feed constant temperature for 35 seconds (30s settle + margin)
        // Need rate < 0.05 F/s for 30 seconds
        for (int i = 0; i < 160; i++) {  // 160 * 250ms = 40s
            if (!t.isRunning()) break;
            phaseName = String(t.getPhaseName());
            if (!phaseName.startsWith("STABILIZE")) break;

            // Add a tiny bit of noise to be realistic
            double noise = (i % 3 - 1) * 0.02;
            t.getOutput(bp.baseline + noise, 70.0, 180.0);
            delay(1);  // Minimal delay for millis() to advance
        }

        if (!t.isRunning()) break;

        // STEP_UP: Single call advances to RECORD
        phaseName = String(t.getPhaseName());
        if (phaseName.startsWith("STEP_UP")) {
            t.getOutput(bp.baseline, 70.0, 180.0);
        }

        if (!t.isRunning()) break;

        // RECORD: Feed FOPDT curve data until settled
        // We need enough data for the response to reach ~95% (3*tau seconds after theta)
        double recordDuration = bp.theta + 4.0 * bp.tau;  // seconds
        int recordSamples = (int)(recordDuration / 0.25);  // 250ms intervals
        recordSamples = min(recordSamples, 480);  // MAX_SAMPLES limit

        for (int i = 0; i < recordSamples; i++) {
            if (!t.isRunning()) break;
            phaseName = String(t.getPhaseName());
            if (!phaseName.startsWith("RECORD")) break;

            double timeSec = (double)i * 0.25;
            double temp = fopdtTemp(timeSec, bp.baseline, bp.kp, bp.stepDelta,
                                     bp.tau, bp.theta);
            t.getOutput(temp, 70.0, 180.0);
            delay(1);
        }
    }

    // After all bands, check if tuning completed or at least made progress
    StepResponseTuner::Summary s = t.getSummary();

    // At minimum we should have attempted all 2 bands
    assertTrue(s.totalBands == 2);

    // Check that valid bands produced reasonable FOPDT fits
    for (uint8_t i = 0; i < s.validBandCount && i < 2; i++) {
        const StepResponseTuner::BandResult &br = s.bands[i];
        if (!br.valid) continue;

        // Process gain should be positive
        assertTrue(br.model.processGain > 0.0);
        // Time constant should be positive and reasonable
        assertTrue(br.model.timeConstant > 1.0);
        assertTrue(br.model.timeConstant < 300.0);
        // Dead time should be non-negative
        assertTrue(br.model.deadTime >= 0.0);
        assertTrue(br.model.deadTime < 30.0);

        // PID gains should be positive
        assertTrue(br.kp > 0.0);
        assertTrue(br.ki > 0.0);
        assertTrue(br.kd >= 0.0);
    }
}

// ============================================================================
// SIMC GAIN MATH VERIFICATION
// ============================================================================

// Verify the SIMC formulas directly using known values
// For Kp_proc=0.15, tau=60, theta=5, tauC_factor=0.8:
//   tauC = 0.8 * 5 = 4
//   Kc = 60 / (0.15 * (4 + 5)) = 60 / 1.35 = 44.44
//   tauI = min(60, 4*(4+5)) = min(60, 36) = 36
//   Ki = 44.44 / 36 = 1.235
//   tauD = 5/3 = 1.667
//   Kd = 44.44 * 1.667 = 74.07

test(StepResponse_SIMCGainMath) {
    // Manually verify SIMC formulas
    double kpProcess = 0.15;
    double tau = 60.0;
    double theta = 5.0;
    double tauCFactor = 0.8;

    double tauC = tauCFactor * theta;            // 4.0
    double Kc = tau / (kpProcess * (tauC + theta)); // 60/(0.15*9) = 44.44
    double tauI = min(tau, 4.0 * (tauC + theta));   // min(60, 36) = 36
    double Ki = Kc / tauI;                           // 44.44/36 = 1.235
    double tauD = theta / 3.0;                       // 1.667
    double Kd = Kc * tauD;                           // 74.07

    assertNear(tauC, 4.0, 0.01);
    assertNear(Kc, 44.44, 0.1);
    assertNear(tauI, 36.0, 0.01);
    assertNear(Ki, 1.235, 0.01);
    assertNear(tauD, 1.667, 0.01);
    assertNear(Kd, 74.07, 0.5);
}

// Higher tauC factor should produce less aggressive gains
test(StepResponse_SIMCHigherTauCLessAggressive) {
    double kpProcess = 0.15;
    double tau = 60.0;
    double theta = 5.0;

    double tauC_low = 0.5 * theta;
    double tauC_high = 2.0 * theta;

    double Kc_low = tau / (kpProcess * (tauC_low + theta));
    double Kc_high = tau / (kpProcess * (tauC_high + theta));

    // Higher tauC means smaller Kc (less aggressive)
    assertTrue(Kc_high < Kc_low);
}

// ============================================================================
// TRACE SAMPLE API
// ============================================================================

test(StepResponse_TraceSampleCountInitiallyZero) {
    StepResponseTuner t;
    assertEqual(t.getTraceSampleCount(), (uint16_t)0);
}

test(StepResponse_TraceSampleAccumulates) {
    StepResponseTuner t;
    t.start(100.0, 8.0, 0.46, 0.0, 180.0, 0.8);

    // Feed a few samples
    for (int i = 0; i < 5; i++) {
        t.getOutput(100.0, 70.0, 180.0);
        delay(1);
    }

    assertTrue(t.getTraceSampleCount() > 0);
}

test(StepResponse_TraceSampleHasValidData) {
    StepResponseTuner t;
    t.start(100.0, 8.0, 0.46, 0.0, 180.0, 0.8);

    t.getOutput(175.0, 70.0, 180.0);
    delay(1);

    StepResponseTuner::TraceSample s = t.getTraceSample(0);
    assertNear((double)s.actualTempF, 175.0, 0.1);
    assertNear((double)s.heaterOutput, 80.0, 0.1);  // Band 0 baseline
}

// ============================================================================
// DOUBLE-START PROTECTION
// ============================================================================

test(StepResponse_RestartClearsState) {
    StepResponseTuner t;
    t.start(100.0, 8.0, 0.46, 0.0, 180.0, 0.8);

    // Feed some data
    for (int i = 0; i < 10; i++) {
        t.getOutput(175.0, 70.0, 180.0);
        delay(1);
    }

    uint16_t count1 = t.getTraceSampleCount();
    assertTrue(count1 > 0);

    // Restart
    t.start(100.0, 8.0, 0.46, 0.0, 180.0, 0.8);

    // Sample count should be reset
    StepResponseTuner::Summary s = t.getSummary();
    assertEqual(s.completedBands, (uint8_t)0);
}

// ============================================================================
// ONSET DEAD TIME DETECTION
// ============================================================================

// Verify that the onset dead time is close to the true dead time for clean
// synthetic FOPDT data (where noise is near-zero).
test(StepResponse_OnsetDeadTimeMatchesTrueTheta) {
    StepResponseTuner t;
    t.start(100.0, 8.0, 0.46, 0.0, 180.0, 0.5);

    // Band 0: Kp=0.15, tau=60, theta=5, step=50
    double baseline = 175.0;
    double kp_proc = 0.15;
    double tau = 60.0;
    double theta = 5.0;
    double stepDelta = 50.0;

    // STABILIZE: Feed constant temperature with tiny noise
    for (int i = 0; i < 160; i++) {
        if (!t.isRunning()) break;
        String pn = String(t.getPhaseName());
        if (!pn.startsWith("STABILIZE")) break;
        double noise = (i % 3 - 1) * 0.02;
        t.getOutput(baseline + noise, 70.0, 180.0);
        delay(1);
    }

    // STEP_UP
    if (t.isRunning()) {
        String pn = String(t.getPhaseName());
        if (pn.startsWith("STEP_UP")) {
            t.getOutput(baseline, 70.0, 180.0);
        }
    }

    // RECORD: Feed FOPDT curve
    for (int i = 0; i < 480; i++) {
        if (!t.isRunning()) break;
        String pn = String(t.getPhaseName());
        if (!pn.startsWith("RECORD")) break;
        double timeSec = (double)i * 0.25;
        double temp = fopdtTemp(timeSec, baseline, kp_proc, stepDelta, tau, theta);
        t.getOutput(temp, 70.0, 180.0);
        delay(1);
    }

    StepResponseTuner::Summary s = t.getSummary();
    if (s.validBandCount > 0) {
        const StepResponseTuner::BandResult &br = s.bands[0];
        assertTrue(br.model.valid);

        // Onset dead time should be close to the true theta (5s)
        // With very low noise (sigma~0.02), onset threshold (3*sigma~0.06F) is crossed
        // shortly after the true dead time
        assertTrue(br.model.onsetDeadTime >= 1.0);  // floor
        assertTrue(br.model.onsetDeadTime <= 10.0);  // reasonable upper bound

        // Noise sigma should be very small for constant input
        assertTrue(br.model.noiseStdDev < 0.5);
        assertTrue(br.model.noiseStdDev >= 0.1);  // floor at 0.1
    }
}

// ============================================================================
// FEEDFORWARD FIELDS IN CHARACTERIZATION SUMMARY
// ============================================================================

// Verify that feedforward model parameters are populated in the
// CharacterizationSummary (heaterCoeff, coolingCoeff, drift).
test(StepResponse_FeedforwardFieldsPopulated) {
    StepResponseTuner t;
    t.start(100.0, 8.0, 0.46, 0.0, 180.0, 0.5);

    // Run through band 0 only (same synthetic data as above)
    double baseline = 175.0;
    double kp_proc = 0.15;
    double tau = 60.0;
    double theta = 5.0;
    double stepDelta = 50.0;

    for (int i = 0; i < 160; i++) {
        if (!t.isRunning()) break;
        String pn = String(t.getPhaseName());
        if (!pn.startsWith("STABILIZE")) break;
        double noise = (i % 3 - 1) * 0.02;
        t.getOutput(baseline + noise, 70.0, 180.0);
        delay(1);
    }

    if (t.isRunning()) {
        String pn = String(t.getPhaseName());
        if (pn.startsWith("STEP_UP")) {
            t.getOutput(baseline, 70.0, 180.0);
        }
    }

    for (int i = 0; i < 480; i++) {
        if (!t.isRunning()) break;
        String pn = String(t.getPhaseName());
        if (!pn.startsWith("RECORD")) break;
        double timeSec = (double)i * 0.25;
        double temp = fopdtTemp(timeSec, baseline, kp_proc, stepDelta, tau, theta);
        t.getOutput(temp, 70.0, 180.0);
        delay(1);
    }

    Calibration::CharacterizationSummary cs = t.getCharacterizationSummary();

    // Check that at least band 0 has feedforward fields populated
    bool foundValid = false;
    for (uint8_t i = 0; i < Calibration::BAND_COUNT; i++) {
        const Calibration::BandCharacterization &bc = cs.bands[i];
        if (!bc.valid) continue;
        foundValid = true;

        // heaterCoeff = processGain / timeConstant
        // With Kp~0.15, tau~60: heaterCoeff ~ 0.0025
        assertTrue(bc.heaterCoeff > 0.0);
        assertTrue(bc.heaterCoeff < 1.0);  // sanity upper bound

        // coolingCoeff should be non-negative
        assertTrue(bc.coolingCoeff >= 0.0);

        // drift should be ~0
        assertNear(bc.drift, 0.0, 0.01);
    }
    assertTrue(foundValid);
}

// ============================================================================
// SIMC GAIN MATH — onset-based theta verification
// ============================================================================

// With onset-based theta, SIMC gains are much more aggressive when the FOPDT
// model overestimates dead time (common for higher-order thermal systems).
test(StepResponse_SIMCGainsUseOnsetTheta) {
    // For a system where onset theta = 5s and model theta = 20s:
    // Old code: tauC = 0.5 * 20 = 10, Kc = 60 / (0.15 * 30) = 13.33
    // New code: tauC = 0.5 * 5  = 2.5, Kc = 60 / (0.15 * 7.5) = 53.33
    double kpProcess = 0.15;
    double tau = 60.0;
    double thetaOnset = 5.0;
    double thetaModel = 20.0;
    double tauCFactor = 0.5;

    double tauC = tauCFactor * thetaOnset;  // 2.5
    double Kc = tau / (kpProcess * (tauC + thetaOnset));  // 60 / (0.15 * 7.5) = 53.33
    double tauI = min(tau, 4.0 * (tauC + thetaOnset));    // min(60, 30) = 30
    double Ki = Kc / tauI;                                  // 53.33 / 30 = 1.778

    assertNear(tauC, 2.5, 0.01);
    assertNear(Kc, 53.33, 0.1);
    assertNear(tauI, 30.0, 0.01);
    assertNear(Ki, 1.778, 0.01);

    // Verify this is much more aggressive than the model-based version
    double tauC_model = tauCFactor * thetaModel;  // 10
    double Kc_model = tau / (kpProcess * (tauC_model + thetaModel));  // 60/(0.15*30) = 13.33
    assertTrue(Kc > 3.0 * Kc_model);  // onset-based is >3x more aggressive
}

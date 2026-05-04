# Fix: Autotuned PID Lags 10–20°F Behind Ramp Setpoint

## Diagnosis

### Observed Behavior

The step-response autotuner produces Kp=1.077, Ki=0.031, Kd=7.64 with τ_c factor 0.5.
During a roast with ~0.5°F/s ramp rate, the actual temperature consistently lags
10–20°F behind the setpoint.

### Root Cause 1: FOPDT Dead Time is Grossly Overestimated

The SIMC steady-state ramp tracking error for a PI controller on a FOPDT plant is
**exactly**:

$$e_{ss} = R \times (\tau_c + \theta)$$

where $R$ is the ramp rate (°F/s).

With the calibration results (Band 2):
- θ_model = 21.28s (from S-K / least-squares fit)
- τ_c = 0.5 × 21.28 = 10.64s
- Ramp rate ≈ 0.5°F/s

$$e_{ss} = 0.5 \times (10.64 + 21.28) = \textbf{16.0°F}$$

This matches the observed lag perfectly.

**But the 21s dead time is wrong.** Looking at the Band 2 step response data, the
temperature begins rising within 3–5 seconds of the step input:

| Time (s) | Temp (°F) | ΔT from baseline |
|-----------|-----------|------------------|
| 0         | 232.1     | 0.0              |
| 3         | 232.7     | +0.6 (noise)     |
| 5.75      | 234.1     | +2.0 (real)      |
| 10        | 234.5     | +2.4             |
| 20        | 240.0     | +7.9             |

The noise floor during stabilization is σ ≈ 0.4°F (samples oscillate ±0.5°F).
The temperature first exceeds baseline + 3σ ≈ 233.3°F at about **t = 5s**.

The true transport delay (thermocouple thermal mass + air transit) is ~3–5 seconds.
The FOPDT model reports θ = 21s because the Sundaresan-Krishnaswamy and least-squares
methods lump the system's **multiple thermal time constants** (heater → drum → air →
thermocouple) into a single first-order + dead time approximation. Since the real
system is higher order than FOPDT, the fit compensates by inflating θ.

This is a well-known limitation of FOPDT modeling for higher-order thermal systems
(see Skogestad & Grimholt 2012, §5.3.2: "For processes with several time constants,
the FOPDT approximation gives too large a delay").

### Root Cause 2: PIDRuntimeController Feedforward is Broken

The `PIDRuntimeController` computes ramp feedforward as:

```cpp
double rawFeedforward = (desiredRate - band.drift - band.coolingCoeff * (...)) 
                        / max(band.heaterCoeff, 1e-6);
```

The step-response tuner's `getCharacterizationSummary()` **never populates**
`drift`, `coolingCoeff`, or `heaterCoeff` in `BandCharacterization` — they're
all left at 0.0. This causes:

- `heaterCoeff = 0` → denominator becomes `1e-6`
- `rawFeedforward = desiredRate / 1e-6` → ~500,000
- After `constrain(..., 0, 255)` → feedforward saturates at **255 always**
- Total output = `clamp(PID + 255, 0, 255)` = **heater stuck at max**

The PID then fights against a full-on heater by driving its integral term
negative. The result is erratic output and poor tracking.

> **Note:** The sawtooth tuner has the same bug — it also never sets these
> three fields. The bug was latent because the sawtooth tuner rarely produces
> 2+ valid bands needed to enable the scheduler.

### Combined Effect

With broken feedforward AND inflated dead time, the autotuned controller has:
- PID gains that are 4× too conservative (Kc = 1.08 vs. needed ~4.0)
- A feedforward term that either does nothing (schedule off) or saturates at max

## Proposed Fix: Three Changes

### Change 1: Response Onset Dead Time Detector

Add a direct measurement of when the temperature first responds to the step input,
and use this shorter value for SIMC gain computation.

**Algorithm:**

During the STABILIZE phase, compute the noise standard deviation σ from the last
120 samples (30 seconds at 250ms intervals). During the RECORD phase, detect when
T(t) first exceeds T_baseline + k×σ (where k = 3 for 99.7% confidence).

```
θ_onset = time when T first exceeds baseline + 3σ
θ_onset = max(θ_onset, 1.0)  // always at least 1 second
```

Use `θ_onset` for SIMC gain computation instead of the FOPDT-fitted θ:

```
τ_c = configTauCFactor × θ_onset
Kc = τ / (Kp × (τ_c + θ_onset))
τ_I = min(τ, 4 × (τ_c + θ_onset))
τ_D = θ_onset / 3
```

Keep the FOPDT-fitted `θ_model` for logging, display, and feedforward lead time.

**Expected gains with θ_onset ≈ 5s:**
- τ_c = 0.5 × 5 = 2.5s
- Kc = 34.5 / (1.0 × 7.5) = **4.60** (vs. current 1.08)
- Ki = 4.60 / 34.5 = **0.133** (vs. current 0.031)
- Kd = 4.60 × 5/3 = **7.67** (similar)
- Ramp error: 0.5 × 7.5 = **3.75°F** (vs. current 16°F)

**Implementation in `StepResponseTuner.hpp`:**

1. Add `double noiseStdDev` to the class state, computed during STABILIZE from the
   last 120 samples' standard deviation.

2. Add `double onsetDeadTime` to `FOPDTModel` and `BandResult`.

3. During RECORD phase, find the first sample where `T > baselineTemp + 3 × noiseStdDev`.
   Store the time as `onsetDeadTime`.

4. In `computeSIMC()`, use `max(onsetDeadTime, 1.0)` for θ in the SIMC formulas.
   Use the FOPDT-fitted `deadTime` for the derivative term if larger (conservative D).

### Change 2: Populate Feedforward Model Fields

The step-response FOPDT model provides exactly the parameters needed for
`PIDRuntimeController` feedforward. Add mappings in `getCharacterizationSummary()`:

```cpp
// FOPDT process gain = ΔT / Δu (°F per PWM count)
// heaterCoeff in the feedforward formula = dT/dt per unit heater output
// For a FOPDT model: heaterCoeff ≈ processGain / timeConstant
bc.heaterCoeff = br.model.processGain / max(br.model.timeConstant, 1.0);

// coolingCoeff: approximate from steady-state energy balance
// At baseline: heaterCoeff * u_baseline = coolingCoeff * (T_baseline - T_ambient)
// So: coolingCoeff = heaterCoeff * u_baseline / (T_baseline - T_ambient)
double tempDiff = br.model.baselineTemp - ambientTemp;
if (tempDiff > 10.0) {
    bc.coolingCoeff = bc.heaterCoeff * br.model.baselineOutput / tempDiff;
} else {
    bc.coolingCoeff = 0.0;
}

// drift: should be ~0 at steady state, but include for completeness
bc.drift = 0.0;
```

This enables the `PIDRuntimeController` feedforward to compute physically
meaningful values instead of dividing by 1e-6.

**With correct heaterCoeff** (≈ 1.0/34.5 ≈ 0.029 °F/s per PWM count):

```
// At 0.5°F/s ramp rate:
rawFeedforward = (0.5 - 0 - coolingCorrection) / 0.029 ≈ 17 PWM
```

This is a sensible feedforward value that pre-loads the heater output
proportionally to the ramp rate.

### Change 3: Use θ_model for Feedforward Lead Term

The `PIDRuntimeController` already has a setpoint lead feature:

```cpp
double leadTarget = setpointTemp + desiredRate * min(band.deadTime, 12.0);
```

With the correctly populated `deadTime` from the FOPDT model (21s, capped at 12s),
this advances the setpoint by up to 12 × 0.5 = 6°F, helping the feedforward
anticipate the ramp. This is a simple form of dead-time compensation.

Currently this works because `deadTime` IS populated from the FOPDT model. After
Change 2 populates `heaterCoeff`, the feedforward will compute meaningful values
and this lead term will properly reduce ramp lag.

## Expected Outcome

| Metric | Current | After Fix |
|--------|---------|-----------|
| SIMC θ used | 21.3s (FOPDT fit) | ~5s (onset measured) |
| Kc (proportional) | 1.08 | ~4.6 |
| Ki (integral) | 0.031 | ~0.133 |
| Ramp error (PID only) | 16°F | ~3.8°F |
| Feedforward | Broken (0 or 255) | ~17 PWM at 0.5°F/s |
| Ramp error (PID + FF) | 16°F | **~1–2°F** |

## Safety Considerations

- Higher Kc increases the risk of oscillation if the onset dead time underestimates
  the true effective delay. Mitigate by clamping θ_onset ≥ 1.0s and monitoring Ms
  (sensitivity peak). The SIMC rules with τ_c = 0.5θ give Ms ≈ 1.8–2.0, which is
  acceptable for this thermal system.

- The feedforward is already constrained to [0, 255] and filtered (α = 0.35 EMA).
  Correct heaterCoeff values will produce moderate feedforward (10–30 PWM) instead
  of the current saturated 255.

- The heater output sum `PID + feedforward` is already clamped to [0, 255].
  No additional safety measures needed.

## Implementation Scope

| File | Change |
|------|--------|
| `StepResponseTuner.hpp` | Add noise σ computation in STABILIZE, onset detector in RECORD, `onsetDeadTime` field in FOPDTModel, use onset θ in `computeSIMC()` |
| `StepResponseTuner.hpp` | Populate `drift`, `coolingCoeff`, `heaterCoeff` in `getCharacterizationSummary()` |
| `PIDAutotuner.hpp` | Same feedforward field fix in sawtooth `getSummary()` (populate from model) |
| Tests | Update `test_step_response.ino` expected PID gains for onset-based θ |

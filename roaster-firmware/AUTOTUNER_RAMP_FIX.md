# Bounded autotuning and ramp tracking

The active implementation is [src/control/StepResponseTuner.hpp](src/control/StepResponseTuner.hpp).
The older proposal in this document has been superseded; replacing effective
dead time with response onset did not address fitting, feedforward, or validation.

## Changes

- One local FOPDT model is identified from at most a 175-second transient with baseline
  80 PWM and two ascending heater changes to 130 and 180 PWM. No equilibrium
  temperature or final recorded sample is assumed to be the asymptote.
- For each bounded time-constant/delay candidate, two linear coefficients
  (thermal offset and process gain) are fitted using actual sample timestamps and
  delayed heater inputs. The last 20% is held out for prediction checking.
- Incremental searches start after 80 seconds of acquisition and repeat every 10
  seconds. Identification can finish early when successive fits agree within 10%
  in gain/time constant and one second in delay, the window spans at least 2.5
  time constants, and confidence checks pass. Early fits matter on real roasters:
  the late high-power data drifts above a first-order model. At 175 seconds, heating stops
  for the final search. Each search runs one time-constant slice per control tick.
  Poor excitation, an inlet reading that moves more than 5F during the 10-second
  preflight, ambiguous time constants, boundary fits, implausible
  coefficients/gains, or excessive prediction error reject the run. The inlet
  thermocouple warms with the heater (about 13% of the bean rise on roaster-fred),
  so it is not gated during the heater steps.
- Conservative SIMC PI uses the fitted effective delay. Derivative defaults to
  zero; a first-order fit does not identify a trustworthy second thermal pole.
- The candidate is checked for 85 seconds: a 50-second 0.25F/s ramp and 35-second
  hold. The first 15 seconds of each phase are recovery windows, excluded from
  tracking metrics. A 295-second deadline includes fan startup and fitting.
- A result is verified only with enough ramp and hold points, ramp MAE <= 2F,
  at least 90% of ramp points within 3F, hold MAE <= 1F, overshoot <= 4F, and
  less than 20% saturation in the scored intervals.
- Cooling is independent of the tuning deadline. Failed or cancelled runs never
  overwrite the current controller. A verified result still requires explicit Apply.

## Consistent runtime model

The heater uses a phase-stable one-second time-proportional driver. The prior
relay library restarts its off interval on every `setPWM()` call: at 250ms updates,
80/130/180 PWM never reached their first on pulse. The host checks now verify GPIO
duty, not only the requested command. Zero duty is immediate, including during
cooling and idle.

`dT/dt = heaterCoeff*u - coolingCoeff*T + drift`

Both heater and cooling coefficients are positive. For the fitted model,
`coolingCoeff = 1/tau`, `heaterCoeff = processGain/tau`, and
`drift = fittedOffset/tau`, so feedforward is the model inverse
`u = (rate*tau + T - fittedOffset)/processGain`. The live inlet reading is not
used: it warms with the heater and made feedforward under-drive (setpoint lag).
Model storage v3 drops v2 feedforward bands (inlet-relative drift) and keeps
their gains. Required heater feedforward **adds** thermal losses. It does not divide by an epsilon when the model is invalid.

The control path uses fractional profile temperatures and the actual segment
slope, rather than differentiating a rounded display target. Feedback trim may
be negative so it can subtract from excessive feedforward; anti-windup sees the
same bounds as the final combined heater output. Gain changes preserve the
existing P+I+D correction where possible.

Versioned, checksummed NVS snapshots save and restore the whole controller.
`/pid` offers autotune and manual gain adjustment. History remains available
through the PID API; see the [firmware README](README.md#pid-tuning).

## Reproducible checks and limitations

```bash
./tools/tests.sh run host
./tools/tests.sh run host-json
./tools/tests.sh compile step-response --board jc4827w543c
./tools/tests.sh compile pid --board jc4827w543c
./tools/tests.sh compile safety --board jc4827w543c
./tools/firmware.sh build --board jc4827w543c
```

The host regressions drive the real tuner/runtime with virtual time, a delayed
thermal plant, noise, higher-order lag, an inlet that warms with the heater,
unsteady inlet before the step, weak
excitation, and a deliberately changed plant during verification.
They check parameter accuracy, elapsed time, negative trim, saturation,
cancellation, invalid sensor input, history rotation, reboot, corruption, and
write failures. They do not operate real heater or fan hardware.

A local quick tune is not proof of full roast-range accuracy. The UI displays
the observed range and warns when targets extrapolate outside it. Qualify
representative roast profiles, sensor calibration, fan speeds, and bean loads
on supervised hardware before relying on the result.

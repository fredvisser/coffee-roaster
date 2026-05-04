#ifndef PID_CONTROLLER_HPP
#define PID_CONTROLLER_HPP

#include <Arduino.h>

class PIDController {
public:
    PIDController(double *input,
                  double *setpoint,
                  double *output,
                  double outputMin,
                  double outputMax,
                  double kp,
                  double ki,
                  double kd)
        : input(input),
          setpoint(setpoint),
          output(output),
          outputMin(outputMin),
          outputMax(outputMax) {
        setGains(kp, ki, kd);
    }

    void setGains(double newKp, double newKi, double newKd) {
        kp = newKp;
        ki = newKi;
        kd = newKd;
    }

    void setOutputRange(double newOutputMin, double newOutputMax) {
        outputMin = newOutputMin;
        outputMax = newOutputMax;
        integral = constrain(integral, integralLowerBound(), integralUpperBound());
    }

    void setTimeStep(unsigned long newTimeStepMs) {
        timeStepMs = max(1UL, newTimeStepMs);
    }

    void run() {
        if (stopped) {
            stopped = false;
            reset();
        }

        unsigned long now = millis();
        unsigned long dtMs = now - lastStepMs;
        if (dtMs < timeStepMs) {
            return;
        }

        lastStepMs = now;
        double dtSeconds = static_cast<double>(dtMs) / 1000.0;
        double processValue = *input;
        double error = *setpoint - processValue;

        if (!previousInputValid) {
            previousInput = processValue;
            previousInputValid = true;
        }

        double measuredRate = (processValue - previousInput) / max(dtSeconds, 1e-6);
        previousInput = processValue;
        filteredMeasurementRate += DERIVATIVE_FILTER * (measuredRate - filteredMeasurementRate);

        double proportional = kp * error;
        double derivative = -kd * filteredMeasurementRate;

        double candidateIntegral = integral + 0.5 * (error + previousError) * dtSeconds;
        double unsaturated = proportional + ki * candidateIntegral + derivative;

        bool saturatingHigh = unsaturated > outputMax && error > 0.0;
        bool saturatingLow = unsaturated < outputMin && error < 0.0;
        if (!saturatingHigh && !saturatingLow) {
            integral = constrain(candidateIntegral, integralLowerBound(), integralUpperBound());
        }

        previousError = error;
        double command = proportional + ki * integral + derivative;
        *output = constrain(command, outputMin, outputMax);
    }

    void stop() {
        stopped = true;
        reset();
    }

    void reset() {
        lastStepMs = millis();
        integral = 0.0;
        previousError = 0.0;
        filteredMeasurementRate = 0.0;
        previousInput = input ? *input : 0.0;
        previousInputValid = input != nullptr;
    }

    bool isStopped() const {
        return stopped;
    }

    double getIntegral() const {
        return integral;
    }

    void setIntegral(double newIntegral) {
        integral = constrain(newIntegral, integralLowerBound(), integralUpperBound());
    }

private:
    static constexpr double DERIVATIVE_FILTER = 0.35;

    double *input = nullptr;
    double *setpoint = nullptr;
    double *output = nullptr;
    double kp = 0.0;
    double ki = 0.0;
    double kd = 0.0;
    double integral = 0.0;
    double previousError = 0.0;
    double previousInput = 0.0;
    double filteredMeasurementRate = 0.0;
    double outputMin = 0.0;
    double outputMax = 255.0;
    unsigned long timeStepMs = 1000UL;
    unsigned long lastStepMs = 0UL;
    bool stopped = true;
    bool previousInputValid = false;

    double integralLowerBound() const {
        if (ki <= 1e-9) {
            return outputMin;
        }
        return outputMin / ki;
    }

    double integralUpperBound() const {
        if (ki <= 1e-9) {
            return outputMax;
        }
        return outputMax / ki;
    }
};

#endif
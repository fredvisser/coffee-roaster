#ifndef PID_AUTOTUNER_HPP
#define PID_AUTOTUNER_HPP

#include <Arduino.h>
#include "DebugLog.hpp"

class PIDAutotuner {
public:
    PIDAutotuner() {}

    void start(double target, double outputStep = 255.0) {
        targetTemp = target;
        step = outputStep;
        running = true;
        cycleCount = 0;
        startTime = millis();
        
        // Initialize state
        justChanged = false;
        heating = true; // Start by heating
        
        cycleMax = -999;
        cycleMin = 999;
        
        // Reset calculated values
        calculatedKp = 0;
        calculatedKi = 0;
        calculatedKd = 0;
        
        LOG_INFOF("Autotune started: Target=%.1f, Step=%.1f, Hysteresis=%.1f", target, step, hysteresis);
    }

    void stop() {
        running = false;
        LOG_INFO("Autotune stopped");
    }

    bool isRunning() { return running; }
    bool isComplete() { return !running && calculatedKp > 0; }

    double getOutput(double input) {
        if (!running) return 0;

        unsigned long now = millis();
        
        // Fan spinup delay (3 seconds)
        if (now - startTime < 3000) {
            LOG_INFO("Autotune: Waiting for fan spinup...");
            return 0;
        }

        LOG_INFOF("Autotune: Input=%.2f, Target=%.2f, Heating=%d", input, targetTemp, heating);
        
        // Safety timeout (20 minutes)
        if (now - startTime > 1200000) {
            LOG_ERROR("Autotune timed out!");
            stop();
            return 0;
        }

        // Track min/max
        if (input > cycleMax) cycleMax = input;
        if (input < cycleMin) cycleMin = input;

        // Relay logic with Hysteresis
        double output = 0;
        
        if (heating) {
            if (input > targetTemp + hysteresis) {
                // Crossed setpoint upwards (plus hysteresis)
                heating = false;
                justChanged = true;
                LOG_INFO("Autotune: Crossed setpoint UP -> Cooling");
            }
            output = step;
        } else {
            if (input < targetTemp - hysteresis) {
                // Crossed setpoint downwards (minus hysteresis)
                heating = true;
                justChanged = true;
                LOG_INFO("Autotune: Crossed setpoint DOWN -> Heating");
                
                // Completed a half-cycle (or full cycle depending on how you count)
                // Let's count full cycles (Up -> Down -> Up)
                // Actually, simpler: Just count zero crossings.
                // But we need to measure peaks.
            }
            output = 0;
        }

        if (justChanged) {
            justChanged = false;
            // We just crossed the setpoint.
            // If we were heating and now cooling, we just finished a "heating half-cycle".
            // The peak will happen shortly AFTER this due to lag.
            // So we can't just take the current max.
            
            // Actually, for Ziegler-Nichols Relay method, we need the Amplitude of the oscillation.
            // The oscillation naturally stabilizes.
            
            // Let's count "cycles" as transitions from Cooling -> Heating (Bottom of wave)
            if (heating) {
                cycleCount++;
                LOG_INFOF("Autotune Cycle %d started. Last Max=%.1f, Last Min=%.1f", cycleCount, cycleMax, cycleMin);
                
                if (cycleCount > 1) {
                    // Store the amplitude of the previous full cycle
                    double amp = (cycleMax - cycleMin) / 2.0;
                    amplitudes[cycleCount-2] = amp;
                    
                    // Store period
                    unsigned long currentCycleTime = now;
                    if (cycleCount > 2) {
                        periods[cycleCount-3] = currentCycleTime - lastCycleTime;
                    }
                    lastCycleTime = currentCycleTime;
                }
                
                // Reset min/max for next cycle
                cycleMax = -999;
                cycleMin = 999;
                
                if (cycleCount >= 5) { // Run 5 cycles to stabilize
                    finish();
                    return 0;
                }
            }
        }

        return output;
    }

    void getPID(double &kp, double &ki, double &kd) {
        kp = calculatedKp;
        ki = calculatedKi;
        kd = calculatedKd;
    }

private:
    bool running = false;
    double targetTemp = 0;
    double step = 255;
    
    bool heating = true;
    bool justChanged = false;
    
    double cycleMax = -999;
    double cycleMin = 999;
    
    int cycleCount = 0;
    unsigned long startTime = 0;
    unsigned long lastCycleTime = 0;
    
    double amplitudes[10];
    unsigned long periods[10];

    double calculatedKp = 0;
    double calculatedKi = 0;
    double calculatedKd = 0;
    
    double hysteresis = 4.0; // Increased to 4.0F to force larger oscillation amplitude

    void finish() {
        // Average the last 3 amplitudes and periods
        double avgAmp = 0;
        double avgPeriod = 0;
        int count = 0;
        
        // We have data from cycle 2, 3, 4 (indices 0, 1, 2)
        // periods are from cycle 3, 4 (indices 0, 1)
        
        for(int i=1; i<3; i++) { // Use last 2 complete cycles
            avgAmp += amplitudes[i];
            avgPeriod += periods[i-1];
            count++;
        }
        
        avgAmp /= count;
        avgPeriod /= count;
        
        double Tu = avgPeriod / 1000.0; // Seconds
        double d = step;
        double Ku = (4.0 * d) / (3.14159 * avgAmp);

        LOG_INFOF("Autotune Result: Ku=%.4f, Tu=%.4f, Amp=%.2f", Ku, Tu, avgAmp);

        // Switch to Tyreus-Luyben tuning rules
        // Ziegler-Nichols is too aggressive for this thermal system (causes overshoot)
        // Tyreus-Luyben is more conservative and robust for lag-dominant processes
        
        // We could apply an additional damping factor to target Kp ~8 (based on known good values)
        // Standard TL: Kp = Ku / 3.2
        // Damped TL:   Kp = Ku / 5.0
        
        calculatedKp = Ku / 3.2;
        calculatedKi = calculatedKp / (2.2 * Tu);
        calculatedKd = calculatedKp * (Tu / 6.3);
        
        LOG_INFOF("Calculated PID (Damped TL): Kp=%.4f, Ki=%.4f, Kd=%.4f", calculatedKp, calculatedKi, calculatedKd);
        running = false;
    }
};

#endif

"""Run with python3 tests/host/firmware_lockout_test.py. No device is contacted."""

import os
from pathlib import Path
import subprocess
from tempfile import TemporaryDirectory

firmware = Path(__file__).resolve().parents[2]
source = (firmware / "roaster-firmware.ino").read_text()
entry = source.split("inline void enterEmergencyErrorState(", 1)[1].split("\nMAX6675 ", 1)[0]
error_case = source.split("    case ERROR:\n", 1)[1].split("    case CALIBRATING:\n", 1)[0]

program = r"""
#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>
enum { IDLE, START_ROAST, ROASTING, COOLING, ERROR, CALIBRATING };
enum { HEATER, LOW };
int roasterState, setpointFanSpeed, bdcFanMs, heaterPin;
double currentTemp, heaterOutputVal;
std::string activeFaultCode;
const char* activeFaultMessage = "Sensor fault";
struct Relay {
    int output = 0;
    void setPWM(int value) { output = value; }
} heaterRelay, fanRelay;
struct Servo {
    int output = 0;
    void writeMicroseconds(int value) { output = value; }
} bdcFan;
struct Tuner { void abort(const char*) {} } stepTuner;
struct DisplayTelemetry {
    int roasterState, currentTempF, fanPercent, bdcFanMicros;
} displayed;
void digitalWrite(int, int value) { heaterPin = value; }
void resetRoastControllerState() { heaterOutputVal = 0; }
void setActiveFault(const char* code, const char*) { activeFaultCode = code; }
void finishPIDTuningRecord() {}
void systemLinkUpdateLastFault(const char*) {}
const int SYSTEMLINK_OUTCOME_ERRORED = 1;
void systemLinkFinishRoast(int, const char*) {}
void displayShowErrorMessage(const char*) {}
void finalizeValidationIfRunning(bool, const char*) {}
void displayUpdateTelemetry(DisplayTelemetry value) { displayed = value; }
void checkOutputs() {
    assert(roasterState == ERROR && activeFaultCode == "sensor_failed");
    assert(heaterPin == LOW && heaterRelay.output == 0 && heaterOutputVal == 0);
    assert(setpointFanSpeed == 255 && fanRelay.output == 255);
    assert(bdcFan.output == 2000 && bdcFanMs == 2000);
}
void enterEmergencyErrorState(""" + entry + """
void tickStateMachine() {
    switch (roasterState) {
    case ERROR:
""" + error_case + """
    default: assert(false);
    }
}
int main() {
    for (int initialState : {ROASTING, COOLING, CALIBRATING}) {
        roasterState = initialState;
        heaterOutputVal = heaterRelay.output = heaterPin = 200;
        currentTemp = 318.2;
        enterEmergencyErrorState("sensor_failed", "Sensor fault");
        checkOutputs();
        for (double temperature : {318.2, 181.9, 83.3}) {
            currentTemp = temperature;
            for (int tick = 0; tick < 10; ++tick) {
                tickStateMachine();
                checkOutputs();
                assert(displayed.roasterState == ERROR);
                assert(displayed.fanPercent == 100 && displayed.bdcFanMicros == 2000);
            }
        }
    }
    puts("Sensor lockout maintains heater off, full PWM/BDC cooling and 100% telemetry");
}
"""

with TemporaryDirectory() as directory:
    cpp = Path(directory) / "lockout.cpp"
    binary = Path(directory) / "lockout"
    cpp.write_text(program)
    subprocess.run([os.environ.get("CXX", "c++"), "-std=c++17", "-Wall", "-Wextra",
                    "-Werror", str(cpp), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)

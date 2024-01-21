#if ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif
#include <EEPROM.h>

//------------------------------------------------------
// ensure this library description is only included once
//--------------------------------------------------------
#ifndef RoasterProfiles_h
#define RoasterProfiles_h

class Profiles
{
private:
    typedef struct
    {
        int time;
        int temp;
        int fanSpeed;
    } Setpoint;
    Setpoint setpoints[10]; // Create an array of Setpoints with size 10
    int setpointCount = 0;  // Keep track of how many setpoints are in the array
    uint32_t startTime = 0; // Keep track of when the profile started

public:
    // Constructor
    Profiles();

    // Start the profile
    void startProfile(int currentTemp, uint32_t tickTime = 0);

    // Calculate targetTemp based on current time
    int getTargetTemp(uint32_t tickTime = 0);

    // Get final targetTemp
    int getFinalTargetTemp();

    // Calculate targetFanSpeed based on current time
    int getTargetFanSpeed(uint32_t tickTime = 0);

    // Get profile progress based on current time
    int getProfileProgress(uint32_t tickTime = 0);

    // Get setpoint count
    int getSetpointCount();

    // Clear all setpoints
    void clearSetpoints();

    // Add a setpoint to the array
    void addSetpoint(int time, int temp, int fanSpeed);

    // Get the setpoint at index
    Setpoint getSetpoint(int index);

    // Save the profile to EEPROM
    void saveProfileToEEPROM();

    // Load the profile from EEPROM
    void loadProfileFromEEPROM();
};

#endif // RoasterProfiles_h
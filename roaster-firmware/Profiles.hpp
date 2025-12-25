#ifndef PROFILES_HPP
#define PROFILES_HPP

#include <stdio.h>

// NOTE: All temperature values in this file are in Fahrenheit (°F)
class Profiles
{
private:
    typedef struct
    {
        uint32_t time;      // Time in seconds
        uint32_t temp;      // Temperature in Fahrenheit (°F)
        uint32_t fanSpeed;  // Fan speed (0-255)
    } Setpoint;
    Setpoint _setpoints[10];    // Create an array of Setpoints with size 10
    int32_t _setpointCount = 0; // Keep track of how many setpoints are in the array
    uint32_t _startTime = 0;    // Keep track of when the profile started
    uint8_t _profileVersion = 1; // Profile data structure version for future compatibility

public:
    // Constructor
    Profiles();

    // Start the profile
    void startProfile(uint32_t currentTemp, uint32_t tickTime);

    // Calculate targetTemp based on current time
    uint32_t getTargetTemp(uint32_t tickTime) const;

    // Calculate targetTemp at absolute time (no start offset)
    uint32_t getTargetTempAtTime(uint32_t timeMs) const;

    // Get final targetTemp
    uint32_t getFinalTargetTemp() const;

    // Calculate targetFanSpeed based on current time
    uint32_t getTargetFanSpeed(uint32_t tickTime) const;

    // Get profile progress based on current time
    uint32_t getProfileProgress(uint32_t tickTime) const;

    // Get setpoint count
    int getSetpointCount() const;

    // Clear all setpoints
    void clearSetpoints();

    // Add a setpoint to the array
    void addSetpoint(uint32_t time, uint32_t temp, uint32_t fanSpeed);

    // Get the setpoint at index
    Setpoint getSetpoint(int index) const;
    
    // Validate setpoint values are within safe bounds
    bool validateSetpoint(uint32_t temp, uint32_t fanSpeed) const;

    // Flatten the profile into a byte array
    void flattenProfile(uint8_t *buffer);

    // Unflatten the profile from a byte array
    void unflattenProfile(uint8_t *buffer);
};

Profiles::Profiles()
{
    this->addSetpoint(0, 0, 0);
}

void Profiles::startProfile(uint32_t currentTemp, uint32_t tickTime)
{
    _startTime = tickTime;
    // startTime = (tickTime == 0) ? millis() : tickTime;
    _setpoints[0].temp = currentTemp;
    _setpoints[0].fanSpeed = _setpoints[1].fanSpeed;
}

uint32_t Profiles::getTargetTemp(uint32_t tickTime) const
{
    tickTime = tickTime;
    // tickTime = (tickTime == 0) ? millis() : tickTime;
    uint32_t currentTime = tickTime - _startTime;
    for (int i = 0; i < _setpointCount; i++)
    {
        if (_setpoints[i].time > currentTime)
        {
            if (i == 0)
            {
                return _setpoints[i].temp;
            }
            else
            {
                uint32_t prevTemp = _setpoints[i - 1].temp;
                uint32_t nextTemp = _setpoints[i].temp;
                uint32_t prevTime = _setpoints[i - 1].time;
                uint32_t nextTime = _setpoints[i].time;
                
                // Prevent division by zero
                if (nextTime == prevTime) {
                    return nextTemp;
                }
                
                float timeRatio = (float)(currentTime - prevTime) / (nextTime - prevTime);
                return prevTemp + (int)(nextTemp - prevTemp) * timeRatio;
            }
        }
    }
    return _setpoints[_setpointCount - 1].temp;
}

uint32_t Profiles::getTargetTempAtTime(uint32_t timeMs) const
{
    uint32_t currentTime = timeMs;
    for (int i = 0; i < _setpointCount; i++)
    {
        if (_setpoints[i].time > currentTime)
        {
            if (i == 0)
            {
                return _setpoints[i].temp;
            }
            else
            {
                uint32_t prevTemp = _setpoints[i - 1].temp;
                uint32_t nextTemp = _setpoints[i].temp;
                uint32_t prevTime = _setpoints[i - 1].time;
                uint32_t nextTime = _setpoints[i].time;

                // Prevent division by zero
                if (nextTime == prevTime)
                {
                    return nextTemp;
                }

                float timeRatio = (float)(currentTime - prevTime) / (nextTime - prevTime);
                return prevTemp + (int)(nextTemp - prevTemp) * timeRatio;
            }
        }
    }
    return _setpoints[_setpointCount - 1].temp;
}

uint32_t Profiles::getFinalTargetTemp() const
{
    return _setpoints[_setpointCount - 1].temp;
}

uint32_t Profiles::getTargetFanSpeed(uint32_t tickTime) const
{
    uint32_t currentTime = tickTime - _startTime;
    for (int i = 0; i < _setpointCount; i++)
    {
        if (_setpoints[i].time >= currentTime)
        {
            if (i == 0)
            {
                return ((long)_setpoints[i].fanSpeed * 255L) / 100L;
            }
            else
            {
                uint32_t prevFanSpeed = _setpoints[i - 1].fanSpeed;
                uint32_t nextFanSpeed = _setpoints[i].fanSpeed;
                uint32_t prevTime = _setpoints[i - 1].time;
                uint32_t nextTime = _setpoints[i].time;
                
                // Prevent division by zero
                if (nextTime == prevTime) {
                    return ((long)nextFanSpeed * 255L) / 100L;
                }
                
                float timeRatio = (float)(currentTime - prevTime) / (nextTime - prevTime);
                return ((long)(prevFanSpeed + (int)(nextFanSpeed - prevFanSpeed) * timeRatio) * 255L) / 100L;
            }
        }
    }
    return ((long)_setpoints[_setpointCount - 1].fanSpeed * 255L) / 100L;
}

uint32_t Profiles::getProfileProgress(uint32_t tickTime) const
{
    // tickTime = (tickTime == 0) ? millis() : tickTime;
    uint32_t currentTime = tickTime - _startTime;
    if (currentTime >= _setpoints[_setpointCount - 1].time)
    {
        return 100;
    }
    else
    {
        return (uint32_t)((float)currentTime / _setpoints[_setpointCount - 1].time * 100);
    }
}

int Profiles::getSetpointCount() const
{
    return _setpointCount;
}

void Profiles::clearSetpoints()
{
    _setpointCount = 0;
    this->addSetpoint(0, 0, 0);
}

void Profiles::addSetpoint(uint32_t time, uint32_t temp, uint32_t fanSpeed)
{
    if (_setpointCount < 10)
    {
        // Clamp values to safe ranges (uint32_t is always >= 0)
        temp = min(temp, (uint32_t)500);      // 0-500°F
        fanSpeed = min(fanSpeed, (uint32_t)100); // 0-100%
        
        _setpoints[_setpointCount].time = time;
        _setpoints[_setpointCount].temp = temp;
        _setpoints[_setpointCount].fanSpeed = fanSpeed;
        _setpointCount++;
    }
}

bool Profiles::validateSetpoint(uint32_t temp, uint32_t fanSpeed) const
{
    // Validate temperature (0-500°F) and fan speed (0-100%)
    // Note: uint32_t is always >= 0, so only check upper bounds
    return (temp <= 500 && fanSpeed <= 100);
}

Profiles::Setpoint Profiles::getSetpoint(int index) const
{
    return _setpoints[index];
}

void Profiles::flattenProfile(uint8_t *buffer)
{
    // Version byte at position 0 for future compatibility
    buffer[0] = _profileVersion;
    
    // Setpoint count at positions 1-4
    buffer[1] = (uint8_t)(_setpointCount >> 24);
    buffer[2] = (uint8_t)(_setpointCount >> 16);
    buffer[3] = (uint8_t)(_setpointCount >> 8);
    buffer[4] = (uint8_t)(_setpointCount);

    for (int i = 0; i < _setpointCount; i++)
    {
        buffer[i * 12 + 5] = (uint8_t)(_setpoints[i].time >> 24);
        buffer[i * 12 + 6] = (uint8_t)(_setpoints[i].time >> 16);
        buffer[i * 12 + 7] = (uint8_t)(_setpoints[i].time >> 8);
        buffer[i * 12 + 8] = (uint8_t)(_setpoints[i].time);
        buffer[i * 12 + 9] = (uint8_t)(_setpoints[i].temp >> 24);
        buffer[i * 12 + 10] = (uint8_t)(_setpoints[i].temp >> 16);
        buffer[i * 12 + 11] = (uint8_t)(_setpoints[i].temp >> 8);
        buffer[i * 12 + 12] = (uint8_t)(_setpoints[i].temp);
        buffer[i * 12 + 13] = (uint8_t)(_setpoints[i].fanSpeed >> 24);
        buffer[i * 12 + 14] = (uint8_t)(_setpoints[i].fanSpeed >> 16);
        buffer[i * 12 + 15] = (uint8_t)(_setpoints[i].fanSpeed >> 8);
        buffer[i * 12 + 16] = (uint8_t)(_setpoints[i].fanSpeed);
    }
}

void Profiles::unflattenProfile(uint8_t *buffer)
{
    // Read and validate version byte
    _profileVersion = buffer[0];
    // For now, only support version 1
    // In the future, could handle migration from old versions here
    
    // Read setpoint count from positions 1-4
    uint64_t _tempSetpointCount = (static_cast<uint64_t>(buffer[1]) << 24) | (static_cast<uint64_t>(buffer[2]) << 16) | (static_cast<uint64_t>(buffer[3]) << 8) | (static_cast<uint64_t>(buffer[4]));
    if (_tempSetpointCount <= 10 && _tempSetpointCount > 0)
    {
        _setpointCount = static_cast<uint32_t>(_tempSetpointCount);
        for (int i = 0; i < _setpointCount; i++)
        {
            uint32_t time = (static_cast<uint32_t>(buffer[i * 12 + 5]) << 24) | (static_cast<uint32_t>(buffer[i * 12 + 6]) << 16) | (static_cast<uint32_t>(buffer[i * 12 + 7]) << 8) | (static_cast<uint32_t>(buffer[i * 12 + 8]));
            uint32_t temp = (static_cast<uint32_t>(buffer[i * 12 + 9]) << 24) | (static_cast<uint32_t>(buffer[i * 12 + 10]) << 16) | (static_cast<uint32_t>(buffer[i * 12 + 11]) << 8) | (static_cast<uint32_t>(buffer[i * 12 + 12]));
            uint32_t fanSpeed = (static_cast<uint32_t>(buffer[i * 12 + 13]) << 24) | (static_cast<uint32_t>(buffer[i * 12 + 14]) << 16) | (static_cast<uint32_t>(buffer[i * 12 + 15]) << 8) | (static_cast<uint32_t>(buffer[i * 12 + 16]));

            _setpoints[i].time = time;
            _setpoints[i].temp = temp;
            _setpoints[i].fanSpeed = fanSpeed;
        }
    }
}

#endif // PROFILES_HPP

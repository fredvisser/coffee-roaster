#include <stdio.h>

class Profiles
{
private:
    typedef struct
    {
        uint32_t time;
        uint32_t temp;
        uint32_t fanSpeed;
    } Setpoint;
    Setpoint _setpoints[10];    // Create an array of Setpoints with size 10
    int32_t _setpointCount = 0; // Keep track of how many setpoints are in the array
    uint32_t _startTime = 0;    // Keep track of when the profile started

public:
    // Constructor
    Profiles();

    // Start the profile
    void startProfile(uint32_t currentTemp, uint32_t tickTime);

    // Calculate targetTemp based on current time
    uint32_t getTargetTemp(uint32_t tickTime);

    // Get final targetTemp
    uint32_t getFinalTargetTemp();

    // Calculate targetFanSpeed based on current time
    uint32_t getTargetFanSpeed(uint32_t tickTime);

    // Get profile progress based on current time
    uint32_t getProfileProgress(uint32_t tickTime);

    // Get setpoint count
    int getSetpointCount();

    // Clear all setpoints
    void clearSetpoints();

    // Add a setpoint to the array
    void addSetpoint(uint32_t time, uint32_t temp, uint32_t fanSpeed);

    // Get the setpoint at index
    Setpoint getSetpoint(int index);

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

uint32_t Profiles::getTargetTemp(uint32_t tickTime)
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
                float timeRatio = (float)(currentTime - prevTime) / (nextTime - prevTime);
                return prevTemp + (int)(nextTemp - prevTemp) * timeRatio;
            }
        }
    }
    return _setpoints[_setpointCount - 1].temp;
}

uint32_t Profiles::getFinalTargetTemp()
{
    return _setpoints[_setpointCount - 1].temp;
}

uint32_t Profiles::getTargetFanSpeed(uint32_t tickTime)
{
    uint32_t currentTime = tickTime - _startTime;
    for (int i = 0; i < _setpointCount; i++)
    {
        if (_setpoints[i].time >= currentTime)
        {
            if (i == 0)
            {
                return (_setpoints[i].fanSpeed * 255) / 100;
            }
            else
            {
                uint32_t prevFanSpeed = _setpoints[i - 1].fanSpeed;
                uint32_t nextFanSpeed = _setpoints[i].fanSpeed;
                uint32_t prevTime = _setpoints[i - 1].time;
                uint32_t nextTime = _setpoints[i].time;
                float timeRatio = (float)(currentTime - prevTime) / (nextTime - prevTime);
                return (prevFanSpeed + (int)(nextFanSpeed - prevFanSpeed) * timeRatio) * 255 / 100;
            }
        }
    }
    return (_setpoints[_setpointCount - 1].fanSpeed * 255) / 100;
}

uint32_t Profiles::getProfileProgress(uint32_t tickTime)
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

int Profiles::getSetpointCount()
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
        _setpoints[_setpointCount].time = time;
        _setpoints[_setpointCount].temp = temp;
        _setpoints[_setpointCount].fanSpeed = fanSpeed;
        _setpointCount++;
    }
}

Profiles::Setpoint Profiles::getSetpoint(int index)
{
    return _setpoints[index];
}

void Profiles::flattenProfile(uint8_t *buffer)
{
    buffer[0] = (uint8_t)(_setpointCount >> 24);
    buffer[1] = (uint8_t)(_setpointCount >> 16);
    buffer[2] = (uint8_t)(_setpointCount >> 8);
    buffer[3] = (uint8_t)(_setpointCount);

    for (int i = 0; i < _setpointCount; i++)
    {
        buffer[i * 12 + 4] = (uint8_t)(_setpoints[i].time >> 24);
        buffer[i * 12 + 5] = (uint8_t)(_setpoints[i].time >> 16);
        buffer[i * 12 + 6] = (uint8_t)(_setpoints[i].time >> 8);
        buffer[i * 12 + 7] = (uint8_t)(_setpoints[i].time);
        buffer[i * 12 + 8] = (uint8_t)(_setpoints[i].temp >> 24);
        buffer[i * 12 + 9] = (uint8_t)(_setpoints[i].temp >> 16);
        buffer[i * 12 + 10] = (uint8_t)(_setpoints[i].temp >> 8);
        buffer[i * 12 + 11] = (uint8_t)(_setpoints[i].temp);
        buffer[i * 12 + 12] = (uint8_t)(_setpoints[i].fanSpeed >> 24);
        buffer[i * 12 + 13] = (uint8_t)(_setpoints[i].fanSpeed >> 16);
        buffer[i * 12 + 14] = (uint8_t)(_setpoints[i].fanSpeed >> 8);
        buffer[i * 12 + 15] = (uint8_t)(_setpoints[i].fanSpeed);
    }
}

void Profiles::unflattenProfile(uint8_t *buffer)
{
    uint64_t _tempSetpointCount = (static_cast<uint64_t>(buffer[0]) << 24) | (static_cast<uint64_t>(buffer[1]) << 16) | (static_cast<uint64_t>(buffer[2]) << 8) | (static_cast<uint64_t>(buffer[3]));
    if (_tempSetpointCount <= 10 && _tempSetpointCount > 0)
    {
        _setpointCount = static_cast<uint32_t>(_tempSetpointCount);
        for (int i = 0; i < _setpointCount; i++)
        {
            uint32_t time = (static_cast<uint32_t>(buffer[i * 12 + 4]) << 24) | (static_cast<uint32_t>(buffer[i * 12 + 5]) << 16) | (static_cast<uint32_t>(buffer[i * 12 + 6]) << 8) | (static_cast<uint32_t>(buffer[i * 12 + 7]));
            uint32_t temp = (static_cast<uint32_t>(buffer[i * 12 + 8]) << 24) | (static_cast<uint32_t>(buffer[i * 12 + 9]) << 16) | (static_cast<uint32_t>(buffer[i * 12 + 10]) << 8) | (static_cast<uint32_t>(buffer[i * 12 + 11]));
            uint32_t fanSpeed = (static_cast<uint32_t>(buffer[i * 12 + 12]) << 24) | (static_cast<uint32_t>(buffer[i * 12 + 13]) << 16) | (static_cast<uint32_t>(buffer[i * 12 + 14]) << 8) | (static_cast<uint32_t>(buffer[i * 12 + 15]));

            _setpoints[i].time = time;
            _setpoints[i].temp = temp;
            _setpoints[i].fanSpeed = fanSpeed;
        }
    }
}

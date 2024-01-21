#include "Profiles.h"

Profiles::Profiles()
{
    this->addSetpoint(0, 0, 0);
}

void Profiles::startProfile(int currentTemp, uint32_t tickTime)
{
    startTime = (tickTime == 0) ? millis() : tickTime;
    setpoints[0].temp = currentTemp;
}

int Profiles::getTargetTemp(uint32_t tickTime)
{
    tickTime = (tickTime == 0) ? millis() : tickTime;
    uint32_t currentTime = tickTime - startTime;
    for (int i = 0; i < setpointCount; i++)
    {
        if (setpoints[i].time > currentTime)
        {
            if (i == 0)
            {
                return setpoints[i].temp;
            }
            else
            {
                int prevTemp = setpoints[i - 1].temp;
                int nextTemp = setpoints[i].temp;
                int prevTime = setpoints[i - 1].time;
                int nextTime = setpoints[i].time;
                float timeRatio = (float)(currentTime - prevTime) / (nextTime - prevTime);
                return prevTemp + (nextTemp - prevTemp) * timeRatio;
            }
        }
    }
    return setpoints[setpointCount - 1].temp;
}

int Profiles::getFinalTargetTemp()
{
    return setpoints[setpointCount - 1].temp;
}

int Profiles::getTargetFanSpeed(uint32_t tickTime)
{
    tickTime = (tickTime == 0) ? millis() : tickTime;
    uint32_t currentTime = tickTime - startTime;
    for (int i = 0; i < setpointCount; i++)
    {
        if (setpoints[i].time > currentTime)
        {
            if (i == 0)
            {
                return setpoints[i].fanSpeed;
            }
            else
            {
                int prevFanSpeed = setpoints[i - 1].fanSpeed;
                int nextFanSpeed = setpoints[i].fanSpeed;
                int prevTime = setpoints[i - 1].time;
                int nextTime = setpoints[i].time;
                float timeRatio = (float)(currentTime - prevTime) / (nextTime - prevTime);
                return prevFanSpeed + (nextFanSpeed - prevFanSpeed) * timeRatio;
            }
        }
    }
}

int Profiles::getProfileProgress(uint32_t tickTime)
{
    tickTime = (tickTime == 0) ? millis() : tickTime;
    uint32_t currentTime = tickTime - startTime;
    if (currentTime >= setpoints[setpointCount - 1].time)
    {
        return 100;
    }
    else
    {
        return (int)((float)currentTime / setpoints[setpointCount - 1].time * 100);
    }
}

int Profiles::getSetpointCount()
{
    return setpointCount;
}

void Profiles::clearSetpoints()
{
    setpointCount = 0;
    this->addSetpoint(0, 0, 0);
}

void Profiles::addSetpoint(int time, int temp, int fanSpeed)
{
    if (setpointCount < 10)
    {
        setpoints[setpointCount].time = time;
        setpoints[setpointCount].temp = temp;
        setpoints[setpointCount].fanSpeed = fanSpeed;
        setpointCount++;
    }
}

Profiles::Setpoint Profiles::getSetpoint(int index)
{
    return setpoints[index];
}

void Profiles::saveProfileToEEPROM()
{
    EEPROM.put(0, setpointCount);
    EEPROM.put(4, setpoints);
}

void Profiles::loadProfileFromEEPROM()
{
    EEPROM.get(0, setpointCount);
    EEPROM.get(4, setpoints);
}
#ifndef THERMOCOUPLE_SAFETY_HPP
#define THERMOCOUPLE_SAFETY_HPP

#include <math.h>
#include "../platform/RoasterTypes.hpp"

inline bool thermocoupleReadingIsValid(double reading, double minimum, double maximum)
{
  return isfinite(reading) && reading != 32.0 && reading >= minimum && reading <= maximum;
}

inline bool absoluteTemperatureLimitExceeded(double temperature)
{
  return temperature > MAX_SAFE_TEMP;
}

inline bool roastTemperatureLimitExceeded(double temperature, bool roastActive)
{
  return roastActive && temperature > MAX_ROAST_TEMP;
}

inline bool heaterRiseTimeoutElapsed(uint32_t elapsedMs)
{
  return elapsedMs >= MAX_HEATER_NO_RISE_MS;
}

#endif
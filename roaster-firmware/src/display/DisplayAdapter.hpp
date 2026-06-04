#ifndef DISPLAY_ADAPTER_HPP
#define DISPLAY_ADAPTER_HPP

#include <Arduino.h>
#include "DisplayBackendConfig.hpp"
#include "DisplayTypes.hpp"

#if ROASTER_DISPLAY_BACKEND == ROASTER_DISPLAY_BACKEND_LVGL
#include "LvglDisplay.hpp"
#endif

inline void displayBegin(unsigned long baudRate = 115200)
{
#if ROASTER_DISPLAY_BACKEND == ROASTER_DISPLAY_BACKEND_LVGL
  LvglDisplay::begin();
#else
  (void)baudRate;
#endif
}

inline void displayTick()
{
#if ROASTER_DISPLAY_BACKEND == ROASTER_DISPLAY_BACKEND_LVGL
  LvglDisplay::tick();
#endif
}

inline bool displayPopAction(DisplayAction &action)
{
#if ROASTER_DISPLAY_BACKEND == ROASTER_DISPLAY_BACKEND_LVGL
  return LvglDisplay::popAction(action);
#else
  action = DisplayAction::None;
  return false;
#endif
}

inline int displayReadFinalTargetTemp()
{
#if ROASTER_DISPLAY_BACKEND == ROASTER_DISPLAY_BACKEND_LVGL
  return LvglDisplay::readFinalTargetTemp();
#else
  return DISPLAY_READ_ERROR;
#endif
}

inline void displayShowScreen(DisplayScreen screen)
{
#if ROASTER_DISPLAY_BACKEND == ROASTER_DISPLAY_BACKEND_LVGL
  LvglDisplay::showScreen(screen);
#else
  (void)screen;
#endif
}

inline void displaySetCurrentTemp(int tempF)
{
#if ROASTER_DISPLAY_BACKEND == ROASTER_DISPLAY_BACKEND_LVGL
  LvglDisplay::setCurrentTempValue(tempF);
#else
  (void)tempF;
#endif
}

inline void displaySetTargetTemp(int tempF)
{
#if ROASTER_DISPLAY_BACKEND == ROASTER_DISPLAY_BACKEND_LVGL
  LvglDisplay::setTargetTempValue(tempF);
#else
  (void)tempF;
#endif
}

inline void displaySetFinalTargetTemp(int tempF)
{
#if ROASTER_DISPLAY_BACKEND == ROASTER_DISPLAY_BACKEND_LVGL
  LvglDisplay::setFinalTargetTempValue(tempF);
#else
  (void)tempF;
#endif
}

inline void displaySetFanPercent(int percent)
{
#if ROASTER_DISPLAY_BACKEND == ROASTER_DISPLAY_BACKEND_LVGL
  LvglDisplay::setFanPercentValue(percent);
#else
  (void)percent;
#endif
}

inline void displaySetProgress(int progress)
{
#if ROASTER_DISPLAY_BACKEND == ROASTER_DISPLAY_BACKEND_LVGL
  LvglDisplay::setProgressValue(progress);
#else
  (void)progress;
#endif
}

inline void displayUpdateTelemetry(const DisplayTelemetry &telemetry)
{
#if ROASTER_DISPLAY_BACKEND == ROASTER_DISPLAY_BACKEND_LVGL
  LvglDisplay::updateTelemetry(telemetry);
#endif

  displaySetCurrentTemp(telemetry.currentTempF);

  if (telemetry.targetTempF >= 0)
  {
    displaySetTargetTemp(telemetry.targetTempF);
  }

  if (telemetry.finalTargetTempF >= 0)
  {
    displaySetFinalTargetTemp(telemetry.finalTargetTempF);
  }

  if (telemetry.fanPercent >= 0)
  {
    displaySetFanPercent(telemetry.fanPercent);
  }

  if (telemetry.progressSeconds >= 0)
  {
    displaySetProgress(telemetry.progressSeconds);
  }
}

inline void displaySetWifiIp(const String &ipAddress)
{
#if ROASTER_DISPLAY_BACKEND == ROASTER_DISPLAY_BACKEND_LVGL
  LvglDisplay::setWifiStatusText(ipAddress);
#else
  (void)ipAddress;
#endif
}

inline DisplayWifiFormState displayReadWifiFormState()
{
#if ROASTER_DISPLAY_BACKEND == ROASTER_DISPLAY_BACKEND_LVGL
  return LvglDisplay::readWifiFormState();
#else
  return DisplayWifiFormState{};
#endif
}

inline void displaySetRevision(const char *revision)
{
#if ROASTER_DISPLAY_BACKEND == ROASTER_DISPLAY_BACKEND_LVGL
  LvglDisplay::setRevisionText(String(revision));
#else
  (void)revision;
#endif
}

inline void displaySetActiveProfileLabel(const String &label)
{
#if ROASTER_DISPLAY_BACKEND == ROASTER_DISPLAY_BACKEND_LVGL
  LvglDisplay::setActiveProfileText(label);
#else
  (void)label;
#endif
}

inline void displayShowErrorMessage(const char *message)
{
#if ROASTER_DISPLAY_BACKEND == ROASTER_DISPLAY_BACKEND_LVGL
  LvglDisplay::showScreen(DisplayScreen::Error);
  LvglDisplay::setErrorMessageText(String(message));
#else
  (void)message;
#endif
}

inline void displayClearProfileWaveform()
{
#if ROASTER_DISPLAY_BACKEND == ROASTER_DISPLAY_BACKEND_LVGL
  LvglDisplay::clearProfileWaveformData();
#endif
}

inline void displayAddProfileWaveformPoint(uint32_t value)
{
#if ROASTER_DISPLAY_BACKEND == ROASTER_DISPLAY_BACKEND_LVGL
  LvglDisplay::appendProfileWaveformDataPoint(static_cast<int32_t>(value));
#else
  (void)value;
#endif
}

inline void displayRefreshProfilePlotButton()
{
#if ROASTER_DISPLAY_BACKEND == ROASTER_DISPLAY_BACKEND_LVGL
  LvglDisplay::refreshProfilePlot();
#endif
}

#endif
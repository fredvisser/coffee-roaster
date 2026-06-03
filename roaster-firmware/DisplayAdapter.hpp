#ifndef DISPLAY_ADAPTER_HPP
#define DISPLAY_ADAPTER_HPP

#include <Arduino.h>
#include "DisplayBackendConfig.hpp"
#include "DisplayTypes.hpp"

#if ROASTER_DISPLAY_BACKEND == ROASTER_DISPLAY_BACKEND_NEXTION
#include <EasyNextionLibrary.h>
#else
#include "LvglDisplay.hpp"
#endif

#if ROASTER_DISPLAY_BACKEND == ROASTER_DISPLAY_BACKEND_NEXTION
extern EasyNex myNex;
#endif

inline const char *displayScreenCommand(DisplayScreen screen)
{
  switch (screen)
  {
  case DisplayScreen::Start:
    return "page Start";
  case DisplayScreen::Network:
#if ROASTER_DISPLAY_BACKEND == ROASTER_DISPLAY_BACKEND_NEXTION
    return "page Start";
#else
    return "page Network";
#endif
  case DisplayScreen::Roasting:
    return "page Roasting";
  case DisplayScreen::Cooling:
    return "page Cooling";
  case DisplayScreen::Error:
    return "page Error";
  case DisplayScreen::ProfileActive:
    return "page ProfileActive";
  default:
    return "page Start";
  }
}

inline void displayBegin(unsigned long baudRate = 115200)
{
#if ROASTER_DISPLAY_BACKEND == ROASTER_DISPLAY_BACKEND_NEXTION
  myNex.begin(baudRate);
#else
  LV_UNUSED(baudRate);
  LvglDisplay::begin();
#endif
}

inline void displayTick()
{
#if ROASTER_DISPLAY_BACKEND == ROASTER_DISPLAY_BACKEND_NEXTION
  myNex.NextionListen();
#else
  LvglDisplay::tick();
#endif
}

inline bool displayPopAction(DisplayAction &action)
{
#if ROASTER_DISPLAY_BACKEND == ROASTER_DISPLAY_BACKEND_NEXTION
  (void)action;
  return false;
#else
  return LvglDisplay::popAction(action);
#endif
}

inline int displayReadNumber(const char *component)
{
#if ROASTER_DISPLAY_BACKEND == ROASTER_DISPLAY_BACKEND_NEXTION
  return myNex.readNumber(component);
#else
  return LvglDisplay::readNumber(component);
#endif
}

inline String displayReadText(const char *component)
{
#if ROASTER_DISPLAY_BACKEND == ROASTER_DISPLAY_BACKEND_NEXTION
  return myNex.readStr(component);
#else
  return LvglDisplay::readText(component);
#endif
}

inline void displayWriteNumber(const char *component, int value)
{
#if ROASTER_DISPLAY_BACKEND == ROASTER_DISPLAY_BACKEND_NEXTION
  myNex.writeNum(component, value);
#else
  LvglDisplay::writeNumber(component, value);
#endif
}

inline void displayWriteText(const char *component, const char *value)
{
#if ROASTER_DISPLAY_BACKEND == ROASTER_DISPLAY_BACKEND_NEXTION
  myNex.writeStr(component, value);
#else
  LvglDisplay::writeText(component, String(value));
#endif
}

inline void displayWriteText(const char *component, const String &value)
{
#if ROASTER_DISPLAY_BACKEND == ROASTER_DISPLAY_BACKEND_NEXTION
  myNex.writeStr(component, value);
#else
  LvglDisplay::writeText(component, value);
#endif
}

inline void displaySendCommand(const char *command)
{
#if ROASTER_DISPLAY_BACKEND == ROASTER_DISPLAY_BACKEND_NEXTION
  myNex.writeStr(command);
#else
  LvglDisplay::sendCommand(command);
#endif
}

inline void displayShowScreen(DisplayScreen screen)
{
  displaySendCommand(displayScreenCommand(screen));
}

inline void displaySetCurrentTemp(int tempF)
{
  displayWriteNumber("globals.currentTempNum.val", tempF);
}

inline void displaySetTargetTemp(int tempF)
{
  displayWriteNumber("globals.nextSetTempNum.val", tempF);
}

inline void displaySetFinalTargetTemp(int tempF)
{
  displayWriteNumber("globals.setTempNum.val", tempF);
}

inline void displaySetFanPercent(int percent)
{
  displayWriteNumber("globals.setpointFan.val", percent);
}

inline void displaySetProgress(int progress)
{
  displayWriteNumber("globals.setpointProg.val", progress);
}

inline void displayUpdateTelemetry(const DisplayTelemetry &telemetry)
{
#if ROASTER_DISPLAY_BACKEND != ROASTER_DISPLAY_BACKEND_NEXTION
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
  displayWriteText("ConfigWifi.ip.txt", ipAddress);
}

inline DisplayWifiFormState displayReadWifiFormState()
{
  DisplayWifiFormState form;
  form.ssid = displayReadText("ConfigWifi.ssid.txt");
  form.password = displayReadText("ConfigWifi.password.txt");
  return form;
}

inline void displaySetRevision(const char *revision)
{
  displayWriteText("ConfigNav.rev.txt", revision);
}

inline void displaySetActiveProfileLabel(const String &label)
{
  displayWriteText("ProfileActive.t1.txt", label);
}

inline void displayShowErrorMessage(const char *message)
{
  displayShowScreen(DisplayScreen::Error);
  displayWriteText("Error.message.txt", message);
}

inline void displayClearProfileWaveform()
{
  displaySendCommand("s0.clr");
}

inline void displayAddProfileWaveformPoint(uint32_t value)
{
  char command[32];
  snprintf(command, sizeof(command), "add 2,0,%lu", (unsigned long)value);
  displaySendCommand(command);
}

inline void displayRefreshProfilePlotButton()
{
  displaySendCommand("ref b1");
}

#endif
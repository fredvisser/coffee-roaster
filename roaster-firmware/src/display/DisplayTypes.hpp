#ifndef DISPLAY_TYPES_HPP
#define DISPLAY_TYPES_HPP

#include <Arduino.h>
#include "../platform/RoasterTypes.hpp"

enum class DisplayScreen
{
  Start,
  Network,
  Roasting,
  Cooling,
  Error,
  ProfileList,
  ProfileActive
};

enum class DisplayAction
{
  None,
  StartRoast,
  OpenNetwork,
  StopRoast,
  StopCooling,
  ApplyWifi,
  OpenActiveProfile,
  ActivateSelectedProfile,
  ViewSelectedProfileGraph,
  SaveFinalTargetToProfile,
  DuplicateSelectedProfile,
  PreviousProfile,
  NextProfile,
  ReturnToProfileList,
  ReturnToStateScreen
};

struct DisplayProfileSummary
{
  String name;
  int finalTargetF = -1;
  bool active = false;
};

struct DisplayTelemetry
{
  RoasterState roasterState = IDLE;
  int currentTempF = 0;
  int targetTempF = -1;
  int finalTargetTempF = -1;
  int fanPercent = -1;
  int progressSeconds = -1;
  int elapsedSeconds = -1;
  int heaterOutput = -1;
  int bdcFanMicros = -1;
  int fanTempF = -1;
};

struct DisplayWifiFormState
{
  String ssid;
  String password;
};

#endif
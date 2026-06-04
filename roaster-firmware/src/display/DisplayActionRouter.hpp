#ifndef DISPLAY_ACTION_ROUTER_HPP
#define DISPLAY_ACTION_ROUTER_HPP

#include <Arduino.h>
#include <vector>

#include "DisplayAdapter.hpp"
#include "../control/RoastSessionLifecycle.hpp"
#include "../profiles/ProfileEditor.hpp"
#include "../profiles/ProfileManager.hpp"
#include "../support/DebugLog.hpp"

extern int finalTempOverride;

extern RoastProfile profile;
extern ProfileManager profileManager;

void handleStartRoastCommand();
void handleStopRoastCommand();
void handleStopCoolingCommand();
void handleApplyWifiCommand();
void handleOpenActiveProfileCommand();

inline bool cycleActiveProfile(int direction)
{
  if (roasterState != IDLE)
  {
    LOG_WARN("Ignoring profile change request while roaster is not idle");
    return false;
  }

  std::vector<String> profileIds = getProfileIds();
  if (profileIds.empty())
  {
    LOG_WARN("No profiles available to cycle");
    return false;
  }

  String activeId = getActiveProfileId();
  size_t activeIndex = 0;
  bool foundActive = false;
  for (size_t index = 0; index < profileIds.size(); ++index)
  {
    if (profileIds[index] == activeId)
    {
      activeIndex = index;
      foundActive = true;
      break;
    }
  }

  if (!foundActive)
  {
    activeIndex = 0;
  }

  int nextIndex = static_cast<int>(activeIndex) + direction;
  if (nextIndex < 0)
  {
    nextIndex = static_cast<int>(profileIds.size()) - 1;
  }
  else if (nextIndex >= static_cast<int>(profileIds.size()))
  {
    nextIndex = 0;
  }

  String nextId = profileIds[nextIndex];
  if (!profileManager.loadProfile(nextId))
  {
    LOG_WARNF("Failed to load profile id=%s", nextId.c_str());
    return false;
  }

  setActiveProfileId(nextId);
  finalTempOverride = profile.getFinalTargetTemp();
  displaySetFinalTargetTemp(getEffectiveFinalTargetTemp());
  onProfileActivePageEnter();
  displayShowScreen(DisplayScreen::ProfileActive);
  LOG_INFOF("Cycled active profile to id=%s", nextId.c_str());
  return true;
}

inline void handleDisplayAction(DisplayAction action)
{
  switch (action)
  {
  case DisplayAction::StartRoast:
    handleStartRoastCommand();
    break;
  case DisplayAction::OpenNetwork:
    displayShowScreen(DisplayScreen::Network);
    break;
  case DisplayAction::StopRoast:
    handleStopRoastCommand();
    break;
  case DisplayAction::StopCooling:
    handleStopCoolingCommand();
    break;
  case DisplayAction::ApplyWifi:
    handleApplyWifiCommand();
    break;
  case DisplayAction::OpenActiveProfile:
    handleOpenActiveProfileCommand();
    break;
  case DisplayAction::PreviousProfile:
    cycleActiveProfile(-1);
    break;
  case DisplayAction::NextProfile:
    cycleActiveProfile(1);
    break;
  case DisplayAction::ReturnToStateScreen:
    displayShowScreen(displayScreenForCurrentState());
    break;
  case DisplayAction::None:
  default:
    break;
  }
}

inline void handleDisplayActions()
{
  DisplayAction action = DisplayAction::None;
  while (displayPopAction(action))
  {
    handleDisplayAction(action);
  }
}

#endif // DISPLAY_ACTION_ROUTER_HPP
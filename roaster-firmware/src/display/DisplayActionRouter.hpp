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

inline std::vector<String> profileBrowserIds;
inline std::vector<DisplayProfileSummary> profileBrowserEntries;
inline String profileBrowserFocusedId;

inline void plotProfileOnDisplay(const RoastProfile &profileToPlot)
{
  int count = profileToPlot.getSetpointCount();
  if (count < 2)
  {
    displaySetProfileGraphBounds(-1, 0);
    displayClearProfileWaveform();
    displayRefreshProfilePlotButton();
    return;
  }

  auto finalSetpoint = profileToPlot.getSetpoint(count - 1);
  uint32_t maxTime = finalSetpoint.time;
  uint32_t maxTemp = finalSetpoint.temp;
  if (maxTime == 0 || maxTemp == 0)
  {
    displaySetProfileGraphBounds(-1, 0);
    displayClearProfileWaveform();
    displayRefreshProfilePlotButton();
    return;
  }

  const int waveformWidth = 480;
  const int waveformHeight = 170;

  displayClearProfileWaveform();
  displaySetProfileGraphBounds(static_cast<int>(maxTemp), maxTime / 1000UL);
  for (int index = 0; index < waveformWidth; ++index)
  {
    if ((index & 0x0F) == 0)
    {
      yield();
    }

    uint32_t timeAtX = (maxTime * static_cast<uint32_t>(waveformWidth - 1 - index)) / static_cast<uint32_t>(waveformWidth);
    uint32_t interpolatedTemp = profileToPlot.getTargetTempAtTime(timeAtX);
    uint32_t scaledTemp = (interpolatedTemp * static_cast<uint32_t>(waveformHeight)) / maxTemp;
    if (scaledTemp > static_cast<uint32_t>(waveformHeight))
    {
      scaledTemp = static_cast<uint32_t>(waveformHeight);
    }

    displayAddProfileWaveformPoint(scaledTemp);
  }

  displayRefreshProfilePlotButton();
}

inline void syncActiveProfileDisplay(bool syncFinalTarget)
{
  String activeId = profileManager.getActiveProfileId();
  if (activeId.length() == 0)
  {
    return;
  }

  String activeName;
  int finalTarget = -1;
  bool active = false;
  if (!profileManager.getProfileSummary(activeId, activeName, finalTarget, active))
  {
    return;
  }

  displaySetActiveProfileLabel(activeName);
  displaySetStoredProfileFinalTarget(finalTarget);
  if (syncFinalTarget)
  {
    displaySetFinalTargetTemp(getEffectiveFinalTargetTemp());
  }
}

inline void refreshProfileBrowser(const String &preferredId = String())
{
  profileBrowserIds.clear();
  profileBrowserEntries.clear();

  String selectedId = preferredId.length() > 0 ? preferredId : profileBrowserFocusedId;
  if (selectedId.length() == 0)
  {
    selectedId = profileManager.getActiveProfileId();
  }

  std::vector<String> ids = profileManager.getProfileIds();
  int selectedIndex = -1;
  for (size_t index = 0; index < ids.size(); ++index)
  {
    String name;
    int finalTarget = -1;
    bool active = false;
    if (!profileManager.getProfileSummary(ids[index], name, finalTarget, active))
    {
      continue;
    }

    profileBrowserIds.push_back(ids[index]);
    profileBrowserEntries.push_back(DisplayProfileSummary{name, finalTarget, active});
    if (ids[index] == selectedId)
    {
      selectedIndex = static_cast<int>(profileBrowserEntries.size()) - 1;
    }
  }

  if (profileBrowserEntries.empty())
  {
    displaySetProfileBrowserEntries(profileBrowserEntries, -1);
    profileBrowserFocusedId = String();
    return;
  }

  if (selectedIndex < 0)
  {
    selectedIndex = 0;
  }

  profileBrowserFocusedId = profileBrowserIds[static_cast<size_t>(selectedIndex)];
  displaySetProfileBrowserEntries(profileBrowserEntries, selectedIndex);
  displaySetProfileBrowserFocus(profileBrowserEntries[static_cast<size_t>(selectedIndex)].name,
                                profileBrowserEntries[static_cast<size_t>(selectedIndex)].finalTargetF);
}

inline bool readSelectedProfileBrowserEntry(String &idOut,
                                            DisplayProfileSummary &summaryOut,
                                            RoastProfile *profileOut = nullptr)
{
  int selectedIndex = displayReadSelectedProfileIndex();
  if (selectedIndex < 0 || selectedIndex >= static_cast<int>(profileBrowserIds.size()))
  {
    return false;
  }

  idOut = profileBrowserIds[static_cast<size_t>(selectedIndex)];
  summaryOut = profileBrowserEntries[static_cast<size_t>(selectedIndex)];
  profileBrowserFocusedId = idOut;

  if (profileOut != nullptr && !profileManager.readProfile(idOut, *profileOut))
  {
    return false;
  }

  return true;
}

inline bool openProfileBrowser()
{
  refreshProfileBrowser(profileManager.getActiveProfileId());
  if (profileBrowserEntries.empty())
  {
    LOG_WARN("No profiles available to browse");
    return false;
  }

  displayShowScreen(DisplayScreen::ProfileList);
  return true;
}

inline bool openSelectedProfileGraph()
{
  String selectedId;
  DisplayProfileSummary selectedSummary;
  RoastProfile selectedProfile;
  if (!readSelectedProfileBrowserEntry(selectedId, selectedSummary, &selectedProfile))
  {
    LOG_WARN("No profile selected for graph view");
    return false;
  }

  displaySetProfileBrowserFocus(selectedSummary.name, selectedSummary.finalTargetF);
  plotProfileOnDisplay(selectedProfile);
  displayShowScreen(DisplayScreen::ProfileActive);
  return true;
}

inline bool activateSelectedProfile()
{
  if (roasterState != IDLE)
  {
    LOG_WARN("Ignoring profile activation while roaster is not idle");
    return false;
  }

  String selectedId;
  DisplayProfileSummary selectedSummary;
  if (!readSelectedProfileBrowserEntry(selectedId, selectedSummary))
  {
    return false;
  }

  if (!profileManager.activateProfile(selectedId))
  {
    LOG_WARNF("Failed to activate profile id=%s", selectedId.c_str());
    return false;
  }

  finalTempOverride = profile.getFinalTargetTemp();
  syncActiveProfileDisplay(false);
  displaySetFinalTargetTemp(finalTempOverride);
  refreshProfileBrowser(selectedId);
  displayShowScreen(displayScreenForCurrentState());
  return true;
}

inline bool saveActiveProfileFinalTarget()
{
  String activeId = profileManager.getActiveProfileId();
  if (activeId.length() == 0)
  {
    return false;
  }

  int requestedFinalTarget = displayReadFinalTargetTemp();
  if (requestedFinalTarget == DISPLAY_READ_ERROR || requestedFinalTarget <= 0)
  {
    return false;
  }

  ProfileOperationResult result = profileManager.updateProfileFinalTarget(activeId, static_cast<uint32_t>(requestedFinalTarget));
  if (!result.success)
  {
    LOG_WARNF("Failed to save final target for profile id=%s", activeId.c_str());
    return false;
  }

  finalTempOverride = requestedFinalTarget;
  displaySetStoredProfileFinalTarget(requestedFinalTarget);
  displaySetFinalTargetTemp(requestedFinalTarget);
  refreshProfileBrowser(activeId);
  syncActiveProfileDisplay(false);
  return true;
}

inline bool duplicateSelectedProfile()
{
  String selectedId;
  DisplayProfileSummary selectedSummary;
  if (!readSelectedProfileBrowserEntry(selectedId, selectedSummary))
  {
    return false;
  }

  String requestedName = displayReadProfileNameInput();
  requestedName.trim();
  if (requestedName.length() == 0)
  {
    requestedName = selectedSummary.name + " Copy";
  }

  ProfileOperationResult result = profileManager.duplicateProfile(selectedId, requestedName);
  if (!result.success)
  {
    LOG_WARNF("Failed to duplicate profile id=%s", selectedId.c_str());
    return false;
  }

  refreshProfileBrowser(result.id);
  return openSelectedProfileGraph();
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
    openProfileBrowser();
    break;
  case DisplayAction::ActivateSelectedProfile:
    activateSelectedProfile();
    break;
  case DisplayAction::ViewSelectedProfileGraph:
    openSelectedProfileGraph();
    break;
  case DisplayAction::SaveFinalTargetToProfile:
    saveActiveProfileFinalTarget();
    break;
  case DisplayAction::DuplicateSelectedProfile:
    duplicateSelectedProfile();
    break;
  case DisplayAction::PreviousProfile:
  case DisplayAction::NextProfile:
    break;
  case DisplayAction::ReturnToProfileList:
    refreshProfileBrowser(profileBrowserFocusedId);
    displayShowScreen(DisplayScreen::ProfileList);
    break;
  case DisplayAction::ReturnToStateScreen:
    syncActiveProfileDisplay(true);
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
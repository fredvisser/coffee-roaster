#ifndef LVGL_DISPLAY_HPP
#define LVGL_DISPLAY_HPP

#include <Arduino.h>
#include "DisplayBackendConfig.hpp"

#if ROASTER_DISPLAY_BACKEND == ROASTER_DISPLAY_BACKEND_LVGL

#include <lvgl.h>
#include <vector>
#include <PINS_JC4827W543.h>
#include <TAMC_GT911.h>
#include <esp_heap_caps.h>
#include "../platform/BoardConfig.hpp"
#include "../support/DebugLog.hpp"
#include "DisplayTypes.hpp"

namespace LvglDisplay
{
inline TAMC_GT911 touchController(
    BoardConfig::TouchSdaPin,
    BoardConfig::TouchSclPin,
    BoardConfig::TouchIntPin,
    BoardConfig::TouchResetPin,
    BoardConfig::DisplayWidth,
    BoardConfig::DisplayHeight);

inline lv_display_t *displayInstance = nullptr;
inline lv_indev_t *inputDevice = nullptr;
inline lv_color_t *drawBuffer = nullptr;
inline uint32_t drawBufferPixelCount = 0;
inline DisplayScreen activeScreen = DisplayScreen::Start;
inline RoasterState currentRoasterState = IDLE;
inline DisplayWifiFormState wifiFormState;
inline constexpr uint16_t ProfileWaveformCapacity = 480;
inline constexpr uint8_t ActionQueueCapacity = 8;

inline int currentTempValue = 0;
inline int targetTempValue = -1;
inline int finalTargetTempValue = -1;
inline int storedProfileFinalTargetValue = -1;
inline int fanPercentValue = -1;
inline int progressPercentValue = -1;
inline int elapsedSecondsValue = -1;
inline int fanTempValue = -1;
inline int heaterOutputValue = -1;
inline int bdcFanMicrosValue = -1;
inline String wifiStatusText = "No network";
inline String activeProfileText = "No profile";
inline String profileBrowserFocusText;
inline String revisionText;
inline String errorMessageText;
inline int profileBrowserFocusFinalTargetValue = -1;
inline int profileGraphMaxTempValue = -1;
inline uint32_t profileGraphMaxTimeSeconds = 0;

inline constexpr char TempUnitSymbol[] = "°";
inline constexpr char TempUnitSuffix[] = "°F";

inline constexpr uint32_t ColorBackground = 0x111111;
inline constexpr uint32_t ColorHeader = 0x252525;
inline constexpr uint32_t ColorPanel = 0x1E1E1E;
inline constexpr uint32_t ColorPanelMuted = 0x262626;
inline constexpr uint32_t ColorTextPrimary = 0xF1F1F1;
inline constexpr uint32_t ColorTextMuted = 0x969696;
inline constexpr uint32_t ColorAccentReady = 0x06B97A;
inline constexpr uint32_t ColorAccentHeat = 0xFF3C45;
inline constexpr uint32_t ColorAccentCool = 0x0B607D;
inline constexpr uint32_t ColorAccentFault = 0xFF3C45;
inline constexpr uint32_t ColorAccentOutline = 0x454545;
inline constexpr uint32_t ColorProgress = 0xF59A23;
inline constexpr uint32_t ColorChartLine = 0xF6A21A;
inline constexpr uint32_t ColorChartGrid = 0x3B3B3B;

inline lv_obj_t *screenRoot = nullptr;
inline lv_obj_t *headerBar = nullptr;
inline lv_obj_t *accentDot = nullptr;
inline lv_obj_t *mainCard = nullptr;
inline lv_obj_t *footerCard = nullptr;
inline lv_obj_t *mainEyebrowLabel = nullptr;
inline lv_obj_t *mainBodyLabel = nullptr;
inline lv_obj_t *mainSupportLabel = nullptr;
inline lv_obj_t *progressBar = nullptr;
inline lv_obj_t *ssidCaptionLabel = nullptr;
inline lv_obj_t *passwordCaptionLabel = nullptr;
inline lv_obj_t *secondaryButton = nullptr;
inline lv_obj_t *titleLabel = nullptr;
inline lv_obj_t *stateLabel = nullptr;
inline lv_obj_t *currentTempLabel = nullptr;
inline lv_obj_t *fanTempLabel = nullptr;
inline lv_obj_t *targetTempLabel = nullptr;
inline lv_obj_t *fanLabel = nullptr;
inline lv_obj_t *progressLabel = nullptr;
inline lv_obj_t *heaterLabel = nullptr;
inline lv_obj_t *bdcFanLabel = nullptr;
inline lv_obj_t *wifiLabel = nullptr;
inline lv_obj_t *profileLabel = nullptr;
inline lv_obj_t *errorLabel = nullptr;
inline lv_obj_t *revisionLabel = nullptr;
inline lv_obj_t *profileChart = nullptr;
inline lv_obj_t *profileGraphYAxisMaxLabel = nullptr;
inline lv_obj_t *profileGraphYAxisMinLabel = nullptr;
inline lv_obj_t *profileGraphXAxisStartLabel = nullptr;
inline lv_obj_t *profileGraphXAxisEndLabel = nullptr;
inline lv_obj_t *profileListContainer = nullptr;
inline lv_chart_series_t *profileSeries = nullptr;
inline lv_obj_t *roastStartButton = nullptr;
inline lv_obj_t *roastStopButton = nullptr;
inline lv_obj_t *coolingStopButton = nullptr;
inline lv_obj_t *profileOpenButton = nullptr;
inline lv_obj_t *profilePrevButton = nullptr;
inline lv_obj_t *profileNextButton = nullptr;
inline lv_obj_t *profileRefreshButton = nullptr;
inline lv_obj_t *profileBackButton = nullptr;
inline lv_obj_t *wifiApplyButton = nullptr;
inline lv_obj_t *networkProfileButton = nullptr;
inline lv_obj_t *networkBackButton = nullptr;
inline lv_obj_t *finalTargetDownButton = nullptr;
inline lv_obj_t *finalTargetUpButton = nullptr;
inline lv_obj_t *ssidTextArea = nullptr;
inline lv_obj_t *passwordTextArea = nullptr;
inline lv_obj_t *profileNameModal = nullptr;
inline lv_obj_t *profileNameTitleLabel = nullptr;
inline lv_obj_t *profileNameTextArea = nullptr;
inline lv_obj_t *profileNameSaveButton = nullptr;
inline lv_obj_t *profileNameCancelButton = nullptr;
inline lv_obj_t *keyboard = nullptr;
inline lv_obj_t *activeTextArea = nullptr;

inline int32_t profileWaveformValues[ProfileWaveformCapacity] = {0};
inline int32_t profileWaveformRenderValues[ProfileWaveformCapacity] = {0};
inline uint16_t profileWaveformPointCount = 0;
inline DisplayAction actionQueue[ActionQueueCapacity] = {DisplayAction::None};
inline uint8_t actionQueueHead = 0;
inline uint8_t actionQueueTail = 0;
inline std::vector<DisplayProfileSummary> profileBrowserEntries;
inline std::vector<lv_obj_t *> profileListRows;
inline int selectedProfileIndex = -1;
inline bool profileNameModalVisible = false;

inline void ensureUiBuilt();
inline void refreshScreenLayout();
inline void updateDerivedLabels();
inline void rebuildProfileList();
inline void refreshProfileListSelection();

inline void formatDurationLabel(uint32_t totalSeconds, char *buffer, size_t bufferSize)
{
  if (buffer == nullptr || bufferSize == 0)
  {
    return;
  }

  snprintf(buffer, bufferSize, "%02lu:%02lu",
           static_cast<unsigned long>(totalSeconds / 60U),
           static_cast<unsigned long>(totalSeconds % 60U));
}

inline bool isFinalTargetDirty()
{
  return storedProfileFinalTargetValue >= 0 &&
         finalTargetTempValue >= 0 &&
         finalTargetTempValue != storedProfileFinalTargetValue;
}

inline lv_color_t stateAccentColor(RoasterState state)
{
  switch (state)
  {
  case START_ROAST:
  case ROASTING:
    return lv_color_hex(ColorAccentHeat);
  case COOLING:
    return lv_color_hex(ColorAccentCool);
  case ERROR:
    return lv_color_hex(ColorAccentFault);
  case IDLE:
  case CALIBRATING:
  default:
    return lv_color_hex(ColorAccentReady);
  }
}

inline String statusBannerText()
{
  switch (activeScreen)
  {
  case DisplayScreen::Start:
    return currentRoasterState == IDLE ? String("Start - Idle ready") : String("Start");
  case DisplayScreen::Network:
    return wifiStatusText.indexOf("Connecting") >= 0 ? String("Network - Connecting") : String("Network - Ready");
  case DisplayScreen::Roasting:
    return currentRoasterState == START_ROAST ? String("Roasting - Fan ramp") : String("Roasting - Heater on");
  case DisplayScreen::Cooling:
    return String("Cooling - Airflow max");
  case DisplayScreen::Error:
    return String("Error - Start blocked");
  case DisplayScreen::ProfileList:
    return String("Profiles - Select one");
  case DisplayScreen::ProfileActive:
    return profileBrowserFocusText.length() > 0 ? String("Profile graph - ") + profileBrowserFocusText : String("Profile graph");
  default:
    return String("ROASTER");
  }
}

inline bool queueAction(DisplayAction action)
{
  if (action == DisplayAction::None)
  {
    return false;
  }

  uint8_t nextTail = (actionQueueTail + 1) % ActionQueueCapacity;
  if (nextTail == actionQueueHead)
  {
    return false;
  }

  actionQueue[actionQueueTail] = action;
  actionQueueTail = nextTail;
  return true;
}

inline bool popAction(DisplayAction &action)
{
  if (actionQueueHead == actionQueueTail)
  {
    return false;
  }

  action = actionQueue[actionQueueHead];
  actionQueueHead = (actionQueueHead + 1) % ActionQueueCapacity;
  return true;
}

inline const char *screenTitle(DisplayScreen screen)
{
  LV_UNUSED(screen);
  return "ROASTER";
}

inline const char *roasterStateLabel(RoasterState state)
{
  switch (state)
  {
  case IDLE:
    return "IDLE";
  case START_ROAST:
    return "STARTING";
  case ROASTING:
    return "ROASTING";
  case COOLING:
    return "COOLING";
  case ERROR:
    return "FAULT";
  case CALIBRATING:
    return "CALIBRATING";
  default:
    return "UNKNOWN";
  }
}

inline uint32_t millisCallback()
{
  return millis();
}

inline void flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixelMap)
{
  uint32_t width = lv_area_get_width(area);
  uint32_t height = lv_area_get_height(area);
  gfx->draw16bitRGBBitmap(area->x1, area->y1, reinterpret_cast<uint16_t *>(pixelMap), width, height);
  lv_display_flush_ready(display);
}

inline void touchRead(lv_indev_t *indev, lv_indev_data_t *data)
{
  LV_UNUSED(indev);

  touchController.read();
  if (touchController.isTouched && touchController.touches > 0)
  {
    int32_t touchX = touchController.points[0].x;
    int32_t touchY = touchController.points[0].y;

    if (BoardConfig::TouchInvertX)
    {
      touchX = (BoardConfig::DisplayWidth - 1) - touchX;
    }

    if (BoardConfig::TouchInvertY)
    {
      touchY = (BoardConfig::DisplayHeight - 1) - touchY;
    }

    data->point.x = constrain(touchX, 0, BoardConfig::DisplayWidth - 1);
    data->point.y = constrain(touchY, 0, BoardConfig::DisplayHeight - 1);
    data->state = LV_INDEV_STATE_PRESSED;
    return;
  }

  data->state = LV_INDEV_STATE_RELEASED;
}

inline lv_obj_t *makeValueLabel(lv_obj_t *parent, lv_align_t align, int xOffset, int yOffset)
{
  lv_obj_t *label = lv_label_create(parent);
  lv_obj_set_style_text_color(label, lv_color_hex(ColorTextMuted), 0);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
  lv_obj_align(label, align, xOffset, yOffset);
  lv_label_set_text(label, "");
  return label;
}

inline lv_obj_t *makeButton(lv_obj_t *parent,
                            const char *text,
                            lv_align_t align,
                            int xOffset,
                            int yOffset,
                            int width,
                            int height,
                            uint32_t backgroundColor)
{
  lv_obj_t *button = lv_button_create(parent);
  lv_obj_set_size(button, width, height);
  lv_obj_align(button, align, xOffset, yOffset);
  lv_obj_set_style_radius(button, 0, 0);
  lv_obj_set_style_bg_color(button, lv_color_hex(backgroundColor), 0);
  lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(button, 0, 0);
  lv_obj_set_style_shadow_width(button, 0, 0);
  lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *label = lv_label_create(button);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(label, lv_color_hex(ColorTextPrimary), 0);
  lv_obj_center(label);
  return button;
}

inline lv_obj_t *makePanel(lv_obj_t *parent,
                           int width,
                           int height,
                           uint32_t backgroundColor,
                           uint32_t borderColor = ColorAccentOutline,
                           int borderWidth = 1)
{
  lv_obj_t *panel = lv_obj_create(parent);
  lv_obj_remove_style_all(panel);
  lv_obj_set_size(panel, width, height);
  lv_obj_set_style_bg_color(panel, lv_color_hex(backgroundColor), 0);
  lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(panel, lv_color_hex(borderColor), 0);
  lv_obj_set_style_border_width(panel, borderWidth, 0);
  lv_obj_set_style_radius(panel, 0, 0);
  lv_obj_set_style_pad_all(panel, 0, 0);
  lv_obj_set_style_shadow_width(panel, 0, 0);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
  return panel;
}

inline void styleButton(lv_obj_t *button,
                        uint32_t backgroundColor,
                        uint32_t borderColor,
                        int borderWidth = 1,
                        uint32_t textColor = ColorTextPrimary)
{
  if (button == nullptr)
  {
    return;
  }

  lv_obj_set_style_bg_color(button, lv_color_hex(backgroundColor), 0);
  lv_obj_set_style_border_color(button, lv_color_hex(borderColor), 0);
  lv_obj_set_style_border_width(button, borderWidth, 0);

  lv_obj_t *label = lv_obj_get_child(button, 0);
  if (label != nullptr)
  {
    lv_obj_set_style_text_color(label, lv_color_hex(textColor), 0);
  }
}

inline void setButtonText(lv_obj_t *button, const char *text)
{
  if (button == nullptr)
  {
    return;
  }

  lv_obj_t *label = lv_obj_get_child(button, 0);
  if (label != nullptr)
  {
    lv_label_set_text(label, text);
  }
}

inline void setWidgetHidden(lv_obj_t *widget, bool hidden)
{
  if (widget == nullptr)
  {
    return;
  }

  if (hidden)
  {
    lv_obj_add_flag(widget, LV_OBJ_FLAG_HIDDEN);
  }
  else
  {
    lv_obj_remove_flag(widget, LV_OBJ_FLAG_HIDDEN);
  }
}

inline void syncWifiFormStateFromInputs()
{
  if (ssidTextArea != nullptr)
  {
    wifiFormState.ssid = String(lv_textarea_get_text(ssidTextArea));
  }

  if (passwordTextArea != nullptr)
  {
    wifiFormState.password = String(lv_textarea_get_text(passwordTextArea));
  }
}

inline void syncWifiInputFromState(lv_obj_t *textArea, const String &value)
{
  if (textArea == nullptr || textArea == activeTextArea)
  {
    return;
  }

  const char *currentText = lv_textarea_get_text(textArea);
  if (currentText == nullptr || strcmp(currentText, value.c_str()) != 0)
  {
    lv_textarea_set_text(textArea, value.c_str());
  }
}

inline void syncWifiInputsFromState()
{
  syncWifiInputFromState(ssidTextArea, wifiFormState.ssid);
  syncWifiInputFromState(passwordTextArea, wifiFormState.password);
}

inline void hideKeyboard()
{
  if (keyboard == nullptr)
  {
    activeTextArea = nullptr;
    return;
  }

  const bool wasActive = activeTextArea != nullptr || !lv_obj_has_flag(keyboard, LV_OBJ_FLAG_HIDDEN);

  lv_keyboard_set_textarea(keyboard, nullptr);
  lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
  activeTextArea = nullptr;

  if (wasActive)
  {
    refreshScreenLayout();
  }
}

inline void showKeyboardFor(lv_obj_t *textArea)
{
  if (keyboard == nullptr || textArea == nullptr)
  {
    return;
  }

  activeTextArea = textArea;
  lv_keyboard_set_textarea(keyboard, textArea);
  lv_obj_remove_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
  refreshScreenLayout();
}

inline void layoutNetworkWidgets()
{
  if (ssidCaptionLabel == nullptr || passwordCaptionLabel == nullptr || wifiLabel == nullptr || revisionLabel == nullptr ||
      ssidTextArea == nullptr || passwordTextArea == nullptr || wifiApplyButton == nullptr || networkBackButton == nullptr)
  {
    return;
  }

  if (activeScreen != DisplayScreen::Network)
  {
    return;
  }

  const bool keyboardVisible = activeTextArea != nullptr;
  const int fieldWidth = keyboardVisible ? BoardConfig::DisplayWidth - 24 : 302;

  lv_obj_align(ssidCaptionLabel, LV_ALIGN_TOP_LEFT, 12, 44);
  lv_obj_align(passwordCaptionLabel, LV_ALIGN_TOP_LEFT, 12, keyboardVisible ? 100 : 108);
  lv_obj_set_size(ssidTextArea, fieldWidth, 38);
  lv_obj_set_size(passwordTextArea, fieldWidth, 38);

  if (keyboardVisible)
  {
    lv_obj_align(ssidTextArea, LV_ALIGN_TOP_LEFT, 12, 62);
    lv_obj_align(passwordTextArea, LV_ALIGN_TOP_LEFT, 12, 118);
    lv_obj_align(wifiLabel, LV_ALIGN_BOTTOM_LEFT, 12, -118);
    lv_obj_align(revisionLabel, LV_ALIGN_BOTTOM_LEFT, 12, -96);
    return;
  }

  lv_obj_align(ssidTextArea, LV_ALIGN_TOP_LEFT, 12, 62);
  lv_obj_align(passwordTextArea, LV_ALIGN_TOP_LEFT, 12, 126);
  lv_obj_set_size(wifiApplyButton, 116, 72);
  lv_obj_align(wifiApplyButton, LV_ALIGN_TOP_RIGHT, -12, 62);
  lv_obj_set_size(networkBackButton, 116, 52);
  lv_obj_align(networkBackButton, LV_ALIGN_TOP_RIGHT, -12, 144);
  lv_obj_align(wifiLabel, LV_ALIGN_BOTTOM_LEFT, 12, -34);
  lv_obj_align(revisionLabel, LV_ALIGN_BOTTOM_LEFT, 12, -14);
}

inline void setFinalTargetValue(int value)
{
  finalTargetTempValue = constrain(value, 0, 500);
  updateDerivedLabels();
  if (activeScreen == DisplayScreen::Start)
  {
    refreshScreenLayout();
  }
}

inline void actionButtonEvent(lv_event_t *event)
{
  if (lv_event_get_code(event) != LV_EVENT_CLICKED)
  {
    return;
  }

  DisplayAction action = static_cast<DisplayAction>(reinterpret_cast<intptr_t>(lv_event_get_user_data(event)));
  if (action == DisplayAction::ApplyWifi)
  {
    syncWifiFormStateFromInputs();
    hideKeyboard();
  }

  queueAction(action);
}

inline void finalTargetAdjustEvent(lv_event_t *event)
{
  if (lv_event_get_code(event) != LV_EVENT_CLICKED)
  {
    return;
  }

  int delta = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(event)));
  int baseValue = finalTargetTempValue >= 0 ? finalTargetTempValue : targetTempValue;
  if (baseValue < 0)
  {
    baseValue = 400;
  }

  setFinalTargetValue(baseValue + delta);
}

inline void profileLabelEvent(lv_event_t *event)
{
  if (lv_event_get_code(event) != LV_EVENT_CLICKED)
  {
    return;
  }

  if (activeScreen == DisplayScreen::Start)
  {
    queueAction(DisplayAction::OpenActiveProfile);
  }
}

inline void profileListRowEvent(lv_event_t *event)
{
  if (lv_event_get_code(event) != LV_EVENT_CLICKED)
  {
    return;
  }

  selectedProfileIndex = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(event)));
  if (selectedProfileIndex >= 0 && selectedProfileIndex < static_cast<int>(profileBrowserEntries.size()))
  {
    profileBrowserFocusText = profileBrowserEntries[static_cast<size_t>(selectedProfileIndex)].name;
    profileBrowserFocusFinalTargetValue = profileBrowserEntries[static_cast<size_t>(selectedProfileIndex)].finalTargetF;
  }

  refreshProfileListSelection();
  lv_obj_scroll_to_view(lv_event_get_target_obj(event), LV_ANIM_OFF);
  updateDerivedLabels();
}

inline void profileSecondaryButtonEvent(lv_event_t *event)
{
  if (lv_event_get_code(event) != LV_EVENT_CLICKED)
  {
    return;
  }

  if (activeScreen == DisplayScreen::ProfileList)
  {
    queueAction(DisplayAction::ViewSelectedProfileGraph);
    return;
  }

  if (activeScreen != DisplayScreen::ProfileActive || profileNameModal == nullptr || profileNameTextArea == nullptr)
  {
    return;
  }

  String suggestedName = profileBrowserFocusText.length() > 0 ? profileBrowserFocusText + " Copy" : String("Profile Copy");
  lv_label_set_text(profileNameTitleLabel, "Copy profile as");
  lv_textarea_set_text(profileNameTextArea, suggestedName.c_str());
  profileNameModalVisible = true;
  showKeyboardFor(profileNameTextArea);
  refreshScreenLayout();
}

inline void profileBackButtonEvent(lv_event_t *event)
{
  if (lv_event_get_code(event) != LV_EVENT_CLICKED)
  {
    return;
  }

  if (profileNameModalVisible)
  {
    profileNameModalVisible = false;
    hideKeyboard();
    refreshScreenLayout();
    return;
  }

  queueAction(activeScreen == DisplayScreen::ProfileActive ? DisplayAction::ReturnToProfileList : DisplayAction::ReturnToStateScreen);
}

inline void profileNameSaveEvent(lv_event_t *event)
{
  if (lv_event_get_code(event) != LV_EVENT_CLICKED)
  {
    return;
  }

  hideKeyboard();
  profileNameModalVisible = false;
  queueAction(DisplayAction::DuplicateSelectedProfile);
  refreshScreenLayout();
}

inline void profileNameCancelEvent(lv_event_t *event)
{
  if (lv_event_get_code(event) != LV_EVENT_CLICKED)
  {
    return;
  }

  profileNameModalVisible = false;
  hideKeyboard();
  refreshScreenLayout();
}

inline void profileNameTextAreaEvent(lv_event_t *event)
{
  lv_event_code_t code = lv_event_get_code(event);
  lv_obj_t *target = lv_event_get_target_obj(event);

  if (code == LV_EVENT_FOCUSED || code == LV_EVENT_CLICKED)
  {
    showKeyboardFor(target);
    return;
  }

  if (code == LV_EVENT_DEFOCUSED && target == activeTextArea)
  {
    hideKeyboard();
    return;
  }

  if (code == LV_EVENT_READY)
  {
    hideKeyboard();
  }
}

inline void wifiTextAreaEvent(lv_event_t *event)
{
  lv_event_code_t code = lv_event_get_code(event);
  lv_obj_t *target = lv_event_get_target_obj(event);

  if (code == LV_EVENT_FOCUSED || code == LV_EVENT_CLICKED)
  {
    showKeyboardFor(target);
    return;
  }

  if (code == LV_EVENT_VALUE_CHANGED)
  {
    syncWifiFormStateFromInputs();
    return;
  }

  if (code == LV_EVENT_DEFOCUSED)
  {
    syncWifiFormStateFromInputs();
    if (target == activeTextArea)
    {
      hideKeyboard();
    }
    return;
  }

  if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL)
  {
    syncWifiFormStateFromInputs();
    hideKeyboard();
  }
}

inline void keyboardEvent(lv_event_t *event)
{
  lv_event_code_t code = lv_event_get_code(event);
  if (code != LV_EVENT_READY && code != LV_EVENT_CANCEL)
  {
    return;
  }

  lv_obj_t *completedTextArea = activeTextArea;
  syncWifiFormStateFromInputs();
  hideKeyboard();

  if (code == LV_EVENT_READY && activeScreen == DisplayScreen::Network && completedTextArea == passwordTextArea)
  {
    queueAction(DisplayAction::ApplyWifi);
  }
}

inline void refreshProfileChart()
{
  if (profileChart == nullptr || profileSeries == nullptr)
  {
    return;
  }

  if (profileWaveformPointCount == 0)
  {
    static const int32_t emptySeries[] = {0};
    profileGraphMaxTempValue = -1;
    profileGraphMaxTimeSeconds = 0;
    lv_chart_set_point_count(profileChart, 1);
    lv_chart_set_series_values(profileChart, profileSeries, emptySeries, 1);
    lv_chart_refresh(profileChart);
    return;
  }

  for (uint16_t index = 0; index < profileWaveformPointCount; ++index)
  {
    profileWaveformRenderValues[index] = profileWaveformValues[profileWaveformPointCount - 1 - index];
  }

  lv_chart_set_point_count(profileChart, profileWaveformPointCount);
  lv_chart_set_series_values(profileChart, profileSeries, profileWaveformRenderValues, profileWaveformPointCount);
  lv_chart_refresh(profileChart);
}

inline void clearProfileWaveform()
{
  profileWaveformPointCount = 0;
  refreshProfileChart();
}

inline void appendProfileWaveformPoint(int32_t value)
{
  if (profileWaveformPointCount >= ProfileWaveformCapacity)
  {
    return;
  }

  profileWaveformValues[profileWaveformPointCount++] = value;
}

inline void refreshProfileListSelection()
{
  for (size_t index = 0; index < profileListRows.size(); ++index)
  {
    lv_obj_t *row = profileListRows[index];
    bool selected = static_cast<int>(index) == selectedProfileIndex;
    bool active = index < profileBrowserEntries.size() ? profileBrowserEntries[index].active : false;

    lv_obj_set_style_bg_color(row, lv_color_hex(selected ? ColorAccentReady : ColorPanel), 0);
    lv_obj_set_style_border_color(row, lv_color_hex(selected ? ColorAccentReady : (active ? ColorAccentCool : ColorAccentOutline)), 0);
    lv_obj_set_style_border_width(row, selected || active ? 2 : 1, 0);

    lv_obj_t *nameLabel = lv_obj_get_child(row, 0);
    lv_obj_t *metaLabel = lv_obj_get_child(row, 1);
    if (nameLabel != nullptr)
    {
      lv_obj_set_style_text_color(nameLabel, lv_color_hex(selected ? ColorBackground : ColorTextPrimary), 0);
    }
    if (metaLabel != nullptr)
    {
      lv_obj_set_style_text_color(metaLabel, lv_color_hex(selected ? ColorBackground : ColorTextMuted), 0);
    }
  }
}

inline void rebuildProfileList()
{
  if (profileListContainer == nullptr)
  {
    return;
  }

  lv_obj_clean(profileListContainer);
  profileListRows.clear();

  for (size_t index = 0; index < profileBrowserEntries.size(); ++index)
  {
    const DisplayProfileSummary &entry = profileBrowserEntries[index];
    lv_obj_t *row = lv_button_create(profileListContainer);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, 50);
    lv_obj_set_style_radius(row, 0, 0);
    lv_obj_set_style_shadow_width(row, 0, 0);
    lv_obj_set_style_bg_color(row, lv_color_hex(ColorPanel), 0);
    lv_obj_set_style_border_color(row, lv_color_hex(ColorAccentOutline), 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_pad_all(row, 10, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(row, profileListRowEvent, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<intptr_t>(index)));

    lv_obj_t *nameLabel = lv_label_create(row);
    lv_obj_set_style_text_font(nameLabel, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(nameLabel, lv_color_hex(ColorTextPrimary), 0);
    lv_label_set_text(nameLabel, entry.name.c_str());
    lv_obj_align(nameLabel, LV_ALIGN_LEFT_MID, 0, -8);

    lv_obj_t *metaLabel = lv_label_create(row);
    lv_obj_set_style_text_font(metaLabel, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(metaLabel, lv_color_hex(ColorTextMuted), 0);
    if (entry.active)
    {
      lv_label_set_text_fmt(metaLabel, "Final %d%s  ACTIVE", entry.finalTargetF, TempUnitSuffix);
    }
    else
    {
      lv_label_set_text_fmt(metaLabel, "Final %d%s", entry.finalTargetF, TempUnitSuffix);
    }
    lv_obj_align(metaLabel, LV_ALIGN_LEFT_MID, 0, 10);

    profileListRows.push_back(row);
  }

  refreshProfileListSelection();
}

inline void refreshScreenLayout()
{
  if (profileChart == nullptr || headerBar == nullptr || mainCard == nullptr || footerCard == nullptr || profileListContainer == nullptr)
  {
    return;
  }

  const int gutter = 12;
  const int sideColumnWidth = 116;
  const int leftColumnWidth = BoardConfig::DisplayWidth - (gutter * 4) - sideColumnWidth;
  const int topY = 44;

  const bool showStartControls = activeScreen == DisplayScreen::Start;
  const bool showNetworkControls = activeScreen == DisplayScreen::Network;
  const bool showRoastControls = activeScreen == DisplayScreen::Roasting;
  const bool showCoolingControls = activeScreen == DisplayScreen::Cooling;
  const bool showErrorControls = activeScreen == DisplayScreen::Error;
  const bool showProfileListControls = activeScreen == DisplayScreen::ProfileList;
  const bool showProfileGraphControls = activeScreen == DisplayScreen::ProfileActive && !profileNameModalVisible;
  const bool showProfileControls = showProfileListControls || showProfileGraphControls;
  const bool showMainCard = showStartControls || showRoastControls || showCoolingControls || showErrorControls || showProfileControls;
  const bool showFooterCard = showStartControls || showRoastControls;
  const bool showNetworkFields = showNetworkControls;
  const bool showNetworkButtons = showNetworkFields && activeTextArea == nullptr;
  const bool showKeyboard = activeTextArea != nullptr && (showNetworkFields || profileNameModalVisible);

  lv_obj_align(headerBar, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_size(headerBar, BoardConfig::DisplayWidth, 30);
  lv_obj_align(accentDot, LV_ALIGN_TOP_LEFT, 12, 9);
  lv_obj_align(titleLabel, LV_ALIGN_TOP_LEFT, 28, 7);
  lv_obj_align(stateLabel, LV_ALIGN_TOP_RIGHT, -12, 7);

  setWidgetHidden(mainCard, !showMainCard);
  setWidgetHidden(footerCard, !showFooterCard);
  setWidgetHidden(mainEyebrowLabel, !(showStartControls || showRoastControls || showCoolingControls || showErrorControls));
  setWidgetHidden(mainBodyLabel, !(showCoolingControls || showErrorControls));
  setWidgetHidden(mainSupportLabel, !(showCoolingControls || showErrorControls));
  setWidgetHidden(currentTempLabel, !(showStartControls || showRoastControls || showCoolingControls));
  setWidgetHidden(targetTempLabel, !(showRoastControls || showProfileGraphControls));
  setWidgetHidden(progressBar, !showRoastControls);
  setWidgetHidden(progressLabel, !showRoastControls);
  setWidgetHidden(fanLabel, !showRoastControls);
  setWidgetHidden(heaterLabel, true);
  setWidgetHidden(fanTempLabel, true);
  setWidgetHidden(bdcFanLabel, true);
  setWidgetHidden(errorLabel, !showErrorControls);
  setWidgetHidden(profileChart, !showProfileGraphControls);
  setWidgetHidden(profileGraphYAxisMaxLabel, !showProfileGraphControls);
  setWidgetHidden(profileGraphYAxisMinLabel, !showProfileGraphControls);
  setWidgetHidden(profileGraphXAxisStartLabel, !showProfileGraphControls);
  setWidgetHidden(profileGraphXAxisEndLabel, !showProfileGraphControls);
  setWidgetHidden(profileListContainer, !showProfileListControls);
  setWidgetHidden(profileLabel, !(showStartControls || showProfileGraphControls));
  setWidgetHidden(wifiLabel, !showNetworkControls);
  setWidgetHidden(revisionLabel, !showNetworkControls);
  setWidgetHidden(ssidCaptionLabel, !showNetworkFields);
  setWidgetHidden(passwordCaptionLabel, !showNetworkFields);
  setWidgetHidden(profileNameModal, !profileNameModalVisible);

  lv_obj_t *startWidgets[] = {
      roastStartButton,
      profileOpenButton,
      networkProfileButton,
      finalTargetDownButton,
      finalTargetUpButton,
      profileRefreshButton,
  };

  for (lv_obj_t *widget : startWidgets)
  {
    setWidgetHidden(widget, !showStartControls);
  }

  lv_obj_t *networkFieldWidgets[] = {
      ssidTextArea,
      passwordTextArea,
  };

  for (lv_obj_t *widget : networkFieldWidgets)
  {
    setWidgetHidden(widget, !showNetworkFields);
  }

  lv_obj_t *networkButtonWidgets[] = {
      wifiApplyButton,
      networkBackButton,
  };

  for (lv_obj_t *widget : networkButtonWidgets)
  {
    setWidgetHidden(widget, !showNetworkButtons);
  }

  setWidgetHidden(roastStopButton, !(showRoastControls || showErrorControls));
  setWidgetHidden(coolingStopButton, !showCoolingControls);

  lv_obj_t *profileWidgets[] = {
      profilePrevButton,
      profileNextButton,
      profileBackButton,
  };

  for (lv_obj_t *widget : profileWidgets)
  {
    setWidgetHidden(widget, !showProfileControls);
  }

  setWidgetHidden(secondaryButton, true);
  setWidgetHidden(keyboard, !showKeyboard);
  layoutNetworkWidgets();

  if (!showNetworkFields && !profileNameModalVisible)
  {
    if (keyboard != nullptr)
    {
      lv_keyboard_set_textarea(keyboard, nullptr);
      lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    }
    activeTextArea = nullptr;
  }

  if (showStartControls)
  {
    lv_obj_set_size(mainCard, leftColumnWidth, 156);
    lv_obj_align(mainCard, LV_ALIGN_TOP_LEFT, gutter, topY);
    lv_obj_set_style_border_color(mainCard, lv_color_hex(ColorAccentOutline), 0);
    lv_obj_set_style_border_width(mainCard, 1, 0);
    lv_obj_set_style_bg_color(mainCard, lv_color_hex(ColorPanel), 0);

    lv_obj_set_size(footerCard, leftColumnWidth, 46);
    lv_obj_align(footerCard, LV_ALIGN_TOP_LEFT, gutter, topY + 166);
    lv_obj_set_style_bg_color(footerCard, lv_color_hex(ColorPanel), 0);
    lv_obj_set_style_border_color(footerCard, lv_color_hex(ColorAccentOutline), 0);
    lv_obj_set_style_border_width(footerCard, 1, 0);

    lv_obj_align_to(mainEyebrowLabel, mainCard, LV_ALIGN_TOP_MID, 0, 18);
    lv_obj_align_to(currentTempLabel, mainCard, LV_ALIGN_CENTER, 0, 6);

    lv_obj_set_size(roastStartButton, 116, 84);
    lv_obj_align(roastStartButton, LV_ALIGN_TOP_RIGHT, -12, topY);
    lv_obj_set_size(profileOpenButton, 116, 46);
    lv_obj_align(profileOpenButton, LV_ALIGN_TOP_RIGHT, -12, topY + 92);
    lv_obj_set_size(networkProfileButton, 116, 46);
    lv_obj_align(networkProfileButton, LV_ALIGN_TOP_RIGHT, -12, topY + 146);
    lv_obj_set_size(finalTargetDownButton, 52, 52);
    lv_obj_align_to(finalTargetDownButton, mainCard, LV_ALIGN_LEFT_MID, 22, 0);
    lv_obj_set_size(finalTargetUpButton, 52, 52);
    lv_obj_align_to(finalTargetUpButton, mainCard, LV_ALIGN_RIGHT_MID, -22, 0);

    lv_obj_set_size(profileRefreshButton, 82, 30);
    lv_obj_align_to(profileRefreshButton, footerCard, LV_ALIGN_RIGHT_MID, -8, 0);
    setWidgetHidden(profileRefreshButton, !isFinalTargetDirty());

    lv_obj_set_width(profileLabel, isFinalTargetDirty() ? leftColumnWidth - 114 : leftColumnWidth - 20);
    lv_obj_align_to(profileLabel, footerCard, LV_ALIGN_LEFT_MID, 10, 0);
  }

  if (showRoastControls)
  {
    lv_obj_set_size(mainCard, leftColumnWidth, 130);
    lv_obj_align(mainCard, LV_ALIGN_TOP_LEFT, gutter, topY);
    lv_obj_set_style_border_color(mainCard, lv_color_hex(ColorAccentOutline), 0);
    lv_obj_set_style_border_width(mainCard, 1, 0);
    lv_obj_set_style_bg_color(mainCard, lv_color_hex(ColorPanel), 0);

    lv_obj_set_size(footerCard, leftColumnWidth, 58);
    lv_obj_align(footerCard, LV_ALIGN_TOP_LEFT, gutter, topY + 144);
    lv_obj_set_style_bg_color(footerCard, lv_color_hex(ColorPanel), 0);
    lv_obj_set_style_border_color(footerCard, lv_color_hex(ColorAccentOutline), 0);
    lv_obj_set_style_border_width(footerCard, 1, 0);

    lv_obj_align_to(mainEyebrowLabel, mainCard, LV_ALIGN_TOP_LEFT, 12, 10);
    lv_obj_align_to(currentTempLabel, mainCard, LV_ALIGN_LEFT_MID, 12, 10);
    lv_obj_set_width(targetTempLabel, 126);
    lv_obj_align_to(targetTempLabel, mainCard, LV_ALIGN_RIGHT_MID, -18, 0);

    lv_obj_set_size(roastStopButton, 116, 124);
    lv_obj_align(roastStopButton, LV_ALIGN_TOP_RIGHT, -12, topY);

    lv_obj_align_to(progressLabel, footerCard, LV_ALIGN_TOP_LEFT, 10, 8);
    lv_obj_set_width(fanLabel, 172);
    lv_obj_set_style_text_align(fanLabel, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align_to(fanLabel, footerCard, LV_ALIGN_TOP_RIGHT, -10, 8);
    lv_obj_set_size(progressBar, 284, 10);
    lv_obj_align_to(progressBar, footerCard, LV_ALIGN_BOTTOM_MID, 0, -10);
  }

  if (showCoolingControls)
  {
    lv_obj_set_size(mainCard, 304, 144);
    lv_obj_align(mainCard, LV_ALIGN_TOP_LEFT, 12, 44);
    lv_obj_set_style_border_color(mainCard, lv_color_hex(ColorAccentCool), 0);
    lv_obj_set_style_border_width(mainCard, 1, 0);
    lv_obj_set_style_bg_color(mainCard, lv_color_hex(ColorPanel), 0);

    lv_obj_align_to(mainEyebrowLabel, mainCard, LV_ALIGN_TOP_LEFT, 14, 18);
    lv_obj_align_to(currentTempLabel, mainCard, LV_ALIGN_LEFT_MID, 14, 0);
    lv_obj_align_to(mainBodyLabel, mainCard, LV_ALIGN_BOTTOM_LEFT, 14, -32);
    lv_obj_align_to(mainSupportLabel, mainCard, LV_ALIGN_BOTTOM_LEFT, 14, -12);

    lv_obj_set_size(coolingStopButton, 116, 124);
    lv_obj_align(coolingStopButton, LV_ALIGN_TOP_RIGHT, -12, 44);
  }

  if (showErrorControls)
  {
    lv_obj_set_size(mainCard, 314, 136);
    lv_obj_align(mainCard, LV_ALIGN_TOP_LEFT, 12, 44);
    lv_obj_set_style_border_color(mainCard, lv_color_hex(ColorAccentFault), 0);
    lv_obj_set_style_border_width(mainCard, 1, 0);
    lv_obj_set_style_bg_color(mainCard, lv_color_hex(ColorPanel), 0);

    lv_obj_align_to(mainEyebrowLabel, mainCard, LV_ALIGN_TOP_LEFT, 14, 14);
    lv_obj_set_width(errorLabel, 280);
    lv_obj_align_to(errorLabel, mainCard, LV_ALIGN_TOP_LEFT, 14, 40);
    lv_obj_align_to(mainSupportLabel, mainCard, LV_ALIGN_BOTTOM_LEFT, 14, -12);

    lv_obj_set_size(roastStopButton, 116, 84);
    lv_obj_align(roastStopButton, LV_ALIGN_TOP_RIGHT, -12, 44);
  }

  if (showProfileListControls)
  {
    lv_obj_set_size(mainCard, leftColumnWidth, BoardConfig::DisplayHeight - topY - 14);
    lv_obj_align(mainCard, LV_ALIGN_TOP_LEFT, gutter, topY);
    lv_obj_set_style_border_color(mainCard, lv_color_hex(ColorAccentOutline), 0);
    lv_obj_set_style_border_width(mainCard, 1, 0);
    lv_obj_set_style_bg_color(mainCard, lv_color_hex(ColorPanel), 0);

    lv_obj_set_size(profileListContainer, leftColumnWidth - 18, BoardConfig::DisplayHeight - topY - 30);
    lv_obj_align_to(profileListContainer, mainCard, LV_ALIGN_TOP_LEFT, 8, 8);

    lv_obj_set_size(profilePrevButton, 116, 48);
    lv_obj_align(profilePrevButton, LV_ALIGN_TOP_RIGHT, -12, topY);
    lv_obj_set_size(profileNextButton, 116, 48);
    lv_obj_align(profileNextButton, LV_ALIGN_TOP_RIGHT, -12, topY + 58);
    lv_obj_set_size(profileBackButton, 116, 48);
    lv_obj_align(profileBackButton, LV_ALIGN_TOP_RIGHT, -12, topY + 116);
  }

  if (showProfileGraphControls)
  {
    lv_obj_set_size(mainCard, leftColumnWidth, BoardConfig::DisplayHeight - topY - 14);
    lv_obj_align(mainCard, LV_ALIGN_TOP_LEFT, gutter, topY);
    lv_obj_set_style_border_color(mainCard, lv_color_hex(ColorAccentOutline), 0);
    lv_obj_set_style_border_width(mainCard, 1, 0);
    lv_obj_set_style_bg_color(mainCard, lv_color_hex(ColorPanel), 0);

    lv_obj_set_width(profileLabel, leftColumnWidth - 124);
    lv_obj_align_to(profileLabel, mainCard, LV_ALIGN_TOP_LEFT, 10, 10);
    lv_obj_set_width(targetTempLabel, 100);
    lv_obj_align_to(targetTempLabel, mainCard, LV_ALIGN_TOP_RIGHT, -10, 10);
    lv_obj_set_size(profileChart, leftColumnWidth - 44, 156);
    lv_obj_align_to(profileChart, mainCard, LV_ALIGN_TOP_LEFT, 34, 34);

    lv_obj_align_to(profileGraphYAxisMaxLabel, mainCard, LV_ALIGN_TOP_LEFT, 8, 36);
    lv_obj_align_to(profileGraphYAxisMinLabel, mainCard, LV_ALIGN_BOTTOM_LEFT, 8, -26);
    lv_obj_align_to(profileGraphXAxisStartLabel, mainCard, LV_ALIGN_BOTTOM_LEFT, 34, -8);
    lv_obj_align_to(profileGraphXAxisEndLabel, mainCard, LV_ALIGN_BOTTOM_RIGHT, -12, -8);

    lv_obj_set_size(profilePrevButton, 116, 48);
    lv_obj_align(profilePrevButton, LV_ALIGN_TOP_RIGHT, -12, topY);
    lv_obj_set_size(profileNextButton, 116, 48);
    lv_obj_align(profileNextButton, LV_ALIGN_TOP_RIGHT, -12, topY + 58);
    lv_obj_set_size(profileBackButton, 116, 48);
    lv_obj_align(profileBackButton, LV_ALIGN_TOP_RIGHT, -12, topY + 116);
  }

  if (profileNameModalVisible && profileNameModal != nullptr)
  {
    lv_obj_set_size(profileNameModal, 296, 112);
    lv_obj_align(profileNameModal, LV_ALIGN_TOP_MID, 0, activeTextArea == profileNameTextArea ? 38 : 68);
    lv_obj_set_width(profileNameTitleLabel, 272);
    lv_obj_align(profileNameTitleLabel, LV_ALIGN_TOP_LEFT, 12, 10);
    lv_obj_set_size(profileNameTextArea, 272, 40);
    lv_obj_align(profileNameTextArea, LV_ALIGN_TOP_LEFT, 12, 34);
    lv_obj_set_size(profileNameCancelButton, 96, 30);
    lv_obj_align(profileNameCancelButton, LV_ALIGN_BOTTOM_LEFT, 12, -10);
    lv_obj_set_size(profileNameSaveButton, 96, 30);
    lv_obj_align(profileNameSaveButton, LV_ALIGN_BOTTOM_RIGHT, -12, -10);
  }
}

inline void updateDerivedLabels()
{
  ensureUiBuilt();

  const lv_color_t accentColor = stateAccentColor(currentRoasterState);
  const int displayTargetValue = finalTargetTempValue >= 0 ? finalTargetTempValue : targetTempValue;
  const int heaterPercent = heaterOutputValue >= 0 ? (heaterOutputValue * 100 + 127) / 255 : -1;
  char elapsedBuffer[24] = "--:-- elapsed";

  if (elapsedSecondsValue >= 0)
  {
    snprintf(elapsedBuffer, sizeof(elapsedBuffer), "%02d:%02d elapsed", elapsedSecondsValue / 60, elapsedSecondsValue % 60);
  }

  lv_label_set_text(titleLabel, screenTitle(activeScreen));
  lv_obj_set_style_text_color(titleLabel, lv_color_hex(ColorAccentReady), 0);
  String bannerText = statusBannerText();
  lv_label_set_text(stateLabel, bannerText.c_str());
  lv_obj_set_style_text_color(stateLabel,
                              activeScreen == DisplayScreen::Error ? lv_color_hex(ColorAccentFault) :
                              activeScreen == DisplayScreen::Cooling ? lv_color_hex(ColorTextMuted) :
                              activeScreen == DisplayScreen::Roasting ? accentColor :
                              lv_color_hex(ColorTextMuted),
                              0);

  lv_bar_set_value(progressBar, constrain(progressPercentValue, 0, 100), LV_ANIM_OFF);

  setButtonText(roastStartButton, "START");
  setButtonText(profileOpenButton, "Profiles");
  setButtonText(networkProfileButton, "Wi-Fi");
  setButtonText(wifiApplyButton, "Apply");
  setButtonText(networkBackButton, "Back");
  setButtonText(roastStopButton, activeScreen == DisplayScreen::Error ? "Force cooling" : "STOP");
  setButtonText(coolingStopButton, "Stop cooling");
  setButtonText(profilePrevButton, "Use");
  setButtonText(profileNextButton, activeScreen == DisplayScreen::ProfileList ? "Graph" : "Copy");
  setButtonText(profileRefreshButton, "Save");
  setButtonText(profileBackButton, activeScreen == DisplayScreen::ProfileActive ? "Back" : "Cancel");
  setButtonText(profileNameSaveButton, "Create");
  setButtonText(profileNameCancelButton, "Cancel");

  styleButton(roastStartButton, ColorAccentReady, ColorAccentReady);
  styleButton(profileOpenButton, ColorPanelMuted, ColorAccentOutline);
  styleButton(networkProfileButton, ColorPanelMuted, ColorAccentOutline);
  styleButton(wifiApplyButton, ColorAccentReady, ColorAccentReady);
  styleButton(networkBackButton, ColorPanelMuted, ColorAccentOutline);
  styleButton(roastStopButton,
              activeScreen == DisplayScreen::Error ? ColorAccentFault : ColorAccentFault,
              activeScreen == DisplayScreen::Error ? ColorAccentFault : ColorAccentFault);
  styleButton(coolingStopButton, ColorAccentCool, ColorAccentCool);
  styleButton(profilePrevButton, ColorAccentReady, ColorAccentReady);
  styleButton(profileNextButton, ColorPanelMuted, ColorAccentOutline);
  styleButton(profileRefreshButton, ColorAccentReady, ColorAccentReady);
  styleButton(profileBackButton, ColorPanelMuted, ColorAccentOutline);
  styleButton(profileNameSaveButton, ColorAccentReady, ColorAccentReady);
  styleButton(profileNameCancelButton, ColorPanelMuted, ColorAccentOutline);

  if (secondaryButton != nullptr)
  {
    lv_obj_t *secondaryLabel = lv_obj_get_child(secondaryButton, 0);
    if (secondaryLabel != nullptr)
    {
      lv_label_set_text(secondaryLabel, "Safe idle");
    }
  }

  lv_obj_set_style_text_color(currentTempLabel,
                              activeScreen == DisplayScreen::Cooling ? lv_color_hex(ColorTextPrimary) :
                              activeScreen == DisplayScreen::Roasting ? lv_color_hex(ColorTextPrimary) :
                              lv_color_hex(ColorTextPrimary),
                              0);
  lv_obj_set_style_text_color(mainEyebrowLabel,
                              activeScreen == DisplayScreen::Cooling ? lv_color_hex(ColorAccentCool) :
                              activeScreen == DisplayScreen::Error ? lv_color_hex(ColorAccentFault) :
                              lv_color_hex(0x6F6F6F),
                              0);
  lv_obj_set_style_text_color(mainBodyLabel, lv_color_hex(ColorTextPrimary), 0);
  lv_obj_set_style_text_color(mainSupportLabel, lv_color_hex(ColorTextMuted), 0);
  lv_obj_set_style_text_color(profileLabel, lv_color_hex(ColorTextPrimary), 0);
  lv_obj_set_style_text_color(wifiLabel, lv_color_hex(ColorTextMuted), 0);
  lv_obj_set_style_text_color(revisionLabel, lv_color_hex(ColorTextMuted), 0);

  if (activeScreen == DisplayScreen::Start)
  {
    lv_label_set_text(mainEyebrowLabel, "FINAL TARGET");
    lv_label_set_text_fmt(currentTempLabel, "%d%s", displayTargetValue >= 0 ? displayTargetValue : 0, TempUnitSuffix);
    String profileSummary = activeProfileText;
    if (isFinalTargetDirty())
    {
      profileSummary += " *";
    }
    lv_label_set_text(profileLabel, profileSummary.c_str());
  }
  else if (activeScreen == DisplayScreen::Network)
  {
    lv_label_set_text(ssidCaptionLabel, "SSID");
    lv_label_set_text(passwordCaptionLabel, "Password");
    lv_label_set_text_fmt(wifiLabel, "%s", wifiStatusText.c_str());
    lv_label_set_text_fmt(revisionLabel, "Firmware %s", revisionText.c_str());
  }
  else if (activeScreen == DisplayScreen::Roasting)
  {
    lv_label_set_text(mainEyebrowLabel, "BEAN TEMP");
    lv_label_set_text_fmt(currentTempLabel, "%d%s", currentTempValue, TempUnitSuffix);
    if (targetTempValue >= 0 && displayTargetValue >= 0)
    {
      lv_label_set_text_fmt(targetTempLabel, "Target %d%s\nStop %d%s", targetTempValue, TempUnitSuffix, displayTargetValue, TempUnitSuffix);
    }
    else
    {
      lv_label_set_text(targetTempLabel, "Target --\nStop --");
    }
    lv_label_set_text(progressLabel, elapsedBuffer);
    if (fanPercentValue >= 0 && heaterPercent >= 0)
    {
      lv_label_set_text_fmt(fanLabel, "Fan %d%% | Heat %d%%", fanPercentValue, heaterPercent);
    }
    else if (fanPercentValue >= 0)
    {
      lv_label_set_text_fmt(fanLabel, "Fan %d%%", fanPercentValue);
    }
    else
    {
      lv_label_set_text(fanLabel, "Fan --");
    }
  }
  else if (activeScreen == DisplayScreen::Cooling)
  {
    lv_label_set_text(mainEyebrowLabel, "COOLING ACTIVE");
    lv_label_set_text_fmt(currentTempLabel, "%d%s", currentTempValue, TempUnitSuffix);
    lv_label_set_text_fmt(mainBodyLabel, "Target below 120%s", TempUnitSuffix);
    lv_label_set_text(mainSupportLabel, "Heater off | blower at maximum");
  }
  else if (activeScreen == DisplayScreen::Error)
  {
    lv_label_set_text(mainEyebrowLabel, "FAULT LOCKOUT");
    lv_label_set_text(errorLabel, errorMessageText.length() > 0 ? errorMessageText.c_str() : "Controller fault detected.");
    lv_label_set_text(mainSupportLabel, "Roast start is disabled until the controller reports a safe state.");
  }
  else if (activeScreen == DisplayScreen::ProfileList)
  {
    refreshProfileListSelection();
  }
  else if (activeScreen == DisplayScreen::ProfileActive)
  {
    lv_label_set_text(profileLabel, profileBrowserFocusText.length() > 0 ? profileBrowserFocusText.c_str() : activeProfileText.c_str());
    int graphFinalTarget = profileBrowserFocusFinalTargetValue >= 0 ? profileBrowserFocusFinalTargetValue : displayTargetValue;
    if (graphFinalTarget >= 0)
    {
      lv_label_set_text_fmt(targetTempLabel, "Final %d%s", graphFinalTarget, TempUnitSuffix);
    }
    else
    {
      lv_label_set_text(targetTempLabel, "Final --");
    }

    char durationBuffer[16] = "--:--";
    if (profileGraphMaxTimeSeconds > 0)
    {
      formatDurationLabel(profileGraphMaxTimeSeconds, durationBuffer, sizeof(durationBuffer));
    }

    if (profileGraphMaxTempValue > 0)
    {
      lv_label_set_text_fmt(profileGraphYAxisMaxLabel, "%d%s", profileGraphMaxTempValue, TempUnitSuffix);
    }
    else
    {
      lv_label_set_text(profileGraphYAxisMaxLabel, "--");
    }
    lv_label_set_text_fmt(profileGraphYAxisMinLabel, "0%s", TempUnitSymbol);
    lv_label_set_text(profileGraphXAxisStartLabel, "0:00");
    lv_label_set_text(profileGraphXAxisEndLabel, durationBuffer);
  }

  const int finalTargetValue = finalTargetTempValue >= 0 ? finalTargetTempValue : displayTargetValue;
  if (finalTargetValue >= 0)
  {
    lv_obj_t *downLabel = finalTargetDownButton != nullptr ? lv_obj_get_child(finalTargetDownButton, 0) : nullptr;
    lv_obj_t *upLabel = finalTargetUpButton != nullptr ? lv_obj_get_child(finalTargetUpButton, 0) : nullptr;
    if (downLabel != nullptr)
    {
      lv_label_set_text(downLabel, "-");
      lv_obj_set_style_text_font(downLabel, &lv_font_montserrat_22, 0);
    }
    if (upLabel != nullptr)
    {
      lv_label_set_text(upLabel, "+");
      lv_obj_set_style_text_font(upLabel, &lv_font_montserrat_22, 0);
    }
  }

  syncWifiInputsFromState();
}

inline void buildScreen()
{
  screenRoot = lv_obj_create(nullptr);
  lv_obj_remove_style_all(screenRoot);
  lv_obj_set_style_bg_color(screenRoot, lv_color_hex(ColorBackground), 0);
  lv_obj_set_style_bg_opa(screenRoot, LV_OPA_COVER, 0);

  headerBar = makePanel(screenRoot, BoardConfig::DisplayWidth, 30, ColorHeader, ColorAccentOutline, 1);
  lv_obj_align(headerBar, LV_ALIGN_TOP_MID, 0, 0);

  accentDot = makePanel(screenRoot, 10, 10, ColorAccentReady, ColorAccentReady, 0);
  lv_obj_align(accentDot, LV_ALIGN_TOP_LEFT, 12, 9);

  titleLabel = lv_label_create(screenRoot);
  lv_obj_set_style_text_color(titleLabel, lv_color_hex(ColorAccentReady), 0);
  lv_obj_set_style_text_font(titleLabel, &lv_font_montserrat_14, 0);
  lv_obj_align(titleLabel, LV_ALIGN_TOP_LEFT, 28, 7);
  lv_label_set_text(titleLabel, "ROASTER");

  stateLabel = makeValueLabel(screenRoot, LV_ALIGN_TOP_RIGHT, -16, 10);
  lv_obj_set_style_text_font(stateLabel, &lv_font_montserrat_14, 0);

  mainCard = makePanel(screenRoot, 304, 156, ColorPanel, ColorAccentOutline, 1);
  footerCard = makePanel(screenRoot, 304, 54, ColorPanel, ColorAccentOutline, 1);

  mainEyebrowLabel = lv_label_create(screenRoot);
  lv_obj_set_style_text_color(mainEyebrowLabel, lv_color_hex(ColorTextMuted), 0);
  lv_obj_set_style_text_font(mainEyebrowLabel, &lv_font_montserrat_14, 0);

  mainBodyLabel = lv_label_create(screenRoot);
  lv_obj_set_style_text_color(mainBodyLabel, lv_color_hex(ColorTextPrimary), 0);
  lv_obj_set_style_text_font(mainBodyLabel, &lv_font_montserrat_22, 0);

  mainSupportLabel = lv_label_create(screenRoot);
  lv_obj_set_style_text_color(mainSupportLabel, lv_color_hex(ColorTextMuted), 0);
  lv_obj_set_style_text_font(mainSupportLabel, &lv_font_montserrat_14, 0);
  lv_obj_set_width(mainSupportLabel, 284);
  lv_label_set_long_mode(mainSupportLabel, LV_LABEL_LONG_WRAP);

  currentTempLabel = makeValueLabel(screenRoot, LV_ALIGN_CENTER, 0, -48);
  lv_obj_set_style_text_font(currentTempLabel, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_letter_space(currentTempLabel, 1, 0);

  targetTempLabel = makeValueLabel(screenRoot, LV_ALIGN_CENTER, 0, 2);
  lv_obj_set_style_text_font(targetTempLabel, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(targetTempLabel, lv_color_hex(ColorTextPrimary), 0);
  lv_obj_set_width(targetTempLabel, 112);
  lv_label_set_long_mode(targetTempLabel, LV_LABEL_LONG_WRAP);

  progressLabel = makeValueLabel(screenRoot, LV_ALIGN_CENTER, 0, 34);
  lv_obj_set_style_text_font(progressLabel, &lv_font_montserrat_14, 0);

  fanTempLabel = makeValueLabel(screenRoot, LV_ALIGN_BOTTOM_LEFT, 16, -118);
  heaterLabel = makeValueLabel(screenRoot, LV_ALIGN_BOTTOM_RIGHT, -16, -118);
  fanLabel = makeValueLabel(screenRoot, LV_ALIGN_BOTTOM_LEFT, 16, -92);
  lv_obj_set_style_text_font(fanLabel, &lv_font_montserrat_14, 0);
  bdcFanLabel = makeValueLabel(screenRoot, LV_ALIGN_BOTTOM_RIGHT, -16, -92);
  wifiLabel = makeValueLabel(screenRoot, LV_ALIGN_BOTTOM_LEFT, 16, -66);
  lv_obj_set_style_text_font(wifiLabel, &lv_font_montserrat_14, 0);
  profileLabel = makeValueLabel(screenRoot, LV_ALIGN_BOTTOM_RIGHT, -16, -66);
  lv_obj_set_style_text_font(profileLabel, &lv_font_montserrat_14, 0);
  lv_obj_add_flag(profileLabel, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(profileLabel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(profileLabel, profileLabelEvent, LV_EVENT_CLICKED, nullptr);

  revisionLabel = lv_label_create(screenRoot);
  lv_obj_set_style_text_color(revisionLabel, lv_color_hex(ColorTextMuted), 0);
  lv_obj_set_style_text_font(revisionLabel, &lv_font_montserrat_14, 0);

  progressBar = lv_bar_create(screenRoot);
  lv_obj_remove_style_all(progressBar);
  lv_obj_set_size(progressBar, 284, 10);
  lv_obj_set_style_bg_color(progressBar, lv_color_hex(ColorPanelMuted), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(progressBar, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(progressBar, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_color(progressBar, lv_color_hex(ColorProgress), LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(progressBar, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_radius(progressBar, 0, LV_PART_INDICATOR);
  lv_bar_set_range(progressBar, 0, 100);

  roastStartButton = makeButton(screenRoot, "START", LV_ALIGN_BOTTOM_LEFT, 16, -12, 216, 56, ColorAccentReady);
  lv_obj_add_event_cb(roastStartButton, actionButtonEvent, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<intptr_t>(DisplayAction::StartRoast)));

  profileOpenButton = makeButton(screenRoot, "Profiles", LV_ALIGN_BOTTOM_RIGHT, -16, -12, 216, 56, ColorPanelMuted);
  lv_obj_add_event_cb(profileOpenButton, actionButtonEvent, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<intptr_t>(DisplayAction::OpenActiveProfile)));

  finalTargetDownButton = makeButton(screenRoot, "-", LV_ALIGN_CENTER, -136, 4, 52, 52, ColorPanelMuted);
  lv_obj_add_event_cb(finalTargetDownButton, finalTargetAdjustEvent, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<intptr_t>(-1)));

  finalTargetUpButton = makeButton(screenRoot, "+", LV_ALIGN_CENTER, 136, 4, 52, 52, ColorPanelMuted);
  lv_obj_add_event_cb(finalTargetUpButton, finalTargetAdjustEvent, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<intptr_t>(1)));

  roastStopButton = makeButton(screenRoot, "STOP", LV_ALIGN_BOTTOM_MID, 0, -12, BoardConfig::DisplayWidth - 32, 56, ColorAccentHeat);
  lv_obj_add_event_cb(roastStopButton, actionButtonEvent, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<intptr_t>(DisplayAction::StopRoast)));

  coolingStopButton = makeButton(screenRoot, "Stop cooling", LV_ALIGN_BOTTOM_MID, 0, -12, BoardConfig::DisplayWidth - 32, 56, ColorAccentCool);
  lv_obj_add_event_cb(coolingStopButton, actionButtonEvent, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<intptr_t>(DisplayAction::StopCooling)));

  profilePrevButton = makeButton(screenRoot, "PREV", LV_ALIGN_BOTTOM_LEFT, 16, -12, 100, 48, ColorPanel);
  lv_obj_add_event_cb(profilePrevButton, actionButtonEvent, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<intptr_t>(DisplayAction::ActivateSelectedProfile)));

  profileNextButton = makeButton(screenRoot, "NEXT", LV_ALIGN_BOTTOM_LEFT, 124, -12, 100, 48, ColorPanel);
  lv_obj_add_event_cb(profileNextButton, profileSecondaryButtonEvent, LV_EVENT_CLICKED, nullptr);

  profileRefreshButton = makeButton(screenRoot, "SAVE", LV_ALIGN_BOTTOM_RIGHT, -124, -12, 100, 48, ColorAccentReady);
  lv_obj_add_event_cb(profileRefreshButton, actionButtonEvent, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<intptr_t>(DisplayAction::SaveFinalTargetToProfile)));

  profileBackButton = makeButton(screenRoot, "BACK", LV_ALIGN_BOTTOM_RIGHT, -16, -12, 100, 48, ColorPanel);
  lv_obj_add_event_cb(profileBackButton, profileBackButtonEvent, LV_EVENT_CLICKED, nullptr);

  secondaryButton = makePanel(screenRoot, 116, 52, ColorPanelMuted, ColorAccentOutline, 1);
  lv_obj_t *secondaryLabel = lv_label_create(secondaryButton);
  lv_obj_set_style_text_color(secondaryLabel, lv_color_hex(ColorTextPrimary), 0);
  lv_obj_set_style_text_font(secondaryLabel, &lv_font_montserrat_14, 0);
  lv_label_set_text(secondaryLabel, "Safe idle");
  lv_obj_center(secondaryLabel);

  ssidCaptionLabel = lv_label_create(screenRoot);
  lv_obj_set_style_text_color(ssidCaptionLabel, lv_color_hex(ColorTextMuted), 0);
  lv_obj_set_style_text_font(ssidCaptionLabel, &lv_font_montserrat_14, 0);

  passwordCaptionLabel = lv_label_create(screenRoot);
  lv_obj_set_style_text_color(passwordCaptionLabel, lv_color_hex(ColorTextMuted), 0);
  lv_obj_set_style_text_font(passwordCaptionLabel, &lv_font_montserrat_14, 0);

  ssidTextArea = lv_textarea_create(screenRoot);
  lv_obj_set_size(ssidTextArea, BoardConfig::DisplayWidth - 32, 34);
  lv_obj_align(ssidTextArea, LV_ALIGN_BOTTOM_LEFT, 16, -112);
  lv_textarea_set_one_line(ssidTextArea, true);
  lv_textarea_set_max_length(ssidTextArea, 63);
  lv_textarea_set_placeholder_text(ssidTextArea, "WiFi SSID");
  lv_obj_set_style_radius(ssidTextArea, 0, 0);
  lv_obj_set_style_bg_color(ssidTextArea, lv_color_hex(ColorPanel), 0);
  lv_obj_set_style_border_color(ssidTextArea, lv_color_hex(ColorAccentOutline), 0);
  lv_obj_set_style_text_color(ssidTextArea, lv_color_hex(ColorTextPrimary), 0);
  lv_obj_add_event_cb(ssidTextArea, wifiTextAreaEvent, LV_EVENT_ALL, nullptr);

  passwordTextArea = lv_textarea_create(screenRoot);
  lv_obj_set_size(passwordTextArea, BoardConfig::DisplayWidth - 32, 34);
  lv_obj_align(passwordTextArea, LV_ALIGN_BOTTOM_LEFT, 16, -70);
  lv_textarea_set_one_line(passwordTextArea, true);
  lv_textarea_set_max_length(passwordTextArea, 63);
  lv_textarea_set_password_mode(passwordTextArea, true);
  lv_textarea_set_placeholder_text(passwordTextArea, "WiFi Password");
  lv_obj_set_style_radius(passwordTextArea, 0, 0);
  lv_obj_set_style_bg_color(passwordTextArea, lv_color_hex(ColorPanel), 0);
  lv_obj_set_style_border_color(passwordTextArea, lv_color_hex(ColorAccentOutline), 0);
  lv_obj_set_style_text_color(passwordTextArea, lv_color_hex(ColorTextPrimary), 0);
  lv_obj_add_event_cb(passwordTextArea, wifiTextAreaEvent, LV_EVENT_ALL, nullptr);

  wifiApplyButton = makeButton(screenRoot, "Apply", LV_ALIGN_BOTTOM_MID, 0, -28, 148, 34, ColorAccentReady);
  lv_obj_add_event_cb(wifiApplyButton, actionButtonEvent, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<intptr_t>(DisplayAction::ApplyWifi)));

  networkProfileButton = makeButton(screenRoot, "Wi-Fi", LV_ALIGN_BOTTOM_LEFT, 16, -28, 132, 34, ColorPanel);
  lv_obj_add_event_cb(networkProfileButton, actionButtonEvent, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<intptr_t>(DisplayAction::OpenNetwork)));

  networkBackButton = makeButton(screenRoot, "Done", LV_ALIGN_BOTTOM_RIGHT, -16, -28, 104, 34, ColorPanel);
  lv_obj_add_event_cb(networkBackButton, actionButtonEvent, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<intptr_t>(DisplayAction::ReturnToStateScreen)));

  keyboard = lv_keyboard_create(screenRoot);
  lv_obj_set_size(keyboard, BoardConfig::DisplayWidth, 108);
  lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(keyboard, keyboardEvent, LV_EVENT_READY, nullptr);
  lv_obj_add_event_cb(keyboard, keyboardEvent, LV_EVENT_CANCEL, nullptr);

  profileChart = lv_chart_create(screenRoot);
  lv_obj_set_size(profileChart, BoardConfig::DisplayWidth - 32, 154);
  lv_obj_align(profileChart, LV_ALIGN_TOP_MID, 0, 60);
  lv_obj_set_style_bg_color(profileChart, lv_color_hex(ColorPanelMuted), 0);
  lv_obj_set_style_border_color(profileChart, lv_color_hex(ColorAccentOutline), 0);
  lv_obj_set_style_border_width(profileChart, 1, 0);
  lv_obj_set_style_line_color(profileChart, lv_color_hex(ColorChartGrid), LV_PART_MAIN);
  lv_obj_set_style_line_color(profileChart, lv_color_hex(ColorChartLine), LV_PART_ITEMS);
  lv_obj_set_style_line_width(profileChart, 2, LV_PART_ITEMS);
  lv_obj_remove_flag(profileChart, LV_OBJ_FLAG_SCROLLABLE);
  lv_chart_set_type(profileChart, LV_CHART_TYPE_LINE);
  lv_chart_set_axis_range(profileChart, LV_CHART_AXIS_PRIMARY_Y, 0, 170);
  lv_chart_set_div_line_count(profileChart, 5, 8);
  lv_chart_set_point_count(profileChart, 1);
  profileSeries = lv_chart_add_series(profileChart, lv_color_hex(ColorChartLine), LV_CHART_AXIS_PRIMARY_Y);

  profileGraphYAxisMaxLabel = lv_label_create(screenRoot);
  lv_obj_set_style_text_color(profileGraphYAxisMaxLabel, lv_color_hex(ColorTextMuted), 0);
  lv_obj_set_style_text_font(profileGraphYAxisMaxLabel, &lv_font_montserrat_14, 0);

  profileGraphYAxisMinLabel = lv_label_create(screenRoot);
  lv_obj_set_style_text_color(profileGraphYAxisMinLabel, lv_color_hex(ColorTextMuted), 0);
  lv_obj_set_style_text_font(profileGraphYAxisMinLabel, &lv_font_montserrat_14, 0);

  profileGraphXAxisStartLabel = lv_label_create(screenRoot);
  lv_obj_set_style_text_color(profileGraphXAxisStartLabel, lv_color_hex(ColorTextMuted), 0);
  lv_obj_set_style_text_font(profileGraphXAxisStartLabel, &lv_font_montserrat_14, 0);

  profileGraphXAxisEndLabel = lv_label_create(screenRoot);
  lv_obj_set_style_text_color(profileGraphXAxisEndLabel, lv_color_hex(ColorTextMuted), 0);
  lv_obj_set_style_text_font(profileGraphXAxisEndLabel, &lv_font_montserrat_14, 0);

  profileListContainer = lv_obj_create(screenRoot);
  lv_obj_set_size(profileListContainer, BoardConfig::DisplayWidth - 32, 154);
  lv_obj_align(profileListContainer, LV_ALIGN_TOP_LEFT, 16, 60);
  lv_obj_set_style_radius(profileListContainer, 0, 0);
  lv_obj_set_style_bg_color(profileListContainer, lv_color_hex(ColorPanel), 0);
  lv_obj_set_style_bg_opa(profileListContainer, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(profileListContainer, 0, 0);
  lv_obj_set_style_pad_all(profileListContainer, 0, 0);
  lv_obj_set_style_pad_row(profileListContainer, 6, 0);
  lv_obj_set_scrollbar_mode(profileListContainer, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_scroll_dir(profileListContainer, LV_DIR_VER);
  lv_obj_set_layout(profileListContainer, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(profileListContainer, LV_FLEX_FLOW_COLUMN);

  profileNameModal = makePanel(screenRoot, 296, 112, ColorPanel, ColorAccentOutline, 1);
  lv_obj_set_style_bg_opa(profileNameModal, LV_OPA_COVER, 0);
  lv_obj_add_flag(profileNameModal, LV_OBJ_FLAG_HIDDEN);

  profileNameTitleLabel = lv_label_create(profileNameModal);
  lv_obj_set_style_text_color(profileNameTitleLabel, lv_color_hex(ColorTextPrimary), 0);
  lv_obj_set_style_text_font(profileNameTitleLabel, &lv_font_montserrat_14, 0);
  lv_label_set_text(profileNameTitleLabel, "Copy profile as");

  profileNameTextArea = lv_textarea_create(profileNameModal);
  lv_textarea_set_one_line(profileNameTextArea, true);
  lv_textarea_set_max_length(profileNameTextArea, 31);
  lv_obj_set_style_radius(profileNameTextArea, 0, 0);
  lv_obj_set_style_bg_color(profileNameTextArea, lv_color_hex(ColorPanelMuted), 0);
  lv_obj_set_style_border_color(profileNameTextArea, lv_color_hex(ColorAccentOutline), 0);
  lv_obj_set_style_text_color(profileNameTextArea, lv_color_hex(ColorTextPrimary), 0);
  lv_obj_add_event_cb(profileNameTextArea, profileNameTextAreaEvent, LV_EVENT_ALL, nullptr);

  profileNameCancelButton = makeButton(profileNameModal, "Cancel", LV_ALIGN_BOTTOM_LEFT, 12, -10, 96, 30, ColorPanelMuted);
  lv_obj_add_event_cb(profileNameCancelButton, profileNameCancelEvent, LV_EVENT_CLICKED, nullptr);

  profileNameSaveButton = makeButton(profileNameModal, "Create", LV_ALIGN_BOTTOM_RIGHT, -12, -10, 96, 30, ColorAccentReady);
  lv_obj_add_event_cb(profileNameSaveButton, profileNameSaveEvent, LV_EVENT_CLICKED, nullptr);

  errorLabel = lv_label_create(screenRoot);
  lv_obj_set_width(errorLabel, 280);
  lv_obj_set_style_text_color(errorLabel, lv_color_hex(ColorAccentFault), 0);
  lv_obj_set_style_text_font(errorLabel, &lv_font_montserrat_22, 0);
  lv_label_set_long_mode(errorLabel, LV_LABEL_LONG_WRAP);
  lv_obj_align(errorLabel, LV_ALIGN_CENTER, 0, 42);
  lv_label_set_text(errorLabel, "");

  lv_screen_load(screenRoot);
  updateDerivedLabels();
  refreshProfileChart();
  refreshScreenLayout();
}

inline void ensureUiBuilt()
{
  if (screenRoot == nullptr)
  {
    buildScreen();
  }
}

inline bool begin()
{
  if (!gfx->begin())
  {
    return false;
  }

  pinMode(GFX_BL, OUTPUT);
  digitalWrite(GFX_BL, HIGH);
  gfx->fillScreen(RGB565_BLACK);

  touchController.begin();
  lv_init();
  lv_tick_set_cb(millisCallback);

  uint32_t screenWidth = gfx->width();
  uint32_t screenHeight = gfx->height();
  drawBufferPixelCount = screenWidth * 40;

  drawBuffer = static_cast<lv_color_t *>(heap_caps_malloc(drawBufferPixelCount * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (drawBuffer == nullptr)
  {
    drawBuffer = static_cast<lv_color_t *>(heap_caps_malloc(drawBufferPixelCount * sizeof(lv_color_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  }

  if (drawBuffer == nullptr)
  {
    drawBuffer = static_cast<lv_color_t *>(heap_caps_malloc(drawBufferPixelCount * sizeof(lv_color_t), MALLOC_CAP_8BIT));
  }

  if (drawBuffer == nullptr)
  {
    return false;
  }

  displayInstance = lv_display_create(screenWidth, screenHeight);
  lv_display_set_flush_cb(displayInstance, flush);
  lv_display_set_buffers(displayInstance, drawBuffer, nullptr, drawBufferPixelCount * sizeof(lv_color_t), LV_DISPLAY_RENDER_MODE_PARTIAL);

  inputDevice = lv_indev_create();
  lv_indev_set_type(inputDevice, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(inputDevice, touchRead);

  ensureUiBuilt();
  return true;
}

inline void tick()
{
  lv_timer_handler();
}

inline void showScreen(DisplayScreen screen)
{
  activeScreen = screen;
  if (screen != DisplayScreen::ProfileActive)
  {
    profileNameModalVisible = false;
  }
  ensureUiBuilt();
  lv_label_set_text(titleLabel, screenTitle(screen));
  refreshScreenLayout();
  updateDerivedLabels();
  if (screen != DisplayScreen::Error)
  {
    lv_label_set_text(errorLabel, "");
  }
}

inline int readFinalTargetTemp()
{
  return finalTargetTempValue;
}

inline DisplayWifiFormState readWifiFormState()
{
  syncWifiFormStateFromInputs();
  return wifiFormState;
}

inline void setCurrentTempValue(int value)
{
  ensureUiBuilt();
  currentTempValue = value;
  updateDerivedLabels();
}

inline void setTargetTempValue(int value)
{
  ensureUiBuilt();
  targetTempValue = value;
  updateDerivedLabels();
}

inline void setFinalTargetTempValue(int value)
{
  ensureUiBuilt();
  finalTargetTempValue = value;
  updateDerivedLabels();
  if (activeScreen == DisplayScreen::Start)
  {
    refreshScreenLayout();
  }
}

inline void setStoredProfileFinalTargetValue(int value)
{
  ensureUiBuilt();
  storedProfileFinalTargetValue = value;
  if (finalTargetTempValue < 0)
  {
    finalTargetTempValue = value;
  }
  updateDerivedLabels();
  if (activeScreen == DisplayScreen::Start)
  {
    refreshScreenLayout();
  }
}

inline void setFanPercentValue(int value)
{
  ensureUiBuilt();
  fanPercentValue = value;
  updateDerivedLabels();
}

inline void setProgressValue(int value)
{
  ensureUiBuilt();
  progressPercentValue = value;
  updateDerivedLabels();
}

inline void setWifiStatusText(const String &value)
{
  ensureUiBuilt();
  wifiStatusText = value;
  updateDerivedLabels();
}

inline void setWifiFormState(const DisplayWifiFormState &value)
{
  ensureUiBuilt();
  wifiFormState = value;
  syncWifiInputsFromState();
}

inline void setRevisionText(const String &value)
{
  ensureUiBuilt();
  revisionText = value;
  updateDerivedLabels();
}

inline void setActiveProfileText(const String &value)
{
  ensureUiBuilt();
  activeProfileText = value;
  updateDerivedLabels();
}

inline void setProfileBrowserFocus(const String &value, int finalTargetF)
{
  ensureUiBuilt();
  profileBrowserFocusText = value;
  profileBrowserFocusFinalTargetValue = finalTargetF;
  updateDerivedLabels();
}

inline void setProfileGraphBounds(int maxTempF, uint32_t maxTimeSeconds)
{
  ensureUiBuilt();
  profileGraphMaxTempValue = maxTempF;
  profileGraphMaxTimeSeconds = maxTimeSeconds;
  updateDerivedLabels();
}

inline void setProfileBrowserEntries(const std::vector<DisplayProfileSummary> &entries, int selectedIndex)
{
  ensureUiBuilt();
  profileBrowserEntries = entries;
  if (entries.empty())
  {
    selectedProfileIndex = -1;
    profileBrowserFocusText = String();
    profileBrowserFocusFinalTargetValue = -1;
  }
  else
  {
    if (selectedIndex < 0 || selectedIndex >= static_cast<int>(entries.size()))
    {
      selectedProfileIndex = 0;
    }
    else
    {
      selectedProfileIndex = selectedIndex;
    }
    profileBrowserFocusText = entries[static_cast<size_t>(selectedProfileIndex)].name;
    profileBrowserFocusFinalTargetValue = entries[static_cast<size_t>(selectedProfileIndex)].finalTargetF;
  }
  rebuildProfileList();
  updateDerivedLabels();
}

inline int readSelectedProfileIndex()
{
  return selectedProfileIndex;
}

inline String readProfileNameInput()
{
  ensureUiBuilt();
  if (profileNameTextArea == nullptr)
  {
    return String();
  }
  return String(lv_textarea_get_text(profileNameTextArea));
}

inline void setErrorMessageText(const String &value)
{
  ensureUiBuilt();
  errorMessageText = value;
  updateDerivedLabels();
}

inline void clearProfileWaveformData()
{
  clearProfileWaveform();
}

inline void appendProfileWaveformDataPoint(int32_t value)
{
  appendProfileWaveformPoint(value);
}

inline void refreshProfilePlot()
{
  refreshProfileChart();
}

inline void updateTelemetry(const DisplayTelemetry &telemetry)
{
  ensureUiBuilt();
  currentRoasterState = telemetry.roasterState;

  if (telemetry.currentTempF >= 0)
  {
    currentTempValue = telemetry.currentTempF;
  }

  if (telemetry.targetTempF >= 0)
  {
    targetTempValue = telemetry.targetTempF;
  }

  if (telemetry.finalTargetTempF >= 0)
  {
    finalTargetTempValue = telemetry.finalTargetTempF;
  }

  if (telemetry.fanPercent >= 0)
  {
    fanPercentValue = telemetry.fanPercent;
  }

  progressPercentValue = telemetry.progressSeconds;
  elapsedSecondsValue = telemetry.elapsedSeconds;

  fanTempValue = telemetry.fanTempF;
  heaterOutputValue = telemetry.heaterOutput;
  bdcFanMicrosValue = telemetry.bdcFanMicros;
  updateDerivedLabels();
}

}

#endif

#endif
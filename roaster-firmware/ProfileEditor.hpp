#ifndef PROFILE_EDITOR_HPP
#define PROFILE_EDITOR_HPP

#include <Arduino.h>
#include <ArduinoJson.h>
#include <EasyNextionLibrary.h>
#include "Types.hpp"
#include "Profiles.hpp"
#include "DebugLog.hpp"
#include <vector>

// Profile editor backend logic
// Handles saving, loading, activating, deleting profiles

extern Profiles profile;  // Profile configuration from main firmware
extern Preferences preferences;  // NVS preferences from main firmware
extern uint8_t profileBuffer[200];
extern EasyNex myNex;  // Nextion display from main firmware
extern int finalTempOverride;  // Final temperature override from Nextion

// Forward declarations for helper functions
String sanitizeProfileName(const String& name);
std::vector<String> splitNames(const String& csv);
String joinNames(const std::vector<String>& names);
void addProfileName(const String& name);
void removeProfileName(const String& name);
String saveProfile(const JsonDocument& requestDoc);  // Forward declaration for ensureDefaultProfile
void plotProfileOnWaveform();  // Forward declaration for activateProfile

// Helper: sanitize profile name for use as preferences key (alphanumeric + underscore)
String sanitizeProfileName(const String& name) {
  String sanitized;
  for (unsigned int i = 0; i < name.length(); i++) {
    char c = name[i];
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-') {
      sanitized += c;
    } else {
      sanitized += '_';
    }
  }
  return sanitized.length() > 0 ? sanitized : String("profile");
}

// Helper: split comma-separated names
std::vector<String> splitNames(const String& csv) {
  std::vector<String> names;
  int start = 0;
  while (true) {
    int idx = csv.indexOf(',', start);
    if (idx == -1) {
      String token = csv.substring(start);
      token.trim();
      if (token.length() > 0) names.push_back(token);
      break;
    }
    String token = csv.substring(start, idx);
    token.trim();
    if (token.length() > 0) names.push_back(token);
    start = idx + 1;
  }
  return names;
}

// Helper: join names to CSV
String joinNames(const std::vector<String>& names) {
  String out;
  for (size_t i = 0; i < names.size(); i++) {
    out += names[i];
    if (i + 1 < names.size()) out += ",";
  }
  return out;
}

// Helper: ensure a name exists in list
void addProfileName(const String& name) {
  String csv = preferences.getString("profile_names", "");
  auto names = splitNames(csv);
  bool exists = false;
  for (auto& n : names) if (n == name) { exists = true; break; }
  if (!exists) {
    names.push_back(name);
    preferences.putString("profile_names", joinNames(names));
  }
}

// Helper: remove name from list
void removeProfileName(const String& name) {
  String csv = preferences.getString("profile_names", "");
  auto names = splitNames(csv);
  std::vector<String> filtered;
  for (auto& n : names) if (n != name) filtered.push_back(n);
  preferences.putString("profile_names", joinNames(filtered));
}

/**
 * Get current active profile data
 * Returns JSON with setpoints and active profile name
 */
String getActiveProfileData() {
  JsonDocument doc;
  JsonArray arr = doc["setpoints"].to<JsonArray>();
  
  for (int i = 0; i < profile.getSetpointCount(); i++) {
    auto sp = profile.getSetpoint(i);
    JsonObject spObj = arr.add<JsonObject>();
    spObj["time"] = sp.time / 1000;  // Convert milliseconds to seconds
    spObj["temp"] = sp.temp;
    spObj["fanSpeed"] = sp.fanSpeed;
  }
  
  doc["activeName"] = preferences.getString("profile_active", "");
  
  String output;
  serializeJson(doc, output);
  return output;
}

/**
 * Ensure a default profile exists
 * Creates "Default" profile if no profiles exist
 */
void ensureDefaultProfile() {
  String csv = preferences.getString("profile_names", "");
  auto names = splitNames(csv);
  
  LOG_DEBUGF("ensureDefaultProfile: Found %d existing profiles", names.size());
  
  if (names.empty()) {
    // No profiles exist - create a default
    LOG_INFO("No profiles found - creating Default profile");
    
    JsonDocument doc;
    doc["name"] = "Default";
    doc["activate"] = true;
    
    JsonArray setpoints = doc["setpoints"].to<JsonArray>();
    JsonObject sp1 = setpoints.add<JsonObject>();
    sp1["time"] = 0;
    sp1["temp"] = 200;
    sp1["fanSpeed"] = 30;
    
    JsonObject sp2 = setpoints.add<JsonObject>();
    sp2["time"] = 150;
    sp2["temp"] = 300;
    sp2["fanSpeed"] = 50;
    
    JsonObject sp3 = setpoints.add<JsonObject>();
    sp3["time"] = 300;
    sp3["temp"] = 380;
    sp3["fanSpeed"] = 70;
    
    JsonObject sp4 = setpoints.add<JsonObject>();
    sp4["time"] = 480;
    sp4["temp"] = 440;
    sp4["fanSpeed"] = 80;
    
    String result = saveProfile(doc);
    LOG_DEBUGF("ensureDefaultProfile: saveProfile result: %s", result.c_str());
  } else {
    LOG_DEBUGF("ensureDefaultProfile: Profiles already exist, skipping creation");
  }
}

/**
 * Reload the active profile into the global profile object
 * Call after activateProfile() or on startup
 * Returns true if successful, false if no active profile
 */
bool reloadActiveProfile() {
  String activeName = preferences.getString("profile_active", "");
  
  LOG_DEBUGF("reloadActiveProfile: Active profile name is '%s'", activeName.c_str());
  
  if (activeName.length() == 0) {
    LOG_WARN("No active profile set");
    return false;
  }
  
  // Load profile data
  String sanitized = sanitizeProfileName(activeName);
  String key = String("p_") + sanitized;
  uint8_t buf[200];
  size_t readLen = preferences.getBytes(key.c_str(), buf, sizeof(buf));
  
  LOG_DEBUGF("reloadActiveProfile: Read %d bytes from key '%s'", readLen, key.c_str());
  
  if (readLen == 0) {
    LOG_WARNF("Active profile not found: %s", activeName.c_str());
    return false;
  }
  
  // Load into global profile object
  profile.unflattenProfile(buf);
  int count = profile.getSetpointCount();
  finalTempOverride = profile.getFinalTargetTemp();
  myNex.writeNum("globals.setTempNum.val", finalTempOverride);
  LOG_INFOF("Loaded active profile '%s' with %d setpoints (final target %dF)", activeName.c_str(), count, finalTempOverride);
  
  // Log first few setpoints for verification
  for (int i = 0; i < min(count, 3); i++) {
    auto sp = profile.getSetpoint(i);
    LOG_DEBUGF("  Setpoint %d: time=%dms, temp=%d, fan=%d", i, sp.time, sp.temp, sp.fanSpeed);
  }
  
  return true;
}

/**
 * Plot the active profile on the Nextion waveform (s0 control on ProfileActive page)
 * Scales time (x-axis) and temperature (y-axis) to fit waveform dimensions
 */
void plotProfileOnWaveform() {
  int count = profile.getSetpointCount();
  if (count < 2) {
    LOG_WARN("plotProfileOnWaveform: Profile has fewer than 2 setpoints, skipping plot");
    return;
  }
  
  // Get final time (last setpoint time in milliseconds)
  auto finalSetpoint = profile.getSetpoint(count - 1);
  uint32_t maxTime = finalSetpoint.time;
  uint32_t maxTemp = finalSetpoint.temp;
  
  LOG_INFOF("plotProfileOnWaveform: Plotting %d setpoints, duration=%dms, maxTemp=%d", 
            count, maxTime, maxTemp);
  
  // Nextion waveform: s0 is 480 pixels wide, 170 pixels tall
  // Component ID is 2, channel 0
  const int WAVEFORM_WIDTH = 480;  // Number of data points to send (matches pixel width)
  const int WAVEFORM_HEIGHT = 170; // Y-axis range (0-170 pixels)
  
  // Safety: ensure temp doesn't exceed waveform range
  if (maxTemp == 0) {
    LOG_WARN("plotProfileOnWaveform: maxTemp is 0, cannot plot");
    return;
  }
  
  // Clear the waveform (component-specific, not whole page)
  myNex.writeStr("s0.clr");
  delay(100);  // Give Nextion time to process clear
  LOG_DEBUG("plotProfileOnWaveform: Cleared waveform");
  
  // Send interpolated data points at regular time intervals
  int pointsSent = 0;
  for (int i = 0; i < WAVEFORM_WIDTH; i++) {
    // Calculate time for this x position (reverse: maxTime to 0)
    // This fixes the inverted x-axis so profile renders left-to-right
    uint32_t timeAtX = (maxTime * (WAVEFORM_WIDTH - 1 - i)) / WAVEFORM_WIDTH;
    
    // Get interpolated temperature at this time
    uint32_t interpolatedTemp = profile.getTargetTempAtTime(timeAtX);
    
    // Scale temperature to waveform height (0-170)
    uint8_t scaledTemp = (interpolatedTemp * WAVEFORM_HEIGHT) / maxTemp;
    scaledTemp = constrain(scaledTemp, 0, WAVEFORM_HEIGHT);
    
    // Send to waveform: add <componentID>,<channel>,<value>
    // Component ID 2 = s0, channel 0
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "add 2,0,%d", scaledTemp);
    myNex.writeStr(cmd);
    
    // Reduced delay for faster plotting (480 points * 2ms = ~1 second)
    delay(2);
    pointsSent++;
    
    // Log sample points for debugging
    if (i < 5 || i % 100 == 0) {
      LOG_DEBUGF("  Point %d: time=%dms, temp=%d, scaled=%d", i, timeAtX, interpolatedTemp, scaledTemp);
    }
  }
  
  LOG_INFOF("plotProfileOnWaveform: Sent %d data points to waveform", pointsSent);
  
  // Refresh the button to ensure it stays visible
  myNex.writeStr("ref b1");
}

/**
 * Plot the current active profile when ProfileActive page is navigated to
 * Called via Nextion preinit event or trigger
 */
void onProfileActivePageEnter() {
  if (profile.getSetpointCount() < 2) {
    LOG_WARN("onProfileActivePageEnter: Profile has fewer than 2 setpoints, skipping plot");
    return;
  }
  
  LOG_INFO("onProfileActivePageEnter: Plotting active profile");
  String activeName = preferences.getString("profile_active", "");
  if (activeName.length() > 0) {
    String displayName = activeName + " active";
    myNex.writeStr("ProfileActive.t1.txt", displayName);
  }
  plotProfileOnWaveform();
}

/**
 * Get list of all saved profiles
 * Returns JSON with array of profile names and active profile
 */
String getProfilesList() {
  JsonDocument doc;
  JsonArray arr = doc["profiles"].to<JsonArray>();
  
  String csv = preferences.getString("profile_names", "");
  auto names = splitNames(csv);
  for (auto& name : names) {
    arr.add(name);
  }
  
  doc["active"] = preferences.getString("profile_active", "");
  
  String output;
  serializeJson(doc, output);
  LOG_DEBUGF("Profiles list: %s", output.c_str());
  return output;
}

/**
 * Get a specific saved profile by name
 * Returns JSON with setpoints for that profile
 */
String getProfileByName(const String& name) {
  JsonDocument doc;
  
  if (name.length() == 0) {
    doc["error"] = "empty_name";
    String output;
    serializeJson(doc, output);
    return output;
  }
  
  // Load profile data
  String sanitized = sanitizeProfileName(name);
  String key = String("p_") + sanitized;
  uint8_t buf[200];
  size_t readLen = preferences.getBytes(key.c_str(), buf, sizeof(buf));
  
  if (readLen == 0) {
    doc["error"] = "not_found";
    LOG_WARNF("Profile not found: %s (key: %s)", name.c_str(), key.c_str());
    String output;
    serializeJson(doc, output);
    return output;
  }
  
  // Load into temporary profile
  Profiles tempProfile;
  tempProfile.unflattenProfile(buf);
  
  // Serialize to JSON
  JsonArray arr = doc["setpoints"].to<JsonArray>();
  for (int i = 0; i < tempProfile.getSetpointCount(); i++) {
    auto sp = tempProfile.getSetpoint(i);
    // Skip any default/dummy zero setpoint
    if (sp.time == 0 && sp.temp == 0 && sp.fanSpeed == 0) {
      continue;
    }
    JsonObject spObj = arr.add<JsonObject>();
    spObj["time"] = sp.time / 1000;  // Convert milliseconds to seconds
    spObj["temp"] = sp.temp;
    spObj["fanSpeed"] = sp.fanSpeed;
  }
  
  LOG_DEBUGF("Loaded profile '%s' with %d setpoints", name.c_str(), tempProfile.getSetpointCount());
  
  String output;
  serializeJson(doc, output);
  return output;
}

/**
 * Save a profile to NVS
 * If name already exists, overwrites it
 * If activate is true, makes it the active profile
 */
String saveProfile(const JsonDocument& requestDoc) {
  JsonDocument responseDoc;
  
  // Debug: Log received document
  String debugStr;
  serializeJson(requestDoc, debugStr);
  LOG_DEBUGF("saveProfile received: %s", debugStr.c_str());
  
  // Validate request
  if (!requestDoc.containsKey("setpoints")) {
    responseDoc["ok"] = false;
    responseDoc["error"] = "missing_setpoints";
    LOG_WARN("saveProfile: setpoints key not found");
    String output;
    serializeJson(responseDoc, output);
    return output;
  }
  
  JsonVariantConst setpointsVariant = requestDoc["setpoints"];
  if (!setpointsVariant.is<JsonArrayConst>()) {
    responseDoc["ok"] = false;
    responseDoc["error"] = "setpoints_not_array";
    LOG_WARN("saveProfile: setpoints is not array");
    String output;
    serializeJson(responseDoc, output);
    return output;
  }
  
  JsonArrayConst setpointsArray = requestDoc["setpoints"].as<JsonArrayConst>();
  size_t spCount = setpointsArray.size();
  if (spCount == 0 || spCount > 10) {
    responseDoc["ok"] = false;
    responseDoc["error"] = "invalid_setpoint_count";
    String output;
    serializeJson(responseDoc, output);
    return output;
  }
  
  String profileName = requestDoc["name"].as<String>();
  if (profileName.length() == 0) {
    profileName = "Unnamed";
  }
  profileName = profileName.substring(0, 20);  // Truncate to 20 chars
  
  bool activate = requestDoc["activate"].as<bool>();
  
  // Build temporary profile using Profiles API
  Profiles tempProfile;
  tempProfile.clearSetpoints();
  
  for (size_t i = 0; i < spCount; i++) {
    JsonVariantConst sp = setpointsArray[i];
    uint32_t timeSec = sp["time"].as<uint32_t>();
    uint32_t temp = sp["temp"].as<uint32_t>();
    uint32_t fan = sp["fanSpeed"].as<uint32_t>();
    
    // Convert seconds to milliseconds
    uint32_t timeMs = timeSec * 1000UL;
    
    // Validate and add setpoint
    if (!tempProfile.validateSetpoint(temp, fan)) {
      responseDoc["ok"] = false;
      responseDoc["error"] = "setpoint_out_of_bounds";
      String output;
      serializeJson(responseDoc, output);
      return output;
    }
    
    tempProfile.addSetpoint(timeMs, temp, fan);
  }
  
  // Flatten to buffer
  tempProfile.flattenProfile(profileBuffer);
  
  LOG_DEBUGF("Flattened profile buffer (first 20 bytes): %02x %02x %02x %02x...", 
    profileBuffer[0], profileBuffer[1], profileBuffer[2], profileBuffer[3]);
  
  // Save to preferences
  String sanitized = sanitizeProfileName(profileName);
  String key = String("p_") + sanitized;
  
  LOG_DEBUGF("Attempting to save profile with key: %s (length: %d)", key.c_str(), key.length());
  
  // Ensure preferences is accessible (thread-safe for async handlers)
  // Open in RW mode specifically for this operation
  Preferences prefs;
  if (!prefs.begin("roaster", false)) {
    responseDoc["ok"] = false;
    responseDoc["error"] = "preferences_open_failed";
    LOG_ERROR("Failed to open preferences namespace for write");
    String output;
    serializeJson(responseDoc, output);
    return output;
  }
  
  size_t written = 0;
  written = prefs.putBytes(key.c_str(), profileBuffer, 200);
  prefs.end();
  
  LOG_DEBUGF("prefs.putBytes returned: %d bytes", written);
  
  if (written != 200) {
    responseDoc["ok"] = false;
    responseDoc["error"] = "write_failed";
    responseDoc["written"] = written;
    responseDoc["expected"] = 200;
    LOG_ERRORF("Failed to write profile to NVS (wrote %d bytes, expected 200)", written);
    String output;
    serializeJson(responseDoc, output);
    return output;
  }
  
  // Add to profile names list - reopen prefs
  Preferences prefs2;
  if (prefs2.begin("roaster", false)) {
    String csv = prefs2.getString("profile_names", "");
    auto names = splitNames(csv);
    bool exists = false;
    for (auto& n : names) if (n == profileName) { exists = true; break; }
    if (!exists) {
      names.push_back(profileName);
      prefs2.putString("profile_names", joinNames(names));
    }
    prefs2.end();
  }
  
  LOG_INFOF("Successfully saved profile '%s' (%d bytes)", profileName.c_str(), written);
  
  // If activate flag is set, load this profile as active
  if (activate) {
    profile.clearSetpoints();
    for (int i = 0; i < tempProfile.getSetpointCount(); i++) {
      auto sp = tempProfile.getSetpoint(i);
      profile.addSetpoint(sp.time, sp.temp, sp.fanSpeed);
    }
    preferences.putString("profile_active", profileName);
    LOG_INFOF("Activated profile '%s'", profileName.c_str());
  }
  
  responseDoc["ok"] = true;
  responseDoc["name"] = profileName;
  if (activate) {
    responseDoc["active"] = profileName;
  }
  
  // Return the saved setpoints so UI can update
  JsonArray responseSetpoints = responseDoc["setpoints"].to<JsonArray>();
  for (int i = 0; i < tempProfile.getSetpointCount(); i++) {
    auto sp = tempProfile.getSetpoint(i);
    // Skip any default/dummy zero setpoint
    if (sp.time == 0 && sp.temp == 0 && sp.fanSpeed == 0) {
      continue;
    }
    JsonObject spObj = responseSetpoints.add<JsonObject>();
    spObj["time"] = sp.time / 1000;  // Convert ms back to seconds
    spObj["temp"] = sp.temp;
    spObj["fanSpeed"] = sp.fanSpeed;
  }
  
  String output;
  serializeJson(responseDoc, output);
  return output;
}

/**
 * Activate a saved profile by name
 * Loads it from NVS into the active profile
 */
String activateProfile(const String& name) {
  JsonDocument responseDoc;
  
  if (name.length() == 0) {
    responseDoc["ok"] = false;
    responseDoc["error"] = "empty_name";
    String output;
    serializeJson(responseDoc, output);
    return output;
  }
  
  // Load profile data
  String sanitized = sanitizeProfileName(name);
  String key = String("p_") + sanitized;
  uint8_t buf[200];
  size_t readLen = preferences.getBytes(key.c_str(), buf, sizeof(buf));
  
  if (readLen == 0) {
    responseDoc["ok"] = false;
    responseDoc["error"] = "profile_not_found";
    LOG_WARNF("Profile not found for activation: %s", name.c_str());
    String output;
    serializeJson(responseDoc, output);
    return output;
  }
  
  // Load into active profile
  profile.unflattenProfile(buf);
  finalTempOverride = profile.getFinalTargetTemp();
  myNex.writeNum("globals.setTempNum.val", finalTempOverride);
  preferences.putString("profile_active", name);
  
  LOG_INFOF("Activated profile '%s' (final target %dF)", name.c_str(), finalTempOverride);
  
  // Switch to ProfileActive page and plot the profile on waveform
  myNex.writeStr("page ProfileActive");
  delay(100);  // Wait for page to load
  String displayName = name + " active";
  myNex.writeStr("ProfileActive.t1.txt", displayName);
  LOG_INFO("activateProfile: plotting profile on waveform");
  plotProfileOnWaveform();
  LOG_INFO("activateProfile: waveform plot complete");
  
  responseDoc["ok"] = true;
  responseDoc["active"] = name;
  
  String output;
  serializeJson(responseDoc, output);
  return output;
}

/**
 * Delete a saved profile by name
 * Cannot delete the currently active profile
 */
String deleteProfile(const String& name) {
  JsonDocument responseDoc;
  
  if (name.length() == 0) {
    responseDoc["ok"] = false;
    responseDoc["error"] = "empty_name";
    String output;
    serializeJson(responseDoc, output);
    return output;
  }
  
  // Check if trying to delete active profile
  String activeName = preferences.getString("profile_active", "");
  if (name.equals(activeName)) {
    responseDoc["ok"] = false;
    responseDoc["error"] = "cannot_delete_active";
    LOG_WARNF("Attempted to delete active profile '%s'", name.c_str());
    String output;
    serializeJson(responseDoc, output);
    return output;
  }
  
  // Remove from storage
  String sanitized = sanitizeProfileName(name);
  String key = String("p_") + sanitized;
  preferences.remove(key.c_str());
  
  // Remove from names list
  removeProfileName(name);
  
  LOG_INFOF("Deleted profile '%s'", name.c_str());
  
  responseDoc["ok"] = true;
  
  String output;
  serializeJson(responseDoc, output);
  return output;
}

/**
 * Create a new profile with default setpoints
 * 4 points ending at 444°F
 */
String createNewProfile(const String& name) {
  JsonDocument doc;
  
  String profileName = name;
  if (profileName.length() == 0) {
    profileName = "New Profile";
  }
  
  // Default setpoints: 4 points ending at 444°F
  JsonArray arr = doc["setpoints"].to<JsonArray>();
  
  JsonObject sp1 = arr.add<JsonObject>();
  sp1["time"] = 0;
  sp1["temp"] = 200;
  sp1["fanSpeed"] = 30;
  
  JsonObject sp2 = arr.add<JsonObject>();
  sp2["time"] = 180;  // 3 minutes
  sp2["temp"] = 350;
  sp2["fanSpeed"] = 50;
  
  JsonObject sp3 = arr.add<JsonObject>();
  sp3["time"] = 420;  // 7 minutes
  sp3["temp"] = 400;
  sp3["fanSpeed"] = 70;
  
  JsonObject sp4 = arr.add<JsonObject>();
  sp4["time"] = 600;  // 10 minutes
  sp4["temp"] = 444;
  sp4["fanSpeed"] = 80;
  
  doc["name"] = profileName;
  
  LOG_INFOF("Created new profile template: %s", profileName.c_str());
  
  String output;
  serializeJson(doc, output);
  return output;
}

/**
 * Rename the currently active profile
 * Updates the profile name in the names list and storage key
 */
String renameActiveProfile(const String& newName) {
  JsonDocument responseDoc;
  
  if (newName.length() == 0) {
    responseDoc["ok"] = false;
    responseDoc["error"] = "empty_name";
    String output;
    serializeJson(responseDoc, output);
    return output;
  }
  
  String oldName = preferences.getString("profile_active", "");
  if (oldName.length() == 0) {
    responseDoc["ok"] = false;
    responseDoc["error"] = "no_active_profile";
    String output;
    serializeJson(responseDoc, output);
    return output;
  }
  
  String truncatedName = newName.substring(0, 20);
  
  // Load the current profile data
  String oldSanitized = sanitizeProfileName(oldName);
  String oldKey = String("p_") + oldSanitized;
  uint8_t buf[200];
  size_t readLen = preferences.getBytes(oldKey.c_str(), buf, sizeof(buf));
  
  if (readLen > 0) {
    // Save under new name
    String newSanitized = sanitizeProfileName(truncatedName);
    String newKey = String("p_") + newSanitized;
    preferences.putBytes(newKey.c_str(), buf, readLen);
    
    // Remove old entry if different
    if (oldSanitized != newSanitized) {
      preferences.remove(oldKey.c_str());
    }
    
    // Update names list
    removeProfileName(oldName);
    addProfileName(truncatedName);
    
    // Update active name
    preferences.putString("profile_active", truncatedName);
    
    LOG_INFOF("Renamed profile from '%s' to '%s'", oldName.c_str(), truncatedName.c_str());
  }
  
  responseDoc["ok"] = true;
  responseDoc["name"] = truncatedName;
  
  String output;
  serializeJson(responseDoc, output);
  return output;
}

#endif // PROFILE_EDITOR_HPP

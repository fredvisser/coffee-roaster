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
std::vector<String> splitNames(const String& csv);
String joinNames(const std::vector<String>& names);
String saveProfileById(const String& id, const JsonDocument& requestDoc, bool allowCreate);
String createProfile(const JsonDocument& requestDoc);
void plotProfileOnWaveform();  // Forward declaration for activateProfile

// Base32 encode 64-bit into 13 chars (RFC 4648 alphabet)
String base32_64(uint64_t v) {
  const char* ALPH = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
  String out;
  out.reserve(13);
  // 64 bits / 5 = 12.8 -> 13 chars
  for (int i = 12; i >= 0; --i) {
    int shift = i * 5;
    uint8_t idx = (shift >= 64) ? 0 : (uint8_t)((v >> shift) & 0x1F);
    out += ALPH[idx];
  }
  return out;
}

// ==== ID-centric storage helpers ====
static const char* PROFILE_IDS_CSV       = "profile_ids";        // Comma-separated list of profile ids
static const char* ACTIVE_PROFILE_ID_KEY = "active_profile_id";  // Currently active profile id

String generateProfileId() {
  // 64 bits of randomness, but we only use first 8 chars to keep NVS key length < 15
  // NVS Key Limit: 15 chars. Prefix "pf_" is 3 chars. ID must be <= 12 chars.
  // We use 8 chars for safety and readability.
  uint64_t r = ((uint64_t)esp_random() << 32) | (uint64_t)esp_random();
  return base32_64(r).substring(0, 8);
}

String profileDataKey(const String& id) { return String("pf_") + id; }
String profileMetaKey(const String& id) { return String("pm_") + id; }

std::vector<String> getProfileIds() {
  String csv = preferences.getString(PROFILE_IDS_CSV, "");
  return splitNames(csv);
}

void setProfileIds(const std::vector<String>& ids) {
  preferences.putString(PROFILE_IDS_CSV, joinNames(ids));
}

bool loadProfileMeta(const String& id, String& nameOut) {
  String metaStr = preferences.getString(profileMetaKey(id).c_str(), "");
  if (metaStr.length() == 0) return false;
  StaticJsonDocument<256> metaDoc;
  DeserializationError err = deserializeJson(metaDoc, metaStr);
  if (err) return false;
  nameOut = metaDoc["name"].as<String>();
  return nameOut.length() > 0;
}

bool saveProfileMeta(const String& id, const String& name) {
  StaticJsonDocument<256> metaDoc;
  metaDoc["id"] = id;
  metaDoc["name"] = name;
  String out;
  serializeJson(metaDoc, out);
  preferences.putString(profileMetaKey(id).c_str(), out);
  return true;
}

bool profileExists(const String& id) {
  uint8_t b[1];
  size_t n = preferences.getBytes(profileDataKey(id).c_str(), b, sizeof(b));
  return n > 0;
}

String getActiveProfileId() {
  return preferences.getString(ACTIVE_PROFILE_ID_KEY, "");
}

void setActiveProfileId(const String& id) {
  preferences.putString(ACTIVE_PROFILE_ID_KEY, id);
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

/**
 * Get current active profile data
 * Returns JSON with setpoints and active profile name
 */
String getActiveProfileData() {
  DynamicJsonDocument doc(1024);
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

// Purge all profile-related keys from NVS (data/meta/list/active)
void purgeAllProfiles() {
  LOG_WARN("Purging all profiles from NVS");
  preferences.remove(PROFILE_IDS_CSV);
  preferences.remove(ACTIVE_PROFILE_ID_KEY);
  // Remove all pf_*/pm_* keys by scanning ids list if present
  auto ids = getProfileIds();
  for (auto& id : ids) {
    preferences.remove(profileDataKey(id).c_str());
    preferences.remove(profileMetaKey(id).c_str());
    yield();
  }
  // Also remove legacy lists if they exist
  preferences.remove("profile_names");
  preferences.remove("profile_keys");
}

/**
 * Ensure a default profile exists
 * Creates "Default" profile if no profiles exist
 */
void ensureDefaultProfile() {
  auto ids = getProfileIds();
  
  // Cleanup: Remove any IDs that don't have corresponding data (orphans)
  // This handles cases where NVS writes failed (e.g. due to key length limits)
  bool listChanged = false;
  for (auto it = ids.begin(); it != ids.end(); ) {
    if (!profileExists(*it)) {
      LOG_WARNF("Removing orphan profile ID: %s (data missing)", it->c_str());
      it = ids.erase(it);
      listChanged = true;
    } else {
      ++it;
    }
  }
  if (listChanged) {
    setProfileIds(ids);
  }

  LOG_DEBUGF("ensureDefaultProfile: Found %d existing profiles", ids.size());
  if (!ids.empty()) {
    // If there's no active id yet, set the first existing id as active
    String activeId = getActiveProfileId();
    if (activeId.length() == 0) {
      setActiveProfileId(ids.front());
      LOG_WARNF("No active profile id set - defaulting to first existing id=%s", ids.front().c_str());
    }
    LOG_DEBUG("ensureDefaultProfile: Profiles already exist, skipping creation");
    return;
  }

  LOG_INFO("No profiles found - creating Default profile");
  // Build default profile document
  DynamicJsonDocument doc(1024);
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

  // Save default profile and set active
  String defaultId = generateProfileId();
  Profiles tempProfile;
  tempProfile.clearSetpoints();
  for (JsonObject spObj : setpoints) {
    uint32_t timeMs = spObj["time"].as<uint32_t>() * 1000UL;
    uint32_t temp = spObj["temp"].as<uint32_t>();
    uint32_t fan  = spObj["fanSpeed"].as<uint32_t>();
    tempProfile.addSetpoint(timeMs, temp, fan);
  }
  tempProfile.flattenProfile(profileBuffer);
  preferences.putBytes(profileDataKey(defaultId).c_str(), profileBuffer, sizeof(profileBuffer));
  saveProfileMeta(defaultId, "Default");
  ids.push_back(defaultId);
  setProfileIds(ids);
  setActiveProfileId(defaultId);
  LOG_INFOF("ensureDefaultProfile: created default profile id=%s", defaultId.c_str());
}

/**
 * Reload the active profile into the global profile object
 * Call after activateProfile() or on startup
 * Returns true if successful, false if no active profile
 */
bool reloadActiveProfile() {
  String activeId = getActiveProfileId();
  LOG_DEBUGF("reloadActiveProfile: Active profile id is '%s'", activeId.c_str());
  if (activeId.length() == 0) {
    auto ids = getProfileIds();
    if (!ids.empty()) {
      activeId = ids.front();
      setActiveProfileId(activeId);
      LOG_WARNF("No active id set, defaulting to first id=%s", activeId.c_str());
    } else {
      LOG_WARN("No active profile id set");
      return false;
    }
  }

  uint8_t buf[200];
  size_t readLen = preferences.getBytes(profileDataKey(activeId).c_str(), buf, sizeof(buf));
  LOG_DEBUGF("reloadActiveProfile: Read %d bytes from key '%s'", readLen, profileDataKey(activeId).c_str());
  if (readLen == 0) {
    auto ids = getProfileIds();
    if (!ids.empty()) {
      activeId = ids.front();
      setActiveProfileId(activeId);
      readLen = preferences.getBytes(profileDataKey(activeId).c_str(), buf, sizeof(buf));
      LOG_WARNF("Active profile not found, fell back to id=%s readLen=%d", activeId.c_str(), (int)readLen);
      if (readLen == 0) return false;
    } else {
      LOG_WARNF("Active profile not found: id=%s", activeId.c_str());
      return false;
    }
  }

  // Load into global profile object
  profile.unflattenProfile(buf);
  int count = profile.getSetpointCount();
  finalTempOverride = profile.getFinalTargetTemp();
  // Removed blocking Nextion call from here to prevent WDT timeouts during setup
  // myNex.writeNum("globals.setTempNum.val", finalTempOverride);

  String activeName;
  loadProfileMeta(activeId, activeName);
  LOG_INFOF("Loaded active profile id=%s name='%s' with %d setpoints (final target %dF)",
            activeId.c_str(), activeName.c_str(), count, finalTempOverride);

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
  delay(50);  // Give Nextion time to process clear, keep UI responsive
  LOG_DEBUG("plotProfileOnWaveform: Cleared waveform");
  
  // Send interpolated data points at regular time intervals
  int pointsSent = 0;
  for (int i = 0; i < WAVEFORM_WIDTH; i++) {
    // Yield every 16 points to service watchdog and network
    if ((i & 0x0F) == 0) yield();
    
    // Calculate time for this x position (reverse: maxTime to 0)
    // This fixes the inverted x-axis so profile renders left-to-right
    uint32_t timeAtX = (maxTime * (WAVEFORM_WIDTH - 1 - i)) / WAVEFORM_WIDTH;
    
    // Get interpolated temperature at this time
    uint32_t interpolatedTemp = profile.getTargetTempAtTime(timeAtX);
    
    // Scale temperature to waveform height (0-170) using wider intermediates
    uint32_t scaledTemp32 = ((uint32_t)interpolatedTemp * (uint32_t)WAVEFORM_HEIGHT) / (uint32_t)maxTemp;
    if (scaledTemp32 > (uint32_t)WAVEFORM_HEIGHT) scaledTemp32 = (uint32_t)WAVEFORM_HEIGHT;
    
    // Send to waveform: add <componentID>,<channel>,<value>
    // Component ID 2 = s0, channel 0
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "add 2,0,%lu", (unsigned long)scaledTemp32);
    myNex.writeStr(cmd);
    
    // No delay needed - yield() at loop start is sufficient
    pointsSent++;
    
    // Log sample points for debugging
    if (i < 5 || i % 100 == 0) {
      LOG_DEBUGF("  Point %d: time=%dms, temp=%d, scaled=%lu", i, timeAtX, interpolatedTemp, (unsigned long)scaledTemp32);
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
  String activeId = getActiveProfileId();
  String activeName;
  if (activeId.length() > 0) loadProfileMeta(activeId, activeName);
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
  DynamicJsonDocument doc(2048);
  JsonArray arr = doc["profiles"].to<JsonArray>();

  String activeId = getActiveProfileId();
  auto ids = getProfileIds();
  for (auto& id : ids) {
    String name;
    loadProfileMeta(id, name);
    JsonObject obj = arr.add<JsonObject>();
    obj["id"] = id;
    obj["name"] = name;
    obj["active"] = (id == activeId);
  }

  doc["active"] = activeId;

  String output;
  serializeJson(doc, output);
  LOG_DEBUGF("Profiles list: %s", output.c_str());
  return output;
}

/**
 * Get a specific saved profile by id
 * Returns JSON with setpoints for that profile
 */
String getProfileById(const String& id) {
  DynamicJsonDocument doc(1024);

  if (id.length() == 0) {
    doc["error"] = "empty_id";
    String output;
    serializeJson(doc, output);
    return output;
  }

  uint8_t buf[200];
  size_t readLen = preferences.getBytes(profileDataKey(id).c_str(), buf, sizeof(buf));
  if (readLen == 0) {
    doc["error"] = "not_found";
    LOG_WARNF("Profile not found: id=%s", id.c_str());
    String output;
    serializeJson(doc, output);
    return output;
  }

  Profiles tempProfile;
  tempProfile.unflattenProfile(buf);

  String name;
  loadProfileMeta(id, name);

  JsonArray arr = doc["setpoints"].to<JsonArray>();
  for (int i = 0; i < tempProfile.getSetpointCount(); i++) {
    auto sp = tempProfile.getSetpoint(i);
    if (sp.time == 0 && sp.temp == 0 && sp.fanSpeed == 0) {
      continue;
    }
    JsonObject spObj = arr.add<JsonObject>();
    spObj["time"] = sp.time / 1000;
    spObj["temp"] = sp.temp;
    spObj["fanSpeed"] = sp.fanSpeed;
  }

  doc["id"] = id;
  doc["name"] = name;
  doc["active"] = (id == getActiveProfileId());

  LOG_DEBUGF("Loaded profile id=%s name='%s' with %d setpoints", id.c_str(), name.c_str(), tempProfile.getSetpointCount());

  String output;
  serializeJson(doc, output);
  return output;
}

/**
 * Save a profile to NVS
 * If id already exists, overwrites it
 * If activate is true, makes it the active profile
 */
String saveProfileById(const String& id, const JsonDocument& requestDoc, bool allowCreate) {
  DynamicJsonDocument responseDoc(1024);

  if (id.length() == 0) {
    responseDoc["ok"] = false;
    responseDoc["error"] = "empty_id";
    String output; serializeJson(responseDoc, output); return output;
  }

  String debugStr; serializeJson(requestDoc, debugStr);
  LOG_DEBUGF("saveProfileById(%s) received: %s", id.c_str(), debugStr.c_str());

  if (!requestDoc.containsKey("setpoints")) {
    responseDoc["ok"] = false;
    responseDoc["error"] = "missing_setpoints";
    LOG_WARN("saveProfileById: setpoints key not found");
    String output; serializeJson(responseDoc, output); return output;
  }

  JsonVariantConst setpointsVariant = requestDoc["setpoints"];
  if (!setpointsVariant.is<JsonArrayConst>()) {
    responseDoc["ok"] = false;
    responseDoc["error"] = "setpoints_not_array";
    LOG_WARN("saveProfileById: setpoints is not array");
    String output; serializeJson(responseDoc, output); return output;
  }

  JsonArrayConst setpointsArray = requestDoc["setpoints"].as<JsonArrayConst>();
  size_t spCount = setpointsArray.size();
  if (spCount == 0 || spCount > 10) {
    responseDoc["ok"] = false;
    responseDoc["error"] = "invalid_setpoint_count";
    String output; serializeJson(responseDoc, output); return output;
  }

  String profileName = requestDoc["name"].as<String>();
  if (profileName.length() == 0) profileName = "Unnamed";
  bool activate = requestDoc["activate"].as<bool>();

  Profiles tempProfile;
  tempProfile.clearSetpoints();

  for (size_t i = 0; i < spCount; i++) {
    JsonVariantConst sp = setpointsArray[i];
    uint32_t timeSec = sp["time"].as<uint32_t>();
    uint32_t temp = sp["temp"].as<uint32_t>();
    uint32_t fan  = sp["fanSpeed"].as<uint32_t>();
    uint32_t timeMs = timeSec * 1000UL;

    if (!tempProfile.validateSetpoint(temp, fan)) {
      responseDoc["ok"] = false;
      responseDoc["error"] = "setpoint_out_of_bounds";
      String output; serializeJson(responseDoc, output); return output;
    }
    tempProfile.addSetpoint(timeMs, temp, fan);
  }

  bool exists = profileExists(id);
  if (!exists && !allowCreate) {
    responseDoc["ok"] = false;
    responseDoc["error"] = "not_found";
    String output; serializeJson(responseDoc, output); return output;
  }

  tempProfile.flattenProfile(profileBuffer);
  // Compute actual serialized length to minimize NVS usage
  size_t serializedLen = 5 + (size_t)tempProfile.getSetpointCount() * 12; // header + per-setpoint bytes
  uint8_t localBuffer[200]; // Use local buffer to avoid race conditions with global profileBuffer
  if (serializedLen > sizeof(localBuffer)) serializedLen = sizeof(localBuffer);
  
  tempProfile.flattenProfile(localBuffer);

  LOG_DEBUGF("saveProfileById: Writing %d bytes to NVS (Heap: %d)", serializedLen, ESP.getFreeHeap());
  size_t written = preferences.putBytes(profileDataKey(id).c_str(), localBuffer, serializedLen);
  
  if (written == 0) {
    LOG_WARNF("NVS write failed for %s, retrying", profileDataKey(id).c_str());
    
    // Simple retry without closing/opening namespace to avoid race conditions
    for (int attempt = 1; attempt <= 3 && written == 0; attempt++) {
      preferences.remove(profileDataKey(id).c_str());
      written = preferences.putBytes(profileDataKey(id).c_str(), localBuffer, serializedLen);
      LOG_WARNF("Retry %d wrote %d bytes", attempt, (int)written);
    }
    
    if (written == 0) {
      LOG_ERRORF("NVS write failed after retries for %s", profileDataKey(id).c_str());
      
      // CRITICAL: If we can't write, the partition might be full or corrupt.
      // We will try to remove the OLDEST profile to free space.
      auto ids = getProfileIds();
      if (ids.size() > 0) {
        String victimId = ids.front(); // Just pick the first one for now
        if (victimId != id) { // Don't delete ourselves if we are somehow in the list
           LOG_WARNF("Emergency cleanup: Deleting profile %s to free space", victimId.c_str());
           preferences.remove(profileDataKey(victimId).c_str());
           preferences.remove(profileMetaKey(victimId).c_str());
           
           // Remove from list
           for (auto it = ids.begin(); it != ids.end(); ) {
             if (*it == victimId) it = ids.erase(it);
             else ++it;
           }
           setProfileIds(ids);
           
           // Try write again
           written = preferences.putBytes(profileDataKey(id).c_str(), localBuffer, serializedLen);
           LOG_INFOF("Write after cleanup: %d bytes", (int)written);
        }
      }
      
      if (written == 0) {
        // If still failing, return error but DO NOT PURGE/CRASH
        DynamicJsonDocument errDoc(256);
        errDoc["ok"] = false;
        errDoc["error"] = "nvs_write_failed_full";
        String out; serializeJson(errDoc, out);
        return out;
      }
    }
  }
  
  saveProfileMeta(id, profileName);

  auto ids = getProfileIds();
  bool inList = false;
  for (auto& existingId : ids) { if (existingId == id) { inList = true; break; } }
  if (!inList) {
    ids.push_back(id);
    setProfileIds(ids);
  }

  LOG_INFOF("Saved profile id=%s name='%s' (%d bytes)", id.c_str(), profileName.c_str(), (int)written);

  if (activate) {
    profile.clearSetpoints();
    for (int i = 0; i < tempProfile.getSetpointCount(); i++) {
      auto sp = tempProfile.getSetpoint(i);
      profile.addSetpoint(sp.time, sp.temp, sp.fanSpeed);
    }
    setActiveProfileId(id);
    LOG_INFOF("Activated profile id=%s name='%s'", id.c_str(), profileName.c_str());
  }

  LOG_DEBUG("saveProfileById: Building response JSON...");
  responseDoc["ok"] = true;
  responseDoc["id"] = id;
  responseDoc["name"] = profileName;
  if (activate) responseDoc["active"] = id;

  JsonArray responseSetpoints = responseDoc["setpoints"].to<JsonArray>();
  for (int i = 0; i < tempProfile.getSetpointCount(); i++) {
    auto sp = tempProfile.getSetpoint(i);
    if (sp.time == 0 && sp.temp == 0 && sp.fanSpeed == 0) continue;
    JsonObject spObj = responseSetpoints.add<JsonObject>();
    spObj["time"] = sp.time / 1000;
    spObj["temp"] = sp.temp;
    spObj["fanSpeed"] = sp.fanSpeed;
  }

  String output; 
  serializeJson(responseDoc, output); 
  LOG_DEBUGF("saveProfileById: Response built (%d bytes), returning...", output.length());
  return output;
}

// Create a new profile with a generated id using the provided document
String createProfile(const JsonDocument& requestDoc) {
  String id = generateProfileId();
  return saveProfileById(id, requestDoc, true);
}

/**
 * Activate a saved profile by id
 * Loads it from NVS into the active profile
 */
String activateProfileById(const String& id) {
  DynamicJsonDocument responseDoc(1024);

  if (id.length() == 0) {
    responseDoc["ok"] = false;
    responseDoc["error"] = "empty_id";
    String output; serializeJson(responseDoc, output); return output;
  }

  uint8_t buf[200];
  size_t readLen = preferences.getBytes(profileDataKey(id).c_str(), buf, sizeof(buf));
  if (readLen == 0) {
    responseDoc["ok"] = false;
    responseDoc["error"] = "profile_not_found";
    LOG_WARNF("Profile not found for activation: id=%s", id.c_str());
    String output; serializeJson(responseDoc, output); return output;
  }

  profile.unflattenProfile(buf);
  finalTempOverride = profile.getFinalTargetTemp();
  myNex.writeNum("globals.setTempNum.val", finalTempOverride);
  setActiveProfileId(id);

  String name; loadProfileMeta(id, name);
  LOG_INFOF("Activated profile id=%s name='%s' (final target %dF)", id.c_str(), name.c_str(), finalTempOverride);

  myNex.writeStr("page ProfileActive");
  delay(100);
  if (name.length() > 0) {
    String displayName = name + " active";
    myNex.writeStr("ProfileActive.t1.txt", displayName);
  }
  LOG_INFO("activateProfileById: plotting profile on waveform");
  plotProfileOnWaveform();
  LOG_INFO("activateProfileById: waveform plot complete");

  responseDoc["ok"] = true;
  responseDoc["active"] = id;
  responseDoc["name"] = name;

  String output; serializeJson(responseDoc, output); return output;
}

/**
 * Delete a saved profile by id
 * Cannot delete the currently active profile
 */
String deleteProfileById(const String& id) {
  StaticJsonDocument<512> responseDoc;

  if (id.length() == 0) {
    responseDoc["ok"] = false;
    responseDoc["error"] = "empty_id";
    String output; serializeJson(responseDoc, output); return output;
  }

  String activeId = getActiveProfileId();
  if (id == activeId) {
    responseDoc["ok"] = false;
    responseDoc["error"] = "cannot_delete_active";
    LOG_WARNF("Attempted to delete active profile id=%s", id.c_str());
    String output; serializeJson(responseDoc, output); return output;
  }

  preferences.remove(profileDataKey(id).c_str());
  preferences.remove(profileMetaKey(id).c_str());

  auto ids = getProfileIds();
  std::vector<String> updated;
  updated.reserve(ids.size());
  for (auto& existingId : ids) if (existingId != id) updated.push_back(existingId);
  setProfileIds(updated);

  LOG_INFOF("Deleted profile id=%s", id.c_str());

  responseDoc["ok"] = true;
  String output; serializeJson(responseDoc, output); return output;
}

/**
 * Create a new profile with default setpoints
 * 4 points ending at 444°F
 */
String createNewProfile(const String& name) {
  StaticJsonDocument<1024> doc;

  String profileName = name.length() ? name : String("New Profile");
  String id = generateProfileId();

  Profiles tempProfile;
  tempProfile.clearSetpoints();
  tempProfile.addSetpoint(0,   200, 30);
  tempProfile.addSetpoint(180000, 350, 50);
  tempProfile.addSetpoint(420000, 400, 70);
  tempProfile.addSetpoint(600000, 444, 80);
  tempProfile.flattenProfile(profileBuffer);

  preferences.putBytes(profileDataKey(id).c_str(), profileBuffer, sizeof(profileBuffer));
  saveProfileMeta(id, profileName);

  auto ids = getProfileIds();
  ids.push_back(id);
  setProfileIds(ids);

  doc["ok"] = true;
  doc["id"] = id;
  doc["name"] = profileName;
  JsonArray arr = doc["setpoints"].to<JsonArray>();
  JsonObject sp1 = arr.createNestedObject();
  sp1["time"] = 0;
  sp1["temp"] = 200;
  sp1["fanSpeed"] = 30;

  JsonObject sp2 = arr.createNestedObject();
  sp2["time"] = 180;
  sp2["temp"] = 350;
  sp2["fanSpeed"] = 50;

  JsonObject sp3 = arr.createNestedObject();
  sp3["time"] = 420;
  sp3["temp"] = 400;
  sp3["fanSpeed"] = 70;

  JsonObject sp4 = arr.createNestedObject();
  sp4["time"] = 600;
  sp4["temp"] = 444;
  sp4["fanSpeed"] = 80;

  LOG_INFOF("Created new profile id=%s name='%s'", id.c_str(), profileName.c_str());

  String output; serializeJson(doc, output); return output;
}

/**
 * Rename the currently active profile
 * Updates the profile name in the names list and storage key
 */
String renameActiveProfile(const String& newName) {
  StaticJsonDocument<1024> responseDoc;
  
  if (newName.length() == 0) {
    responseDoc["ok"] = false;
    responseDoc["error"] = "empty_name";
    String output;
    serializeJson(responseDoc, output);
    return output;
  }
  
  String activeId = getActiveProfileId();
  if (activeId.length() == 0) {
    responseDoc["ok"] = false;
    responseDoc["error"] = "no_active_profile";
    String output;
    serializeJson(responseDoc, output);
    return output;
  }
  
  saveProfileMeta(activeId, newName);
  LOG_INFOF("Renamed active profile id=%s to '%s'", activeId.c_str(), newName.c_str());
  
  responseDoc["ok"] = true;
  responseDoc["name"] = newName;
  responseDoc["id"] = activeId;
  
  String output;
  serializeJson(responseDoc, output);
  return output;
}

#endif // PROFILE_EDITOR_HPP

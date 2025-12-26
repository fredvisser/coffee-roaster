#ifndef PROFILE_MANAGER_HPP
#define PROFILE_MANAGER_HPP

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <vector>
#include "Profiles.hpp"
#include "DebugLog.hpp"
#include "Types.hpp"

// Forward declarations
extern Profiles profile;
extern Preferences preferences;

struct ProfileOperationResult {
    bool success;
    String id;
    String error;
};

class ProfileManager {
private:
    // Helpers
    String profileDataKey(const String& id) { return String("pf_") + id; }
    String profileMetaKey(const String& id) { return String("pm_") + id; }
    
    // Base32 encoding for IDs
    String base32_64(uint64_t v) {
        const char* ALPH = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
        String out;
        out.reserve(13);
        for (int i = 12; i >= 0; --i) {
            int shift = i * 5;
            uint8_t idx = (shift >= 64) ? 0 : (uint8_t)((v >> shift) & 0x1F);
            out += ALPH[idx];
        }
        return out;
    }

    String generateId() {
        uint64_t r = ((uint64_t)esp_random() << 32) | (uint64_t)esp_random();
        return base32_64(r).substring(0, 8);
    }

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

    String joinNames(const std::vector<String>& names) {
        String out;
        for (size_t i = 0; i < names.size(); i++) {
            out += names[i];
            if (i + 1 < names.size()) out += ",";
        }
        return out;
    }

public:
    ProfileManager() {}

    std::vector<String> getProfileIds() {
        String csv = preferences.getString("profile_ids", "");
        return splitNames(csv);
    }

    void setProfileIds(const std::vector<String>& ids) {
        preferences.putString("profile_ids", joinNames(ids));
    }

    String getActiveProfileId() {
        String id = preferences.getString("active_id", "");
        LOG_DEBUGF("getActiveProfileId returning: %s", id.c_str());
        return id;
    }

    void setActiveProfileId(const String& id) {
        size_t written = preferences.putString("active_id", id);
        if (written == 0) {
            LOG_ERRORF("Failed to write active_id to NVS! (Key len: %d)", String("active_id").length());
        } else {
            LOG_DEBUGF("Set active_id to %s (written %d bytes)", id.c_str(), written);
        }
    }

    bool loadProfileMeta(const String& id, String& nameOut) {
        String metaStr = preferences.getString(profileMetaKey(id).c_str(), "");
        if (metaStr.length() == 0) return false;
        
        DynamicJsonDocument metaDoc(256);
        DeserializationError err = deserializeJson(metaDoc, metaStr);
        if (err) return false;
        
        nameOut = metaDoc["name"].as<String>();
        return nameOut.length() > 0;
    }

    void saveProfileMeta(const String& id, const String& name) {
        DynamicJsonDocument metaDoc(256);
        metaDoc["id"] = id;
        metaDoc["name"] = name;
        String out;
        serializeJson(metaDoc, out);
        preferences.putString(profileMetaKey(id).c_str(), out);
    }

    bool profileExists(const String& id) {
        return preferences.isKey(profileDataKey(id).c_str());
    }

    // Save a profile from a JSON string
    // Returns result struct instead of full JSON to save stack
    ProfileOperationResult saveProfile(const String& jsonBody, String id = "") {
        LOG_DEBUG("ProfileManager::saveProfile start");
        ProfileOperationResult result = {false, "", ""};
        
        { // Scope for DynamicJsonDocument to ensure destruction before return
            // 1. Parse JSON
            // Calculate size: body length + overhead. 
            // For small profiles, 4096 is overkill. 
            // JSON object overhead is roughly 16 bytes per element + string storage.
            size_t capacity = jsonBody.length() * 2 + 512;
            if (capacity > 4096) capacity = 4096;
            
            LOG_DEBUGF("Allocating JSON doc: %d bytes", capacity);
            DynamicJsonDocument doc(capacity);
            
            DeserializationError err = deserializeJson(doc, jsonBody);
            if (err) {
                LOG_ERRORF("JSON deserialize failed: %s", err.c_str());
                result.error = "invalid_json";
                return result;
            }

            // 2. Validate
            if (!doc.containsKey("setpoints") || !doc["setpoints"].is<JsonArray>()) {
                LOG_ERROR("Missing setpoints array");
                result.error = "invalid_setpoints";
                return result;
            }

            // 3. Generate ID if needed
            if (id.length() == 0) {
                id = generateId();
            }
            result.id = id;
            LOG_DEBUGF("Using ID: %s", id.c_str());

            // 4. Convert to Profiles object
            Profiles tempProfile;
            tempProfile.clearSetpoints();
            JsonArray setpoints = doc["setpoints"];
            
            for (JsonObject sp : setpoints) {
                uint32_t time = sp["time"].as<uint32_t>() * 1000;
                uint32_t temp = sp["temp"].as<uint32_t>();
                uint32_t fan = sp["fanSpeed"].as<uint32_t>();
                
                if (!tempProfile.validateSetpoint(temp, fan)) {
                    LOG_ERROR("Setpoint out of bounds");
                    result.error = "setpoint_out_of_bounds";
                    return result;
                }
                tempProfile.addSetpoint(time, temp, fan);
            }
            LOG_DEBUGF("Parsed %d setpoints", tempProfile.getSetpointCount());

            // 5. Serialize to binary buffer
            uint8_t buffer[200];
            tempProfile.flattenProfile(buffer);
            size_t len = 5 + (tempProfile.getSetpointCount() * 12);
            if (len > sizeof(buffer)) len = sizeof(buffer);

            // 6. Write to NVS
            LOG_DEBUG("Writing to NVS...");
            esp_task_wdt_reset(); // Pet watchdog before NVS op
            size_t written = preferences.putBytes(profileDataKey(id).c_str(), buffer, len);
            
            if (written == 0) {
                LOG_WARN("First write failed, retrying...");
                // Retry logic
                for (int i = 0; i < 3; i++) {
                    esp_task_wdt_reset();
                    preferences.remove(profileDataKey(id).c_str());
                    written = preferences.putBytes(profileDataKey(id).c_str(), buffer, len);
                    if (written > 0) break;
                }
                
                if (written == 0) {
                    LOG_WARN("Retries failed, attempting cleanup...");
                    // Emergency cleanup: delete oldest profile
                    auto ids = getProfileIds();
                    if (!ids.empty()) {
                        String victim = ids.front();
                        if (victim != id) {
                            LOG_WARNF("Deleting %s to free space", victim.c_str());
                            preferences.remove(profileDataKey(victim).c_str());
                            preferences.remove(profileMetaKey(victim).c_str());
                            
                            // Remove from list
                            for (auto it = ids.begin(); it != ids.end(); ) {
                                if (*it == victim) it = ids.erase(it);
                                else ++it;
                            }
                            setProfileIds(ids);
                            
                            // Final try
                            esp_task_wdt_reset();
                            written = preferences.putBytes(profileDataKey(id).c_str(), buffer, len);
                        }
                    }
                }
            }

            if (written == 0) {
                LOG_ERROR("NVS write completely failed");
                result.error = "nvs_write_failed";
                return result;
            }
            LOG_DEBUG("NVS write successful");

            // 7. Save Metadata
            String name = doc["name"] | "Unnamed";
            saveProfileMeta(id, name);

            // 8. Update ID list
            auto ids = getProfileIds();
            bool found = false;
            for (const auto& existing : ids) {
                if (existing == id) { found = true; break; }
            }
            if (!found) {
                ids.push_back(id);
                setProfileIds(ids);
            }

            // 9. Activate if requested
            if (doc["activate"] | false) {
                LOG_DEBUG("Activating profile...");
                profile.clearSetpoints();
                for (int i = 0; i < tempProfile.getSetpointCount(); i++) {
                    auto sp = tempProfile.getSetpoint(i);
                    profile.addSetpoint(sp.time, sp.temp, sp.fanSpeed);
                }
                setActiveProfileId(id);
            }
            
            result.success = true;
        } // doc destroyed here

        LOG_DEBUG("ProfileManager::saveProfile success - doc destroyed");
        return result;
    }

    // Load a profile into the global 'profile' object
    bool loadProfile(const String& id) {
        if (id.length() == 0) return false;
        
        uint8_t buffer[200];
        size_t len = preferences.getBytes(profileDataKey(id).c_str(), buffer, sizeof(buffer));
        
        if (len == 0) return false;
        
        profile.unflattenProfile(buffer);
        return true;
    }

    // Get JSON list of profiles
    String getProfilesList() {
        DynamicJsonDocument doc(2048);
        JsonArray arr = doc.createNestedArray("profiles");
        
        String activeId = getActiveProfileId();
        doc["active"] = activeId; // Add active ID to root
        auto ids = getProfileIds();
        
        for (const auto& id : ids) {
            String name;
            if (loadProfileMeta(id, name)) {
                JsonObject obj = arr.createNestedObject();
                obj["id"] = id;
                obj["name"] = name;
                obj["active"] = (id == activeId);
            }
        }
        
        String out;
        serializeJson(doc, out);
        return out;
    }

    // Get JSON for a specific profile
    String getProfile(const String& id) {
        DynamicJsonDocument doc(1024);
        
        if (!profileExists(id)) {
            doc["error"] = "not_found";
        } else {
            uint8_t buffer[200];
            preferences.getBytes(profileDataKey(id).c_str(), buffer, sizeof(buffer));
            
            Profiles temp;
            temp.unflattenProfile(buffer);
            
            String name;
            loadProfileMeta(id, name);
            
            doc["id"] = id;
            doc["name"] = name;
            doc["active"] = (id == getActiveProfileId());
            
            JsonArray arr = doc.createNestedArray("setpoints");
            for (int i = 0; i < temp.getSetpointCount(); i++) {
                auto sp = temp.getSetpoint(i);
                if (sp.time == 0 && sp.temp == 0 && sp.fanSpeed == 0) continue;
                JsonObject obj = arr.createNestedObject();
                obj["time"] = sp.time / 1000;
                obj["temp"] = sp.temp;
                obj["fanSpeed"] = sp.fanSpeed;
            }
        }
        
        String out;
        serializeJson(doc, out);
        return out;
    }

    bool activateProfile(const String& id) {
        LOG_DEBUGF("activateProfile called for ID: %s", id.c_str());
        if (!profileExists(id)) {
            LOG_WARN("activateProfile: Profile does not exist");
            return false;
        }
        
        uint8_t buffer[200];
        size_t len = preferences.getBytes(profileDataKey(id).c_str(), buffer, sizeof(buffer));
        if (len == 0) {
            LOG_WARN("activateProfile: Failed to read profile data");
            return false;
        }
        
        profile.unflattenProfile(buffer);
        setActiveProfileId(id);
        LOG_INFOF("Profile %s activated successfully", id.c_str());
        return true;
    }

    ProfileOperationResult deleteProfile(const String& id) {
        ProfileOperationResult result = {false, id, ""};
        
        if (id == getActiveProfileId()) {
            result.error = "cannot_delete_active";
            return result;
        }
        
        if (!profileExists(id)) {
            result.error = "not_found";
            return result;
        }
        
        preferences.remove(profileDataKey(id).c_str());
        preferences.remove(profileMetaKey(id).c_str());
        
        auto ids = getProfileIds();
        for (auto it = ids.begin(); it != ids.end(); ) {
            if (*it == id) it = ids.erase(it);
            else ++it;
        }
        setProfileIds(ids);
        
        result.success = true;
        return result;
    }

    void deleteAllProfiles() {
        auto ids = getProfileIds();
        for (const auto& id : ids) {
            preferences.remove(profileDataKey(id).c_str());
            preferences.remove(profileMetaKey(id).c_str());
        }
        preferences.remove("profile_ids");
        preferences.remove("active_id");
        profile.clearSetpoints();
    }
    
    void ensureDefault() {
        auto ids = getProfileIds();
        if (ids.empty()) {
            LOG_INFO("Creating default profile...");
            String defaultJson = "{\"name\":\"Default\",\"activate\":true,\"setpoints\":[{\"time\":0,\"temp\":200,\"fanSpeed\":30},{\"time\":180,\"temp\":350,\"fanSpeed\":50},{\"time\":420,\"temp\":400,\"fanSpeed\":70},{\"time\":600,\"temp\":444,\"fanSpeed\":80}]}";
            saveProfile(defaultJson);
        }
    }
};

#endif

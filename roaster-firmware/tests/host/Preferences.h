#pragma once
#include <map>
#include <string>
#include <vector>
#include <cstring>
class Preferences {
    std::map<std::string, std::vector<unsigned char>> data;
public:
    bool failWrites = false;
    size_t putBytes(const char *key, const void *value, size_t count) {
        if (failWrites) return 0;
        auto bytes = static_cast<const unsigned char *>(value);
        data[key] = std::vector<unsigned char>(bytes, bytes + count);
        return count;
    }
    size_t getBytesLength(const char *key) { return data[key].size(); }
    size_t getBytes(const char *key, void *out, size_t count) {
        if (data[key].size() != count) return 0;
        memcpy(out, data[key].data(), count); return count;
    }
    bool remove(const char *key) { return data.erase(key) > 0; }
    double getDouble(const char *key, double fallback = 0) { getBytes(key, &fallback, sizeof(fallback)); return fallback; }
    bool getBool(const char *key, bool fallback = false) { getBytes(key, &fallback, sizeof(fallback)); return fallback; }
    uint32_t getUInt(const char *key, uint32_t fallback = 0) { getBytes(key, &fallback, sizeof(fallback)); return fallback; }
    size_t putDouble(const char *key, double value) { return putBytes(key, &value, sizeof(value)); }
    size_t putBool(const char *key, bool value) { return putBytes(key, &value, sizeof(value)); }
    size_t putUInt(const char *key, uint32_t value) { return putBytes(key, &value, sizeof(value)); }
};

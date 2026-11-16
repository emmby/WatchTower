#pragma once
#include <Arduino.h>
#include <map>
#include <string>

class Preferences {
public:
    std::map<std::string, std::string> strings;
    std::map<std::string, bool> bools;

    void clear() {
        strings.clear();
        bools.clear();
    }
    void begin(const char* name, bool readOnly = false) {}
    void putBool(const char* key, bool value) {
        if (key) bools[key] = value;
    }
    bool getBool(const char* key, bool defaultValue = false) {
        if (!key) return defaultValue;
        auto it = bools.find(key);
        return (it != bools.end()) ? it->second : defaultValue;
    }
    void putString(const char* key, String value) {
        if (key) strings[key] = value.c_str();
    }
    String getString(const char* key, String defaultValue = "") {
        if (!key) return defaultValue;
        auto it = strings.find(key);
        return (it != strings.end()) ? String(it->second) : defaultValue;
    }
};

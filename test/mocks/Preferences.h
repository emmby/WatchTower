#pragma once

class Preferences {
public:
    void begin(const char* name, bool readOnly = false) {}
    void putBool(const char* key, bool value) {}
    bool getBool(const char* key, bool defaultValue = false) { return defaultValue; }
};

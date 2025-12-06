#pragma once

class Preferences {
public:
    void begin(const char* name, bool readOnly = false) {}
    void putBool(const char* key, bool value) {}
    bool getBool(const char* key, bool defaultValue = false) { return defaultValue; }
    void putString(const char* key, String value) {}
    String getString(const char* key, String defaultValue = "") { return defaultValue; }
};

#pragma once
#include <Arduino.h>

class WiFiManager {
public:
    static bool calledAutoConnect;
    static char lastSSID[32];

    void setAPCallback(void (*func)(WiFiManager*)) {}
    bool autoConnect(const char* ssid) { 
        calledAutoConnect = true;
        strncpy(lastSSID, ssid, sizeof(lastSSID) - 1);
        return true; 
    }
};

bool WiFiManager::calledAutoConnect = false;
char WiFiManager::lastSSID[32] = "";

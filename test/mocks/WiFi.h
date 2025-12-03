#pragma once
#include <Arduino.h>

#define WIFI_ALL_CHANNEL_SCAN 0

class WiFiClass {
public:
    void setScanMethod(int method) {}
    IPAddress localIP() { return IPAddress(127,0,0,1); }
};

extern WiFiClass WiFi;

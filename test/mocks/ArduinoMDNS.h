#pragma once
#include <Arduino.h>
#include <WiFiUdp.h>

class MDNS {
public:
    MDNS(WiFiUDP& udp) {}
    void begin(IPAddress ip, const char* name) {}
    void run() {}
};

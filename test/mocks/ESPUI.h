#pragma once
#include <Arduino.h>

enum Verbosity { Quiet };
enum ControlColor { Sunflower, Turquoise, Emerald, Peterriver, Carrot, Alizarin };

class ESPUIClass {
public:
    void setVerbosity(Verbosity v) {}
    uint16_t label(const char* label, ControlColor color, const char* value) { return 0; }
    void setPanelWide(uint16_t id, bool wide) {}
    void setElementStyle(uint16_t id, const char* style) {}
    void setCustomJS(const char* js) {}
    void begin(const char* title) {}
    void print(uint16_t id, const char* value) {}
};

extern ESPUIClass ESPUI;

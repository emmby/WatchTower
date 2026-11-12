#pragma once
#include <Arduino.h>
#include <map>
#include <string>

enum Verbosity { Quiet };
enum ControlColor { Sunflower, Turquoise, Emerald, Peterriver, Carrot, Alizarin };

class ESPUIClass {
public:
    uint16_t nextId = 1;
    std::map<uint16_t, std::string> printedValues;

    void setVerbosity(Verbosity v) {}
    uint16_t label(const char* label, ControlColor color, const char* value) {
        uint16_t id = nextId++;
        if (value) printedValues[id] = value;
        return id;
    }
    void setPanelWide(uint16_t id, bool wide) {}
    void setElementStyle(uint16_t id, const char* style) {}
    void setCustomJS(const char* js) {}
    void begin(const char* title) {}
    void print(uint16_t id, const char* value) {
        if (value) printedValues[id] = value;
    }
};

extern ESPUIClass ESPUI;

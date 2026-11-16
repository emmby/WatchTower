#pragma once
#include <Arduino.h>
#include <map>
#include <string>

enum Verbosity { Quiet };
enum ControlColor { Sunflower, Turquoise, Emerald, Peterriver, Carrot, Alizarin, Dark, Wisteria, Wetasphalt };
enum class ControlType { Label, Button, Switch, Option, Select, Text, Number, Slider, Pad, Graph };

struct Control {
    uint16_t id;
    void* ptr;
    String value; // Added value
    static uint16_t noParent;
};

// Define static member (inline for header-only mock)
inline uint16_t Control::noParent = 0;

#define S_ACTIVE -7
#define S_INACTIVE 7

class ESPUIClass {
public:
    uint16_t nextId = 1;
    std::map<uint16_t, std::string> labels;
    std::map<uint16_t, std::string> printedValues;
    std::map<uint16_t, std::string> panelClasses;
    const char* customJS = nullptr;
    const char* customCSS = nullptr;

    void setVerbosity(Verbosity v) {}
    uint16_t label(const char* label, ControlColor color, const char* value) {
        uint16_t id = nextId++;
        if (label) labels[id] = label;
        if (value) printedValues[id] = value;
        return id;
    }
    void setPanelWide(uint16_t id, bool wide) {}
    void setPanelClass(uint16_t id, const String& pClass, int clientId = -1) {
        panelClasses[id] = pClass.c_str();
    }
    void setElementStyle(uint16_t id, const char* style) {}
    void setCustomJS(const char* js) { customJS = js; }
    void setCustomCSS(const char* css) { customCSS = css; }
    void begin(const char* title) {}
    void print(uint16_t id, const char* value) {
        if (value) printedValues[id] = value;
    }
    
    // Mock switcher
    uint16_t switcher(const char* label, void (*callback)(Control*, int), ControlColor color, bool startState = false) {
        uint16_t id = nextId++;
        if (label) labels[id] = label;
        printedValues[id] = startState ? "1" : "0";
        return id;
    }
    
    // Mock text
    uint16_t text(const char* label, void (*callback)(Control*, int), ControlColor color, const char* value) {
        uint16_t id = nextId++;
        if (label) labels[id] = label;
        if (value) printedValues[id] = value;
        return id;
    }

    // Mock updateText
    void updateText(uint16_t id, const String& value, int clientId = -1) {
        printedValues[id] = value.c_str();
    }
    
    // Mock updateVisibility
    void updateVisibility(uint16_t id, bool visible) {}

    // Mock setInputType
    void setInputType(uint16_t id, const char* type) {}

    // Mock addControl
    uint16_t addControl(ControlType type, const char* label, const char* value, ControlColor color, uint16_t parent, void (*callback)(Control*, int) = nullptr) {
        uint16_t id = nextId++;
        if (label) labels[id] = label;
        if (value) printedValues[id] = value;
        return id;
    }
    
    // Mock updateSelect
    void updateSelect(uint16_t id, const char* value) {}
};

extern ESPUIClass ESPUI;

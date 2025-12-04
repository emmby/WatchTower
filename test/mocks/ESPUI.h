#pragma once
#include <Arduino.h>

enum Verbosity { Quiet };
enum ControlColor { Sunflower, Turquoise, Emerald, Peterriver, Carrot, Alizarin, Dark, Wisteria };
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

class ESPUIClass {
public:
    void setVerbosity(Verbosity v) {}
    uint16_t label(const char* label, ControlColor color, const char* value) { return 0; }
    void setPanelWide(uint16_t id, bool wide) {}
    void setElementStyle(uint16_t id, const char* style) {}
    void setCustomJS(const char* js) {}
    void begin(const char* title) {}
    void print(uint16_t id, const char* value) {}
    
    // Mock switcher
    uint16_t switcher(const char* label, void (*callback)(Control*, int), ControlColor color, bool startState = false) { return 0; }
    
    // Mock text
    uint16_t text(const char* label, void (*callback)(Control*, int), ControlColor color, const char* value) { return 0; }
    
    // Mock updateVisibility
    void updateVisibility(uint16_t id, bool visible) {}

    // Mock setInputType
    void setInputType(uint16_t id, const char* type) {}

    // Mock addControl
    uint16_t addControl(ControlType type, const char* label, const char* value, ControlColor color, uint16_t parent, void (*callback)(Control*, int) = nullptr) { return 0; }
    
    // Mock updateSelect
    void updateSelect(uint16_t id, const char* value) {}
};

extern ESPUIClass ESPUI;

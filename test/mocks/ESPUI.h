#pragma once
#include <Arduino.h>

enum Verbosity { Quiet };
enum ControlColor { Sunflower, Turquoise, Emerald, Peterriver, Carrot, Alizarin };

struct Control {
    uint16_t id;
    void* ptr;
};

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
    // Overload for when callback is passed differently or arguments order differs?
    // In WatchTower.ino: ESPUI.switcher("Network Sync", updateSyncCallback, ControlColor::Sunflower, networkSyncEnabled);
    // Signature: (const char*, void(*)(Control*, int), ControlColor, bool)
    // This matches what I wrote above.
};

extern ESPUIClass ESPUI;

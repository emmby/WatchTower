#pragma once
#include <Arduino.h>

class EspClass {
public:
    int restartCount = 0;
    void restart() { restartCount++; }
};

extern EspClass ESP;

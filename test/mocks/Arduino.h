#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include <sys/time.h>

#define OUTPUT 1
#define INPUT 0
#define LOW 0
#define HIGH 1

typedef uint8_t byte;
typedef bool boolean;

class IPAddress {
public:
    IPAddress() {}
    IPAddress(uint8_t, uint8_t, uint8_t, uint8_t) {}
};

class String {
public:
    String(const char* s = "") {}
    String(int i) {}
};

void delay(unsigned long ms);
unsigned long millis();
void pinMode(uint8_t pin, uint8_t mode);
void digitalWrite(uint8_t pin, uint8_t val);

// Serial is defined in test_bootstrap.cpp or here
// We'll let test_bootstrap.cpp handle Serial to avoid circular deps if needed
// But standard Arduino.h declares Serial.
// For now, we'll declare a generic Print/Stream if needed, but WatchTower uses Serial directly.

class Print {
public:
    virtual size_t write(uint8_t) = 0;
};

// Add other core functions as needed

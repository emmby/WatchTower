#pragma once

#include <Adafruit_NeoPixel.h>

// Wraps the optional onboard NeoPixel so that WatchTower.ino
// doesn't need #ifdef or null-check guards scattered everywhere.
// Boards without PIN_NEOPIXEL get silent no-ops.
class StatusLED {
public:
    void begin() {
#ifdef PIN_NEOPIXEL
        _pixel.begin();
        _pixel.setBrightness(_BRIGHTNESS);
        _pixel.clear();
        _pixel.show();
#endif
    }

    void setLoading() {
#ifdef PIN_NEOPIXEL
        _pixel.setPixelColor(0, _COLOR_LOADING);
        _pixel.show();
#endif
    }

    void setReady() {
#ifdef PIN_NEOPIXEL
        _pixel.setPixelColor(0, _COLOR_READY);
        _pixel.show();
        delay(3000);
        _pixel.clear();
        _pixel.show();
#endif
    }

    void setError() {
#ifdef PIN_NEOPIXEL
        _pixel.setPixelColor(0, _COLOR_ERROR);
        _pixel.show();
#endif
    }

    void setTransmitting(bool on) {
#ifdef PIN_NEOPIXEL
        if (on) {
            _pixel.setPixelColor(0, _COLOR_TRANSMIT);
        } else {
            _pixel.clear();
        }
#endif
    }

    void show() {
#ifdef PIN_NEOPIXEL
        _pixel.show();
#endif
    }

private:
#ifdef PIN_NEOPIXEL
    Adafruit_NeoPixel _pixel{1, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800};

    static constexpr uint8_t  _BRIGHTNESS    = 10;       // very dim, 0-255
    static constexpr uint32_t _COLOR_READY    = (0 << 16) | (60 << 8) | 0;    // green
    static constexpr uint32_t _COLOR_LOADING  = (60 << 16) | (32 << 8) | 0;   // orange
    static constexpr uint32_t _COLOR_ERROR    = (150 << 16) | (0 << 8) | 0;   // red
    static constexpr uint32_t _COLOR_TRANSMIT = (32 << 16) | (0 << 8) | 0;    // dim red
#endif
};

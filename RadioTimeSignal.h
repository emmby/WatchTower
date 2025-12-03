#ifndef RADIO_TIME_SIGNAL_H
#define RADIO_TIME_SIGNAL_H

#include <Arduino.h>

enum class SignalBit_T {
  ZERO = 0,
  ONE = 1,
  MARK = 2,
  IDLE = 3 // Used for DCF77 59th second
};

class RadioTimeSignal {
public:
    virtual ~RadioTimeSignal() {}

    // Returns the carrier frequency in Hz
    virtual int getFrequency() = 0;

    // Returns the signal bit type for the given time
    virtual SignalBit_T getBit(
        int hour,                // 0 - 23
        int minute,              // 0 - 59
        int second,              // 0 - 59 (leap 60)
        int yday,                // days since January 1 eg. Jan 1 is 0
        int year,                // year since 0, eg. 2025
        int today_start_isdst,   // was this morning DST?
        int tomorrow_start_isdst // is tomorrow morning DST?
    ) = 0;

    // Returns a logical high or low to indicate whether the
    // PWM signal should be high or low based on the current time
    virtual bool getSignalLevel(SignalBit_T bit, int millis) = 0;
};

#endif // RADIO_TIME_SIGNAL_H

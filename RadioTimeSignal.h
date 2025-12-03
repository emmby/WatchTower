#ifndef RADIO_TIME_SIGNAL_H
#define RADIO_TIME_SIGNAL_H

#include <Arduino.h>

enum class SignalBit_T {
  ZERO = 0,
  ONE = 1,
  MARK = 2,
  IDLE = 3, // Used for DCF77 59th second
  MSF_01 = 4, // MSF: A=0, B=1
  MSF_11 = 5  // MSF: A=1, B=1
};

class RadioTimeSignal {
public:
    virtual ~RadioTimeSignal() {}

    // Returns the carrier frequency in Hz
    virtual int getFrequency() = 0;

    // Returns the signal bit type for the given time
    virtual SignalBit_T getBit(const struct tm& timeinfo, int today_isdst, int tomorrow_isdst) = 0;

    // Returns a logical high or low to indicate whether the
    // PWM signal should be high or low based on the current time
    virtual bool getSignalLevel(SignalBit_T bit, int millis) = 0;
};

#endif // RADIO_TIME_SIGNAL_H

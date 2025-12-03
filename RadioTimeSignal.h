#ifndef RADIO_TIME_SIGNAL_H
#define RADIO_TIME_SIGNAL_H

#include <Arduino.h>

enum class TimeCodeSymbol {
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

    // Encodes the signal for the upcoming minute.
    // This should be called once at the beginning of each minute (second 0)
    // before calling getSymbolForSecond
    virtual void encodeMinute(const struct tm& timeinfo, int today_isdst, int tomorrow_isdst) = 0;

    // Returns the signal symbol for the given second (0-59)
    // for the minute encoded by encodeMinute
    virtual TimeCodeSymbol getSymbolForSecond(int second) = 0;

    // Returns a logical high or low to indicate whether the
    // PWM signal should be high or low based on the current time
    virtual bool getSignalLevel(TimeCodeSymbol symbol, int millis) = 0;
};

#endif // RADIO_TIME_SIGNAL_H

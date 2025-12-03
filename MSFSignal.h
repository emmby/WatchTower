#ifndef MSF_SIGNAL_H
#define MSF_SIGNAL_H

#include "RadioTimeSignal.h"

class MSFSignal : public RadioTimeSignal {
public:
    int getFrequency() override {
        return 60000;
    }

    SignalBit_T getBit(const struct tm& timeinfo, int today_start_isdst, int tomorrow_start_isdst) override {
        int hour = timeinfo.tm_hour;
        int minute = timeinfo.tm_min;
        int second = timeinfo.tm_sec;
        int yday = timeinfo.tm_yday + 1;
        int year = timeinfo.tm_year + 1900;
        // MSF Format (Simplified)
        // 0: Minute Marker (MARK)
        // 1-16: DUT1 (0)
        // 17-24: Year (BCD)
        // 25-29: Month (BCD)
        // 30-35: Day of Month (BCD)
        // 36-38: Day of Week (BCD)
        // 39-51: Hour (BCD)
        // 52-59: Minute (BCD)
        
        if (second == 0) return SignalBit_T::MARK;
        
        // Placeholder implementation - returning ZERO for now
        // Proper implementation requires full BCD encoding of date/time
        return SignalBit_T::ZERO;
    }

    bool getSignalLevel(SignalBit_T bit, int millis) override {
        // MSF
        // 0: 100ms Low
        // 1: 200ms Low
        // MARK: 500ms Low
        if (bit == SignalBit_T::MARK) {
            return millis >= 500;
        } else if (bit == SignalBit_T::ONE) {
            return millis >= 200;
        } else { // ZERO
            return millis >= 100;
        }
    }
};

#endif // MSF_SIGNAL_H

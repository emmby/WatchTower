#ifndef WWVB_SIGNAL_H
#define WWVB_SIGNAL_H

#include "RadioTimeSignal.h"

class WWVBSignal : public RadioTimeSignal {
public:
    int getFrequency() override {
        return 60000;
    }

    String getName() override {
        return "WWVB";
    }

    void encodeMinute(const struct tm& timeinfo, int today_start_isdst, int tomorrow_start_isdst) override {
        // Calculate data bits using existing logic
        uint64_t dataBits = 0;
        
        // Minute: 01, 02, 03, 05, 06, 07, 08 (Bits 58-51? No, 59-sec)
        dataBits |= to_padded5_bcd(timeinfo.tm_min) << (59 - 8);

        // Hour: 12, 13, 15, 16, 17, 18 (Bits 59-18)
        dataBits |= to_padded5_bcd(timeinfo.tm_hour) << (59 - 18);

        // Day of Year: 22, 23, 25, 26, 27, 28, 30, 31, 32, 33 (Bits 59-33)
        dataBits |= to_padded5_bcd(timeinfo.tm_yday + 1) << (59 - 33);

        // Year: 45, 46, 47, 48, 50, 51, 52, 53 (Bits 59-53)
        dataBits |= to_padded5_bcd((timeinfo.tm_year + 1900) % 100) << (59 - 53);

        // Leap Year: 55
        if (is_leap_year(timeinfo.tm_year + 1900)) {
            dataBits |= 1ULL << (59 - 55);
        }

        // DST: 57, 58
        bool dst1 = false;
        bool dst2 = false;

        if (today_start_isdst && tomorrow_start_isdst) {
            dst1 = true; dst2 = true;
        } else if (!today_start_isdst && !tomorrow_start_isdst) {
            dst1 = false; dst2 = false;
        } else if (!today_start_isdst && tomorrow_start_isdst) {
            dst1 = true; dst2 = false;
        } else {
            dst1 = false; dst2 = true;
        }

        if (dst1) dataBits |= 1ULL << (59 - 57);
        if (dst2) dataBits |= 1ULL << (59 - 58);

        // Populate array
        for (int i = 0; i < 60; i++) {
            // Markers
            if (i == 0 || i == 9 || i == 19 || i == 29 || i == 39 || i == 49 || i == 59) {
                frameBits_[i] = TimeCodeSymbol::MARK;
            } else if( (dataBits >> (59 - i)) & 1 ) {
                frameBits_[i] = TimeCodeSymbol::ONE;
            } else {
                frameBits_[i] = TimeCodeSymbol::ZERO;
            }
        }
    }

    TimeCodeSymbol getSymbolForSecond(int second) override {
        if (second < 0 || second > 59) return TimeCodeSymbol::ZERO;
        return frameBits_[second];
    }

    bool getSignalLevel(TimeCodeSymbol symbol, int millis) override {
        // WWVB
        // 0: 200ms Low, 800ms High
        // 1: 500ms Low, 500ms High
        // MARK: 800ms Low, 200ms High
        if (symbol == TimeCodeSymbol::MARK) {
            return millis >= 800;
        } else if (symbol == TimeCodeSymbol::ONE) {
            return millis >= 500;
        } else { // ZERO
            return millis >= 200;
        }
    }

private:
    TimeCodeSymbol frameBits_[60];
};

#endif // WWVB_SIGNAL_H

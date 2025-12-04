#ifndef MSF_SIGNAL_H
#define MSF_SIGNAL_H

#include "RadioTimeSignal.h"

class MSFSignal : public RadioTimeSignal {
public:
    int getFrequency() override {
        return 60000;
    }

    void encodeMinute(const struct tm& timeinfo, int today_start_isdst, int tomorrow_start_isdst) override {
        // MSF sends the UPCOMING minute.
        struct tm breakdown = timeinfo;
        breakdown.tm_min += 1;
        mktime(&breakdown); // Normalize
        
        uint64_t aBits = 0b1111110; // Bits 53-59 (Marker)
        
        aBits |= to_bcd(breakdown.tm_year % 100) << (59 - 24);
        aBits |= to_bcd(breakdown.tm_mon + 1) << (59 - 29);
        aBits |= to_bcd(breakdown.tm_mday) << (59 - 35);
        aBits |= to_bcd(breakdown.tm_wday) << (59 - 38);
        aBits |= to_bcd(breakdown.tm_hour) << (59 - 44);
        aBits |= to_bcd(breakdown.tm_min) << (59 - 51);
        
        uint64_t bBits = 0;
        // DUT1 (1-16) - 0
        // Summer time warning (53) - 0
        
        // Year parity (17-24)
        if (countSetBits(aBits, 59 - 24, 59 - 17) % 2 == 0) {
            bBits |= 1ULL << (59 - 54);
        }
        // Day parity (25-35)
        if (countSetBits(aBits, 59 - 35, 59 - 25) % 2 == 0) {
            bBits |= 1ULL << (59 - 55);
        }
        // Weekday parity (36-38)
        if (countSetBits(aBits, 59 - 38, 59 - 36) % 2 == 0) {
            bBits |= 1ULL << (59 - 56);
        }
        // Time parity (39-51)
        if (countSetBits(aBits, 59 - 51, 59 - 39) % 2 == 0) {
            bBits |= 1ULL << (59 - 57);
        }
        
        // DST (58)
        if (breakdown.tm_isdst) {
            bBits |= 1ULL << (59 - 58);
        }

        // Populate array
        for (int i = 0; i < 60; i++) {
            if (i == 0) {
                frameBits_[i] = TimeCodeSymbol::MARK;
            } else {
                bool a = (aBits >> (59 - i)) & 1;
                bool b = (bBits >> (59 - i)) & 1;
                
                if (!a && !b) frameBits_[i] = TimeCodeSymbol::ZERO;
                else if (a && !b) frameBits_[i] = TimeCodeSymbol::ONE;
                else if (!a && b) frameBits_[i] = TimeCodeSymbol::MSF_01;
                else if (a && b) frameBits_[i] = TimeCodeSymbol::MSF_11;
            }
        }
    }

    TimeCodeSymbol getSymbolForSecond(int second) override {
        if (second < 0 || second > 59) return TimeCodeSymbol::ZERO;
        return frameBits_[second];
    }

    bool getSignalLevel(TimeCodeSymbol symbol, int millis) override {
        // MSF
        // 0-100: OFF
        // 100-200: A (OFF if 1)
        // 200-300: B (OFF if 1)
        // 300+: HIGH
        
        if (symbol == TimeCodeSymbol::MARK) {
            return millis >= 500;
        }
        
        bool a = (symbol == TimeCodeSymbol::ONE || symbol == TimeCodeSymbol::MSF_11);
        bool b = (symbol == TimeCodeSymbol::MSF_01 || symbol == TimeCodeSymbol::MSF_11);
        
        if (millis < 100) return false; // Always OFF 0-100
        if (millis < 200) return !a;    // OFF if A=1
        if (millis < 300) return !b;    // OFF if B=1
        
        return true; // HIGH otherwise
    }

private:
    TimeCodeSymbol frameBits_[60];
};

#endif // MSF_SIGNAL_H

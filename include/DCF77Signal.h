#ifndef DCF77_SIGNAL_H
#define DCF77_SIGNAL_H

#include "RadioTimeSignal.h"

class DCF77Signal : public RadioTimeSignal {
public:
    int getFrequency() override {
        return 77500;
    }

    String getName() override {
        return "DCF77";
    }

    void encodeMinute(const struct tm& timeinfo, int today_start_isdst, int tomorrow_start_isdst) override {
        // DCF77 sends the UPCOMING minute.
        struct tm next_min = timeinfo;
        next_min.tm_min += 1;
        mktime(&next_min); // Normalize
        
        frameBits_ = 0;
        
        // 0: Start of minute (0) - Implicitly 0
        // 1-14: Meteo (0) - Implicitly 0
        // 15: Call bit (0) - Implicitly 0
        // 16: Summer time announcement (0) - Implicitly 0
        
        // 17: CEST (1 if DST)
        // 18: CET (1 if not DST)
        if (next_min.tm_isdst) {
            frameBits_ |= 1ULL << (59 - 17);
        } else {
            frameBits_ |= 1ULL << (59 - 18);
        }
        
        // 19: Leap second (0) - Implicitly 0
        
        // 20: Start of time (1)
        frameBits_ |= 1ULL << (59 - 20);
        
        // 21-27: Minute (BCD)
        // LSB at Sec 21 (Bit 38).
        // reverse8Bits puts LSB at Bit 7.
        // 7 + 31 = 38.
        frameBits_ |= (uint64_t)reverse8Bits(toBCD(next_min.tm_min)) << 31;
        
        // 28: Parity Minute
        if (countSetBits(toBCD(next_min.tm_min)) % 2 != 0) {
            frameBits_ |= 1ULL << (59 - 28);
        }
        
        // 29-34: Hour (BCD)
        // LSB at Sec 29 (Bit 30).
        // 7 + 23 = 30.
        frameBits_ |= (uint64_t)reverse8Bits(toBCD(next_min.tm_hour)) << 23;
        
        // 35: Parity Hour
        if (countSetBits(toBCD(next_min.tm_hour)) % 2 != 0) {
            frameBits_ |= 1ULL << (59 - 35);
        }
        
        // 36-41: Day (BCD)
        // LSB at Sec 36 (Bit 23).
        // 7 + 16 = 23.
        frameBits_ |= (uint64_t)reverse8Bits(toBCD(next_min.tm_mday)) << 16;
        
        // 42-44: Day of Week (1=Mon...7=Sun)
        // tm_wday: 0=Sun, 1=Mon...
        int wday = next_min.tm_wday == 0 ? 7 : next_min.tm_wday;
        // LSB at Sec 42 (Bit 17).
        // wday is 3 bits.
        // reverse8Bits puts LSB at Bit 7.
        // 7 + 10 = 17.
        frameBits_ |= (uint64_t)reverse8Bits(wday) << 10;
        
        // 45-49: Month (BCD)
        // LSB at Sec 45 (Bit 14).
        // 7 + 7 = 14.
        frameBits_ |= (uint64_t)reverse8Bits(toBCD(next_min.tm_mon + 1)) << 7;
        
        // 50-57: Year (BCD)
        // LSB at Sec 50 (Bit 9).
        // 7 + 2 = 9.
        frameBits_ |= (uint64_t)reverse8Bits(toBCD((next_min.tm_year + 1900) % 100)) << 2;
        
        // 58: Parity Date
        // Parity of Day, WDay, Month, Year.
        int p = 0;
        p += countSetBits(toBCD(next_min.tm_mday));
        p += countSetBits(wday);
        p += countSetBits(toBCD(next_min.tm_mon + 1));
        p += countSetBits(toBCD((next_min.tm_year + 1900) % 100));
        if (p % 2 != 0) {
            frameBits_ |= 1ULL << (59 - 58);
        }
    }

    TimeCodeSymbol getSymbolForSecond(int second) override {
        if (second < 0 || second > 59) return TimeCodeSymbol::ZERO;
        if (second == 59) return TimeCodeSymbol::IDLE;
        
        bool bitSet = (frameBits_ >> (59 - second)) & 1;
        return bitSet ? TimeCodeSymbol::ONE : TimeCodeSymbol::ZERO;
    }

    bool getLevelForTimeCodeSymbol(TimeCodeSymbol symbol, int millis) override {
        // DCF77
        // 0: 100ms Low, 900ms High
        // 1: 200ms Low, 800ms High
        // IDLE: High (no modulation)
        if (symbol == TimeCodeSymbol::IDLE) {
            return true; // Always High
        } else if (symbol == TimeCodeSymbol::ZERO) {
            return millis >= 100;
        } else { // ONE
            return millis >= 200;
        }
    }

private:
    uint64_t frameBits_ = 0;
};

#endif // DCF77_SIGNAL_H

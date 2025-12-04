#ifndef JJY_SIGNAL_H
#define JJY_SIGNAL_H

#include "RadioTimeSignal.h"

class JJYSignal : public RadioTimeSignal {
public:
    int getFrequency() override {
        return 60000; // Can be 40kHz or 60kHz, defaulting to 60kHz
    }

    void encodeMinute(const struct tm& timeinfo, int today_start_isdst, int tomorrow_start_isdst) override {
        uint64_t dataBits = 0;
        
        // Minute: 1-8 (8 bits)
        // 59 - 8 = 51.
        dataBits |= to_padded5_bcd(timeinfo.tm_min) << (59 - 8);
        
        // Hour: 12-18 (7 bits)
        // 59 - 18 = 41.
        dataBits |= to_padded5_bcd(timeinfo.tm_hour) << (59 - 18);
        
        // Day of Year: 22-30 (9 bits? No, 3 digits padded)
        // 59 - 33 = 26.
        dataBits |= to_padded5_bcd(timeinfo.tm_yday + 1) << (59 - 33);
        
        // Year: 41-48 (8 bits)
        // 59 - 48 = 11.
        dataBits |= to_bcd((timeinfo.tm_year + 1900) % 100) << (59 - 48);
        
        // Day of Week: 50-52 (3 bits)
        // 59 - 52 = 7.
        dataBits |= to_bcd(timeinfo.tm_wday) << (59 - 52);
        
        // Parity
        // PA1 (36): Hour parity (12-18)
        // txtempus: `parity(time_bits_, 59 - 18, 59 - 12) << (59 - 36)`
        if (countSetBits(dataBits, 59 - 18, 59 - 12) % 2 != 0) {
            dataBits |= 1ULL << (59 - 36);
        }
        
        // PA2 (37): Minute parity (1-8)
        // txtempus: `parity(time_bits_, 59 - 8, 59 - 1) << (59 - 37)`
        if (countSetBits(dataBits, 59 - 8, 59 - 1) % 2 != 0) {
            dataBits |= 1ULL << (59 - 37);
        }

        // Populate array
        for (int i = 0; i < 60; i++) {
            // Markers
            if (i == 0 || i == 9 || i == 19 || i == 29 || i == 39 || i == 49 || i == 59) {
                frameBits_[i] = TimeCodeSymbol::MARK;
            } else {
                bool bitSet = (dataBits >> (59 - i)) & 1;
                frameBits_[i] = bitSet ? TimeCodeSymbol::ONE : TimeCodeSymbol::ZERO;
            }
        }
    }

    TimeCodeSymbol getSymbolForSecond(int second) override {
        if (second < 0 || second > 59) return TimeCodeSymbol::ZERO;
        return frameBits_[second];
    }

    bool getSignalLevel(TimeCodeSymbol symbol, int millis) override {
        // JJY
        // 0: 800ms High, 200ms Low
        // 1: 500ms High, 500ms Low
        // MARK: 200ms High, 800ms Low
        
        if (symbol == TimeCodeSymbol::MARK) {
            return millis < 200;
        } else if (symbol == TimeCodeSymbol::ONE) {
            return millis < 500;
        } else { // ZERO
            return millis < 800;
        }
    }

private:
    TimeCodeSymbol frameBits_[60];
};

#endif // JJY_SIGNAL_H

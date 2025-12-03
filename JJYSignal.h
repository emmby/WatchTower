#ifndef JJY_SIGNAL_H
#define JJY_SIGNAL_H

#include "RadioTimeSignal.h"

class JJYSignal : public RadioTimeSignal {
public:
    int getFrequency() override {
        return 60000; // Can be 40kHz or 60kHz, defaulting to 60kHz
    }

    void encodeMinute(const struct tm& timeinfo, int today_start_isdst, int tomorrow_start_isdst) override {
        encodeFrame(timeinfo, today_start_isdst, tomorrow_start_isdst);
    }

    TimeCodeSymbol getSymbolForSecond(int second) override {
        if (second < 0 || second > 59) return TimeCodeSymbol::ZERO;

        // Marker bits are fixed
        if (second == 0 || second == 9 || second == 19 || second == 29 || second == 39 || second == 49 || second == 59) {
            return TimeCodeSymbol::MARK;
        }

        bool bit = (frameBits_ >> (59 - second)) & 1;
        return bit ? TimeCodeSymbol::ONE : TimeCodeSymbol::ZERO;
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
    uint64_t frameBits_ = 0;
    int lastEncodedMinute_ = -1;

    // Helper to encode BCD with a 0 bit inserted between digits (for JJY Min, Hour, YDay)
    uint64_t to_padded5_bcd(int n) {
        return (((n / 100) % 10) << 10) | (((n / 10) % 10) << 5) | (n % 10);
    }

    // Regular BCD for Year and WDay
    uint64_t to_bcd(int n) {
        return (((n / 100) % 10) << 8) | (((n / 10) % 10) << 4) | (n % 10);
    }

    uint64_t parity(uint64_t d, int from, int to_including) {
        int result = 0;
        for (int bit = from; bit <= to_including; ++bit) {
            if (d & (1ULL << bit)) result++;
        }
        return result & 0x1;
    }

    void encodeFrame(const struct tm& timeinfo, int today_start_isdst, int tomorrow_start_isdst) {
        frameBits_ = 0;
        
        // Minute: 1-8 (8 bits)
        // 59 - 8 = 51.
        frameBits_ |= to_padded5_bcd(timeinfo.tm_min) << (59 - 8);
        
        // Hour: 12-18 (7 bits)
        // 59 - 18 = 41.
        frameBits_ |= to_padded5_bcd(timeinfo.tm_hour) << (59 - 18);
        
        // Day of Year: 22-30 (9 bits? No, 3 digits padded)
        // 59 - 33 = 26.
        // Wait, txtempus uses 59-33 for YDay.
        // Let's check JJY spec.
        // 22-30: Day of Year.
        // 22, 23: Hundreds.
        // 24: 0.
        // 25-28: Tens.
        // 29: MARK.
        // 30-33: Units.
        // txtempus: `to_padded5_bcd(breakdown.tm_yday + 1) << (59 - 33)`
        // This puts Units at 30-33.
        // Tens at 25-28.
        // Hundreds at 22-23 (bits 10,11 of padded bcd).
        // Checks out.
        frameBits_ |= to_padded5_bcd(timeinfo.tm_yday + 1) << (59 - 33);
        
        // Year: 41-48 (8 bits)
        // txtempus: `to_bcd(breakdown.tm_year % 100) << (59 - 48)`
        // 59 - 48 = 11.
        frameBits_ |= to_bcd((timeinfo.tm_year + 1900) % 100) << (59 - 48);
        
        // Day of Week: 50-52 (3 bits)
        // txtempus: `to_bcd(breakdown.tm_wday) << (59 - 52)`
        // 59 - 52 = 7.
        frameBits_ |= to_bcd(timeinfo.tm_wday) << (59 - 52);
        
        // Parity
        // PA1 (36): Hour parity (12-18)
        // txtempus: `parity(time_bits_, 59 - 18, 59 - 12) << (59 - 36)`
        frameBits_ |= parity(frameBits_, 59 - 18, 59 - 12) << (59 - 36);
        
        // PA2 (37): Minute parity (1-8)
        // txtempus: `parity(time_bits_, 59 - 8, 59 - 1) << (59 - 37)`
        frameBits_ |= parity(frameBits_, 59 - 8, 59 - 1) << (59 - 37);
    }
};

#endif // JJY_SIGNAL_H

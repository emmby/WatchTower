#ifndef DCF77_SIGNAL_H
#define DCF77_SIGNAL_H

#include "RadioTimeSignal.h"

class DCF77Signal : public RadioTimeSignal {
public:
    int getFrequency() override {
        return 77500;
    }

    SignalBit_T getBit(const struct tm& timeinfo, int today_start_isdst, int tomorrow_start_isdst) override {
        // DCF77 sends the UPCOMING minute.
        struct tm next_min = timeinfo;
        next_min.tm_min += 1;
        mktime(&next_min); // Normalize
        
        encodeFrame(next_min, today_start_isdst, tomorrow_start_isdst);



        int second = timeinfo.tm_sec;
        if (second < 0 || second > 59) return SignalBit_T::ZERO;

        // DCF77 59th second is IDLE (no modulation, i.e., High)
        // But wait, my getSignalLevel(IDLE) returns true (High).
        // So we should return IDLE here.
        if (second == 59) return SignalBit_T::IDLE;

        bool bit = (frameBits_ >> (59 - second)) & 1;
        return bit ? SignalBit_T::ONE : SignalBit_T::ZERO;
    }

    bool getSignalLevel(SignalBit_T bit, int millis) override {
        // DCF77
        // 0: 100ms Low, 900ms High
        // 1: 200ms Low, 800ms High
        // IDLE: High (no modulation)
        if (bit == SignalBit_T::IDLE) {
            return true; // Always High
        } else if (bit == SignalBit_T::ZERO) {
            return millis >= 100;
        } else { // ONE
            return millis >= 200;
        }
    }

private:
    uint64_t frameBits_ = 0;
    int lastEncodedMinute_ = -1;

    uint64_t to_bcd(int n) {
        return ((n / 10) << 4) | (n % 10);
    }

    uint8_t reverse8(uint8_t b) {
        b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
        b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
        b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
        return b;
    }

    bool hasOddParity(uint64_t v) {
        v ^= v >> 1;
        v ^= v >> 2;
        v ^= v >> 4;
        v ^= v >> 8;
        v ^= v >> 16;
        v ^= v >> 32;
        return (v & 1) != 0;
    }
    
    // DCF77 uses Even Parity for the check bit?
    // Wikipedia: "Even parity bit".
    // If the data bits have odd number of 1s, the parity bit must be 1 to make total even.
    // So parity bit = hasOddParity(data).
    // Yes.

    void encodeFrame(const struct tm& timeinfo, int today_start_isdst, int tomorrow_start_isdst) {
        frameBits_ = 0;
        
        // 0: Start of minute (0) - Implicitly 0
        // 1-14: Meteo (0) - Implicitly 0
        // 15: Call bit (0) - Implicitly 0
        // 16: Summer time announcement (0) - Implicitly 0
        
        // 17: CEST (1 if DST)
        // 18: CET (1 if not DST)
        if (timeinfo.tm_isdst) {
            frameBits_ |= 1ULL << (59 - 17);
        } else {
            frameBits_ |= 1ULL << (59 - 18);
        }
        
        // 19: Leap second (0) - Implicitly 0
        
        // 20: Start of time (1)
        frameBits_ |= 1ULL << (59 - 20);
        
        // 21-27: Minute (BCD)
        // LSB at Sec 21 (Bit 38).
        // reverse8 puts LSB at Bit 7.
        // 7 + 31 = 38.
        frameBits_ |= (uint64_t)reverse8(to_bcd(timeinfo.tm_min)) << 31;
        
        // 28: Parity Minute
        // Check bits 21-27.
        // (frameBits_ >> (59 - 27)) & 0x7F -> This extracts bits 32-38.
        // But we want to check parity of the bits we just set.
        // We can check the BCD value directly?
        // Parity of BCD value is same as parity of reversed BCD value.
        if (hasOddParity(to_bcd(timeinfo.tm_min))) {
            frameBits_ |= 1ULL << (59 - 28);
        }
        
        // 29-34: Hour (BCD)
        // LSB at Sec 29 (Bit 30).
        // 7 + 23 = 30.
        frameBits_ |= (uint64_t)reverse8(to_bcd(timeinfo.tm_hour)) << 23;
        
        // 35: Parity Hour
        if (hasOddParity(to_bcd(timeinfo.tm_hour))) {
            frameBits_ |= 1ULL << (59 - 35);
        }
        
        // 36-41: Day (BCD)
        // LSB at Sec 36 (Bit 23).
        // 7 + 16 = 23.
        frameBits_ |= (uint64_t)reverse8(to_bcd(timeinfo.tm_mday)) << 16;
        
        // 42-44: Day of Week (1=Mon...7=Sun)
        // tm_wday: 0=Sun, 1=Mon...
        int wday = timeinfo.tm_wday == 0 ? 7 : timeinfo.tm_wday;
        // LSB at Sec 42 (Bit 17).
        // wday is 3 bits.
        // reverse8 puts LSB at Bit 7.
        // 7 + 10 = 17.
        // But wday is only 3 bits. reverse8 reverses 8 bits.
        // So LSB moves to Bit 7.
        // Correct.
        frameBits_ |= (uint64_t)reverse8(wday) << 10;
        
        // 45-49: Month (BCD)
        // LSB at Sec 45 (Bit 14).
        // 7 + 7 = 14.
        frameBits_ |= (uint64_t)reverse8(to_bcd(timeinfo.tm_mon + 1)) << 7;
        
        // 50-57: Year (BCD)
        // LSB at Sec 50 (Bit 9).
        // 7 + 2 = 9.
        frameBits_ |= (uint64_t)reverse8(to_bcd((timeinfo.tm_year + 1900) % 100)) << 2;
        
        // 58: Parity Date
        // Parity of Day, WDay, Month, Year.
        int p = 0;
        p += hasOddParity(to_bcd(timeinfo.tm_mday));
        p += hasOddParity(wday);
        p += hasOddParity(to_bcd(timeinfo.tm_mon + 1));
        p += hasOddParity(to_bcd((timeinfo.tm_year + 1900) % 100));
        if (p % 2 != 0) {
            frameBits_ |= 1ULL << (59 - 58);
        }
        
        // 59: No modulation (IDLE) - Handled in getBit
    }
};

#endif // DCF77_SIGNAL_H

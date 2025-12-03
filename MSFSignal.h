#ifndef MSF_SIGNAL_H
#define MSF_SIGNAL_H

#include "RadioTimeSignal.h"

class MSFSignal : public RadioTimeSignal {
public:
    int getFrequency() override {
        return 60000;
    }

    SignalBit_T getBit(const struct tm& timeinfo, int today_start_isdst, int tomorrow_start_isdst) override {
        encodeFrame(timeinfo, today_start_isdst, tomorrow_start_isdst);



        int second = timeinfo.tm_sec;
        if (second < 0 || second > 59) return SignalBit_T::ZERO;

        if (second == 0) return SignalBit_T::MARK;

        bool a = (aBits_ >> (59 - second)) & 1;
        bool b = (bBits_ >> (59 - second)) & 1;

        // Encode SignalBit_T based on A and B
        // 00 -> ZERO
        // 10 -> ONE
        // 01 -> MSF_01
        // 11 -> MSF_11
        
        if (!a && !b) return SignalBit_T::ZERO;
        if (a && !b) return SignalBit_T::ONE;
        if (!a && b) return SignalBit_T::MSF_01;
        if (a && b) return SignalBit_T::MSF_11;
        
        return SignalBit_T::ZERO;
    }

    bool getSignalLevel(SignalBit_T bit, int millis) override {
        // MSF
        // 0-100: OFF
        // 100-200: A (OFF if 1)
        // 200-300: B (OFF if 1)
        // 300+: HIGH
        
        if (bit == SignalBit_T::MARK) {
            return millis >= 500;
        }
        
        bool a = (bit == SignalBit_T::ONE || bit == SignalBit_T::MSF_11);
        bool b = (bit == SignalBit_T::MSF_01 || bit == SignalBit_T::MSF_11);
        
        if (millis < 100) return false; // Always OFF 0-100
        if (millis < 200) return !a;    // OFF if A=1
        if (millis < 300) return !b;    // OFF if B=1
        
        return true; // HIGH otherwise
    }

private:
    uint64_t aBits_ = 0;
    uint64_t bBits_ = 0;
    int lastEncodedMinute_ = -1;

    uint64_t to_bcd(int n) {
        return (((n / 10) % 10) << 4) | (n % 10);
    }

    // Returns 1 if we need to add a bit to make parity odd (i.e. if current count is even)
    uint64_t odd_parity(uint64_t d, int from, int to_including) {
        int result = 0;
        for (int bit = from; bit <= to_including; ++bit) {
            if (d & (1ULL << bit)) result++;
        }
        return (result & 0x1) == 0;
    }

    void encodeFrame(const struct tm& timeinfo, int today_start_isdst, int tomorrow_start_isdst) {
        // MSF sends the UPCOMING minute.
        // We assume timeinfo is the current time, so we encode the next minute.
        
        struct tm breakdown = timeinfo;
        breakdown.tm_min += 1;
        mktime(&breakdown); // Normalize
        
        aBits_ = 0b1111110; // Bits 53-59 (Marker)
        
        aBits_ |= to_bcd(breakdown.tm_year % 100) << (59 - 24);
        aBits_ |= to_bcd(breakdown.tm_mon + 1) << (59 - 29);
        aBits_ |= to_bcd(breakdown.tm_mday) << (59 - 35);
        aBits_ |= to_bcd(breakdown.tm_wday) << (59 - 38);
        aBits_ |= to_bcd(breakdown.tm_hour) << (59 - 44);
        aBits_ |= to_bcd(breakdown.tm_min) << (59 - 51);
        
        bBits_ = 0;
        // DUT1 (1-16) - 0
        // Summer time warning (53) - 0
        
        bBits_ |= odd_parity(aBits_, 59 - 24, 59 - 17) << (59 - 54); // Year parity
        bBits_ |= odd_parity(aBits_, 59 - 35, 59 - 25) << (59 - 55); // Day parity
        bBits_ |= odd_parity(aBits_, 59 - 38, 59 - 36) << (59 - 56); // Weekday parity
        bBits_ |= odd_parity(aBits_, 59 - 51, 59 - 39) << (59 - 57); // Time parity
        
        // DST (58)
        if (breakdown.tm_isdst) {
            bBits_ |= 1ULL << (59 - 58);
        }
    }
};

#endif // MSF_SIGNAL_H

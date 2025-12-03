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
        int mday = timeinfo.tm_mday;
        int wday = timeinfo.tm_wday; // 0=Sun
        int month = timeinfo.tm_mon + 1;
        
        // MSF Format
        // 0: Minute Marker (MARK)
        // 1-16: DUT1 (0) - We assume 0 for now
        // 17-24: Year (BCD) - MSB first? No, txtempus shifts (59-24).
        // Let's re-verify txtempus bit order.
        // txtempus: a_bits_ |= to_bcd(year % 100) << (59 - 24);
        // If second=17, check bit 59-17=42.
        // (59-24) = 35.
        // So year bits are at 35..42.
        // Bit 42 corresponds to second 17.
        // Bit 35 corresponds to second 24.
        // So second 17 is MSB (80), second 24 is LSB (1).
        // Wait, to_bcd returns 8 bits.
        // 80 40 20 10 8 4 2 1
        // So 17=80, 18=40, 19=20, 20=10, 21=8, 22=4, 23=2, 24=1.
        
        if (second == 0) return SignalBit_T::MARK;
        
        bool a_bit = false;
        bool b_bit = false;
        
        int year_short = year % 100;

        // A Bits (Data)
        switch(second) {
            case 1 ... 16: a_bit = false; break; // DUT1
            case 17: a_bit = ((year_short / 10) & 8); break; // 80
            case 18: a_bit = ((year_short / 10) & 4); break; // 40
            case 19: a_bit = ((year_short / 10) & 2); break; // 20
            case 20: a_bit = ((year_short / 10) & 1); break; // 10
            case 21: a_bit = ((year_short % 10) & 8); break; // 8
            case 22: a_bit = ((year_short % 10) & 4); break; // 4
            case 23: a_bit = ((year_short % 10) & 2); break; // 2
            case 24: a_bit = ((year_short % 10) & 1); break; // 1
            
            case 25: a_bit = ((month / 10) & 1); break; // 10
            case 26: a_bit = ((month % 10) & 8); break; // 8
            case 27: a_bit = ((month % 10) & 4); break; // 4
            case 28: a_bit = ((month % 10) & 2); break; // 2
            case 29: a_bit = ((month % 10) & 1); break; // 1
            
            case 30: a_bit = ((mday / 10) & 2); break; // 20
            case 31: a_bit = ((mday / 10) & 1); break; // 10
            case 32: a_bit = ((mday % 10) & 8); break; // 8
            case 33: a_bit = ((mday % 10) & 4); break; // 4
            case 34: a_bit = ((mday % 10) & 2); break; // 2
            case 35: a_bit = ((mday % 10) & 1); break; // 1
            
            case 36: a_bit = (wday & 4); break; // 4
            case 37: a_bit = (wday & 2); break; // 2
            case 38: a_bit = (wday & 1); break; // 1
            
            case 39: a_bit = ((hour / 10) & 2); break; // 20
            case 40: a_bit = ((hour / 10) & 1); break; // 10
            case 41: a_bit = ((hour % 10) & 8); break; // 8
            case 42: a_bit = ((hour % 10) & 4); break; // 4
            case 43: a_bit = ((hour % 10) & 2); break; // 2
            case 44: a_bit = ((hour % 10) & 1); break; // 1
            
            case 45: a_bit = ((minute / 10) & 4); break; // 40
            case 46: a_bit = ((minute / 10) & 2); break; // 20
            case 47: a_bit = ((minute / 10) & 1); break; // 10
            case 48: a_bit = ((minute % 10) & 8); break; // 8
            case 49: a_bit = ((minute % 10) & 4); break; // 4
            case 50: a_bit = ((minute % 10) & 2); break; // 2
            case 51: a_bit = ((minute % 10) & 1); break; // 1
            
            case 52 ... 59: a_bit = true; break; // Marker bits (01111110) - 52 is 0?
            // txtempus: a_bits_ = 0b1111110; (Last bits)
            // 59-52 = 7. 1<<7 is 128.
            // 0b1111110 is 126.
            // So bit 7 is 0?
            // 59-52=7.
            // 59-53=6.
            // ...
            // 59-59=0.
            // 0b1111110:
            // Bit 6=1, 5=1, 4=1, 3=1, 2=1, 1=1, 0=0.
            // So 53..58 are 1. 52 is 0? No, bit 7 is not set.
            // So 52 is 0. 59 is 0.
            // Let's check standard.
            // 52-59: 01111110.
            // 52=0, 53=1, 54=1, 55=1, 56=1, 57=1, 58=1, 59=0.
        }
        
        if (second == 52) a_bit = false;
        else if (second >= 53 && second <= 58) a_bit = true;
        else if (second == 59) a_bit = false;

        // B Bits (Parity/DST)
        // 54: Year Parity (17-24)
        // 55: Day Parity (Month + Day: 25-35)
        // 56: Day of Week Parity (36-38)
        // 57: Time Parity (Hour + Minute: 39-51)
        // 58: DST (1 if DST)
        
        if (second == 54) b_bit = (countSetBits(year_short) % 2) == 0;
        else if (second == 55) b_bit = ((countSetBits(month) + countSetBits(mday)) % 2) == 0;
        else if (second == 56) b_bit = (countSetBits(wday) % 2) == 0;
        else if (second == 57) b_bit = ((countSetBits(hour) + countSetBits(minute)) % 2) == 0;
        else if (second == 58) b_bit = timeinfo.tm_isdst;
        
        // Encode SignalBit_T
        if (!a_bit && !b_bit) return SignalBit_T::ZERO; // 00
        if (a_bit && !b_bit) return SignalBit_T::ONE;   // 10
        if (!a_bit && b_bit) return SignalBit_T::MSF_01;// 01
        if (a_bit && b_bit) return SignalBit_T::MSF_11; // 11
        
        return SignalBit_T::ZERO;
    }

    bool getSignalLevel(SignalBit_T bit, int millis) override {
        // MSF
        // 00: 100ms Low
        // 10: 200ms Low
        // 01: 100ms Low, 100ms High, 100ms Low (Total 300ms? No)
        // txtempus:
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
    int countSetBits(int val) {
        int cnt = 0;
        // Units
        int units = val % 10;
        if (units & 1) cnt++;
        if (units & 2) cnt++;
        if (units & 4) cnt++;
        if (units & 8) cnt++;
        // Tens
        int tens = val / 10;
        if (tens & 1) cnt++;
        if (tens & 2) cnt++;
        if (tens & 4) cnt++;
        if (tens & 8) cnt++; // Just in case
        
        return cnt;
    }
};

#endif // MSF_SIGNAL_H

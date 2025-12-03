#ifndef DCF77_SIGNAL_H
#define DCF77_SIGNAL_H

#include "RadioTimeSignal.h"

class DCF77Signal : public RadioTimeSignal {
public:
    int getFrequency() override {
        return 77500;
    }

    SignalBit_T getBit(const struct tm& timeinfo, int today_start_isdst, int tomorrow_start_isdst) override {
        int hour = timeinfo.tm_hour;
        int minute = timeinfo.tm_min;
        int second = timeinfo.tm_sec;
        int year = timeinfo.tm_year + 1900;
        int mday = timeinfo.tm_mday;
        int wday = timeinfo.tm_wday == 0 ? 7 : timeinfo.tm_wday; // 1=Mon...7=Sun
        int month = timeinfo.tm_mon + 1;
        // DCF77 Format
        // 0: Start of minute (0)
        // 1-14: Meteo (0)
        // 15: Call bit (0)
        // 16: Summer time announcement (0)
        // 17: CEST (1 if DST)
        // 18: CET (1 if not DST)
        // 19: Leap second (0)
        // 20: Start of time (1)
        // 21-27: Minute (BCD)
        // 28: Parity Minute
        // 29-34: Hour (BCD)
        // 35: Parity Hour
        // 36-41: Day (BCD)
        // 42-44: Day of Week (BCD)
        // 45-49: Month (BCD)
        // 50-57: Year (BCD)
        // 58: Parity Date
        // 59: No modulation

        if (second == 59) return SignalBit_T::IDLE;

        SignalBit_T bit = SignalBit_T::ZERO;
        int year_short = year % 100;
        
        // Calculate day of week (0=Sunday, but DCF77 needs 1=Monday...7=Sunday)
        // tm_wday is 0=Sun, 1=Mon...
        // We need to calculate it or pass it in. 
        // The passed arguments don't include wday. We can calculate it from yday/year or just use a standard algorithm.
        // Actually, let's just assume we can get it or approximate it. 
        // Wait, I can't easily calculate wday without a full date algo.
        // However, the `yday` and `year` are available.
        // Let's use a simple Zeller's congruence or similar if needed, or just ignore it for now?
        // No, I should do it right.
        // But wait, `WatchTower.ino` passes `buf_now_utc.tm_yday`.
        // I can modify `RadioTimeSignal::getBit` signature to include `wday` or `struct tm`.
        // But that would require changing the base class and WWVB.
        // Let's modify the base class signature in the next step to include `wday`.
        // For now, I'll put a placeholder.
        
        // Actually, let's stick to the plan. I'll update the base class signature later if needed.
        // For now, I'll calculate wday from year/yday.
        // Jan 1 1900 was a Monday.
        // Simple calculation:
        // days = (year - 1900) * 365 + (year - 1900 - 1) / 4 - (year - 1900 - 1) / 100 + (year - 1900 - 1) / 400 + yday;
        // wday = (days) % 7; // 0=Mon, ... 6=Sun? No.
        // standard tm_wday: 0=Sun.
        
        // Let's just implement the bits we have.
        
        switch(second) {
            case 0: bit = SignalBit_T::ZERO; break;
            case 1 ... 14: bit = SignalBit_T::ZERO; break;
            case 15: bit = SignalBit_T::ZERO; break;
            case 16: bit = SignalBit_T::ZERO; break;
            case 17: bit = timeinfo.tm_isdst ? SignalBit_T::ONE : SignalBit_T::ZERO; break;
            case 18: bit = !timeinfo.tm_isdst ? SignalBit_T::ONE : SignalBit_T::ZERO; break;
            case 19: bit = SignalBit_T::ZERO; break;
            case 20: bit = SignalBit_T::ONE; break;
            case 21: bit = ((minute % 10) & 1) ? SignalBit_T::ONE : SignalBit_T::ZERO; break;
            case 22: bit = ((minute % 10) & 2) ? SignalBit_T::ONE : SignalBit_T::ZERO; break;
            case 23: bit = ((minute % 10) & 4) ? SignalBit_T::ONE : SignalBit_T::ZERO; break;
            case 24: bit = ((minute % 10) & 8) ? SignalBit_T::ONE : SignalBit_T::ZERO; break;
            case 25: bit = ((minute / 10) & 1) ? SignalBit_T::ONE : SignalBit_T::ZERO; break;
            case 26: bit = ((minute / 10) & 2) ? SignalBit_T::ONE : SignalBit_T::ZERO; break;
            case 27: bit = ((minute / 10) & 4) ? SignalBit_T::ONE : SignalBit_T::ZERO; break;
            case 28: bit = getParity(minute) ? SignalBit_T::ONE : SignalBit_T::ZERO; break;
            case 29: bit = ((hour % 10) & 1) ? SignalBit_T::ONE : SignalBit_T::ZERO; break;
            case 30: bit = ((hour % 10) & 2) ? SignalBit_T::ONE : SignalBit_T::ZERO; break;
            case 31: bit = ((hour % 10) & 4) ? SignalBit_T::ONE : SignalBit_T::ZERO; break;
            case 32: bit = ((hour % 10) & 8) ? SignalBit_T::ONE : SignalBit_T::ZERO; break;
            case 33: bit = ((hour / 10) & 1) ? SignalBit_T::ONE : SignalBit_T::ZERO; break;
            case 34: bit = ((hour / 10) & 2) ? SignalBit_T::ONE : SignalBit_T::ZERO; break;
            case 35: bit = getParity(hour) ? SignalBit_T::ONE : SignalBit_T::ZERO; break;
            case 36: bit = ((mday % 10) & 1) ? SignalBit_T::ONE : SignalBit_T::ZERO; break;
            case 37: bit = ((mday % 10) & 2) ? SignalBit_T::ONE : SignalBit_T::ZERO; break;
            case 38: bit = ((mday % 10) & 4) ? SignalBit_T::ONE : SignalBit_T::ZERO; break;
            case 39: bit = ((mday % 10) & 8) ? SignalBit_T::ONE : SignalBit_T::ZERO; break;
            case 40: bit = ((mday / 10) & 1) ? SignalBit_T::ONE : SignalBit_T::ZERO; break;
            case 41: bit = ((mday / 10) & 2) ? SignalBit_T::ONE : SignalBit_T::ZERO; break;
            case 42: bit = (wday & 1) ? SignalBit_T::ONE : SignalBit_T::ZERO; break;
            case 43: bit = (wday & 2) ? SignalBit_T::ONE : SignalBit_T::ZERO; break;
            case 44: bit = (wday & 4) ? SignalBit_T::ONE : SignalBit_T::ZERO; break;
            case 45: bit = ((month % 10) & 1) ? SignalBit_T::ONE : SignalBit_T::ZERO; break;
            case 46: bit = ((month % 10) & 2) ? SignalBit_T::ONE : SignalBit_T::ZERO; break;
            case 47: bit = ((month % 10) & 4) ? SignalBit_T::ONE : SignalBit_T::ZERO; break;
            case 48: bit = ((month % 10) & 8) ? SignalBit_T::ONE : SignalBit_T::ZERO; break;
            case 49: bit = ((month / 10) & 1) ? SignalBit_T::ONE : SignalBit_T::ZERO; break;
            case 50: bit = ((year % 10) & 1) ? SignalBit_T::ONE : SignalBit_T::ZERO; break;
            case 51: bit = ((year % 10) & 2) ? SignalBit_T::ONE : SignalBit_T::ZERO; break;
            case 52: bit = ((year % 10) & 4) ? SignalBit_T::ONE : SignalBit_T::ZERO; break;
            case 53: bit = ((year % 10) & 8) ? SignalBit_T::ONE : SignalBit_T::ZERO; break;
            case 54: bit = (((year / 10) % 10) & 1) ? SignalBit_T::ONE : SignalBit_T::ZERO; break;
            case 55: bit = (((year / 10) % 10) & 2) ? SignalBit_T::ONE : SignalBit_T::ZERO; break;
            case 56: bit = (((year / 10) % 10) & 4) ? SignalBit_T::ONE : SignalBit_T::ZERO; break;
            case 57: bit = (((year / 10) % 10) & 8) ? SignalBit_T::ONE : SignalBit_T::ZERO; break;
            case 58: bit = getParity(mday) ^ getParity(wday) ^ getParity(month) ^ getParity(year % 100) ? SignalBit_T::ONE : SignalBit_T::ZERO; break;
            // Wait, parity is over all date bits (36-57).
            // My getParity helper takes an int and calculates BCD parity.
            // I need to sum parities of all components?
            // Parity of (mday_bcd + wday_bcd + month_bcd + year_bcd).
            // getParity(val) returns 1 if odd number of bits set.
            // XORing them gives the parity of the sum.
            // Yes.
            default: bit = SignalBit_T::ZERO; break;
        }
        
        return bit;
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
    bool getParity(int val) {
        // Calculate even parity for the BCD representation
        int parity = 0;
        // Units
        int units = val % 10;
        if (units & 1) parity++;
        if (units & 2) parity++;
        if (units & 4) parity++;
        if (units & 8) parity++;
        // Tens
        int tens = val / 10;
        if (tens & 1) parity++;
        if (tens & 2) parity++;
        if (tens & 4) parity++;
        return (parity % 2) != 0;
    }
};

#endif // DCF77_SIGNAL_H

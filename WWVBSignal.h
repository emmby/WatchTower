#ifndef WWVB_SIGNAL_H
#define WWVB_SIGNAL_H

#include "RadioTimeSignal.h"

class WWVBSignal : public RadioTimeSignal {
public:
    int getFrequency() override {
        return 60000;
    }

    TimeCodeSymbol getSymbol(const struct tm& timeinfo, int today_start_isdst, int tomorrow_start_isdst) override {
        // WWVB sends the current minute.
        // However, the frame bits are calculated based on the minute.
        // If we are in the middle of a minute, we should use the cached frame bits if possible.
        // But since we don't have caching yet, we just recalculate.
        // The only expensive part is the frame calculation which happens once per minute ideally.
        // But here we do it every second. It's fine for now.
        
        encodeFrame(timeinfo, today_start_isdst, tomorrow_start_isdst);

        int second = timeinfo.tm_sec;
        if (second < 0 || second > 59) return TimeCodeSymbol::ZERO; // Should not happen

        // Check the bit in frameBits_
        // frameBits_ is 60 bits. Bit 59 corresponds to second 0.
        // Bit 0 corresponds to second 59.
        // Wait, let's check the encoding loop.
        // for(int i=0; i<60; i++) ... frameBits_ |= ... << (59-i)
        // So second `i` corresponds to bit `59-i`.
        
        // Marker bits are not in frameBits_?
        // Ah, encodeFrame sets frameBits_ for data bits.
        // But markers are fixed.
        // Let's check markers.
        // 0, 9, 19, 29, 39, 49, 59 are Markers.
        if (second == 0 || second == 9 || second == 19 || second == 29 || second == 39 || second == 49 || second == 59) {
            return TimeCodeSymbol::MARK;
        }
        
        // Check data bit
        bool bitSet = (frameBits_ >> (59 - second)) & 1;
        return bitSet ? TimeCodeSymbol::ONE : TimeCodeSymbol::ZERO;
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
    uint64_t frameBits_ = 0;
    int lastEncodedMinute_ = -1;

    // Helper to encode BCD with a 0 bit inserted between digits (for WWVB)
    // Tens digit occupies 3 bits (bits 7,6,5 of the byte? No, just 3 bits).
    // Units digit occupies 4 bits.
    // Structure: [Tens:3] [0] [Units:4]
    uint64_t to_padded5_bcd(int n) {
        return (((n / 100) % 10) << 10) | (((n / 10) % 10) << 5) | (n % 10);
    }

    bool is_leap_year(int year) {
        return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
    }

    void encodeFrame(const struct tm& timeinfo, int today_start_isdst, int tomorrow_start_isdst) {
        frameBits_ = 0;
        
        // Minute: 01, 02, 03, 05, 06, 07, 08 (Bits 58-51? No, 59-sec)
        // txtempus: time_bits_ |= to_padded5_bcd(breakdown.tm_min) << (59 - 8);
        // Bit 8 is the LSB of the minute.
        // 59 - 8 = 51.
        // So minute bits end at bit 51.
        // Minute takes 8 bits (3 tens + 1 pad + 4 units).
        // So it occupies bits 51..58.
        // Wait, let's trace:
        // sec 1: 59-1 = 58.
        // sec 8: 59-8 = 51.
        // So bits 58..51.
        frameBits_ |= to_padded5_bcd(timeinfo.tm_min) << (59 - 8);

        // Hour: 12, 13, 15, 16, 17, 18 (Bits 59-18)
        // Ends at sec 18.
        // 59 - 18 = 41.
        // Occupies 41..48?
        // Wait, sec 12 is start. 59-12 = 47.
        // to_padded5_bcd(hour) << (59 - 18).
        frameBits_ |= to_padded5_bcd(timeinfo.tm_hour) << (59 - 18);

        // Day of Year: 22, 23, 25, 26, 27, 28, 30, 31, 32, 33 (Bits 59-33)
        // Ends at sec 33.
        // 59 - 33 = 26.
        // 3 digits for Day of Year?
        // yday is 0-365. +1 -> 1-366.
        // Hundreds (2 bits: 200, 100), Tens (4 bits), Units (4 bits).
        // Plus pads?
        // WWVB:
        // 22, 23: Hundreds (2 bits).
        // 24: Blank.
        // 25-28: Tens (4 bits).
        // 29: Mark.
        // 30-33: Units (4 bits).
        // txtempus `to_padded5_bcd` handles 3 digits:
        // `((n / 100) % 10) << 10` -> Hundreds shifted by 10.
        // `((n / 10) % 10) << 5` -> Tens shifted by 5.
        // `n % 10` -> Units.
        // Total 12 bits?
        // Hundreds: bits 10, 11 (2 bits? No, %10 gives up to 9, but yday hundreds is max 3).
        // So bits 0..3 for units.
        // Bit 4 is pad (implicit in shift 5).
        // Bits 5..8 for tens.
        // Bit 9 is pad (implicit in shift 10).
        // Bits 10..13 for hundreds.
        // Total 14 bits?
        // Let's check alignment.
        // Shift (59 - 33) = 26.
        // So LSB is at 26.
        // Sec 33: 59-33 = 26. Correct.
        frameBits_ |= to_padded5_bcd(timeinfo.tm_yday + 1) << (59 - 33);

        // Year: 45, 46, 47, 48, 50, 51, 52, 53 (Bits 59-53)
        // Ends at sec 53.
        // 59 - 53 = 6.
        frameBits_ |= to_padded5_bcd((timeinfo.tm_year + 1900) % 100) << (59 - 53);

        // Leap Year: 55
        // 59 - 55 = 4.
        if (is_leap_year(timeinfo.tm_year + 1900)) {
            frameBits_ |= 1ULL << (59 - 55);
        }

        // DST: 57, 58
        // 57: DST State 1
        // 58: DST State 2
        // 59 - 57 = 2.
        // 59 - 58 = 1.
        
        // Logic from previous implementation:
        // 57:
        // if (today && tomorrow) 1
        // else if (!today && !tomorrow) 0
        // else if (!today && tomorrow) 1 (DST starts)
        // else 0 (DST ends)
        
        // 58:
        // if (today && tomorrow) 1
        // else if (!today && !tomorrow) 0
        // else if (!today && tomorrow) 0 (DST starts)
        // else 1 (DST ends)

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

        if (dst1) frameBits_ |= 1ULL << (59 - 57);
        if (dst2) frameBits_ |= 1ULL << (59 - 58);
    }
};

#endif // WWVB_SIGNAL_H

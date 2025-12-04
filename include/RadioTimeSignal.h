#ifndef RADIO_TIME_SIGNAL_H
#define RADIO_TIME_SIGNAL_H

#include <Arduino.h>

enum class TimeCodeSymbol {
  ZERO = 0,
  ONE = 1,
  MARK = 2,
  IDLE = 3, // Used for DCF77 59th second
  MSF_01 = 4, // MSF: A=0, B=1
  MSF_11 = 5  // MSF: A=1, B=1
};

class RadioTimeSignal {
public:
    virtual ~RadioTimeSignal() {}

    // Returns the carrier frequency in Hz
    virtual int getFrequency() = 0;

    // Encodes the signal for the upcoming minute.
    // This should be called once at the beginning of each minute (second 0)
    // before calling getSymbolForSecond
    virtual void encodeMinute(const struct tm& timeinfo, int today_isdst, int tomorrow_isdst) = 0;

    // Returns the signal symbol for the given second (0-59)
    // for the minute encoded by encodeMinute
    virtual TimeCodeSymbol getSymbolForSecond(int second) = 0;

    // Returns a logical high or low to indicate whether the
    // PWM signal should be high or low based on the current time
    virtual bool getSignalLevel(TimeCodeSymbol symbol, int millis) = 0;

protected:
    // Helper to encode BCD
    uint64_t to_bcd(int n) {
        return (((n / 10) % 10) << 4) | (n % 10);
    }

    // Helper to encode BCD with a 0 bit inserted between digits (for WWVB/JJY)
    // Structure: [Tens:3] [0] [Units:4]
    uint64_t to_padded5_bcd(int n) {
        return (((n / 100) % 10) << 10) | (((n / 10) % 10) << 5) | (n % 10);
    }

    // Reverse 8 bits (for DCF77)
    uint8_t reverse8(uint8_t b) {
        b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
        b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
        b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
        return b;
    }

    bool is_leap_year(int year) {
        return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
    }

    // Count set bits in a 64-bit integer
    int countSetBits(uint64_t n) {
        int count = 0;
        while (n > 0) {
            n &= (n - 1);
            count++;
        }
        return count;
    }

    // Count set bits in a range [from, to_including]
    int countSetBits(uint64_t n, int from, int to_including) {
        int count = 0;
        for (int i = from; i <= to_including; i++) {
            if ((n >> i) & 1) {
                count++;
            }
        }
        return count;
    }
};

#endif // RADIO_TIME_SIGNAL_H

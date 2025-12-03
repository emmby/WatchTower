#ifndef JJY_SIGNAL_H
#define JJY_SIGNAL_H

#include "RadioTimeSignal.h"

class JJYSignal : public RadioTimeSignal {
public:
    int getFrequency() override {
        return 60000; // Can be 40kHz or 60kHz, defaulting to 60kHz
    }

    SignalBit_T getBit(
        int hour,
        int minute,
        int second,
        int yday,
        int year,
        int today_start_isdst,
        int tomorrow_start_isdst
    ) override {
        // JJY Format
        // 0: M (MARK)
        // 1-8: Minute (BCD)
        // 9: P1 (MARK)
        // 10-11: Unused (0)
        // 12-18: Hour (BCD)
        // 19: P2 (MARK)
        // 20-21: Unused (0)
        // 22-30: Day of Year (BCD) - Hundreds, Tens, Units
        // 31-35: Parity (Hour, Minute) + Unused
        // 36-37: Leap Second
        // 38: LS (0)
        // 39: P3 (MARK)
        // 40: SU1 (0)
        // 41-48: Year (BCD)
        // 49: P4 (MARK)
        // 50-52: Day of Week (BCD)
        // 53: LS (0)
        // 54-58: Unused (0)
        // 59: P0 (MARK)

        SignalBit_T bit = SignalBit_T::ZERO;
        
        switch(second) {
            case 0: bit = SignalBit_T::MARK; break;
            case 9: bit = SignalBit_T::MARK; break;
            case 19: bit = SignalBit_T::MARK; break;
            case 29: bit = SignalBit_T::MARK; break; // Wait, P3 is at 39? No, P0-P5.
            // JJY Markers: 0, 9, 19, 29, 39, 49, 59.
            case 39: bit = SignalBit_T::MARK; break;
            case 49: bit = SignalBit_T::MARK; break;
            case 59: bit = SignalBit_T::MARK; break;
            
            // Minute (BCD)
            case 1: bit = ((minute / 10) & 4) ? SignalBit_T::ONE : SignalBit_T::ZERO; break; // 40
            case 2: bit = ((minute / 10) & 2) ? SignalBit_T::ONE : SignalBit_T::ZERO; break; // 20
            case 3: bit = ((minute / 10) & 1) ? SignalBit_T::ONE : SignalBit_T::ZERO; break; // 10
            case 4: bit = SignalBit_T::ZERO; break;
            case 5: bit = ((minute % 10) & 8) ? SignalBit_T::ONE : SignalBit_T::ZERO; break; // 8
            case 6: bit = ((minute % 10) & 4) ? SignalBit_T::ONE : SignalBit_T::ZERO; break; // 4
            case 7: bit = ((minute % 10) & 2) ? SignalBit_T::ONE : SignalBit_T::ZERO; break; // 2
            case 8: bit = ((minute % 10) & 1) ? SignalBit_T::ONE : SignalBit_T::ZERO; break; // 1
            
            // Hour (BCD)
            case 12: bit = ((hour / 10) & 2) ? SignalBit_T::ONE : SignalBit_T::ZERO; break; // 20
            case 13: bit = ((hour / 10) & 1) ? SignalBit_T::ONE : SignalBit_T::ZERO; break; // 10
            case 14: bit = SignalBit_T::ZERO; break;
            case 15: bit = ((hour % 10) & 8) ? SignalBit_T::ONE : SignalBit_T::ZERO; break; // 8
            case 16: bit = ((hour % 10) & 4) ? SignalBit_T::ONE : SignalBit_T::ZERO; break; // 4
            case 17: bit = ((hour % 10) & 2) ? SignalBit_T::ONE : SignalBit_T::ZERO; break; // 2
            case 18: bit = ((hour % 10) & 1) ? SignalBit_T::ONE : SignalBit_T::ZERO; break; // 1
            
            // Day of Year (BCD)
            // ...
            
            default: bit = SignalBit_T::ZERO; break;
        }
        
        return bit;
    }

    bool getSignalLevel(SignalBit_T bit, int millis) override {
        // JJY
        // 0: 0.8s High, 0.2s Low (Pulse width 0.8s) -> Wait, logic is inverted in my head.
        // If "Pulse" means High, then:
        // 0: High 800ms, Low 200ms.
        // 1: High 500ms, Low 500ms.
        // MARK: High 200ms, Low 800ms.
        
        // But my getSignalLevel returns true for High (Carrier On).
        // So:
        if (bit == SignalBit_T::ZERO) {
            return millis < 800;
        } else if (bit == SignalBit_T::ONE) {
            return millis < 500;
        } else { // MARK
            return millis < 200;
        }
    }
};

#endif // JJY_SIGNAL_H

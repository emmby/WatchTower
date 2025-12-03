#include <Arduino.h>
#include <unity.h>
#include <ctime>
#include <vector>
#include <string>

// Include our signal classes
#include "../../RadioTimeSignal.h"
#include "../../WWVBSignal.h"
#include "../../DCF77Signal.h"
#include "../../MSFSignal.h"
#include "../../JJYSignal.h"

// Include txtempus headers
#undef HIGH
#undef LOW
#include "time-signal-source.h"

// Helper to convert txtempus modulation to boolean level at a specific millisecond
bool getTxtempusLevel(const TimeSignalSource::SecondModulation& mod, int millis) {
    int current_ms = 0;
    for (const auto& m : mod) {
        int duration = m.duration_ms;
        if (duration == 0) duration = 1000 - current_ms; // Auto-fill
        
        if (millis >= current_ms && millis < current_ms + duration) {
            return m.power == CarrierPower::HIGH;
        }
        current_ms += duration;
    }
    return false; // Should not happen if millis < 1000
}

struct TestCase {
    std::string name;
    int year;
    int month;
    int day;
    int hour;
    int min;
};

std::vector<TestCase> test_cases = {
    {"DST (Summer)", 2025, 7, 15, 12, 0},
    {"STD (Winter)", 2025, 12, 25, 12, 0},
    {"Pre-DST Transition (Spring)", 2025, 3, 9, 1, 59}, // US 2025: Mar 9
    {"Pre-STD Transition (Fall)", 2025, 11, 2, 1, 59},   // US 2025: Nov 2
    {"Leap Year (Feb 29)", 2024, 2, 29, 12, 0},
    {"Year Boundary (Dec 31)", 2025, 12, 31, 23, 59},
    {"EU DST Spring (Mar 30)", 2025, 3, 30, 1, 59}, // EU 2025: Mar 30
    {"EU DST Fall (Oct 26)", 2025, 10, 26, 2, 59},   // EU 2025: Oct 26
    {"Century Leap Year (2000)", 2000, 2, 29, 12, 0},
    {"Non-Leap Century (2100)", 2100, 2, 28, 12, 0},
    {"Stress Test (Max Bits)", 2099, 12, 31, 23, 59}
};

// Helper to run comparison for a specific signal and timezone
template <typename MySignalT, typename RefSignalT>
void run_comparison(const char* timezone, bool input_is_utc, bool add_minute, const std::vector<int>& skip_bits = {}) {
    setenv("TZ", timezone, 1);
    tzset();

    MySignalT mySignal;
    RefSignalT refSignal;

    for (const auto& tc : test_cases) {
        // Construct UTC time_t for the test case
        struct tm tm_utc = {0};
        tm_utc.tm_year = tc.year - 1900;
        tm_utc.tm_mon = tc.month - 1;
        tm_utc.tm_mday = tc.day;
        tm_utc.tm_hour = tc.hour;
        tm_utc.tm_min = tc.min;
        tm_utc.tm_sec = 0;
        tm_utc.tm_isdst = 0; // UTC has no DST
        
        time_t t_utc = timegm(&tm_utc);
        
        // Prepare reference (txtempus)
        refSignal.PrepareMinute(t_utc);

        // Prepare my signal inputs
        struct tm tm_input;
        if (input_is_utc) {
            tm_input = tm_utc;
        } else {
            // Convert UTC to Local for the signal input
            localtime_r(&t_utc, &tm_input);
        }

        // Calculate DST flags for WWVB (today/tomorrow)
        struct tm tm_local_now;
        localtime_r(&t_utc, &tm_local_now);
        
        struct tm tm_today_start = tm_local_now;
        tm_today_start.tm_hour = 0;
        tm_today_start.tm_min = 0;
        tm_today_start.tm_sec = 0;
        time_t t_today_start = mktime(&tm_today_start); // mktime normalizes and sets isdst
        
        struct tm tm_tomorrow_start = tm_today_start;
        tm_tomorrow_start.tm_mday += 1;
        mktime(&tm_tomorrow_start); // Normalize

        time_t t_target = t_utc + (add_minute ? 60 : 0);
        struct tm tm_target;
        if (input_is_utc) {
            gmtime_r(&t_target, &tm_target);
        } else {
            localtime_r(&t_target, &tm_target);
        }

        for (int sec = 0; sec < 60; ++sec) {
            // Skip bits if requested
            bool skip = false;
            for (int s : skip_bits) {
                if (s == sec) {
                    skip = true;
                    break;
                }
            }
            if (skip) continue;

            auto mod = refSignal.GetModulationForSecond(sec);
            
            struct tm t_sec = tm_target;
            t_sec.tm_sec = sec; // Update second
            
            SignalBit_T myBit = mySignal.getBit(
                t_sec, 
                tm_today_start.tm_isdst, 
                tm_tomorrow_start.tm_isdst
            );

            // Check sample points
            int check_points[] = {50, 150, 250, 550, 850};
            for (int ms : check_points) {
                bool myLevel = mySignal.getSignalLevel(myBit, ms);
                bool refLevel = getTxtempusLevel(mod, ms);
                
                if (myLevel != refLevel) {
                    printf("FAIL: %s %s Sec %d MS %d. MyBit: %d. Time: %02d:%02d:%02d YDay: %d DST: %d\n", 
                        tc.name.c_str(), timezone, sec, ms, (int)myBit,
                        t_sec.tm_hour, t_sec.tm_min, t_sec.tm_sec, t_sec.tm_yday, t_sec.tm_isdst);
                }

                char msg[128];
                snprintf(msg, sizeof(msg), "%s: %s mismatch at sec %d, ms %d (Bit: %d)", 
                    tc.name.c_str(), timezone, sec, ms, (int)myBit);
                TEST_ASSERT_EQUAL_MESSAGE(refLevel, myLevel, msg);
            }
        }
    }
}

void test_wwvb_compare(void) {
    // WWVB: UTC time, US DST rules
    // Skip DST bits (57, 58) as txtempus seems to not implement DST logic (or uses UTC)
    std::vector<int> skip;
    skip.push_back(57);
    skip.push_back(58);
    run_comparison<WWVBSignal, WWVBTimeSignalSource>("PST8PDT,M3.2.0,M11.1.0", true, false, skip);
}

void test_dcf77_compare(void) {
    // DCF77: CET/CEST, Local time input
    // We pass false for add_minute because DCF77Signal now handles the +60s internally
    run_comparison<DCF77Signal, DCF77TimeSignalSource>("CET-1CEST,M3.5.0,M10.5.0/3", false, false);
}

void test_jjy_compare(void) {
    // JJY: JST, Local time input (no DST)
    // Skip Day of Year bits (22-34), Year bits (41-48), and Day of Week bits (50-52) as txtempus doesn't seem to implement them
    std::vector<int> skip;
    for(int i=22; i<=34; ++i) skip.push_back(i);
    for(int i=41; i<=48; ++i) skip.push_back(i);
    for(int i=50; i<=52; ++i) skip.push_back(i);
    run_comparison<JJYSignal, JJY60TimeSignalSource>("JST-9", false, false, skip);
}

void test_msf_compare(void) {
    // MSF: UK Time (GMT/BST), Local time input
    // Skip DUT1 bits (1-16) as txtempus sets them to 0
    std::vector<int> skip;
    for(int i=1; i<=16; ++i) skip.push_back(i);
    // We pass false for add_minute because MSFSignal now handles the +60s internally
    run_comparison<MSFSignal, MSFTimeSignalSource>("GMT0BST,M3.5.0/1,M10.5.0", false, false, skip);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_wwvb_compare);
    RUN_TEST(test_dcf77_compare);
    RUN_TEST(test_jjy_compare);
    RUN_TEST(test_msf_compare);
    UNITY_END();
    return 0;
}

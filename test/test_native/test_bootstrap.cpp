#include <Arduino.h>
#include <unity.h>
#include <string>
#include <vector>
#include <sys/time.h>
#include <cstdarg>

// Mocks
#include "WiFiManager.h"
#include "Adafruit_NeoPixel.h"
#include "ESPUI.h"
#include "WiFiUdp.h"
#include "ArduinoMDNS.h"
#include "esp_sntp.h"
#include "WiFi.h"
#include "Esp.h"
#include "SPI.h"

// Globals for mocks
WiFiClass WiFi;
ESPUIClass ESPUI;
EspClass ESP;
SPIClass SPI;

// Core functions
void delay(unsigned long ms) {}
unsigned long millis() { return 0; }
void pinMode(uint8_t pin, uint8_t mode) {}
void digitalWrite(uint8_t pin, uint8_t val) {}

// Mock gettimeofday
struct timeval mock_tv = {0, 0};
int mock_gettimeofday(struct timeval *tv, void *tz) {
    if (tv) *tv = mock_tv;
    return 0;
}
#define gettimeofday mock_gettimeofday

int mock_settimeofday(const struct timeval *tv, const struct timezone *tz) {
    if (tv) mock_tv = *tv;
    return 0;
}
#define settimeofday mock_settimeofday

// Mock Serial to support printf
class SerialMock {
public:
    std::string output;
    void begin(unsigned long) {}
    void println(const char* s) { output += s; output += "\n"; }
    void println(String s) {}
    void println(int i) { output += std::to_string(i); output += "\n"; }
    void println() { output += "\n"; }
    void print(const char* s) { output += s; }
    void print(int i) { output += std::to_string(i); }
    void printf(const char * format, ...) {
        char buffer[256];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        output += buffer;
    }
};
SerialMock MySerial;
#define Serial MySerial

// ESP32 specific mocks
// Global to track last ledc frequency
int last_ledc_freq = 0;
void ledcAttach(uint8_t pin, double freq, uint8_t resolution) {
    last_ledc_freq = (int)freq;
}
void ledcDetach(uint8_t pin) {}
void ledcWrite(uint8_t pin, uint32_t duty) {}
void configTzTime(const char* tz, const char* server1, const char* server2 = nullptr, const char* server3 = nullptr) {}
bool getLocalTime(struct tm* info, uint32_t ms = 5000) {
    time_t now;
    time(&now);
    localtime_r(&now, info);
    return true;
}
void sntp_set_time_sync_notification_cb(sntp_sync_time_cb_t callback) {}
void esp_sntp_stop() {}

// Forward declarations for functions in WatchTower.ino that are used before definition
// (None needed as they are in WatchTower.ino now)

// Rename timezone to avoid conflict with system symbol
#define timezone my_timezone

// Include the sketch
// We need to trick the compiler into treating .ino as .cpp and handling the setup/loop
#include "../../WatchTower.ino"

void setUp(void) {
    MySerial.output = "";
    WiFiManager::calledAutoConnect = false;
    memset(WiFiManager::lastSSID, 0, sizeof(WiFiManager::lastSSID));
    // Set TZ to UTC for predictable testing
    setenv("TZ", "UTC", 1);
    tzset();
}

void tearDown(void) {
}

void test_setup_completes(void) {
    // Arrange
    // (Mocks are already set up to do nothing or return defaults)

    // Act
    setup();

    // Assert
    // Verify that setup completed without crashing
    // We can also verify that specific functions were called if we used strict mocks,
    // but for now we just want to ensure it boots.
    TEST_ASSERT_TRUE(true);
}

void test_wifi_connection_attempt(void) {
    // Arrange
    // Act
    setup();

    // Assert
    TEST_ASSERT_TRUE(WiFiManager::calledAutoConnect);
    TEST_ASSERT_EQUAL_STRING("WatchTower", WiFiManager::lastSSID);
}

void test_serial_date_output(void) {
    // Arrange
    // Set time to 2025-12-25 12:00:01 UTC (Second 1 is usually ZERO, which is High at 500ms for all signals)
    // 1766664001
    mock_tv.tv_sec = 1766664001; 
    mock_tv.tv_usec = 500000; // 500ms
    // ZERO: low 200ms, high 800ms. So at 900ms (0.9s), it is HIGH (Wait, "low 200ms, high 800ms" usually means low FOR 200ms, then high FOR 800ms? Or low UNTIL 200ms?)
    // WatchTower.ino:
    // if (bit == WWVB_T::ZERO) return millis >= 200;
    // So if millis (0-999) >= 200, it returns true (High).
    // So 900 >= 200 is True.
    // So logicValue will be 1.
    // prevLogicValue init is 0.
    // So it should print.
    
    // Act
    setup(); // Initialize
    loop();  // Run loop once

    // Assert
    // Expected output format: "Thursday, December 25 2025 12:00:00.900 +0000 UTC [last sync Pending...]: 1"
    // Note: The exact string depends on strftime implementation and locale, but usually standard C is consistent.
    // Also "Pending..." because lastSync is not set in this test path (unless setup sets it? No, callback does).
    
    // We'll check for the date part to be safe
    TEST_ASSERT_NOT_NULL(strstr(MySerial.output.c_str(), "December 25 2025"));
}

// Shared valid timeinfo for tests (2000-01-01 12:00:00)
const struct tm kValidTimeInfo = []{
    struct tm t = {0};
    t.tm_year = 100; // 2000
    t.tm_mon = 0;    // Jan
    t.tm_mday = 1;   // 1st
    t.tm_hour = 12;
    return t;
}();

void test_wwvb_logic_signal(void) {
    WWVBSignal wwvb;
    struct tm timeinfo = kValidTimeInfo;
    wwvb.encodeMinute(timeinfo, 0, 0);
    
    // Test MARK (0s)
    TimeCodeSymbol bit = wwvb.getSymbolForSecond(0);
    TEST_ASSERT_EQUAL(TimeCodeSymbol::MARK, bit);
    
    TEST_ASSERT_FALSE(wwvb.getSignalLevel(bit, 0));
    TEST_ASSERT_FALSE(wwvb.getSignalLevel(bit, 799));
    TEST_ASSERT_TRUE(wwvb.getSignalLevel(bit, 800));

    // Test ZERO
    // timeinfo.tm_sec = 1; // Not needed for encodeMinute unless we re-configure
    bit = wwvb.getSymbolForSecond(1);
    TEST_ASSERT_EQUAL(TimeCodeSymbol::ZERO, bit);
    TEST_ASSERT_FALSE(wwvb.getSignalLevel(bit, 0));
    TEST_ASSERT_FALSE(wwvb.getSignalLevel(bit, 199));
    TEST_ASSERT_TRUE(wwvb.getSignalLevel(bit, 200));

    // Test ONE
    // We need to find a second that is 1.
    // Bit 58 is DST. If DST=1, then bit is 1.
    // Re-configure for DST
    timeinfo.tm_sec = 0; // Reset sec just in case
    wwvb.encodeMinute(timeinfo, 1, 1); // DST on
    
    bit = wwvb.getSymbolForSecond(58);
    // DST bit 58 is set if dst is on.
    TEST_ASSERT_EQUAL(TimeCodeSymbol::ONE, bit);
    TEST_ASSERT_FALSE(wwvb.getSignalLevel(bit, 0));
    TEST_ASSERT_FALSE(wwvb.getSignalLevel(bit, 499));
    TEST_ASSERT_TRUE(wwvb.getSignalLevel(bit, 500));
}

void test_wwvb_frame_encoding(void) {
    WWVBSignal wwvb;
    // Test a specific known date/time
    // Mar 6 2008 07:30:00 UTC
    struct tm timeinfo = {0};
    timeinfo.tm_year = 2008 - 1900;
    timeinfo.tm_mon = 2; // March
    timeinfo.tm_mday = 6;
    timeinfo.tm_hour = 7;
    timeinfo.tm_min = 30;
    timeinfo.tm_sec = 0;
    timeinfo.tm_yday = 65; // Day 66 (0-indexed 65)
    
    wwvb.encodeMinute(timeinfo, 0, 0);
    
    // Day of Year 66.
    // Hundreds: 0. Tens: 6. Units: 6.
    // Sec 26 (Tens 40): 1.
    // Sec 27 (Tens 20): 1.
    // Sec 31 (Units 4): 1.
    // Sec 32 (Units 2): 1.
    
    TEST_ASSERT_EQUAL(TimeCodeSymbol::ONE, wwvb.getSymbolForSecond(26));
    TEST_ASSERT_EQUAL(TimeCodeSymbol::ONE, wwvb.getSymbolForSecond(27));
    TEST_ASSERT_EQUAL(TimeCodeSymbol::ONE, wwvb.getSymbolForSecond(31));
    TEST_ASSERT_EQUAL(TimeCodeSymbol::ONE, wwvb.getSymbolForSecond(32));
    
    // Check a ZERO bit (e.g. Sec 22 - Hundreds 200)
    TEST_ASSERT_EQUAL(TimeCodeSymbol::ZERO, wwvb.getSymbolForSecond(22));
}

void test_dcf77_signal(void) {
    DCF77Signal dcf77;
    struct tm timeinfo = kValidTimeInfo;
    dcf77.encodeMinute(timeinfo, 0, 0);
    
    // Test IDLE (59th second)
    TEST_ASSERT_EQUAL(TimeCodeSymbol::IDLE, dcf77.getSymbolForSecond(59));
    TEST_ASSERT_TRUE(dcf77.getSignalLevel(TimeCodeSymbol::IDLE, 0));
    
    // Test ZERO
    TEST_ASSERT_EQUAL(TimeCodeSymbol::ZERO, dcf77.getSymbolForSecond(0));
    TEST_ASSERT_FALSE(dcf77.getSignalLevel(TimeCodeSymbol::ZERO, 0));
    TEST_ASSERT_TRUE(dcf77.getSignalLevel(TimeCodeSymbol::ZERO, 100));
    
    // Test ONE (Bit 20 is always 1)
    TEST_ASSERT_EQUAL(TimeCodeSymbol::ONE, dcf77.getSymbolForSecond(20));
    TEST_ASSERT_FALSE(dcf77.getSignalLevel(TimeCodeSymbol::ONE, 0));
    TEST_ASSERT_TRUE(dcf77.getSignalLevel(TimeCodeSymbol::ONE, 200));
}

void test_jjy_signal(void) {
    JJYSignal jjy;
    struct tm timeinfo = kValidTimeInfo;
    jjy.encodeMinute(timeinfo, 0, 0);
    
    // Test MARK (0s)
    TEST_ASSERT_EQUAL(TimeCodeSymbol::MARK, jjy.getSymbolForSecond(0));
    TEST_ASSERT_TRUE(jjy.getSignalLevel(TimeCodeSymbol::MARK, 0));
    TEST_ASSERT_FALSE(jjy.getSignalLevel(TimeCodeSymbol::MARK, 200));
    
    // Test ZERO
    TEST_ASSERT_EQUAL(TimeCodeSymbol::ZERO, jjy.getSymbolForSecond(1));
    TEST_ASSERT_TRUE(jjy.getSignalLevel(TimeCodeSymbol::ZERO, 0));
    TEST_ASSERT_FALSE(jjy.getSignalLevel(TimeCodeSymbol::ZERO, 800));
    
    // Test ONE (Parity usually 1 if 0s?)
    // Hard to force a 1 without setting time.
    // Let's set minute to 1 (0000001).
    // Minute bits: 0-7 (Sec 1-8).
    // Sec 8 (Bit 0): 1.
    timeinfo.tm_min = 1;
    jjy.encodeMinute(timeinfo, 0, 0);
    
    TEST_ASSERT_EQUAL(TimeCodeSymbol::ONE, jjy.getSymbolForSecond(8));
    TEST_ASSERT_TRUE(jjy.getSignalLevel(TimeCodeSymbol::ONE, 0));
    TEST_ASSERT_FALSE(jjy.getSignalLevel(TimeCodeSymbol::ONE, 500));
}

void test_msf_signal(void) {
    MSFSignal msf;
    struct tm timeinfo = kValidTimeInfo;
    msf.encodeMinute(timeinfo, 0, 0);
    
    // Test MARK (0s)
    TEST_ASSERT_EQUAL(TimeCodeSymbol::MARK, msf.getSymbolForSecond(0));
    TEST_ASSERT_FALSE(msf.getSignalLevel(TimeCodeSymbol::MARK, 0));
    TEST_ASSERT_TRUE(msf.getSignalLevel(TimeCodeSymbol::MARK, 500));
    
    // Test Default (second 1) -> ZERO (Placeholder implementation)
    TimeCodeSymbol bit = msf.getSymbolForSecond(1);
    TEST_ASSERT_EQUAL(TimeCodeSymbol::ZERO, bit);
    // ZERO: 100ms Low, 900ms High
    TEST_ASSERT_FALSE(msf.getSignalLevel(bit, 99));
    TEST_ASSERT_TRUE(msf.getSignalLevel(bit, 100));
}

// Access to globals from WatchTower.ino
extern RadioTimeSignal* signalGenerator;
extern void updateSignalCallback(Control *sender, int value);
extern uint16_t ui_signal_select;

// last_ledc_freq is defined globally now

void test_signal_switching(void) {
    // Arrange
    setup(); // Ensure UI is created
    
    // Act - Select DCF77
    Control sender;
    sender.id = ui_signal_select;
    sender.value = "DCF77";
    updateSignalCallback(&sender, S_ACTIVE);
    
    // Assert
    TEST_ASSERT_EQUAL_STRING("DCF77", signalGenerator->getName().c_str());
    TEST_ASSERT_EQUAL(77500, last_ledc_freq);
    
    // Act - Select MSF
    sender.value = "MSF";
    updateSignalCallback(&sender, S_ACTIVE);
    
    // Assert
    TEST_ASSERT_EQUAL_STRING("MSF", signalGenerator->getName().c_str());
    TEST_ASSERT_EQUAL(60000, last_ledc_freq);
    
    // Act - Select JJY
    sender.value = "JJY";
    updateSignalCallback(&sender, S_ACTIVE);
    
    // Assert
    TEST_ASSERT_EQUAL_STRING("JJY", signalGenerator->getName().c_str());
    TEST_ASSERT_EQUAL(40000, last_ledc_freq); // or 60000 depending on impl
    
    // Act - Select WWVB
    sender.value = "WWVB";
    updateSignalCallback(&sender, S_ACTIVE);
    
    // Assert
    TEST_ASSERT_EQUAL_STRING("WWVB", signalGenerator->getName().c_str());
    TEST_ASSERT_EQUAL(60000, last_ledc_freq);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_setup_completes);
    RUN_TEST(test_wifi_connection_attempt);
    RUN_TEST(test_serial_date_output);
    RUN_TEST(test_wwvb_logic_signal);
    RUN_TEST(test_wwvb_frame_encoding);
    RUN_TEST(test_dcf77_signal);
    RUN_TEST(test_jjy_signal);
    RUN_TEST(test_msf_signal);
    RUN_TEST(test_signal_switching);
    UNITY_END();
    return 0;
}

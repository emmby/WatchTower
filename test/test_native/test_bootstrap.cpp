#include <Arduino.h>
#include <unity.h>
#include <string>
#include <vector>
#include <sys/time.h>

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
void ledcAttach(uint8_t pin, double freq, uint8_t resolution) {}
void ledcWrite(uint8_t pin, uint32_t duty) {}
void configTzTime(const char* tz, const char* server1, const char* server2 = nullptr, const char* server3 = nullptr) {}
bool getLocalTime(struct tm* info, uint32_t ms = 5000) {
    time_t now;
    time(&now);
    localtime_r(&now, info);
    return true;
}
void sntp_set_time_sync_notification_cb(sntp_sync_time_cb_t callback) {}

// Forward declarations for functions in WatchTower.ino that are used before definition
bool wwvbLogicSignal(int, int, int, int, int, int, int, int);

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
    // Set time to 2025-12-25 12:00:00 UTC
    // 1766664000
    mock_tv.tv_sec = 1766664000; 
    mock_tv.tv_usec = 900000; // 900ms to ensure logicValue=1 (MARK/ONE/ZERO all high at 900ms? No wait)
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

void test_wwvb_logic_signal(void) {
    // Test ZERO bit (e.g. second 4 is always ZERO/Blank)
    // Expect: False for < 200ms, True for >= 200ms
    TEST_ASSERT_FALSE(wwvbLogicSignal(0, 0, 4, 199, 0, 2025, 0, 0));
    TEST_ASSERT_TRUE(wwvbLogicSignal(0, 0, 4, 200, 0, 2025, 0, 0));

    // Test ONE bit (e.g. second 1, minute 40 -> bit 2 is 1)
    // Minute 40 = 101000 binary? No. 40 / 10 = 4. 4 in binary is 100.
    // Second 1 checks bit 2 of (minute/10). (4 >> 2) & 1 = 1. So it's a ONE.
    // Expect: False for < 500ms, True for >= 500ms
    TEST_ASSERT_FALSE(wwvbLogicSignal(0, 40, 1, 499, 0, 2025, 0, 0));
    TEST_ASSERT_TRUE(wwvbLogicSignal(0, 40, 1, 500, 0, 2025, 0, 0));

    // Test MARK bit (e.g. second 0 is always MARK)
    // Expect: False for < 800ms, True for >= 800ms
    TEST_ASSERT_FALSE(wwvbLogicSignal(0, 0, 0, 799, 0, 2025, 0, 0));
    TEST_ASSERT_TRUE(wwvbLogicSignal(0, 0, 0, 800, 0, 2025, 0, 0));
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_setup_completes);
    RUN_TEST(test_wifi_connection_attempt);
    RUN_TEST(test_serial_date_output);
    RUN_TEST(test_wwvb_logic_signal);
    UNITY_END();
    return 0;
}

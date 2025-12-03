#include <Arduino.h>
#include <unity.h>

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

// Mock Serial to support printf
class SerialMock {
public:
    void begin(unsigned long) {}
    void println(const char*) {}
    void println(String) {}
    void println(int) {}
    void println() {}
    void print(const char*) {}
    void print(int) {}
    void printf(const char * format, ...) {}
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
    WiFiManager::calledAutoConnect = false;
    memset(WiFiManager::lastSSID, 0, sizeof(WiFiManager::lastSSID));

    // Act
    setup();

    // Assert
    TEST_ASSERT_TRUE(WiFiManager::calledAutoConnect);
    TEST_ASSERT_EQUAL_STRING("WatchTower", WiFiManager::lastSSID);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_setup_completes);
    RUN_TEST(test_wifi_connection_attempt);
    UNITY_END();
    return 0;
}

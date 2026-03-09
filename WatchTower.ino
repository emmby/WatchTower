// ========================
// INSTRUCTIONS
// ========================

// - Add the following dependencies to your Arduino libraries:
//     - Adafruit NeoPixel ~1.15.2
//     - ESPUI ~2.2.4
//     - ESP32Async / ESP Async WebServer ~3.9.0
//     - ESP32Async / Async TCP ~3.4.9
//     - WiFiManager ~2.0.17
//     - ArduinoMDNS ~1.0.0
// - set the PIN_ANTENNA to desired output pin
// - set the timezone as desired
// - build and run the code on your device
// - connect your phone to "WatchTower" to set the wifi config for the device
// - connect to http://watchtower.local to view current status

// Designed for the following, but should be easily
// transferable to other components:
// - Adafruit Qt Py ESP32 Pico: https://www.adafruit.com/product/5395
// - Adafruit DRV8833 breakout: https://www.adafruit.com/product/3297
// Also tested on
// - Adafruit ESP32 Feather v2
// - Arduino Nano ESP32 (via wokwi)

// ========================
// Configuration
// ========================

// Flip to false to disable the built-in web ui.
// You might want to do this to avoid leaving unnecessary open ports on your network.
const bool ENABLE_WEB_UI = true;

// Set this to the pin your antenna is connected on
const int PIN_ANTENNA = 13;

// Set to your timezone.
// This is needed for computing DST if applicable
// https://gist.github.com/alwynallan/24d96091655391107939
const char *timezone = "PST8PDT,M3.2.0,M11.1.0"; // America/Los_Angeles

// ========================
// Includes
// ========================

#include <WiFiManager.h>
#include "include/StatusLED.h"
#include <SPI.h>
#include <WiFiUdp.h>
#include <ArduinoMDNS.h>
#include <time.h>
#include <esp_sntp.h>
#include <Preferences.h>
#include "include/RadioTimeSignal.h"
#include "include/WWVBSignal.h"
#include "include/DCF77Signal.h"
#include "include/MSFSignal.h"
#include "include/JJYSignal.h"
#include "include/WebUI.h"


// ========================
// Globals
// ========================

// Default to WWVB if no signal is specified
WWVBSignal wwvb;
DCF77Signal dcf77;
MSFSignal msf;
JJYSignal jjy;
RadioTimeSignal* signalGenerator = &wwvb;

StatusLED statusLED;
WebUI webUI;
WiFiManager wifiManager;
WiFiUDP udp;
MDNS mdns(udp);
Preferences preferences;
bool logicValue = 0; // TODO rename
unsigned long lastSync = 0;
bool networkSyncEnabled = true;
const char* const ntpServer = "pool.ntp.org";


// --- Signal Generation ---
// The signal is generated in two parts:
//  1. loop() encodes the current minute's 60-bit frame into a broadcast[60] buffer
//     using the signal generator (WWVB, DCF77, MSF, or JJY). This involves
//     timezone lookups and daylight savings calculations that are too heavy for an
//     interrupt service routine (ISR).
//  2. A high-priority esp_timer callback (onSignalTimer, every 1ms) reads the
//     pre-computed broadcast buffer, determines the correct pulse-width modulation (PWM)
//     level for the current time, and writes it to the antenna pin.
// This ensures the PWM output is always on time, even when WiFi, ESPUI, or
// other background tasks delay loop(). A double-buffer is used so the timer
// always reads from a fully-written buffer.
TimeCodeSymbol broadcastA[60];
TimeCodeSymbol broadcastB[60];
volatile const TimeCodeSymbol* activeBroadcast = broadcastA;

// Shared state between timer callback and loop()
volatile bool transitionOccurred = false;
volatile unsigned long lastTransitionUsec = 0;
volatile int lastTransitionSecond = 0;

esp_timer_handle_t signalTimer = nullptr;

// ========================
// Helpers
// ========================

// Convert a logical bit into a PWM pulse width.
// Returns 50% duty cycle (128) for high, 0% for low
static inline short dutyCycle(bool logicValue) {
  return logicValue ? (256*0.5) : 0; // 128 == 50% duty cycle
}

void clearBroadcastValues(TimeCodeSymbol* buf) {
    for(int i=0; i<60; ++i) {
        buf[i] = (TimeCodeSymbol)-1; // -1 isn't legal but that's okay, we just need an invalid value
    }
}

// ========================
// Callbacks
// ========================

// A callback that tracks when we last sync'ed the
// time with the ntp server
void time_sync_notification_cb(struct timeval *tv) {
  lastSync = millis();
}

// A callback that is called when the device
// starts up an access point for wifi configuration.
// This is called when the device cannot connect to wifi.
void accesspointCallback(WiFiManager*) {
  Serial.println("Connect to SSID: WatchTower with another device to set wifi configuration.");
}

/**
 * High-priority timer callback (runs every 1ms).
 * Reads the RTC, looks up the pre-computed bit, and updates the PWM pin.
 * This runs at higher priority than WiFi/ESPUI, eliminating network-induced jitter.
 */
void onSignalTimer(void* arg) {
    struct timeval now;
    gettimeofday(&now, NULL);
    int sec = now.tv_sec % 60;
    const TimeCodeSymbol* bits = const_cast<const TimeCodeSymbol*>(activeBroadcast);
    bool level = signalGenerator->getLevelForTimeCodeSymbol(
        bits[sec], now.tv_usec / 1000);
    if (level != logicValue) {
        ledcWrite(PIN_ANTENNA, dutyCycle(level));
        logicValue = level;
        lastTransitionUsec = now.tv_usec;
        lastTransitionSecond = sec;
        transitionOccurred = true;
    }
}

// ========================
// Arduino Setup & Loop
// ========================

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(PIN_ANTENNA, OUTPUT);
  statusLED.begin();
  statusLED.setLoading();

  // E (14621) rmt: rmt_new_tx_channel(269): not able to power down in light sleep
  digitalWrite(PIN_ANTENNA, 0);

  // https://github.com/tzapu/WiFiManager/issues/1426
  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);

  // Connect to WiFi using // https://github.com/tzapu/WiFiManager 
  // If no wifi, start up an SSID called "WatchTower" so
  // the user can configure wifi using their phone.
  wifiManager.setAPCallback(accesspointCallback);
  wifiManager.autoConnect("WatchTower");

  preferences.begin("watchtower", false);
  networkSyncEnabled = preferences.getBool("net_sync", true);
  String savedSignal = preferences.getString("signal", "WWVB");
  if (savedSignal == "DCF77") signalGenerator = &dcf77;
  else if (savedSignal == "MSF") signalGenerator = &msf;
  else if (savedSignal == "JJY") signalGenerator = &jjy;
  else signalGenerator = &wwvb;

  clearBroadcastValues(broadcastA);
  clearBroadcastValues(broadcastB);

  // --- WEB UI SETUP ---
  if( ENABLE_WEB_UI ) {
    webUI.begin(timezone, signalGenerator,
                wwvb, dcf77, msf, jjy,
                preferences, PIN_ANTENNA, ntpServer,
                networkSyncEnabled, lastSync, mdns);
  }
  
  // --- TIME SYNC ---

  // Connect to network time server
  // By default, it will resync every few hours
  sntp_set_time_sync_notification_cb(time_sync_notification_cb);
  
  if (networkSyncEnabled) {
      configTzTime(timezone, ntpServer);
  } else {
      // When network sync is disabled, we still need to configure the timezone
      // so that localtime() works correctly.
      setenv("TZ", timezone, 1);
      tzset();
  }
  
  struct tm timeinfo;
  // Only block on time if sync is enabled
  if (networkSyncEnabled) {
    if (getLocalTime(&timeinfo)) {
      Serial.println("Got the time from NTP");
    } else {
      Serial.println("Failed to obtain time");
      statusLED.setError();
      delay(3000);
      ESP.restart();
    }
  } else {
      Serial.println("Network sync disabled, skipping initial time check.");
  }

  // Start the carrier signal using 8-bit (0-255) resolution
  ledcAttach(PIN_ANTENNA, signalGenerator->getFrequency(), 8);

  statusLED.setReady();

  // Start the high-priority signal timer (1ms interval).
  // This ensures PWM transitions happen on time regardless of WiFi/ESPUI activity.
  const esp_timer_create_args_t timerArgs = {
      .callback = onSignalTimer,
      .arg = NULL,
      .dispatch_method = ESP_TIMER_TASK,
      .name = "signal_timer"
  };
  esp_timer_create(&timerArgs, &signalTimer);
  esp_timer_start_periodic(signalTimer, 1000);  // 1ms = 1000us
  Serial.println("Signal timer started (1ms interval)");
}

void loop() {
  mdns.run();

  struct timeval now; // current time in seconds / millis
  struct tm buf_now_utc; // current time in UTC
  struct tm buf_now_local; // current time in localtime
  struct tm buf_today_start, buf_tomorrow_start; // start of today and tomrrow in localtime
  static int prev_second_display = -1; // for tracking UI updates

  gettimeofday(&now,NULL);
  localtime_r(&now.tv_sec, &buf_now_local);
  gmtime_r(&now.tv_sec, &buf_now_utc); 

  // compute start of today for dst
  struct timeval today_start = now;
  today_start.tv_usec = 0;
  today_start.tv_sec = (today_start.tv_sec / 86400) * 86400; // This is not exact but close enough
  localtime_r(&today_start.tv_sec, &buf_today_start);

  // compute start of tomorrow for dst
  struct timeval tomorrow_start = now;
  tomorrow_start.tv_usec = 0;
  tomorrow_start.tv_sec = ((tomorrow_start.tv_sec / 86400) + 1) * 86400; // again, close enough
  localtime_r(&tomorrow_start.tv_sec, &buf_tomorrow_start);

    static int prevMinute = -1;
    if (buf_now_utc.tm_min != prevMinute) {
        prevMinute = buf_now_utc.tm_min;
        // Write to the INACTIVE buffer, then swap
        TimeCodeSymbol* inactive = (activeBroadcast == broadcastA) ? broadcastB : broadcastA;
        clearBroadcastValues(inactive);
        signalGenerator->encodeMinute(
            buf_now_utc,
            buf_today_start.tm_isdst,
            buf_tomorrow_start.tm_isdst
        );
        for (int s = 0; s < 60; s++) {
            inactive[s] = signalGenerator->getSymbolForSecond(s);
        }
        activeBroadcast = inactive;  // atomic pointer swap
    }


  // --- HANDLE TRANSITIONS DETECTED BY TIMER CALLBACK ---
  if( transitionOccurred ) {
    transitionOccurred = false;
    unsigned long usec = lastTransitionUsec;

    statusLED.setTransmitting(logicValue);

    // do any logging after we set the bit to not slow anything down,
    // serial port I/O is slow!
    char timeStringBuff[100]; // Buffer to hold the formatted time string
    char timeStringBuff2[100];
    char timeStringBuff3[20];
    strftime(timeStringBuff, sizeof(timeStringBuff), "%A, %B %d %Y %H:%M:%S", &buf_now_local); // time
    strftime(timeStringBuff3, sizeof(timeStringBuff3), "%z %Z", &buf_now_local); // timezone
    snprintf(timeStringBuff2, sizeof(timeStringBuff2), "%s.%03d%s", timeStringBuff, now.tv_usec/1000, timeStringBuff3 ); // time+millis+tz

    char lastSyncStringBuff[100]; // Buffer to hold the formatted time string
    if (lastSync == 0) {
        snprintf(lastSyncStringBuff, sizeof(lastSyncStringBuff), "Never");
    } else {
        unsigned long secondsSinceSync = (millis() - lastSync) / 1000;
        snprintf(lastSyncStringBuff, sizeof(lastSyncStringBuff), "%lus ago", secondsSinceSync);
    }
    Serial.printf("%s [last sync %s]: %s\n",timeStringBuff2, lastSyncStringBuff, logicValue ? "1" : "0");


    static int prevSecond = -1;
    if( ENABLE_WEB_UI && prevSecond != buf_now_utc.tm_sec ) {
        prevSecond = buf_now_utc.tm_sec;
        webUI.update(buf_now_local, buf_now_utc, lastSync, activeBroadcast);
    }

    // Check for stale sync (24 hours)
    // Only restart if it's past 12pm local time to avoid rebooting while a device is syncing
    if( networkSyncEnabled && (millis() - lastSync > 24 * 60 * 60 * 1000) && buf_now_local.tm_hour >= 12 ) {
      Serial.println("Last sync more than 24 hours ago, rebooting.");
      statusLED.setError();
      delay(3000);
      ESP.restart();
    }

    statusLED.show();
  }  
}




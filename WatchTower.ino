// INSTRUCTIONS
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

#include <WiFiManager.h>
#include <Adafruit_NeoPixel.h>
#include <SPI.h>
#include <ESPUI.h>
#include <WiFiUdp.h>
#include <ArduinoMDNS.h>
#include <time.h>
#include <esp_sntp.h>
#include "customJS.h"
#include "RadioTimeSignal.h"
#include "WWVBSignal.h"
#include "DCF77Signal.h"
#include "MSFSignal.h"
#include "JJYSignal.h"

// Flip to false to disable the built-in web ui.
// You might want to do this to avoid leaving unnecessary open ports on your network.
const bool ENABLE_WEB_UI = true;

// Set this to the pin your antenna is connected on
const int PIN_ANTENNA = 13;

// Set to your timezone.
// This is needed for computing DST if applicable
// https://gist.github.com/alwynallan/24d96091655391107939
// Set to your timezone.
// This is needed for computing DST if applicable
// https://gist.github.com/alwynallan/24d96091655391107939
const char *timezone = "PST8PDT,M3.2.0,M11.1.0"; // America/Los_Angeles


// Default to WWVB if no signal is specified
#if defined(SIGNAL_DCF77)
RadioTimeSignal* signalGenerator = new DCF77Signal();
#elif defined(SIGNAL_MSF)
RadioTimeSignal* signalGenerator = new MSFSignal();
#elif defined(SIGNAL_JJY)
RadioTimeSignal* signalGenerator = new JJYSignal();
#else
RadioTimeSignal* signalGenerator = new WWVBSignal();
#endif

const char* const ntpServer = "pool.ntp.org";

// Configure the optional onboard neopixel
#ifdef PIN_NEOPIXEL
Adafruit_NeoPixel* const pixel = new Adafruit_NeoPixel(1, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);
#else
Adafruit_NeoPixel* const pixel = NULL;
#endif

const uint8_t LED_BRIGHTNESS = 10; // very dim, 0-255
const uint32_t COLOR_READY = pixel ? pixel->Color(0, 60, 0) : 0; // green https://share.google/4WKm4XDkH9tfm3ESC
const uint32_t COLOR_LOADING = pixel ? pixel->Color(60, 32, 0) : 0; // orange https://share.google/7tT5GPxskZi8t8qmx
const uint32_t COLOR_ERROR = pixel ? pixel->Color(150, 0, 0) : 0; // red https://share.google/nx2jWYSoGtl0opkzL
const uint32_t COLOR_TRANSMIT = pixel ? pixel->Color(32, 0, 0) : 0; // dim red https://share.google/wYFYM3t1kDeOJfr1U

WiFiManager wifiManager;
WiFiUDP udp;
MDNS mdns(udp);
bool logicValue = 0; // TODO rename
struct timeval lastSync;
TimeCodeSymbol broadcast[60];

// ESPUI Interface IDs
uint16_t ui_time;
uint16_t ui_date;
uint16_t ui_timezone;
uint16_t ui_broadcast;
uint16_t ui_uptime;
uint16_t ui_last_sync;

// A callback that tracks when we last sync'ed the
// time with the ntp server
void time_sync_notification_cb(struct timeval *tv) {
  lastSync = *tv;
}

// A callback that is called when the device
// starts up an access point for wifi configuration.
// This is called when the device cannot connect to wifi.
void accesspointCallback(WiFiManager*) {
  Serial.println("Connect to SSID: WatchTower with another device to set wifi configuration.");
}

// Convert a logical bit into a PWM pulse width.
// Returns 50% duty cycle (128) for high, 0% for low
static inline short dutyCycle(bool logicValue) {
  return logicValue ? (256*0.5) : 0; // 128 == 50% duty cycle
}

static inline int is_leap_year(int year) {
    return (year % 4 == 0) && (year % 100 != 0 || year % 400 == 0);
}

void clearBroadcastValues() {
    for(int i=0; i<sizeof(broadcast)/sizeof(broadcast[0]); ++i) {
        broadcast[i] = (TimeCodeSymbol)-1; // -1 isn't legal but that's okay, we just need an invalid value
    }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(PIN_ANTENNA, OUTPUT);
  if( pixel ) {
    pixel->begin();
    pixel->setBrightness(LED_BRIGHTNESS); // very dim
    pixel->setPixelColor(0, COLOR_LOADING );
    pixel->show();
  }

  // E (14621) rmt: rmt_new_tx_channel(269): not able to power down in light sleep
  digitalWrite(PIN_ANTENNA, 0);

  // https://github.com/tzapu/WiFiManager/issues/1426
  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);

  // Connect to WiFi using // https://github.com/tzapu/WiFiManager 
  // If no wifi, start up an SSID called "WatchTower" so
  // the user can configure wifi using their phone.
  wifiManager.setAPCallback(accesspointCallback);
  wifiManager.autoConnect("WatchTower");

  clearBroadcastValues();

  // --- ESPUI SETUP ---
  ESPUI.setVerbosity(Verbosity::Quiet);
  
  // Create Labels
  ui_broadcast = ESPUI.label("Broadcast Waveform", ControlColor::Sunflower, "");
  ui_time = ESPUI.label("Current Time", ControlColor::Turquoise, "Loading...");
  ui_date = ESPUI.label("Date", ControlColor::Emerald, "Loading...");
  ui_timezone = ESPUI.label("Timezone", ControlColor::Peterriver, timezone);
  ui_uptime = ESPUI.label("System Uptime", ControlColor::Carrot, "0s");
  ui_last_sync = ESPUI.label("Last NTP Sync", ControlColor::Alizarin, "Pending...");

  ESPUI.setPanelWide(ui_broadcast, true);
  ESPUI.setElementStyle(ui_broadcast, "font-family: monospace");
  ESPUI.setCustomJS(customJS);

  // You may disable the internal webserver by commenting out this line
  if( ENABLE_WEB_UI ) {
    mdns.begin(WiFi.localIP(), "watchtower");
    Serial.println("Connect to http://watchtower.local for the console");
    ESPUI.begin("WatchTower");
  }
  
  // --- TIME SYNC ---

  // Connect to network time server
  // By default, it will resync every few hours
  sntp_set_time_sync_notification_cb(time_sync_notification_cb);
  configTzTime(timezone, ntpServer);
  
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    if( pixel ) {
        pixel->setPixelColor(0, COLOR_ERROR );
        pixel->show();
    }
    delay(3000);
    ESP.restart();
  }
  Serial.println("Got the time from NTP");

  // Start the carrier signal using 8-bit (0-255) resolution
  ledcAttach(PIN_ANTENNA, signalGenerator->getFrequency(), 8);

  // green means go
  if( pixel ) {
    pixel->setPixelColor(0, COLOR_READY );
    pixel->show();
    delay(3000);
    pixel->clear();  
    pixel->show();
  }
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

  const bool prevLogicValue = logicValue;

    TimeCodeSymbol bit = signalGenerator->getSymbol(
        buf_now_utc,
        buf_today_start.tm_isdst,
        buf_tomorrow_start.tm_isdst
    );

    if(buf_now_utc.tm_sec == 0) {
        clearBroadcastValues();
    }
    broadcast[buf_now_utc.tm_sec] = bit;

    logicValue = signalGenerator->getSignalLevel(bit, now.tv_usec/1000);

  // --- UI UPDATE LOGIC ---
  if( logicValue != prevLogicValue ) {
    ledcWrite(PIN_ANTENNA, dutyCycle(logicValue));  // Update the duty cycle of the PWM

    // light up the pixel if desired
    if( pixel ) {
      if( logicValue == 1 ) {
        pixel->setPixelColor(0, COLOR_TRANSMIT ); // don't call show yet, the color may change
      } else {
        pixel->clear();
      }
    }

    // do any logging after we set the bit to not slow anything down,
    // serial port I/O is slow!
    char timeStringBuff[100]; // Buffer to hold the formatted time string
    char timeStringBuff2[100];
    char timeStringBuff3[20];
    strftime(timeStringBuff, sizeof(timeStringBuff), "%A, %B %d %Y %H:%M:%S", &buf_now_local); // time
    strftime(timeStringBuff3, sizeof(timeStringBuff3), "%z %Z", &buf_now_local); // timezone
    sprintf(timeStringBuff2,"%s.%03d%s", timeStringBuff, now.tv_usec/1000, timeStringBuff3 ); // time+millis+tz

    char lastSyncStringBuff[100]; // Buffer to hold the formatted time string
    struct tm buf_lastSync;
    localtime_r(&lastSync.tv_sec, &buf_lastSync);
    strftime(lastSyncStringBuff, sizeof(lastSyncStringBuff), "%b %d %H:%M", &buf_lastSync);
    Serial.printf("%s [last sync %s]: %s\n",timeStringBuff2, lastSyncStringBuff, logicValue ? "1" : "0");

    static int prevSecond = -1;
    if( prevSecond != buf_now_utc.tm_sec ) {
        prevSecond = buf_now_utc.tm_sec;

        // --- UPDATE THE WEB UI ---

        // Time
        char buf[62];
        strftime(buf, sizeof(buf), "%H:%M:%S%z %Z", &buf_now_local);
        ESPUI.print(ui_time, buf);

        // Date
        strftime(buf, sizeof(buf), "%A, %B %d %Y", &buf_now_local);
        ESPUI.print(ui_date, buf);

        // Broadcast window
        for( int i=0; i<60; ++i ) { // TODO leap seconds
        switch(broadcast[i]) {
            case TimeCodeSymbol::MARK:
                buf[i] = 'M';
                break;
            case TimeCodeSymbol::ZERO:
                buf[i] = '0';
                break;
            case TimeCodeSymbol::ONE:
                buf[i] = '1';
                break;
            case TimeCodeSymbol::IDLE:
                buf[i] = '-';
                break;
            default:
                buf[i] = ' ';
                break;
        }
        }
        ESPUI.print(ui_broadcast, buf);


        // Uptime
        long uptime = millis() / 1000;
        int up_d = uptime / 86400;
        int up_h = (uptime % 86400) / 3600;
        int up_m = (uptime % 3600) / 60;
        int up_s = uptime % 60;
        snprintf(buf, sizeof(buf), "%03dd %02dh %02dm %02ds", up_d, up_h, up_m, up_s);
        ESPUI.print(ui_uptime, buf);

        // Last Sync
        strftime(buf, sizeof(buf), "%b %d %H:%M", &buf_lastSync);
        ESPUI.print(ui_last_sync, buf);
    }

    // Check for stale sync (24 hours)
    if( now.tv_sec - lastSync.tv_sec > 60 * 60 * 24 ) {
      Serial.println("Last sync more than 24 hours ago, rebooting.");
      if( pixel ) {
        pixel->setPixelColor(0, COLOR_ERROR );
        delay(3000);
      }
      ESP.restart();
    }

    if( pixel ) {
        pixel->show();
    }
  }  
}




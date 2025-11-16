// INSTRUCTIONS
// - set the PIN_ANTENNA to desired output pin
// - set the timezone as desired
// - download and run the code on your device
// - connect your phone to "WatchTower" to set the wifi config for the device
// - connect to http://watchtower.local to view current status

// Designed for the following, but should be easily
// transferable to other components:
// - Adafruit Qt Py ESP32 Pico: https://www.adafruit.com/product/5395
// - Adafruit DRV8833 breakout: https://www.adafruit.com/product/3297

#include <WiFiManager.h>
#include <Adafruit_NeoPixel.h>
#include <SPI.h>
#include <ESPUI.h>
#include <WiFiUdp.h>
#include <ArduinoMDNS.h>
#include "time.h"
#include "esp_sntp.h"

// Flip to false to disable the built-in web ui.
// You might want to do this to avoid leaving unnecessary open ports on your network.
const bool ENABLE_WEB_UI = true;

// Set this to the pin your antenna is connected on
const int PIN_ANTENNA = 13;

// Set to your timezone.
// This is needed for computing DST if applicable
// https://gist.github.com/alwynallan/24d96091655391107939
const char *timezone = "PST8PDT,M3.2.0,M11.1.0"; // America/Los_Angeles


enum WWVB_T {
  ZERO = 0,
  ONE = 1,
  MARK = 2,
};

const int KHZ_60 = 60000;
const char* ntpServer = "pool.ntp.org";

// Configure the optional onboard neopixel
const uint8_t LED_BRIGHTNESS = 10; // very dim, 0-255
Adafruit_NeoPixel pixels(1, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);
const uint32_t COLOR_READY = pixels.Color(0, 60, 0); // green https://share.google/4WKm4XDkH9tfm3ESC
const uint32_t COLOR_LOADING = pixels.Color(60, 32, 0); // orange https://share.google/7tT5GPxskZi8t8qmx
const uint32_t COLOR_ERROR = pixels.Color(150, 0, 0); // red https://share.google/nx2jWYSoGtl0opkzL
const uint32_t COLOR_TRANSMIT = pixels.Color(32, 0, 0); // dim red https://share.google/wYFYM3t1kDeOJfr1U

WiFiManager wifiManager;
WiFiUDP udp;
MDNS mdns(udp);
bool logicValue = 0; // TODO rename
struct timeval lastSync;

// ESPUI Interface IDs
uint16_t ui_time;
uint16_t ui_date;
uint16_t ui_timezone;
uint16_t ui_uptime;
uint16_t ui_last_sync;

// A tricky way to force arduino to reboot
// by accessing a protected memory address
void(* forceReboot) (void) = 0;

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

void setup() {
  Serial.begin(115200);
  delay(1000);
  pixels.begin();

  pinMode(PIN_ANTENNA, OUTPUT);
  pixels.setBrightness(LED_BRIGHTNESS); // very dim
  pixels.setPixelColor(0, COLOR_LOADING );
  pixels.show();

  // E (14621) rmt: rmt_new_tx_channel(269): not able to power down in light sleep
  digitalWrite(PIN_ANTENNA, 0);

  // https://github.com/tzapu/WiFiManager/issues/1426
  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);

  // Connect to WiFi using // https://github.com/tzapu/WiFiManager 
  // If no wifi, start up an SSID called "WatchTower" so
  // the user can configure wifi using their phone.
  wifiManager.setAPCallback(accesspointCallback);
  wifiManager.autoConnect("WatchTower");


  // --- ESPUI SETUP ---
  ESPUI.setVerbosity(Verbosity::Quiet);
  
  // Create Labels
  ui_time = ESPUI.label("Current Time", ControlColor::Turquoise, "Loading...");
  ui_date = ESPUI.label("Date", ControlColor::Emerald, "Loading...");
  ui_timezone = ESPUI.label("Timezone", ControlColor::Peterriver, timezone);
  ui_uptime = ESPUI.label("System Uptime", ControlColor::Carrot, "0s");
  ui_last_sync = ESPUI.label("Last NTP Sync", ControlColor::Alizarin, "Pending...");

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
    pixels.setPixelColor(0, COLOR_ERROR );
    pixels.show();
    delay(3000);
    forceReboot();
  }
  Serial.println("Got the time from NTP");

  // Start the 60khz carrier signal using 8-bit (0-255) resolution
  ledcAttach(PIN_ANTENNA, KHZ_60, 8);

  // green means go
  pixels.setPixelColor(0, COLOR_READY );
  pixels.show();
  delay(3000);

  pixels.clear();  
  pixels.show();
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

  logicValue = wwvbLogicSignal(
    buf_now_utc.tm_hour,
    buf_now_utc.tm_min,
    buf_now_utc.tm_sec, 
    now.tv_usec/1000,
    buf_now_utc.tm_yday+1,
    buf_now_utc.tm_year+1900,
    buf_today_start.tm_isdst,
    buf_tomorrow_start.tm_isdst
    );

  // --- UI UPDATE LOGIC ---
  if( logicValue != prevLogicValue ) {
    ledcWrite(PIN_ANTENNA, dutyCycle(logicValue));  // Update the duty cycle of the PWM

    // light up the pixel if desired
    if( logicValue == 1 ) {
      pixels.setPixelColor(0, COLOR_TRANSMIT ); // don't call show yet, the color may change
    } else {
      pixels.clear();
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

    // --- UPDATE THE WEB UI ---

    // Time
    char buf[50];
    strftime(buf, sizeof(buf), "%H:%M:%S%z %Z", &buf_now_local);
    ESPUI.print(ui_time, buf);

    // Date
    strftime(buf, sizeof(buf), "%A, %B %d %Y", &buf_now_local);
    ESPUI.print(ui_date, buf);

    // Uptime
    long uptime = millis() / 1000;
    int up_h = uptime / 3600;
    int up_m = (uptime % 3600) / 60;
    int up_s = uptime % 60;
    snprintf(buf, sizeof(buf), "%02dh %02dm %02ds", up_h, up_m, up_s);
    ESPUI.print(ui_uptime, buf);

    // Last Sync
    strftime(buf, sizeof(buf), "%b %d %H:%M", &buf_lastSync);
    ESPUI.print(ui_last_sync, buf);

    // Check for stale sync (4 hours)
    if( now.tv_sec - lastSync.tv_sec > 60 * 60 * 4 ) {
      Serial.println("Last sync more than four hours ago, rebooting.");
      pixels.setPixelColor(0, COLOR_ERROR );
      delay(3000);
      forceReboot();
    }

    pixels.show();
  }  
}

// Convert a logical bit into a PWM pulse width.
// Returns 50% duty cycle (128) for high, 0% for low
static inline short dutyCycle(bool logicValue) {
  return logicValue ? (256*0.5) : 0; // 128 == 50% duty cycle
}


// Returns a logical high or low to indicate whether the
// PWM signal should be high or low based on the current time
// https://www.nist.gov/pml/time-and-frequency-division/time-distribution/radio-station-wwvb/wwvb-time-code-format
bool wwvbLogicSignal(
    int hour,                // 0 - 23
    int minute,              // 0 - 59
    int second,              // 0 - 59 (leap 60)
    int millis,
    int yday,                // days since January 1 eg. Jan 1 is 0
    int year,                // year since 0, eg. 2025
    int today_start_isdst,   // was this morning DST?
    int tomorrow_start_isdst // is tomorrow morning DST?
) {
    int leap = is_leap_year(year);
    
    WWVB_T bit;
    switch (second) {
        case 0: // mark
            bit = WWVB_T::MARK;
            break;
        case 1: // minute 40
            bit = (WWVB_T)(((minute / 10) >> 2) & 1);
            break;
        case 2: // minute 20
            bit = (WWVB_T)(((minute / 10) >> 1) & 1);
            break;
        case 3: // minute 10
            bit = (WWVB_T)(((minute / 10) >> 0) & 1);
            break;
        case 4: // blank
            bit = WWVB_T::ZERO;
            break;
        case 5: // minute 8
            bit = (WWVB_T)(((minute % 10) >> 3) & 1);
            break;
        case 6: // minute 4
            bit = (WWVB_T)(((minute % 10) >> 2) & 1);
            break;
        case 7: // minute 2
            bit = (WWVB_T)(((minute % 10) >> 1) & 1);
            break;
        case 8: // minute 1
            bit = (WWVB_T)(((minute % 10) >> 0) & 1);
            break;
        case 9: // mark
            bit = WWVB_T::MARK;
            break;
        case 10: // blank
            bit = WWVB_T::ZERO;
            break;
        case 11: // blank
            bit = WWVB_T::ZERO;
            break;
        case 12: // hour 20
            bit = (WWVB_T)(((hour / 10) >> 1) & 1);
            break;
        case 13: // hour 10
            bit = (WWVB_T)(((hour / 10) >> 0) & 1);
            break;
        case 14: // blank
            bit = WWVB_T::ZERO;
            break;
        case 15: // hour 8
            bit = (WWVB_T)(((hour % 10) >> 3) & 1);
            break;
        case 16: // hour 4
            bit = (WWVB_T)(((hour % 10) >> 2) & 1);
            break;
        case 17: // hour 2
            bit = (WWVB_T)(((hour % 10) >> 1) & 1);
            break;
        case 18: // hour 1
            bit = (WWVB_T)(((hour % 10) >> 0) & 1);
            break;
        case 19: // mark
            bit = WWVB_T::MARK;
            break;
        case 20: // blank
            bit = WWVB_T::ZERO;
            break;
        case 21: // blank
            bit = WWVB_T::ZERO;
            break;
        case 22: // yday of year 200
            bit = (WWVB_T)(((yday / 100) >> 1) & 1);
            break;
        case 23: // yday of year 100
            bit = (WWVB_T)(((yday / 100) >> 0) & 1);
            break;
        case 24: // blank
            bit = WWVB_T::ZERO;
            break;
        case 25: // yday of year 80
            bit = (WWVB_T)((((yday / 10) % 10) >> 3) & 1);
            break;
        case 26: // yday of year 40
            bit = (WWVB_T)((((yday / 10) % 10) >> 2) & 1);
            break;
        case 27: // yday of year 20
            bit = (WWVB_T)((((yday / 10) % 10) >> 1) & 1);
            break;
        case 28: // yday of year 10
            bit = (WWVB_T)((((yday / 10) % 10) >> 0) & 1);
            break;
        case 29: // mark
            bit = WWVB_T::MARK;
            break;
        case 30: // yday of year 8
            bit = (WWVB_T)(((yday % 10) >> 3) & 1);
            break;
        case 31: // yday of year 4
            bit = (WWVB_T)(((yday % 10) >> 2) & 1);
            break;
        case 32: // yday of year 2
            bit = (WWVB_T)(((yday % 10) >> 1) & 1);
            break;
        case 33: // yday of year 1
            bit = (WWVB_T)(((yday % 10) >> 0) & 1);
            break;
        case 34: // blank
            bit = WWVB_T::ZERO;
            break;
        case 35: // blank
            bit = WWVB_T::ZERO;
            break;
        case 36: // UTI sign +
            bit = WWVB_T::ONE;
            break;
        case 37: // UTI sign -
            bit = WWVB_T::ZERO;
            break;
        case 38: // UTI sign +
            bit = WWVB_T::ONE;
            break;
        case 39: // mark
            bit = WWVB_T::MARK;
            break;
        case 40: // UTI correction 0.8
            bit = WWVB_T::ZERO;
            break;
        case 41: // UTI correction 0.4
            bit = WWVB_T::ZERO;
            break;
        case 42: // UTI correction 0.2
            bit = WWVB_T::ZERO;
            break;
        case 43: // UTI correction 0.1
            bit = WWVB_T::ZERO;
            break;
        case 44: // blank
            bit = WWVB_T::ZERO;
            break;
        case 45: // year 80
            bit = (WWVB_T)((((year / 10) % 10) >> 3) & 1);
            break;
        case 46: // year 40
            bit = (WWVB_T)((((year / 10) % 10) >> 2) & 1);
            break;
        case 47: // year 20
            bit = (WWVB_T)((((year / 10) % 10) >> 1) & 1);
            break;
        case 48: // year 10
            bit = (WWVB_T)((((year / 10) % 10) >> 0) & 1);
            break;
        case 49: // mark
            bit = WWVB_T::MARK;
            break;
        case 50: // year 8
            bit = (WWVB_T)(((year % 10) >> 3) & 1);
            break;
        case 51: // year 4
            bit = (WWVB_T)(((year % 10) >> 2) & 1);
            break;
        case 52: // year 2
            bit = (WWVB_T)(((year % 10) >> 1) & 1);
            break;
        case 53: // year 1
            bit = (WWVB_T)(((year % 10) >> 0) & 1);
            break;
        case 54: // blank
            bit = WWVB_T::ZERO;
            break;
        case 55: // leap year
            bit = leap ? WWVB_T::ONE : WWVB_T::ZERO;
            break;
        case 56: // leap second
            bit = WWVB_T::ZERO;
            break;
        case 57: // dst bit 1
            bit = today_start_isdst ? WWVB_T::ONE : WWVB_T::ZERO;
            break;
        case 58: // dst bit 2
            bit = tomorrow_start_isdst ? WWVB_T::ONE : WWVB_T::ZERO;
            break;
        case 59: // mark
            bit = WWVB_T::MARK;
            break;
    }

    // Convert a wwvb zero, one, or mark to the appropriate pulse width
    // zero: low 200ms, high 800ms
    // one: low 500ms, high 500ms
    // mark low 800ms, high 200ms
    if (bit == WWVB_T::ZERO) {
      return millis >= 200;
    } else if (bit == WWVB_T::ONE) {
      return millis >= 500;
    } else {
      return millis >= 800;
    }
}

static inline int is_leap_year(int year) {
    return (year % 4 == 0) && (year % 100 != 0 || year % 400 == 0);
}
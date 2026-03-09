#pragma once

#include <ESPUI.h>
#include <esp_sntp.h>
#include <Preferences.h>
#include <WiFiUdp.h>
#include <ArduinoMDNS.h>
#include "customJS.h"
#include "include/RadioTimeSignal.h"
#include "include/WWVBSignal.h"
#include "include/DCF77Signal.h"
#include "include/MSFSignal.h"
#include "include/JJYSignal.h"


// Encapsulates all ESPUI web dashboard setup and per-second updates.
// Keeps the UI plumbing out of WatchTower.ino so the main sketch
// can focus on signal generation and timing.
class WebUI {
public:
    // Call from setup() to wire up all the ESPUI controls.
    // Pointers/references are stored — the caller owns the objects.
    void begin(
        const char* timezone,
        RadioTimeSignal*& signalGenerator,
        WWVBSignal& wwvb, DCF77Signal& dcf77, MSFSignal& msf, JJYSignal& jjy,
        Preferences& preferences,
        int pinAntenna,
        const char* ntpServer,
        bool& networkSyncEnabledRef,
        unsigned long& lastSyncRef,
        MDNS& mdns
    ) {
        _instance = this;
        _timezone = timezone;
        _wwvb = &wwvb; _dcf77 = &dcf77; _msf = &msf; _jjy = &jjy;
        _signalGenerator = &signalGenerator;
        _preferences = &preferences;
        _pinAntenna = pinAntenna;
        _ntpServer = ntpServer;
        _networkSyncEnabled = &networkSyncEnabledRef;
        _lastSync = &lastSyncRef;

        ESPUI.setVerbosity(Verbosity::Quiet);

        // Create Labels
        _ui_broadcast = ESPUI.label("Broadcast Waveform", ControlColor::Sunflower, "");
        _ui_time = ESPUI.label("Current Time", ControlColor::Turquoise, "Loading...");
        _ui_time_utc = ESPUI.addControl(ControlType::Label, "UTC", "Loading...", ControlColor::Turquoise, _ui_time);
        _ui_date = ESPUI.label("Date", ControlColor::Emerald, "Loading...");
        _ui_date_utc = ESPUI.addControl(ControlType::Label, "UTC", "Loading...", ControlColor::Emerald, _ui_date);
        ESPUI.label("Timezone", ControlColor::Peterriver, timezone);
        _ui_uptime = ESPUI.label("System Uptime", ControlColor::Carrot, "0s");
        _ui_last_sync = ESPUI.label("Last NTP Sync", ControlColor::Alizarin, "Pending...");
        _ui_network_sync_switch = ESPUI.switcher("Network time sync", _onSyncToggle, ControlColor::Sunflower, *_networkSyncEnabled);

        _ui_manual_date = ESPUI.text("Manual Date", _onManualTime, ControlColor::Dark, "");
        ESPUI.setInputType(_ui_manual_date, "date");
        ESPUI.updateVisibility(_ui_manual_date, !*_networkSyncEnabled);

        _ui_manual_time = ESPUI.text("Manual Time", _onManualTime, ControlColor::Dark, "");
        ESPUI.setInputType(_ui_manual_time, "time");
        ESPUI.updateVisibility(_ui_manual_time, !*_networkSyncEnabled);

        _ui_signal_select = ESPUI.addControl(ControlType::Select, "Signal Protocol", "", ControlColor::Turquoise, Control::noParent, _onSignalChange);
        ESPUI.addControl(ControlType::Option, "WWVB", "WWVB", ControlColor::Alizarin, _ui_signal_select);
        ESPUI.addControl(ControlType::Option, "DCF77", "DCF77", ControlColor::Alizarin, _ui_signal_select);
        ESPUI.addControl(ControlType::Option, "MSF", "MSF", ControlColor::Alizarin, _ui_signal_select);
        ESPUI.addControl(ControlType::Option, "JJY", "JJY", ControlColor::Alizarin, _ui_signal_select);

        // Set initial selection
        if (*_signalGenerator == _wwvb) ESPUI.updateSelect(_ui_signal_select, "WWVB");
        else if (*_signalGenerator == _dcf77) ESPUI.updateSelect(_ui_signal_select, "DCF77");
        else if (*_signalGenerator == _msf) ESPUI.updateSelect(_ui_signal_select, "MSF");
        else if (*_signalGenerator == _jjy) ESPUI.updateSelect(_ui_signal_select, "JJY");

        ESPUI.setPanelWide(_ui_broadcast, true);
        ESPUI.setElementStyle(_ui_broadcast, "font-family: monospace");



        ESPUI.setCustomJS(customJS);

        mdns.begin(WiFi.localIP(), "watchtower");
        Serial.println("Connect to http://watchtower.local for the console");
        ESPUI.begin("WatchTower");
    }

    // Call once per second from loop() to push updated values to the browser.
    void update(
        const struct tm& local,
        const struct tm& utc,
        unsigned long lastSync,
        volatile const TimeCodeSymbol* broadcast
    ) {
        char buf[62];

        // Time
        strftime(buf, sizeof(buf), "%H:%M:%S%z %Z", &local);
        ESPUI.print(_ui_time, buf);

        // UTC Time
        strftime(buf, sizeof(buf), "%H:%M:%S UTC", &utc);
        ESPUI.print(_ui_time_utc, buf);

        // Date (local with timezone label)
        strftime(buf, sizeof(buf), "%A, %B %d %Y (Day %j) %Z", &local);
        ESPUI.print(_ui_date, buf);

        // UTC Date
        strftime(buf, sizeof(buf), "%A, %B %d %Y (Day %j) UTC", &utc);
        ESPUI.print(_ui_date_utc, buf);

        // Broadcast window
        for (int i = 0; i < 60; ++i) { // TODO leap seconds
            switch (broadcast[i]) {
                case TimeCodeSymbol::MARK: buf[i] = 'M'; break;
                case TimeCodeSymbol::ZERO: buf[i] = '0'; break;
                case TimeCodeSymbol::ONE:  buf[i] = '1'; break;
                case TimeCodeSymbol::IDLE: buf[i] = '-'; break;
                default:                   buf[i] = ' '; break;
            }
        }
        buf[60] = '\0';
        ESPUI.print(_ui_broadcast, buf);

        // Uptime
        long uptime = millis() / 1000;
        int up_d = uptime / 86400;
        int up_h = (uptime % 86400) / 3600;
        int up_m = (uptime % 3600) / 60;
        int up_s = uptime % 60;
        snprintf(buf, sizeof(buf), "%03dd %02dh %02dm %02ds", up_d, up_h, up_m, up_s);
        ESPUI.print(_ui_uptime, buf);

        // Last Sync
        if (lastSync == 0) {
            ESPUI.print(_ui_last_sync, "Never");
        } else {
            unsigned long secondsSinceSync = (millis() - lastSync) / 1000;
            snprintf(buf, sizeof(buf), "%lus ago", secondsSinceSync);
            ESPUI.print(_ui_last_sync, buf);
        }

    }

private:
    // --- Shared state (owned by WatchTower.ino, stored by pointer/ref) ---
    const char* _timezone = nullptr;
    const char* _ntpServer = nullptr;
    int _pinAntenna = 0;
    WWVBSignal* _wwvb = nullptr;
    DCF77Signal* _dcf77 = nullptr;
    MSFSignal* _msf = nullptr;
    JJYSignal* _jjy = nullptr;
    RadioTimeSignal** _signalGenerator = nullptr;
    Preferences* _preferences = nullptr;
    bool* _networkSyncEnabled = nullptr;
    unsigned long* _lastSync = nullptr;

    String _manualDate = "";
    String _manualTime = "";

    // --- Singleton access for ESPUI callbacks ---
    // ESPUI callbacks are plain function pointers, so we use a static
    // instance pointer to route them back into the class.
    static WebUI* _instance;

public:
    // ESPUI control IDs and callback trampolines are public so that
    // tests can simulate ESPUI events.

    // --- ESPUI control IDs ---
    uint16_t _ui_time, _ui_time_utc;
    uint16_t _ui_date, _ui_date_utc;
    uint16_t _ui_broadcast;
    uint16_t _ui_uptime, _ui_last_sync;
    uint16_t _ui_network_sync_switch;
    uint16_t _ui_manual_date, _ui_manual_time;
    uint16_t _ui_signal_select;


    static void _onManualTime(Control* sender, int value) {
        if (_instance) _instance->handleManualTime(sender, value);
    }
    static void _onSignalChange(Control* sender, int value) {
        if (_instance) _instance->handleSignalChange(sender, value);
    }
    static void _onSyncToggle(Control* sender, int value) {
        if (_instance) _instance->handleSyncToggle(sender, value);
    }

private:

    void handleManualTime(Control* sender, int value) {
        if (sender->id == _ui_manual_date) {
            _manualDate = sender->value;
        } else if (sender->id == _ui_manual_time) {
            _manualTime = sender->value;
        }

        struct timeval now;
        gettimeofday(&now, NULL);
        struct tm tm;
        localtime_r(&now.tv_sec, &tm);

        if (_manualDate.length() > 0) {
            strptime(_manualDate.c_str(), "%Y-%m-%d", &tm);
        }
        if (_manualTime.length() > 0) {
            strptime(_manualTime.c_str(), "%H:%M", &tm);
            tm.tm_sec = 0;
        }

        tm.tm_isdst = -1;
        time_t t = mktime(&tm);
        if (t != -1) {
            struct timeval tv = { .tv_sec = t, .tv_usec = 0 };
            settimeofday(&tv, NULL);
            Serial.println("Manual time updated");
        } else {
            Serial.println("Failed to set manual time");
        }
    }

    void handleSignalChange(Control* sender, int value) {
        if (sender->id != _ui_signal_select) return;
        String selected = sender->value;
        if (selected == "WWVB")       *_signalGenerator = _wwvb;
        else if (selected == "DCF77") *_signalGenerator = _dcf77;
        else if (selected == "MSF")   *_signalGenerator = _msf;
        else if (selected == "JJY")   *_signalGenerator = _jjy;

        _preferences->putString("signal", (*_signalGenerator)->getName());
        Serial.println("Signal changed to: " + (*_signalGenerator)->getName());

        ledcDetach(_pinAntenna);
        ledcAttach(_pinAntenna, (*_signalGenerator)->getFrequency(), 8);
    }

    void handleSyncToggle(Control* sender, int value) {
        if (sender->id != _ui_network_sync_switch) return;
        *_networkSyncEnabled = (value == S_ACTIVE);
        _preferences->putBool("net_sync", *_networkSyncEnabled);
        Serial.printf("Network Sync changed to: %s\n", *_networkSyncEnabled ? "ENABLED" : "DISABLED");

        if (*_networkSyncEnabled) {
            esp_sntp_stop();
            configTzTime(_timezone, _ntpServer);
            Serial.println("NTP Sync re-enabled");
            ESPUI.updateVisibility(_ui_manual_date, false);
            ESPUI.updateVisibility(_ui_manual_time, false);
        } else {
            esp_sntp_stop();
            Serial.println("NTP Sync disabled");
            ESPUI.updateVisibility(_ui_manual_date, true);
            ESPUI.updateVisibility(_ui_manual_time, true);
        }
    }
};

// The singleton instance pointer — set automatically in begin().
WebUI* WebUI::_instance = nullptr;

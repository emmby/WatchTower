#pragma once
#include <time.h>

typedef void (*sntp_sync_time_cb_t)(struct timeval *tv);
void sntp_set_time_sync_notification_cb(sntp_sync_time_cb_t callback);
void esp_sntp_stop();

#pragma once

// Mock esp_timer for native testing.
// In production, the real esp_timer fires the callback every 1ms.
// In tests, the test calls onSignalTimer() directly to simulate the timer.

typedef void* esp_timer_handle_t;

#define ESP_TIMER_TASK 0

typedef struct {
    void (*callback)(void*);
    void* arg;
    int dispatch_method;
    const char* name;
} esp_timer_create_args_t;

inline int esp_timer_create(const esp_timer_create_args_t* args, esp_timer_handle_t* handle) {
    (void)args; (void)handle;
    return 0;
}

inline int esp_timer_start_periodic(esp_timer_handle_t handle, uint64_t period) {
    (void)handle; (void)period;
    return 0;
}

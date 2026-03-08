#ifndef TRANSITION_STATS_H
#define TRANSITION_STATS_H

#include <Arduino.h>

/**
 * @brief Tracks how precisely signal transitions align to 100ms boundaries.
 * 
 * Every radio time signal transition should occur at an exact multiple of 100ms.
 * This class measures the "jitter" — the distance from the nearest 100ms boundary —
 * using the ESP32 boot clock (millis()) and stores results in a histogram with
 * 5ms-wide buckets (0–4ms, 5–9ms, ..., 45–50ms).
 * 
 * Stats reset at midnight each day (UTC).
 */
class TransitionStats {
public:
    static const int NUM_BUCKETS = 11;       // 0-4, 5-9, 10-14, ..., 45-50
    static const int BUCKET_WIDTH_MS = 5;
    static const int MAX_JITTER_MS = 50;

    TransitionStats() {
        reset();
    }

    /**
     * Record a transition at the given boot clock time.
     * Computes the delta since the last transition, then measures
     * how far that delta is from the nearest multiple of 100ms.
     */
    void recordTransition(unsigned long currentMillis) {
        if (!hasLastTransition_) {
            // First transition after boot/reset — no delta to compute
            hasLastTransition_ = true;
            lastTransitionMillis_ = currentMillis;
            return;
        }
        
        unsigned long delta = currentMillis - lastTransitionMillis_;
        lastTransitionMillis_ = currentMillis;
        
        int offset = delta % 100;
        int jitter = offset <= MAX_JITTER_MS ? offset : (100 - offset);
        
        int bucket = jitter / BUCKET_WIDTH_MS;
        if (bucket >= NUM_BUCKETS) bucket = NUM_BUCKETS - 1;
        
        histogram_[bucket]++;
        totalCount_++;
        jitterSum_ += jitter;
        if (jitter > 0) nonZeroCount_++;
    }

    /**
     * Check if we've crossed midnight (UTC) and reset if so.
     * Call this periodically from the main loop.
     * @param utcHour current UTC hour (0-23)
     * @param utcMinute current UTC minute (0-59)
     */
    void checkMidnightReset(int utcHour, int utcMinute) {
        bool isNearMidnight = (utcHour == 0 && utcMinute == 0);
        if (isNearMidnight && !resetThisMinute_) {
            reset();
            resetThisMinute_ = true;
        } else if (!isNearMidnight) {
            resetThisMinute_ = false;
        }
    }

    /**
     * Compute the approximate Pth percentile from the histogram.
     * Returns the upper bound of the bucket containing the Pth percentile.
     * @param p percentile (0-100)
     * @return jitter in ms at that percentile
     */
    int getPercentile(int p) const {
        if (totalCount_ == 0) return 0;
        
        unsigned long threshold = ((unsigned long)totalCount_ * p + 99) / 100; // ceiling
        unsigned long cumulative = 0;
        
        for (int i = 0; i < NUM_BUCKETS; i++) {
            cumulative += histogram_[i];
            if (cumulative >= threshold) {
                // Return upper bound of this bucket
                return (i + 1) * BUCKET_WIDTH_MS;
            }
        }
        return MAX_JITTER_MS;
    }

    unsigned long getNonZeroCount() const { return nonZeroCount_; }
    unsigned long getTotalCount() const { return totalCount_; }
    
    float getAverageJitter() const {
        if (totalCount_ == 0) return 0.0f;
        return (float)jitterSum_ / totalCount_;
    }

    unsigned long getHistogramCount(int bucket) const {
        if (bucket < 0 || bucket >= NUM_BUCKETS) return 0;
        return histogram_[bucket];
    }

    void reset() {
        for (int i = 0; i < NUM_BUCKETS; i++) {
            histogram_[i] = 0;
        }
        totalCount_ = 0;
        nonZeroCount_ = 0;
        jitterSum_ = 0;
        resetThisMinute_ = false;
        hasLastTransition_ = false;
        lastTransitionMillis_ = 0;
    }

private:
    unsigned long histogram_[NUM_BUCKETS];
    unsigned long totalCount_;
    unsigned long nonZeroCount_;
    unsigned long jitterSum_;
    bool resetThisMinute_;      // debounce: prevent multiple resets during 00:00
    bool hasLastTransition_;    // true after first transition recorded
    unsigned long lastTransitionMillis_;
};

#endif // TRANSITION_STATS_H

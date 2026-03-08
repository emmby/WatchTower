#ifndef TRANSITION_STATS_H
#define TRANSITION_STATS_H

#include <Arduino.h>

/**
 * @brief Tracks how precisely signal transitions align to 100ms boundaries.
 * 
 * Every radio time signal transition should occur at an exact multiple of 100ms.
 * This class measures the "jitter" — the distance from the nearest 100ms boundary —
 * using micros() for sub-millisecond precision. Results are stored in a histogram
 * with 1ms-wide buckets (0ms, 1ms, 2ms, ..., 50ms).
 * 
 * Stats reset at midnight each day (UTC).
 */
class TransitionStats {
public:
    static const int NUM_BUCKETS = 51;        // 0, 1, 2, ..., 50 (one per ms)
    static const int BUCKET_WIDTH_US = 1000;  // 1ms in micros
    static const int MAX_JITTER_US = 50000;   // 50ms in micros
    static const unsigned long HUNDRED_MS_US = 100000; // 100ms in micros

    TransitionStats() {
        reset();
    }

    /**
     * Record a transition at the given boot clock time (in microseconds).
     * Computes the delta since the last transition, then measures
     * how far that delta is from the nearest multiple of 100ms.
     */
    void recordTransition(unsigned long currentMicros) {
        if (!hasLastTransition_) {
            hasLastTransition_ = true;
            lastTransitionMicros_ = currentMicros;
            return;
        }
        
        unsigned long delta = currentMicros - lastTransitionMicros_;
        lastTransitionMicros_ = currentMicros;
        
        int offsetUs = delta % HUNDRED_MS_US;
        int jitterUs = offsetUs <= MAX_JITTER_US ? offsetUs : (HUNDRED_MS_US - offsetUs);
        
        int bucket = jitterUs / BUCKET_WIDTH_US;
        if (bucket >= NUM_BUCKETS) bucket = NUM_BUCKETS - 1;
        
        histogram_[bucket]++;
        totalCount_++;
        jitterSumUs_ += jitterUs;
        if (jitterUs >= BUCKET_WIDTH_US) nonZeroCount_++; // 1ms+ counts as non-zero
    }

    /**
     * Check if we've crossed midnight (UTC) and reset if so.
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
     * @return jitter in ms at that percentile (upper bound of bucket)
     */
    int getPercentile(int p) const {
        if (totalCount_ == 0) return 0;
        
        unsigned long threshold = ((unsigned long)totalCount_ * p + 99) / 100;
        unsigned long cumulative = 0;
        
        for (int i = 0; i < NUM_BUCKETS; i++) {
            cumulative += histogram_[i];
            if (cumulative >= threshold) {
                return i + 1; // upper bound in ms
            }
        }
        return NUM_BUCKETS;
    }

    unsigned long getNonZeroCount() const { return nonZeroCount_; }
    unsigned long getTotalCount() const { return totalCount_; }
    
    /** @return average jitter in milliseconds (floating point) */
    float getAverageJitter() const {
        if (totalCount_ == 0) return 0.0f;
        return (float)jitterSumUs_ / totalCount_ / 1000.0f;
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
        jitterSumUs_ = 0;
        resetThisMinute_ = false;
        hasLastTransition_ = false;
        lastTransitionMicros_ = 0;
    }

private:
    unsigned long histogram_[NUM_BUCKETS];
    unsigned long totalCount_;
    unsigned long nonZeroCount_;
    unsigned long jitterSumUs_;
    bool resetThisMinute_;
    bool hasLastTransition_;
    unsigned long lastTransitionMicros_;
};

#endif // TRANSITION_STATS_H

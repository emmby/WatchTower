#ifndef TRANSITION_STATS_H
#define TRANSITION_STATS_H

#include <Arduino.h>

/**
 * @brief Tracks how precisely signal transitions align to 100ms boundaries.
 * 
 * Every radio time signal transition should occur at an exact multiple of 100ms
 * within each second. This class measures the "jitter" — how many microseconds
 * past the nearest 100ms boundary the transition was detected — using the RTC
 * microsecond value (tv_usec) directly.
 * 
 * Results are stored in a histogram with 1ms-wide buckets (0ms–99ms).
 * Stats reset at midnight each day (UTC).
 */
class TransitionStats {
public:
    static const int NUM_BUCKETS = 100;
    static const unsigned long HUNDRED_MS_US = 100000;

    TransitionStats() {
        reset();
    }

    /**
     * Record a transition given the RTC's microsecond value (tv_usec).
     * Jitter is computed as tv_usec % 100000, converted to milliseconds.
     */
    void recordTransition(unsigned long tvUsec) {
        int jitterMs = (tvUsec % HUNDRED_MS_US) / 1000;
        
        int bucket = jitterMs;
        if (bucket >= NUM_BUCKETS) bucket = NUM_BUCKETS - 1;
        
        histogram_[bucket]++;
        totalCount_++;
        jitterSumMs_ += jitterMs;
        if (jitterMs > 0) nonZeroCount_++;
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
                return i + 1;
            }
        }
        return NUM_BUCKETS;
    }

    unsigned long getNonZeroCount() const { return nonZeroCount_; }
    unsigned long getTotalCount() const { return totalCount_; }
    
    /** @return average jitter in milliseconds */
    float getAverageJitter() const {
        if (totalCount_ == 0) return 0.0f;
        return (float)jitterSumMs_ / totalCount_;
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
        jitterSumMs_ = 0;
        resetThisMinute_ = false;
    }

private:
    unsigned long histogram_[NUM_BUCKETS];
    unsigned long totalCount_;
    unsigned long nonZeroCount_;
    unsigned long jitterSumMs_;
    bool resetThisMinute_;
};

#endif // TRANSITION_STATS_H

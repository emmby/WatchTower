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
 * Stats reset at midnight each day (local time), with the previous day's summary
 * saved to a 7-day rolling history.
 */
class TransitionStats {
public:
    static const int NUM_BUCKETS = 100;
    static const unsigned long HUNDRED_MS_US = 100000;
    static const int HISTORY_DAYS = 7;

    struct DailySummary {
        unsigned long n;
        unsigned long nz;
        float avg;
        int p90;
        int p95;
        int p99;
        int p999;
        int p100;
        bool valid;
    };

    TransitionStats() {
        reset();
        for (int i = 0; i < HISTORY_DAYS; i++) {
            history_[i].valid = false;
        }
        historyCount_ = 0;
    }

    /**
     * Record a transition given the RTC's microsecond value (tv_usec).
     * Jitter is computed as tv_usec % 100000, converted to milliseconds.
     */
    void recordTransition(unsigned long tvUsec, int second = -1) {
        int jitterMs = (tvUsec % HUNDRED_MS_US) / 1000;
        
        int bucket = jitterMs;
        if (bucket >= NUM_BUCKETS) bucket = NUM_BUCKETS - 1;
        
        histogram_[bucket]++;
        totalCount_++;
        jitterSumMs_ += jitterMs;
        if (jitterMs > 0) {
            nonZeroCount_++;
        }
    }

    /**
     * Called once per minute. Checks for local midnight reset.
     * Saves the current day's summary to rolling history before clearing.
     */
    void onMinuteBoundary(int localHour, int localMinute) {
        // Check local midnight reset
        bool isNearMidnight = (localHour == 0 && localMinute == 0);
        if (isNearMidnight && !resetThisMinute_) {
            // Save today's summary to history before resetting
            if (totalCount_ > 0) {
                // Shift history (oldest falls off)
                for (int i = HISTORY_DAYS - 1; i > 0; i--) {
                    history_[i] = history_[i - 1];
                }
                history_[0].n = totalCount_;
                history_[0].nz = nonZeroCount_;
                history_[0].avg = getAverageJitter();
                history_[0].p90 = getPercentile(90);
                history_[0].p95 = getPercentile(95);
                history_[0].p99 = getPercentile(99);
                history_[0].p999 = getPermille(999);
                history_[0].p100 = getPercentile(100);
                history_[0].valid = true;
                if (historyCount_ < HISTORY_DAYS) historyCount_++;
            }
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

    /**
     * Compute the approximate P-th permille from the histogram.
     * @param p permille value (e.g., 999 for 99.9th percentile)
     * @return jitter in ms at that permille (upper bound of bucket)
     */
    int getPermille(int p) const {
        if (totalCount_ == 0) return 0;
        
        unsigned long threshold = ((unsigned long)totalCount_ * p + 999) / 1000;
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

    int getHistoryCount() const { return historyCount_; }
    const DailySummary& getHistory(int daysAgo) const { return history_[daysAgo]; }

    /**
     * Format stats + sparse histogram + daily history for ESPUI transfer.
     * Format: "n=N|nz=NZ|avg=A|p90=P|p95=P|p99=P|p999=P|p100=P|bucket:count,...||dn,dnz,davg,dp90,dp95,dp99,dp999,dp100||..."
     * Only nonzero buckets are included. History entries separated by ||.
     */
    int formatForUI(char* buf, int bufSize) const {
        int pos = snprintf(buf, bufSize, "n=%lu|nz=%lu|avg=%.1f|p90=%d|p95=%d|p99=%d|p999=%d|p100=%d|",
            totalCount_, nonZeroCount_, getAverageJitter(),
            getPercentile(90), getPercentile(95), getPercentile(99), getPermille(999),
            getPercentile(100));
        
        bool first = true;
        for (int i = 0; i < NUM_BUCKETS && pos < bufSize - 1; i++) {
            if (histogram_[i] > 0) {
                if (!first) {
                    pos += snprintf(buf + pos, bufSize - pos, ",");
                }
                pos += snprintf(buf + pos, bufSize - pos, "%d:%lu", i, histogram_[i]);
                first = false;
            }
        }
        
        // Append daily history
        for (int d = 0; d < historyCount_ && pos < bufSize - 1; d++) {
            pos += snprintf(buf + pos, bufSize - pos, "||%lu,%lu,%.1f,%d,%d,%d,%d,%d",
                history_[d].n, history_[d].nz, history_[d].avg,
                history_[d].p90, history_[d].p95, history_[d].p99, history_[d].p999,
                history_[d].p100);
        }
        return pos;
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
    DailySummary history_[HISTORY_DAYS];
    int historyCount_;
};

#endif // TRANSITION_STATS_H

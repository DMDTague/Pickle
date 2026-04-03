#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

// Retrieves the current time in milliseconds using the OS clock
long long get_time_ms();

struct TimeManager {
    long long start_time;
    long long stop_time;
    int depth_limit;
    bool time_is_up;
    bool stopped; // external flag to forcefully stop searching
};

// Global TimeManger instance
extern TimeManager tm;

// Initialize the time manager with specific limits.
// Setting elements to -1 implies "infinite"
void set_time_limits(long long time_left, long long inc, long long movetime, int depth);

// Read the OS clock and trip time_is_up flag if exceeded
void check_time();

#endif // TIME_MANAGER_H

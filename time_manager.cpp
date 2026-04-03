#include "time_manager.h"
#include <chrono>

TimeManager tm;

long long get_time_ms() {
    auto now = std::chrono::time_point_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now()
    );
    return now.time_since_epoch().count();
}

void set_time_limits(long long time_left, long long inc, long long movetime, int depth) {
    tm.stopped = false;
    tm.time_is_up = false;
    tm.start_time = get_time_ms();
    tm.depth_limit = depth;

    if (movetime > 0) {
        tm.stop_time = tm.start_time + movetime;
    } else if (time_left > 0) {
        // Allocate ~30 moves remaining + exact increment for safety
        long long allocated_time = (time_left / 30) + inc;
        // Never exceed actual time left, keep a 50ms buffer
        if (allocated_time > time_left - 50) {
            allocated_time = time_left - 50;
        }
        if (allocated_time < 0) allocated_time = 0;
        
        tm.stop_time = tm.start_time + allocated_time;
    } else {
        // Infinite time search (will be stopped externally or heavily bounded by depth)
        tm.stop_time = -1;
    }
}

void check_time() {
    if (tm.stopped) {
        tm.time_is_up = true;
        return;
    }
    
    // Only check if we actually have a stop limit
    if (tm.stop_time != -1) {
        if (get_time_ms() >= tm.stop_time) {
            tm.time_is_up = true;
            tm.stopped = true;
        }
    }
}

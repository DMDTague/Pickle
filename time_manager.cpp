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
        tm.optimum_time = movetime;
        tm.max_time = movetime;
        tm.stop_time = tm.start_time + movetime;
    } else if (time_left > 0) {
        // Allocate ~30 moves remaining + exact increment for safety
        tm.optimum_time = (time_left / 30) + (inc / 2);
        tm.max_time = (time_left / 10) + inc;

        if (tm.optimum_time > time_left - 50) tm.optimum_time = time_left - 50;
        if (tm.max_time > time_left - 50) tm.max_time = time_left - 50;
        
        if (tm.optimum_time < 0) tm.optimum_time = 0;
        if (tm.max_time < 0) tm.max_time = 0;
        
        tm.stop_time = tm.start_time + tm.max_time; // Hard stop check_time triggers on max
    } else {
        // Infinite time search (will be stopped externally or heavily bounded by depth)
        tm.optimum_time = -1;
        tm.max_time = -1;
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

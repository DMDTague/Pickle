#include <chrono>
#include "time_manager.h"

TimeManager search_timer;

long long get_time_ms() {
    auto now = std::chrono::time_point_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now()
    );
    return now.time_since_epoch().count();
}

void set_time_limits(long long time_left, long long inc, long long movetime, int depth) {
    search_timer.stopped = false;
    search_timer.time_is_up = false;
    search_timer.start_time = get_time_ms();
    search_timer.depth_limit = depth;

    if (movetime > 0) {
        search_timer.optimum_time = movetime;
        search_timer.max_time = movetime;
        search_timer.stop_time = search_timer.start_time + movetime;
    } else if (time_left > 0) {
        // Allocate roughly one move from a 30-move horizon plus part of increment.
        search_timer.optimum_time = (time_left / 30) + (inc / 2);
        search_timer.max_time = (time_left / 10) + inc;

        if (search_timer.optimum_time > time_left - 50) search_timer.optimum_time = time_left - 50;
        if (search_timer.max_time > time_left - 50) search_timer.max_time = time_left - 50;

        if (search_timer.optimum_time < 0) search_timer.optimum_time = 0;
        if (search_timer.max_time < 0) search_timer.max_time = 0;

        search_timer.stop_time = search_timer.start_time + search_timer.max_time;
    } else {
        search_timer.optimum_time = -1;
        search_timer.max_time = -1;
        search_timer.stop_time = -1;
    }
}

void check_time() {
    if (search_timer.stopped) {
        search_timer.time_is_up = true;
        return;
    }

    if (search_timer.stop_time != -1 && get_time_ms() >= search_timer.stop_time) {
        search_timer.time_is_up = true;
        search_timer.stopped = true;
    }
}

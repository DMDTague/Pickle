#include "attacks.h"
#include "board.h"
#include "evaluate.h"
#include "magics.h"
#include "opening_book.h"
#include "search.h"
#include "time_manager.h"
#include "tt.h"
#include "uci.h"
#include "zobrist.h"
#include <algorithm>
#include <memory>
#include <string>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#define PICKLE_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define PICKLE_EXPORT
#endif

namespace {
bool initialized = false;
std::string last_move_string = "0000";

void ensure_initialized() {
    if (initialized) return;
    init_leapers();
    init_sliders();
    init_mvv_lva();
    init_zobrist();
    init_opening_book();
    init_tt(16);
    initialized = true;
}

std::unique_ptr<Board> board_from_fen(const char* fen) {
    auto board = std::make_unique<Board>();
    board->parse_fen(fen ? std::string(fen) : std::string());
    return board;
}
} // namespace

extern "C" {

PICKLE_EXPORT void pickle_init() {
    ensure_initialized();
}

PICKLE_EXPORT const char* pickle_mate_in_one(const char* fen) {
    ensure_initialized();
    auto board = board_from_fen(fen);
    last_move_string = move_to_string(find_immediate_mate(*board));
    return last_move_string.c_str();
}

PICKLE_EXPORT const char* pickle_best_move(const char* fen, int depth, int movetime_ms) {
    ensure_initialized();
    auto board = board_from_fen(fen);

    depth = std::clamp(depth, 1, 16);
    movetime_ms = std::clamp(movetime_ms, 50, 5000);
    set_time_limits(-1, 0, movetime_ms, depth);

    Move move = search_best_move(*board, depth, false);
    last_move_string = move_to_string(move);
    return last_move_string.c_str();
}

PICKLE_EXPORT int pickle_eval(const char* fen) {
    ensure_initialized();
    auto board = board_from_fen(fen);
    return evaluate(*board);
}

PICKLE_EXPORT int pickle_last_score() {
    return last_search_score;
}

PICKLE_EXPORT int pickle_last_depth() {
    return last_search_depth;
}

PICKLE_EXPORT int pickle_last_nodes() {
    constexpr U64 JS_SAFE_NODE_CAP = 2147483647ULL;
    return static_cast<int>(std::min(nodes_searched, JS_SAFE_NODE_CAP));
}

PICKLE_EXPORT int pickle_last_source() {
    return last_search_source;
}

} // extern "C"

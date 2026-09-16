#include "tt.h"
#include <algorithm>
#include <iostream>

std::vector<TTEntry> TT;

void clear_tt() {
    for (auto& entry : TT) {
        entry.key = 0;
        entry.depth = -1;
        entry.flag = TT_ALPHA;
        entry.score = 0;
        entry.best_move = 0;
    }
}

void init_tt(int size_mb) {
    size_mb = std::max(1, size_mb);
    std::size_t total_bytes = static_cast<std::size_t>(size_mb) * 1024ULL * 1024ULL;
    std::size_t table_size = std::max<std::size_t>(1, total_bytes / sizeof(TTEntry));
    TT.resize(table_size);
    clear_tt();
}

int probe_tt(U64 hash, int depth, int alpha, int beta, Move& tt_move) {
    if (TT.empty()) return TT_UNKNOWN;

    TTEntry& entry = TT[hash % TT.size()];
    if (entry.key != hash) return TT_UNKNOWN;

    tt_move = entry.best_move;
    if (entry.depth < depth) return TT_UNKNOWN;

    if (entry.flag == TT_EXACT) return entry.score;
    if (entry.flag == TT_ALPHA && entry.score <= alpha) return alpha;
    if (entry.flag == TT_BETA && entry.score >= beta) return beta;
    return TT_UNKNOWN;
}

void record_tt(U64 hash, int depth, int flag, int score, Move best_move) {
    if (TT.empty()) return;

    TTEntry& entry = TT[hash % TT.size()];

    // Keep a deeper unrelated entry unless the incoming result is close enough
    // in depth to be useful. Exact entries get a small replacement preference.
    bool same_position = entry.key == hash;
    bool empty = entry.key == 0;
    bool deeper_or_close = depth + (flag == TT_EXACT ? 1 : 0) >= entry.depth - 1;
    if (!empty && !same_position && !deeper_or_close) return;

    entry.key = hash;
    entry.depth = depth;
    entry.flag = flag;
    entry.score = score;
    entry.best_move = best_move;
}

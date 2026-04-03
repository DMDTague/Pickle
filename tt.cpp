#include "tt.h"
#include <iostream>

std::vector<TTEntry> TT;

void init_tt(int size_mb) {
    int entry_bytes = sizeof(TTEntry);
    int total_bytes = size_mb * 1024 * 1024;
    int table_size = total_bytes / entry_bytes;
    
    std::cout << "Allocating Transposition Table: " << size_mb << " MB (" << table_size << " entries)\n";
    TT.resize(table_size);
    for(size_t i = 0; i < TT.size(); i++) {
        TT[i].key = 0;
        TT[i].depth = 0;
        TT[i].flag = 0;
        TT[i].score = 0;
        TT[i].best_move = 0;
    }
}

int probe_tt(U64 hash, int depth, int alpha, int beta, Move& tt_move) {
    if (TT.empty()) return TT_UNKNOWN;

    // Fast pseudo-modulo
    TTEntry& entry = TT[hash % TT.size()];

    if (entry.key == hash) {
        // Even if we can't use the score, we can pull the cached best move for move sorting later
        tt_move = entry.best_move;

        // If the depth cached is deep enough to trust
        if (entry.depth >= depth) {
            if (entry.flag == TT_EXACT) {
                return entry.score;
            }
            if (entry.flag == TT_ALPHA && entry.score <= alpha) {
                return alpha; // Fail low cutoff
            }
            if (entry.flag == TT_BETA && entry.score >= beta) {
                return beta; // Fail high cutoff
            }
        }
    }

    return TT_UNKNOWN;
}

void record_tt(U64 hash, int depth, int flag, int score, Move best_move) {
    if (TT.empty()) return;

    TTEntry& entry = TT[hash % TT.size()];
    
    // Simple "Always Replace" scheme
    // We override everything natively; this ensures the table isn't filled with stale shallow data.
    // In more advanced engines, we might prefer "Replace if deeper" or "Two-Tier".
    entry.key = hash;
    entry.depth = depth;
    entry.flag = flag;
    entry.score = score;
    entry.best_move = best_move;
}

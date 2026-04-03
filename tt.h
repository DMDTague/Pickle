#ifndef TT_H
#define TT_H

#include "board.h"
#include "move.h"
#include <cstdint>
#include <vector>

const int TT_UNKNOWN = -1000000;

enum TTFlag {
    TT_EXACT,
    TT_ALPHA,
    TT_BETA
};

struct TTEntry {
    U64 key;
    int depth;
    int flag;
    int score;
    Move best_move;
};

// Global Transposition Table
extern std::vector<TTEntry> TT;

// Allocate the table by size in Megabytes
void init_tt(int size_mb);

// Returns TT_UNKNOWN if no valid cutoff score is found.
// Fills tt_move with the cached best move (even if score is invalid) for move ordering.
int probe_tt(U64 hash, int depth, int alpha, int beta, Move& tt_move);

// Record a completed search node into the table
void record_tt(U64 hash, int depth, int flag, int score, Move best_move);

#endif // TT_H

#ifndef TT_H
#define TT_H

#include "board.h"
#include "move.h"
#include <cstdint>
#include <vector>

constexpr int TT_UNKNOWN = -1000000;

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

extern std::vector<TTEntry> TT;

void init_tt(int size_mb);
void clear_tt();
int probe_tt(U64 hash, int depth, int alpha, int beta, Move& tt_move, int search_ply);
void record_tt(U64 hash, int depth, int flag, int score, Move best_move, int search_ply);

#endif // TT_H

#ifndef SEARCH_H
#define SEARCH_H

#include "board.h"
#include "move.h"
#include <cstdint>

enum SearchSource {
    SEARCH_SOURCE_SEARCH = 0,
    SEARCH_SOURCE_MATE = 1,
    SEARCH_SOURCE_BOOK = 2,
};

extern U64 nodes_searched;
extern Move best_move;
extern Move previous_best_move;
extern int last_search_score;
extern int last_search_depth;
extern int last_search_source;

extern Move killer_moves[2][MAX_PLY];
extern int history_moves[12][64];

void init_mvv_lva();
void clear_heuristics();

// Returns a legal mate-in-one move, preferring the strongest promotion when
// several immediate mates exist. Returns 0 when there is no mate in one.
Move find_immediate_mate(Board& board);

// Tactical leaf search. In-check nodes search legal evasions instead of using
// stand-pat, which is essential for tactical correctness.
int quiescence(int alpha, int beta, Board& board, int search_ply = 0, int qs_ply = 0);

// Principal-variation negamax with alpha-beta, TT, null move, LMR, killers,
// history ordering, check extensions, and conservative shallow pruning.
int negamax(int depth, int alpha, int beta, Board& board, int search_ply, bool can_null_move = true);

// Search without printing a UCI bestmove line. Checks immediate mate and the
// built-in opening book before iterative deepening.
Move search_best_move(Board& board, int depth, bool print_info = false);

// UCI-facing wrapper.
void search_position(Board& board, int depth);

#endif // SEARCH_H

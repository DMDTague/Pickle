#ifndef SEARCH_H
#define SEARCH_H

#include "board.h"
#include "move.h"
#include <cstdint>

extern U64 nodes_searched;
extern Move best_move;
extern Move previous_best_move;
extern int last_search_score;
extern int last_search_depth;

extern Move killer_moves[2][MAX_PLY];
extern int history_moves[12][64];

void init_mvv_lva();
void clear_heuristics();

// Tactical leaf search. In-check nodes search legal evasions instead of using
// stand-pat, which is essential for tactical correctness.
int quiescence(int alpha, int beta, Board& board, int qs_ply = 0);

// Principal-variation negamax with alpha-beta, TT, null move, LMR, killers,
// history ordering, check extensions, and conservative shallow pruning.
int negamax(int depth, int alpha, int beta, Board& board, int search_ply, bool can_null_move = true);

// Search without printing a UCI bestmove line. Useful for WebAssembly and tests.
Move search_best_move(Board& board, int depth, bool print_info = false);

// UCI-facing wrapper.
void search_position(Board& board, int depth);

#endif // SEARCH_H

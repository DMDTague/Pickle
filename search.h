#ifndef SEARCH_H
#define SEARCH_H

#include "board.h"
#include "move.h"
#include <cstdint>

// -----------------------------------------------------------------------------
// Global Search Variables
// -----------------------------------------------------------------------------
extern U64 nodes_searched;
extern Move best_move;
extern Move previous_best_move;

extern Move killer_moves[2][MAX_PLY];
extern int history_moves[12][64];

// -----------------------------------------------------------------------------
// Search Functions
// -----------------------------------------------------------------------------
void init_mvv_lva();
void clear_heuristics();
// Quiescence Search: resolves all tactical captures to prevent the horizon effect.
int quiescence(int alpha, int beta, Board& board, int qs_ply = 0);

// Negamax (Alpha-Beta): The core recursive search loop
// depth: remaining depth to search
// alpha: worst-case best score for the maximizing player
// beta: worst-case best score for the minimizing player
// board: reference to the game board
// search_ply: the current depth from the root node (0 = root)
// can_null_move: determines whether Null Move Pruning is permitted on this node
int negamax(int depth, int alpha, int beta, Board& board, int search_ply, bool can_null_move = true);

// Main wrapper to kick off the search
void search_position(Board& board, int depth);

#endif // SEARCH_H

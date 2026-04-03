#ifndef MOVEGEN_H
#define MOVEGEN_H

#include "board.h"
#include "move.h"

// -----------------------------------------------------------------------------
// Move Generation for pseudo-legal moves
// -----------------------------------------------------------------------------

// Populate the move list with all pseudo-legal moves for the current side to move
void generate_moves(const Board& board, MoveList& move_list);

#endif // MOVEGEN_H

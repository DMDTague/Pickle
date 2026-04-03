#ifndef EVALUATE_H
#define EVALUATE_H

#include "board.h"

// -----------------------------------------------------------------------------
// Material Values
// -----------------------------------------------------------------------------
// Base values for the pieces (in centipawns: 100 = 1 pawn)
const int MATERIAL_PAWN   = 100;
const int MATERIAL_KNIGHT = 320;
const int MATERIAL_BISHOP = 330;
const int MATERIAL_ROOK   = 500;
const int MATERIAL_QUEEN  = 900;
const int MATERIAL_KING   = 20000;

// Array for quick lookup by PieceType index
const int MATERIAL_VALUES[6] = {
    MATERIAL_PAWN,
    MATERIAL_KNIGHT,
    MATERIAL_BISHOP,
    MATERIAL_ROOK,
    MATERIAL_QUEEN,
    MATERIAL_KING
};

// -----------------------------------------------------------------------------
// Evaluation Function
// -----------------------------------------------------------------------------
// Evaluates the static score of the board.
// Returns a positive score if the side to move is winning, negative if losing.
int evaluate(const Board& board);

#endif // EVALUATE_H

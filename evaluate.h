#ifndef EVALUATE_H
#define EVALUATE_H

#include "board.h"

// Centipawn material values. Pickle's personality now comes from positional
// pressure and initiative rather than changing a piece's value based on turn.
constexpr int MATERIAL_PAWN   = 100;
constexpr int MATERIAL_KNIGHT = 320;
constexpr int MATERIAL_BISHOP = 335;
constexpr int MATERIAL_ROOK   = 500;
constexpr int MATERIAL_QUEEN  = 950;
constexpr int MATERIAL_KING   = 20000;

constexpr int MATERIAL_VALUES[6] = {
    MATERIAL_PAWN,
    MATERIAL_KNIGHT,
    MATERIAL_BISHOP,
    MATERIAL_ROOK,
    MATERIAL_QUEEN,
    MATERIAL_KING
};

// Search uses a small draw preference instead of the old 1.5-pawn contempt.
// A huge contempt value made objectively equal positions look tactically lost.
constexpr int CONTEMPT_FACTOR = 12;

// Static evaluation from the side-to-move perspective.
int evaluate(const Board& board);

#endif // EVALUATE_H

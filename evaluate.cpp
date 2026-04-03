#include "evaluate.h"
#include "bit_utils.h"

// -----------------------------------------------------------------------------
// Piece-Square Tables (Midgame/Aggressive)
// Mapped as LERF: A1=0 ... H8=63
// Positive values are good for White, Negative are bad.
// Black pieces will use mirror_sq = sq ^ 56 to flip the board vertically.
// -----------------------------------------------------------------------------

const int pawn_pst[64] = {
      0,   0,   0,   0,   0,   0,   0,   0, // Rank 1
      5,  10,  10, -20, -20,  10,  10,   5, // Rank 2
      5,  -5, -10,   0,   0, -10,  -5,   5, // Rank 3
      0,   0,   0,  20,  20,   0,   0,   0, // Rank 4
      5,   5,  10,  25,  25,  10,   5,   5, // Rank 5
     10,  10,  20,  30,  30,  20,  10,  10, // Rank 6
     50,  50,  50,  50,  50,  50,  50,  50, // Rank 7
      0,   0,   0,   0,   0,   0,   0,   0  // Rank 8 (Promoted already)
};

const int knight_pst[64] = {
    -50, -40, -30, -30, -30, -30, -40, -50,
    -40, -20,   0,   5,   5,   0, -20, -40,
    -30,   5,  10,  15,  15,  10,   5, -30,
    -30,   0,  15,  20,  20,  15,   0, -30,
    -30,   5,  15,  20,  20,  15,   5, -30,
    -30,   0,  10,  15,  15,  10,   0, -30,
    -40, -20,   0,   0,   0,   0, -20, -40,
    -50, -40, -30, -30, -30, -30, -40, -50
};

const int bishop_pst[64] = {
    -20, -10, -10, -10, -10, -10, -10, -20,
    -10,   5,   0,   0,   0,   0,   5, -10,
    -10,  10,  10,  10,  10,  10,  10, -10,
    -10,   0,  10,  10,  10,  10,   0, -10,
    -10,   5,   5,  10,  10,   5,   5, -10,
    -10,   0,   5,  10,  10,   5,   0, -10,
    -10,   0,   0,   0,   0,   0,   0, -10,
    -20, -10, -10, -10, -10, -10, -10, -20
};

const int rook_pst[64] = {
      0,   0,   0,   5,   5,   0,   0,   0,
     -5,   0,   0,   0,   0,   0,   0,  -5,
     -5,   0,   0,   0,   0,   0,   0,  -5,
     -5,   0,   0,   0,   0,   0,   0,  -5,
     -5,   0,   0,   0,   0,   0,   0,  -5,
     -5,   0,   0,   0,   0,   0,   0,  -5,
      5,  10,  10,  10,  10,  10,  10,   5, // Rook on 7th is extremely aggressive
      0,   0,   0,   0,   0,   0,   0,   0
};

const int queen_pst[64] = {
    -20, -10, -10,  -5,  -5, -10, -10, -20,
    -10,   0,   0,   0,   0,   0,   0, -10,
    -10,   0,   5,   5,   5,   5,   0, -10,
     -5,   0,   5,   5,   5,   5,   0,  -5,
      0,   0,   5,   5,   5,   5,   0,  -5,
    -10,   5,   5,   5,   5,   5,   0, -10,
    -10,   0,   5,   0,   0,   0,   0, -10,
    -20, -10, -10,  -5,  -5, -10, -10, -20
};

const int king_pst[64] = {
     20,  30,  10,   0,   0,  10,  30,  20,
     20,  20,   0,   0,   0,   0,  20,  20,
    -10, -20, -20, -20, -20, -20, -20, -10,
    -20, -30, -30, -40, -40, -30, -30, -20,
    -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30
};

// Map of pointers for rapid iteration
const int* pst[6] = {
    pawn_pst,
    knight_pst,
    bishop_pst,
    rook_pst,
    queen_pst,
    king_pst
};

// -----------------------------------------------------------------------------
// Core Evaluate implementation
// -----------------------------------------------------------------------------

int evaluate(const Board& board) {
    int score = 0;

    // Evaluate White pieces
    Bitboard white_occ = board.get_color_bitboard(WHITE);
    for (int pt = 0; pt < PIECE_TYPE_COUNT; ++pt) {
        Bitboard pieces = board.get_piece_bitboard((PieceType)pt) & white_occ;
        while (pieces) {
            int sq = lsb(pieces);
            score += MATERIAL_VALUES[pt];
            score += pst[pt][sq];
            pieces &= ~(1ULL << sq);
        }
    }

    // Evaluate Black pieces
    Bitboard black_occ = board.get_color_bitboard(BLACK);
    for (int pt = 0; pt < PIECE_TYPE_COUNT; ++pt) {
        Bitboard pieces = board.get_piece_bitboard((PieceType)pt) & black_occ;
        while (pieces) {
            int sq = lsb(pieces);
            int mirror_sq = sq ^ 56; // Flip the board vertically for Black pieces
            score -= MATERIAL_VALUES[pt];
            score -= pst[pt][mirror_sq];
            pieces &= ~(1ULL << sq);
        }
    }

    // Orient relative to side to move
    return (board.get_side_to_move() == WHITE) ? score : -score;
}

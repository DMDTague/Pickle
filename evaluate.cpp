#include "evaluate.h"
#include "bit_utils.h"
#include "attacks.h"
#include "magics.h"

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
    Color us = board.get_side_to_move();
    
    // We compute the raw material & PST score from White's perspective initially, 
    // but the multipliers apply based on who the side to move is.
    int white_multiplier = (us == WHITE) ? OWN_PIECE_DEVAL_PCT : 100;
    int black_multiplier = (us == BLACK) ? OWN_PIECE_DEVAL_PCT : 100;

    int white_material = 0;
    int black_material = 0;
    
    int white_minor_count = 0;
    int black_minor_count = 0;
    int white_rook_count = 0;
    int black_rook_count = 0;

    Bitboard white_occ = board.get_color_bitboard(WHITE);
    Bitboard black_occ = board.get_color_bitboard(BLACK);
    Bitboard all_occ = white_occ | black_occ;

    int w_king_lsb = lsb(board.get_piece_bitboard(KING) & white_occ);
    int b_king_lsb = lsb(board.get_piece_bitboard(KING) & black_occ);
    Square w_king_sq = (w_king_lsb != -1) ? (Square)w_king_lsb : SQUARE_COUNT;
    Square b_king_sq = (b_king_lsb != -1) ? (Square)b_king_lsb : SQUARE_COUNT;
    
    auto get_ring = [](Square sq) {
        if (sq < 0 || sq >= 64) return 0ULL;
        Bitboard ring = king_attacks[sq];
        Bitboard ring2 = ring;
        Bitboard copy = ring;
        while(copy) {
            int s = lsb(copy);
            ring2 |= king_attacks[s];
            copy &= ~(1ULL << s);
        }
        return ring2;
    };
    Bitboard w_king_zone = get_ring(w_king_sq);
    Bitboard b_king_zone = get_ring(b_king_sq);
    
    int white_mobility = 0;
    int black_mobility = 0;
    int white_king_attackers = 0;
    int black_king_attackers = 0;
    
    int white_initiative = 0;
    int black_initiative = 0;

    // Evaluate White pieces
    for (int pt = 0; pt < PIECE_TYPE_COUNT; ++pt) {
        Bitboard pieces = board.get_piece_bitboard((PieceType)pt) & white_occ;
        if (pt == KNIGHT || pt == BISHOP) white_minor_count += pop_count(pieces);
        if (pt == ROOK) white_rook_count += pop_count(pieces);
        
        while (pieces) {
            int sq = lsb(pieces);
            white_material += MATERIAL_VALUES[pt];
            white_material += pst[pt][sq];
            
            // Mobility and Attacks
            Bitboard attacks = 0;
            if (pt == KNIGHT) attacks = knight_attacks[sq];
            else if (pt == BISHOP) attacks = get_bishop_attacks(sq, all_occ);
            else if (pt == ROOK) attacks = get_rook_attacks(sq, all_occ);
            else if (pt == QUEEN) attacks = get_queen_attacks(sq, all_occ);
            
            white_mobility += pop_count(attacks & ~white_occ);
            
            // King Hunt
            if (attacks & b_king_zone) {
                 white_king_attackers++;
            }
            
            // Initiative: Attacking enemy higher-value pieces or checking
            if (pt != QUEEN && pt != KING) {
                if (attacks & (board.get_piece_bitboard(QUEEN) & black_occ)) white_initiative++;
                if (pt != ROOK && (attacks & (board.get_piece_bitboard(ROOK) & black_occ))) white_initiative++;
            }
            if (pt != PAWN && b_king_sq >= 0 && b_king_sq < 64 && (attacks & (1ULL << b_king_sq))) white_initiative += 2; // Check
            
            pieces &= ~(1ULL << sq);
        }
    }

    // Evaluate Black pieces
    for (int pt = 0; pt < PIECE_TYPE_COUNT; ++pt) {
        Bitboard pieces = board.get_piece_bitboard((PieceType)pt) & black_occ;
        if (pt == KNIGHT || pt == BISHOP) black_minor_count += pop_count(pieces);
        if (pt == ROOK) black_rook_count += pop_count(pieces);

        while (pieces) {
            int sq = lsb(pieces);
            int mirror_sq = sq ^ 56;
            black_material += MATERIAL_VALUES[pt];
            black_material += pst[pt][mirror_sq];
            
            // Mobility and Attacks
            Bitboard attacks = 0;
            if (pt == KNIGHT) attacks = knight_attacks[sq];
            else if (pt == BISHOP) attacks = get_bishop_attacks(sq, all_occ);
            else if (pt == ROOK) attacks = get_rook_attacks(sq, all_occ);
            else if (pt == QUEEN) attacks = get_queen_attacks(sq, all_occ);
            
            black_mobility += pop_count(attacks & ~black_occ);
            
            // King Hunt
            if (attacks & w_king_zone) {
                 black_king_attackers++;
            }
            
            // Initiative
            if (pt != QUEEN && pt != KING) {
                if (attacks & (board.get_piece_bitboard(QUEEN) & white_occ)) black_initiative++;
                if (pt != ROOK && (attacks & (board.get_piece_bitboard(ROOK) & white_occ))) black_initiative++;
            }
            if (pt != PAWN && w_king_sq >= 0 && w_king_sq < 64 && (attacks & (1ULL << w_king_sq))) black_initiative += 2; // Check
            
            pieces &= ~(1ULL << sq);
        }
    }

    // Apply Material & Devaluation
    white_material = (white_material * white_multiplier) / 100;
    black_material = (black_material * black_multiplier) / 100;
    score += (white_material - black_material);
    
    // Apply Mobility
    score += (white_mobility - black_mobility) * MOBILITY_WEIGHT;
    
    // Apply Initiative
    score += (white_initiative - black_initiative) * INITIATIVE_BONUS;

    // Coordination Scaling
    if (white_king_attackers > 0) {
        score += (white_king_attackers * white_king_attackers * KING_HUNT_BONUS) / 2;
    }
    if (black_king_attackers > 0) {
        score -= (black_king_attackers * black_king_attackers * KING_HUNT_BONUS) / 2;
    }

    // Asymmetry
    if (white_minor_count != black_minor_count || white_rook_count != black_rook_count) {
        if (us == WHITE) score += ASYMMETRY_BONUS;
        else score -= ASYMMETRY_BONUS;
    }

    // Pawn Storming Enemy Shield
    Bitboard white_pawns = board.get_piece_bitboard(PAWN) & white_occ;
    Bitboard black_pawns = board.get_piece_bitboard(PAWN) & black_occ;
    
    Bitboard w_king_shield = (w_king_sq >= 0 && w_king_sq < 64) ? (king_attacks[w_king_sq] & white_pawns) : 0;
    Bitboard b_king_shield = (b_king_sq >= 0 && b_king_sq < 64) ? (king_attacks[b_king_sq] & black_pawns) : 0;
    
    // If white pawns attack black's king shield
    Bitboard w_pawn_attacks = ((white_pawns << 9) & 0xFEFEFEFEFEFEFEFEULL) | ((white_pawns << 7) & 0x7F7F7F7F7F7F7F7FULL);
    if (w_pawn_attacks & b_king_shield) score += PAWN_STORM_BONUS;
    
    // If black pawns attack white's king shield
    Bitboard b_pawn_attacks = ((black_pawns >> 9) & 0x7F7F7F7F7F7F7F7FULL) | ((black_pawns >> 7) & 0xFEFEFEFEFEFEFEFEULL);
    if (b_pawn_attacks & w_king_shield) score -= PAWN_STORM_BONUS;

    // Orient relative to side to move
    return (us == WHITE) ? score : -score;
}

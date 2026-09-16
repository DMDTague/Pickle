#include "evaluate.h"
#include "bit_utils.h"
#include "attacks.h"
#include "magics.h"
#include <algorithm>
#include <cmath>

namespace {

constexpr U64 FILE_MASKS[8] = {
    0x0101010101010101ULL, 0x0202020202020202ULL,
    0x0404040404040404ULL, 0x0808080808080808ULL,
    0x1010101010101010ULL, 0x2020202020202020ULL,
    0x4040404040404040ULL, 0x8080808080808080ULL
};

constexpr int PHASE_MAX = 24;

// Midgame PSTs. These intentionally keep Pickle active and space-seeking, but
// the tactical personality is now backed by sound material accounting.
constexpr int PAWN_PST[64] = {
      0,   0,   0,   0,   0,   0,   0,   0,
      4,   8,   8, -10, -10,   8,   8,   4,
      3,  -2,  -4,  10,  10,  -4,  -2,   3,
      2,   4,   8,  18,  18,   8,   4,   2,
      8,  10,  16,  24,  24,  16,  10,   8,
     16,  18,  24,  32,  32,  24,  18,  16,
     48,  48,  48,  52,  52,  48,  48,  48,
      0,   0,   0,   0,   0,   0,   0,   0
};

constexpr int KNIGHT_PST[64] = {
    -45, -30, -20, -18, -18, -20, -30, -45,
    -30, -12,   0,   4,   4,   0, -12, -30,
    -20,   4,  12,  18,  18,  12,   4, -20,
    -18,   7,  18,  24,  24,  18,   7, -18,
    -18,   6,  18,  25,  25,  18,   6, -18,
    -20,   4,  12,  18,  18,  12,   4, -20,
    -30, -10,   0,   3,   3,   0, -10, -30,
    -45, -30, -20, -18, -18, -20, -30, -45
};

constexpr int BISHOP_PST[64] = {
    -18, -10, -10, -10, -10, -10, -10, -18,
    -10,   4,   2,   2,   2,   2,   4, -10,
    -10,   7,  10,  10,  10,  10,   7, -10,
    -10,   2,  10,  14,  14,  10,   2, -10,
    -10,   5,   8,  14,  14,   8,   5, -10,
    -10,   3,   8,  10,  10,   8,   3, -10,
    -10,   4,   2,   2,   2,   2,   4, -10,
    -18, -10, -10, -10, -10, -10, -10, -18
};

constexpr int ROOK_PST[64] = {
      0,   0,   2,   6,   6,   2,   0,   0,
     -4,   0,   0,   2,   2,   0,   0,  -4,
     -4,   0,   0,   2,   2,   0,   0,  -4,
     -4,   0,   0,   2,   2,   0,   0,  -4,
     -4,   0,   0,   2,   2,   0,   0,  -4,
     -2,   2,   4,   5,   5,   4,   2,  -2,
      8,  12,  12,  14,  14,  12,  12,   8,
      0,   0,   3,   7,   7,   3,   0,   0
};

constexpr int QUEEN_PST[64] = {
    -16, -10, -10,  -4,  -4, -10, -10, -16,
    -10,   0,   2,   2,   2,   2,   0, -10,
    -10,   2,   5,   5,   5,   5,   2, -10,
     -4,   2,   5,   7,   7,   5,   2,  -4,
     -4,   2,   5,   7,   7,   5,   2,  -4,
    -10,   2,   5,   5,   5,   5,   2, -10,
    -10,   0,   2,   2,   2,   2,   0, -10,
    -16, -10, -10,  -4,  -4, -10, -10, -16
};

constexpr int KING_MG_PST[64] = {
     24,  30,  12,   0,   0,  12,  30,  24,
     18,  18,   2,   0,   0,   2,  18,  18,
     -8, -12, -16, -20, -20, -16, -12,  -8,
    -16, -22, -28, -34, -34, -28, -22, -16,
    -22, -30, -36, -42, -42, -36, -30, -22,
    -26, -34, -40, -46, -46, -40, -34, -26,
    -30, -38, -44, -50, -50, -44, -38, -30,
    -30, -38, -44, -50, -50, -44, -38, -30
};

constexpr const int* PST[6] = {
    PAWN_PST, KNIGHT_PST, BISHOP_PST, ROOK_PST, QUEEN_PST, KING_MG_PST
};

struct EvalTerms {
    int mg = 0;
    int eg = 0;
};

inline int file_of(int sq) { return sq & 7; }
inline int rank_of(int sq) { return sq >> 3; }
inline int relative_sq(Color c, int sq) { return c == WHITE ? sq : (sq ^ 56); }
inline int relative_rank(Color c, int sq) { return c == WHITE ? rank_of(sq) : 7 - rank_of(sq); }

U64 pawn_attacks(U64 pawns, Color c) {
    if (c == WHITE) {
        return ((pawns << 7) & ~FILE_MASKS[7]) | ((pawns << 9) & ~FILE_MASKS[0]);
    }
    return ((pawns >> 7) & ~FILE_MASKS[0]) | ((pawns >> 9) & ~FILE_MASKS[7]);
}

U64 adjacent_file_mask(int file) {
    U64 mask = 0;
    if (file > 0) mask |= FILE_MASKS[file - 1];
    if (file < 7) mask |= FILE_MASKS[file + 1];
    return mask;
}

bool is_passed_pawn(int sq, Color c, U64 enemy_pawns) {
    const int file = file_of(sq);
    const int rank = rank_of(sq);
    U64 copy = enemy_pawns;
    while (copy) {
        int enemy_sq = lsb(copy);
        copy &= copy - 1;
        int ef = file_of(enemy_sq);
        int er = rank_of(enemy_sq);
        if (std::abs(ef - file) <= 1) {
            if ((c == WHITE && er > rank) || (c == BLACK && er < rank)) return false;
        }
    }
    return true;
}

bool protected_by_pawn(int sq, Color c, U64 friendly_pawns) {
    return (pawn_attacks(friendly_pawns, c) & (1ULL << sq)) != 0;
}

bool attacked_by_enemy_pawn(int sq, Color c, U64 enemy_pawns) {
    return (pawn_attacks(enemy_pawns, (Color)(1 - c)) & (1ULL << sq)) != 0;
}

int endgame_king_bonus(int relative_square) {
    int f = file_of(relative_square);
    int r = rank_of(relative_square);
    int center_distance = std::abs(f * 2 - 7) + std::abs(r * 2 - 7);
    return 28 - center_distance * 2;
}

int game_phase(const Board& board) {
    int phase = 0;
    phase += pop_count(board.get_piece_bitboard(KNIGHT)) * 1;
    phase += pop_count(board.get_piece_bitboard(BISHOP)) * 1;
    phase += pop_count(board.get_piece_bitboard(ROOK)) * 2;
    phase += pop_count(board.get_piece_bitboard(QUEEN)) * 4;
    return std::min(PHASE_MAX, phase);
}

EvalTerms evaluate_side(const Board& board, Color us) {
    EvalTerms out;
    const Color them = (Color)(1 - us);
    const U64 our_occ = board.get_color_bitboard(us);
    const U64 their_occ = board.get_color_bitboard(them);
    const U64 all_occ = our_occ | their_occ;
    const U64 our_pawns = board.get_piece_bitboard(PAWN) & our_occ;
    const U64 their_pawns = board.get_piece_bitboard(PAWN) & their_occ;

    int king_sq = lsb(board.get_piece_bitboard(KING) & our_occ);
    int enemy_king_sq = lsb(board.get_piece_bitboard(KING) & their_occ);
    U64 enemy_king_zone = 0;
    if (enemy_king_sq >= 0 && enemy_king_sq < 64) {
        enemy_king_zone = king_attacks[enemy_king_sq] | (1ULL << enemy_king_sq);
    }

    int bishop_count = 0;
    int king_attackers = 0;
    int king_zone_hits = 0;

    // Material, PST, mobility, outposts, rook files, and pressure.
    for (int pt = PAWN; pt <= KING; ++pt) {
        U64 bb = board.get_piece_bitboard((PieceType)pt) & our_occ;
        if (pt == BISHOP) bishop_count = pop_count(bb);

        while (bb) {
            const int sq = lsb(bb);
            bb &= bb - 1;
            const int rsq = relative_sq(us, sq);

            out.mg += MATERIAL_VALUES[pt];
            out.eg += MATERIAL_VALUES[pt];
            out.mg += PST[pt][rsq];
            out.eg += (pt == KING ? endgame_king_bonus(rsq) : PST[pt][rsq] / 2);

            U64 attacks = 0;
            int mobility_weight_mg = 0;
            int mobility_weight_eg = 0;
            if (pt == KNIGHT) {
                attacks = knight_attacks[sq];
                mobility_weight_mg = 4;
                mobility_weight_eg = 3;
            } else if (pt == BISHOP) {
                attacks = get_bishop_attacks(sq, all_occ);
                mobility_weight_mg = 4;
                mobility_weight_eg = 4;
            } else if (pt == ROOK) {
                attacks = get_rook_attacks(sq, all_occ);
                mobility_weight_mg = 2;
                mobility_weight_eg = 3;
            } else if (pt == QUEEN) {
                attacks = get_queen_attacks(sq, all_occ);
                mobility_weight_mg = 1;
                mobility_weight_eg = 2;
            } else if (pt == PAWN) {
                attacks = pawn_attacks(1ULL << sq, us);
            } else if (pt == KING) {
                attacks = king_attacks[sq];
            }

            if (pt >= KNIGHT && pt <= QUEEN) {
                int mobility = pop_count(attacks & ~our_occ);
                out.mg += mobility * mobility_weight_mg;
                out.eg += mobility * mobility_weight_eg;
            }

            if ((attacks & enemy_king_zone) && pt != KING) {
                king_attackers++;
                king_zone_hits += pop_count(attacks & enemy_king_zone);
            }

            // Stable knight outposts are one of Pickle's favorite ways to turn
            // pressure into a real positional asset instead of a speculative sac.
            if (pt == KNIGHT) {
                int rr = relative_rank(us, sq);
                if (rr >= 3 && rr <= 5 && protected_by_pawn(sq, us, our_pawns)
                    && !attacked_by_enemy_pawn(sq, us, their_pawns)) {
                    out.mg += 24;
                    out.eg += 14;
                }
            }

            if (pt == ROOK) {
                const int file = file_of(sq);
                bool friendly_pawn_on_file = (our_pawns & FILE_MASKS[file]) != 0;
                bool enemy_pawn_on_file = (their_pawns & FILE_MASKS[file]) != 0;
                if (!friendly_pawn_on_file && !enemy_pawn_on_file) {
                    out.mg += 20;
                    out.eg += 16;
                } else if (!friendly_pawn_on_file) {
                    out.mg += 11;
                    out.eg += 9;
                }
                if (relative_rank(us, sq) == 6) {
                    out.mg += 18;
                    out.eg += 28;
                }
            }
        }
    }

    if (bishop_count >= 2) {
        out.mg += 28;
        out.eg += 42;
    }

    // Pawn structure: doubled, isolated, connected, and passed pawns.
    for (int file = 0; file < 8; ++file) {
        int count = pop_count(our_pawns & FILE_MASKS[file]);
        if (count > 1) {
            out.mg -= (count - 1) * 13;
            out.eg -= (count - 1) * 18;
        }
    }

    static constexpr int PASSED_MG[8] = {0, 0, 8, 16, 30, 52, 86, 0};
    static constexpr int PASSED_EG[8] = {0, 0, 12, 24, 46, 82, 140, 0};

    U64 pawns = our_pawns;
    while (pawns) {
        int sq = lsb(pawns);
        pawns &= pawns - 1;
        int file = file_of(sq);
        int rr = relative_rank(us, sq);

        if ((our_pawns & adjacent_file_mask(file)) == 0) {
            out.mg -= 10;
            out.eg -= 13;
        }

        bool connected = false;
        U64 neighbors = our_pawns & adjacent_file_mask(file);
        while (neighbors) {
            int nsq = lsb(neighbors);
            neighbors &= neighbors - 1;
            if (std::abs(relative_rank(us, nsq) - rr) <= 1) {
                connected = true;
                break;
            }
        }
        if (connected) {
            out.mg += 5;
            out.eg += 8;
        }

        if (is_passed_pawn(sq, us, their_pawns)) {
            out.mg += PASSED_MG[rr];
            out.eg += PASSED_EG[rr];
            if (connected) {
                out.mg += 8 + rr * 2;
                out.eg += 12 + rr * 3;
            }
        }
    }

    // King safety. Shield and open-file penalties matter mainly in the middlegame.
    if (king_sq >= 0 && king_sq < 64) {
        int kf = file_of(king_sq);
        int kr = rank_of(king_sq);
        int shield = 0;
        int dir = us == WHITE ? 1 : -1;
        int shield_rank = kr + dir;
        if (shield_rank >= 0 && shield_rank < 8) {
            for (int df = -1; df <= 1; ++df) {
                int f = kf + df;
                if (f >= 0 && f < 8) {
                    int sq = shield_rank * 8 + f;
                    if (our_pawns & (1ULL << sq)) shield++;
                }
            }
        }
        out.mg += shield * 12;
        out.mg -= (3 - shield) * 8;

        for (int df = -1; df <= 1; ++df) {
            int f = kf + df;
            if (f < 0 || f >= 8) continue;
            bool own_pawn = (our_pawns & FILE_MASKS[f]) != 0;
            bool enemy_pawn = (their_pawns & FILE_MASKS[f]) != 0;
            if (!own_pawn && !enemy_pawn) out.mg -= 14;
            else if (!own_pawn) out.mg -= 7;
        }
    }

    // Pickle's signature: coordinated pressure grows non-linearly, but only when
    // real pieces are actually touching the enemy king zone.
    if (king_attackers > 0) {
        int pressure = king_attackers * king_attackers * 9 + king_zone_hits * 5;
        out.mg += pressure;
        out.eg += pressure / 4;
    }

    // Pawn storms are rewarded only when the pawns are already attacking the
    // king zone, rather than for merely existing on the same wing.
    if (enemy_king_zone) {
        int storm_hits = pop_count(pawn_attacks(our_pawns, us) & enemy_king_zone);
        out.mg += storm_hits * 14;
        out.eg += storm_hits * 4;
    }

    return out;
}

} // namespace

int evaluate(const Board& board) {
    EvalTerms white = evaluate_side(board, WHITE);
    EvalTerms black = evaluate_side(board, BLACK);

    int mg = white.mg - black.mg;
    int eg = white.eg - black.eg;
    int phase = game_phase(board);

    int score = (mg * phase + eg * (PHASE_MAX - phase)) / PHASE_MAX;

    // Small tempo bonus: enough to value initiative, nowhere near enough to
    // distort material or turn equal positions into fake sacrifices.
    score += board.get_side_to_move() == WHITE ? 10 : -10;

    return board.get_side_to_move() == WHITE ? score : -score;
}

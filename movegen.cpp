#include "movegen.h"
#include "attacks.h"
#include "magics.h"
#include "bit_utils.h"
#include <iostream>

// -----------------------------------------------------------------------------
// Move Generation: Pseudo-Legal moves for the current side
// -----------------------------------------------------------------------------

void generate_moves(const Board& board, MoveList& move_list) {
    Color side = board.get_side_to_move();
    Color them = (Color)(1 - side);
    Bitboard friend_occ = board.get_color_bitboard(side);
    Bitboard enemy_occ = board.get_color_bitboard(them);
    Bitboard all_occ = board.get_occupancy();
    Bitboard empty = ~all_occ;

    // Helper lambda to identify captured pieces
    auto get_victim = [&](int sq) -> int {
        for (int i = 0; i < 6; i++) {
            if (get_bit(board.get_piece_bitboard((PieceType)i), sq) && get_bit(enemy_occ, sq)) {
                return i;
            }
        }
        return PAWN; // Fallback
    };


    // -------------------------------------------------------------------------
    // Pawn Moves
    // -------------------------------------------------------------------------
    Bitboard pawns = board.get_piece_bitboard(PAWN) & friend_occ;
    
    if (side == WHITE) {
        // Single pushes
        Bitboard single_pushes = (pawns << 8) & empty;
        Bitboard sp = single_pushes;
        while (sp) {
            int to = lsb(sp);
            int from = to - 8;
            if (to >= A8) { // Promotion
                move_list.add_move(encode_move(from, to, PAWN, QUEEN, 0, 0, 0, 0, 0));
                move_list.add_move(encode_move(from, to, PAWN, ROOK, 0, 0, 0, 0, 0));
                move_list.add_move(encode_move(from, to, PAWN, BISHOP, 0, 0, 0, 0, 0));
                move_list.add_move(encode_move(from, to, PAWN, KNIGHT, 0, 0, 0, 0, 0));
            } else {
                move_list.add_move(encode_move(from, to, PAWN, 0, 0, 0, 0, 0, 0));
            }
            sp &= ~(1ULL << to);
        }

        // Double pushes
        Bitboard double_pushes = (((pawns & 0xFF00ULL) << 8) & empty) << 8 & empty;
        Bitboard dp = double_pushes;
        while (dp) {
            int to = lsb(dp);
            int from = to - 16;
            move_list.add_move(encode_move(from, to, PAWN, 0, 0, 1, 0, 0, 0));
            dp &= ~(1ULL << to);
        }

        // Captures
        // 0x7F7F7F7F7F7F7F7F is NOT_H_FILE (A-G)
        // 0xFEFEFEFEFEFEFEFE is NOT_A_FILE (B-H)
        Bitboard atks_east = (pawns << 9) & 0xFEFEFEFEFEFEFEFEULL;
        Bitboard caps_east = atks_east & enemy_occ;
        while (caps_east) {
            int to = lsb(caps_east);
            int from = to - 9;
            int vic = get_victim(to);
            if (to >= A8) {
                move_list.add_move(encode_move(from, to, PAWN, QUEEN, 1, 0, 0, 0, vic));
                move_list.add_move(encode_move(from, to, PAWN, ROOK, 1, 0, 0, 0, vic));
                move_list.add_move(encode_move(from, to, PAWN, BISHOP, 1, 0, 0, 0, vic));
                move_list.add_move(encode_move(from, to, PAWN, KNIGHT, 1, 0, 0, 0, vic));
            } else {
                move_list.add_move(encode_move(from, to, PAWN, 0, 1, 0, 0, 0, vic));
            }
            caps_east &= ~(1ULL << to);
        }

        Bitboard atks_west = (pawns << 7) & 0x7F7F7F7F7F7F7F7FULL;
        Bitboard caps_west = atks_west & enemy_occ;
        while (caps_west) {
            int to = lsb(caps_west);
            int from = to - 7;
            int vic = get_victim(to);
            if (to >= A8) {
                move_list.add_move(encode_move(from, to, PAWN, QUEEN, 1, 0, 0, 0, vic));
                move_list.add_move(encode_move(from, to, PAWN, ROOK, 1, 0, 0, 0, vic));
                move_list.add_move(encode_move(from, to, PAWN, BISHOP, 1, 0, 0, 0, vic));
                move_list.add_move(encode_move(from, to, PAWN, KNIGHT, 1, 0, 0, 0, vic));
            } else {
                move_list.add_move(encode_move(from, to, PAWN, 0, 1, 0, 0, 0, vic));
            }
            caps_west &= ~(1ULL << to);
        }

        // En Passant
        if (board.get_en_passant_sq() != SQUARE_COUNT) {
            int ep_sq = board.get_en_passant_sq();
            Bitboard ep_bit = (1ULL << ep_sq);
            if (atks_east & ep_bit) move_list.add_move(encode_move(ep_sq - 9, ep_sq, PAWN, 0, 1, 0, 1, 0, PAWN));
            if (atks_west & ep_bit) move_list.add_move(encode_move(ep_sq - 7, ep_sq, PAWN, 0, 1, 0, 1, 0, PAWN));
        }

    } else { // BLACK side
        // Single pushes
        Bitboard single_pushes = (pawns >> 8) & empty;
        Bitboard sp = single_pushes;
        while (sp) {
            int to = lsb(sp);
            int from = to + 8;
            if (to <= H1) { // Promotion
                move_list.add_move(encode_move(from, to, PAWN, QUEEN, 0, 0, 0, 0, 0));
                move_list.add_move(encode_move(from, to, PAWN, ROOK, 0, 0, 0, 0, 0));
                move_list.add_move(encode_move(from, to, PAWN, BISHOP, 0, 0, 0, 0, 0));
                move_list.add_move(encode_move(from, to, PAWN, KNIGHT, 0, 0, 0, 0, 0));
            } else {
                move_list.add_move(encode_move(from, to, PAWN, 0, 0, 0, 0, 0, 0));
            }
            sp &= ~(1ULL << to);
        }

        // Double pushes
        Bitboard double_pushes = (((pawns & 0x00FF000000000000ULL) >> 8) & empty) >> 8 & empty;
        Bitboard dp = double_pushes;
        while (dp) {
            int to = lsb(dp);
            int from = to + 16;
            move_list.add_move(encode_move(from, to, PAWN, 0, 0, 1, 0, 0, 0));
            dp &= ~(1ULL << to);
        }

        // Captures
        Bitboard atks_east = (pawns >> 7) & 0xFEFEFEFEFEFEFEFEULL;
        Bitboard caps_east = atks_east & enemy_occ;
        while (caps_east) {
            int to = lsb(caps_east);
            int from = to + 7;
            int vic = get_victim(to);
            if (to <= H1) {
                move_list.add_move(encode_move(from, to, PAWN, QUEEN, 1, 0, 0, 0, vic));
                move_list.add_move(encode_move(from, to, PAWN, ROOK, 1, 0, 0, 0, vic));
                move_list.add_move(encode_move(from, to, PAWN, BISHOP, 1, 0, 0, 0, vic));
                move_list.add_move(encode_move(from, to, PAWN, KNIGHT, 1, 0, 0, 0, vic));
            } else {
                move_list.add_move(encode_move(from, to, PAWN, 0, 1, 0, 0, 0, vic));
            }
            caps_east &= ~(1ULL << to);
        }

        Bitboard atks_west = (pawns >> 9) & 0x7F7F7F7F7F7F7F7FULL;
        Bitboard caps_west = atks_west & enemy_occ;
        while (caps_west) {
            int to = lsb(caps_west);
            int from = to + 9;
            int vic = get_victim(to);
            if (to <= H1) {
                move_list.add_move(encode_move(from, to, PAWN, QUEEN, 1, 0, 0, 0, vic));
                move_list.add_move(encode_move(from, to, PAWN, ROOK, 1, 0, 0, 0, vic));
                move_list.add_move(encode_move(from, to, PAWN, BISHOP, 1, 0, 0, 0, vic));
                move_list.add_move(encode_move(from, to, PAWN, KNIGHT, 1, 0, 0, 0, vic));
            } else {
                move_list.add_move(encode_move(from, to, PAWN, 0, 1, 0, 0, 0, vic));
            }
            caps_west &= ~(1ULL << to);
        }

        // En Passant
        if (board.get_en_passant_sq() != SQUARE_COUNT) {
            int ep_sq = board.get_en_passant_sq();
            Bitboard ep_bit = (1ULL << ep_sq);
            if (atks_east & ep_bit) move_list.add_move(encode_move(ep_sq + 7, ep_sq, PAWN, 0, 1, 0, 1, 0, PAWN));
            if (atks_west & ep_bit) move_list.add_move(encode_move(ep_sq + 9, ep_sq, PAWN, 0, 1, 0, 1, 0, PAWN));
        }
    }

    // -------------------------------------------------------------------------
    // Knight Moves
    // -------------------------------------------------------------------------
    Bitboard knights = board.get_piece_bitboard(KNIGHT) & friend_occ;
    while (knights) {
        int from = lsb(knights);
        Bitboard atks = knight_attacks[from] & ~friend_occ;
        while (atks) {
            int to = lsb(atks);
            int cap = (enemy_occ & (1ULL << to)) ? 1 : 0;
            int vic = cap ? get_victim(to) : 0;
            move_list.add_move(encode_move(from, to, KNIGHT, 0, cap, 0, 0, 0, vic));
            atks &= ~(1ULL << to);
        }
        knights &= ~(1ULL << from);
    }

    // -------------------------------------------------------------------------
    // King Moves & Castling
    // -------------------------------------------------------------------------
    Bitboard king = board.get_piece_bitboard(KING) & friend_occ;
    if (king) {
        int from = lsb(king);
        Bitboard atks = king_attacks[from] & ~friend_occ;
        while (atks) {
            int to = lsb(atks);
            int cap = (enemy_occ & (1ULL << to)) ? 1 : 0;
            int vic = cap ? get_victim(to) : 0;
            move_list.add_move(encode_move(from, to, KING, 0, cap, 0, 0, 0, vic));
            atks &= ~(1ULL << to);
        }

        // Castling
        int rights = board.get_castling_rights();
        if (side == WHITE) {
            if ((rights & 1) && !(all_occ & ((1ULL << F1) | (1ULL << G1)))) // Kingside
                move_list.add_move(encode_move(E1, G1, KING, 0, 0, 0, 0, 1, 0));
            if ((rights & 2) && !(all_occ & ((1ULL << D1) | (1ULL << C1) | (1ULL << B1)))) // Queenside
                move_list.add_move(encode_move(E1, C1, KING, 0, 0, 0, 0, 1, 0));
        } else {
            if ((rights & 4) && !(all_occ & ((1ULL << F8) | (1ULL << G8)))) // Kingside
                move_list.add_move(encode_move(E8, G8, KING, 0, 0, 0, 0, 1, 0));
            if ((rights & 8) && !(all_occ & ((1ULL << D8) | (1ULL << C8) | (1ULL << B8)))) // Queenside
                move_list.add_move(encode_move(E8, C8, KING, 0, 0, 0, 0, 1, 0));
        }
    }

    // -------------------------------------------------------------------------
    // Bishop Moves
    // -------------------------------------------------------------------------
    Bitboard bishops = board.get_piece_bitboard(BISHOP) & friend_occ;
    while (bishops) {
        int from = lsb(bishops);
        Bitboard atks = get_bishop_attacks(from, all_occ) & ~friend_occ;
        while (atks) {
            int to = lsb(atks);
            int cap = (enemy_occ & (1ULL << to)) ? 1 : 0;
            int vic = cap ? get_victim(to) : 0;
            move_list.add_move(encode_move(from, to, BISHOP, 0, cap, 0, 0, 0, vic));
            atks &= ~(1ULL << to);
        }
        bishops &= ~(1ULL << from);
    }

    // -------------------------------------------------------------------------
    // Rook Moves
    // -------------------------------------------------------------------------
    Bitboard rooks = board.get_piece_bitboard(ROOK) & friend_occ;
    while (rooks) {
        int from = lsb(rooks);
        Bitboard atks = get_rook_attacks(from, all_occ) & ~friend_occ;
        while (atks) {
            int to = lsb(atks);
            int cap = (enemy_occ & (1ULL << to)) ? 1 : 0;
            int vic = cap ? get_victim(to) : 0;
            move_list.add_move(encode_move(from, to, ROOK, 0, cap, 0, 0, 0, vic));
            atks &= ~(1ULL << to);
        }
        rooks &= ~(1ULL << from);
    }

    // -------------------------------------------------------------------------
    // Queen Moves
    // -------------------------------------------------------------------------
    Bitboard queens = board.get_piece_bitboard(QUEEN) & friend_occ;
    while (queens) {
        int from = lsb(queens);
        Bitboard atks = get_queen_attacks(from, all_occ) & ~friend_occ;
        while (atks) {
            int to = lsb(atks);
            int cap = (enemy_occ & (1ULL << to)) ? 1 : 0;
            int vic = cap ? get_victim(to) : 0;
            move_list.add_move(encode_move(from, to, QUEEN, 0, cap, 0, 0, 0, vic));
            atks &= ~(1ULL << to);
        }
        queens &= ~(1ULL << from);
    }
}

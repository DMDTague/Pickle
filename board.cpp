#include "board.h"
#include "bit_utils.h"
#include "attacks.h"
#include "magics.h"
#include "zobrist.h"
#include <iostream>
#include <algorithm>

const int castling_rights_mask[64] = {
     13, 15, 15, 15, 12, 15, 15, 14,
     15, 15, 15, 15, 15, 15, 15, 15,
     15, 15, 15, 15, 15, 15, 15, 15,
     15, 15, 15, 15, 15, 15, 15, 15,
     15, 15, 15, 15, 15, 15, 15, 15,
     15, 15, 15, 15, 15, 15, 15, 15,
     15, 15, 15, 15, 15, 15, 15, 15,
      7, 15, 15, 15,  3, 15, 15, 11
};

Board::Board() {
    clear_board();
}

void Board::clear_board() {
    for (int i = 0; i < PIECE_TYPE_COUNT; ++i) pieces[i] = 0ULL;
    for (int i = 0; i < COLOR_COUNT; ++i) colors[i] = 0ULL;
    side_to_move = WHITE;
    en_passant_sq = SQUARE_COUNT; // NO_SQUARE
    castling_rights = 0;
    ply = 0;
    half_move_clock = 0;
    hash_key = 0;
}

bool Board::has_non_pawn_material(Color color) const {
    Bitboard non_pawns = pieces[KNIGHT] | pieces[BISHOP] | pieces[ROOK] | pieces[QUEEN];
    return (non_pawns & colors[color]) != 0;
}

bool Board::is_draw() const {
    if (half_move_clock >= 100) return true;
    
    int reps = 0;
    int start_index = std::max(0, ply - half_move_clock);
    for (int i = start_index; i < ply; i++) {
        if (history[i].hash_key == hash_key) {
            reps++;
            if (reps >= 2) return true; // Including current pos, that's 3 times
        }
    }
    return false;
}

std::string Board::get_fen() const {
    std::string fen = "";
    for (int rank = 7; rank >= 0; rank--) {
        int empty_count = 0;
        for (int file = 0; file < 8; file++) {
            int sq = rank * 8 + file;
            char piece_char = ' ';
            
            for (int p = 0; p < 6; p++) {
                if (get_bit(pieces[p], sq)) {
                    if (get_bit(colors[WHITE], sq)) piece_char = "PNBRQK"[p];
                    else piece_char = "pnbrqk"[p];
                    break;
                }
            }
            
            if (piece_char != ' ') {
                if (empty_count > 0) {
                    fen += std::to_string(empty_count);
                    empty_count = 0;
                }
                fen += piece_char;
            } else {
                empty_count++;
            }
        }
        if (empty_count > 0) fen += std::to_string(empty_count);
        if (rank > 0) fen += "/";
    }
    
    fen += (side_to_move == WHITE) ? " w " : " b ";
    
    std::string castling = "";
    if (castling_rights & 1) castling += "K";
    if (castling_rights & 2) castling += "Q";
    if (castling_rights & 4) castling += "k";
    if (castling_rights & 8) castling += "q";
    fen += (castling.empty()) ? "-" : castling;
    
    fen += " ";
    if (en_passant_sq != SQUARE_COUNT) {
        char f = 'a' + (en_passant_sq % 8);
        char r = '1' + (en_passant_sq / 8);
        fen += f;
        fen += r;
    } else {
        fen += "-";
    }
    
    fen += " " + std::to_string(half_move_clock) + " 1";
    
    return fen;
}


void Board::parse_fen(const std::string& fen) {
    clear_board();

    int rank = 7;
    int file = 0;
    size_t i = 0;

    // 1. Piece placement
    for (; i < fen.length() && fen[i] != ' '; i++) {
        char c = fen[i];
        if (c == '/') {
            rank--;
            file = 0;
        } else if (isdigit(c)) {
            file += (c - '0');
        } else {
            Color col = isupper(c) ? WHITE : BLACK;
            PieceType pt = PAWN; // dummy
            char lc = tolower(c);
            if (lc == 'p') pt = PAWN;
            else if (lc == 'n') pt = KNIGHT;
            else if (lc == 'b') pt = BISHOP;
            else if (lc == 'r') pt = ROOK;
            else if (lc == 'q') pt = QUEEN;
            else if (lc == 'k') pt = KING;

            int sq = rank * 8 + file;
            add_piece((Square)sq, col, pt);
            file++;
        }
    }

    // 2. Active color
    if (++i < fen.length()) {
        side_to_move = (fen[i] == 'w') ? WHITE : BLACK;
        i += 2; // skip letter and space
    }

    // 3. Castling availability
    castling_rights = 0;
    while (i < fen.length() && fen[i] != ' ') {
        char c = fen[i++];
        if (c == 'K') castling_rights |= 1;
        else if (c == 'Q') castling_rights |= 2;
        else if (c == 'k') castling_rights |= 4;
        else if (c == 'q') castling_rights |= 8;
        // if '-', loops once and does nothing
    }

    // 4. En passant target square
    if (++i < fen.length() && fen[i] != '-') {
        int f = fen[i] - 'a';
        int r = fen[i+1] - '1';
        en_passant_sq = (Square)(r * 8 + f);
        i += 2;
    } else if (i < fen.length()) {
        en_passant_sq = SQUARE_COUNT;
        i++;
    }

    // 5. Halfmove clock (optional in some FENs)
    if (++i < fen.length()) {
        int clock = 0;
        while (i < fen.length() && isdigit(fen[i])) {
            clock = clock * 10 + (fen[i] - '0');
            i++;
        }
        half_move_clock = clock;
    }
    
    hash_key = generate_hash(*this);
}


void Board::init_start_position() {
    parse_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}



void Board::print_board() const {
    std::cout << "\n";
    // Iterate from Rank 8 down to Rank 1 (to print top-to-bottom on the console)
    for (int rank = 7; rank >= 0; --rank) {
        std::cout << rank + 1 << "  "; // Rank labels
        for (int file = 0; file < 8; ++file) {
            // Calculate the 0-63 square index
            int square = rank * 8 + file;
            char piece_char = '.';

            // Check if there is a piece on the currently probed square
            if (get_bit(colors[WHITE], square)) {
                if      (get_bit(pieces[PAWN],   square)) piece_char = 'P';
                else if (get_bit(pieces[KNIGHT], square)) piece_char = 'N';
                else if (get_bit(pieces[BISHOP], square)) piece_char = 'B';
                else if (get_bit(pieces[ROOK],   square)) piece_char = 'R';
                else if (get_bit(pieces[QUEEN],  square)) piece_char = 'Q';
                else if (get_bit(pieces[KING],   square)) piece_char = 'K';
            } 
            else if (get_bit(colors[BLACK], square)) {
                if      (get_bit(pieces[PAWN],   square)) piece_char = 'p';
                else if (get_bit(pieces[KNIGHT], square)) piece_char = 'n';
                else if (get_bit(pieces[BISHOP], square)) piece_char = 'b';
                else if (get_bit(pieces[ROOK],   square)) piece_char = 'r';
                else if (get_bit(pieces[QUEEN],  square)) piece_char = 'q';
                else if (get_bit(pieces[KING],   square)) piece_char = 'k';
            }

            std::cout << piece_char << " ";
        }
        std::cout << "\n";
    }
    std::cout << "   a b c d e f g h\n\n";
}

// -----------------------------------------------------------------------------
// Move Execution Helpers
// -----------------------------------------------------------------------------

void Board::remove_piece(Square sq, Color c, PieceType p) {
    clear_bit(pieces[p], sq);
    clear_bit(colors[c], sq);
    hash_key ^= piece_keys[c * 6 + p][sq];
}

void Board::add_piece(Square sq, Color c, PieceType p) {
    set_bit(pieces[p], sq);
    set_bit(colors[c], sq);
    hash_key ^= piece_keys[c * 6 + p][sq];
}

void Board::move_piece(Square from, Square to, Color c, PieceType p) {
    clear_bit(pieces[p], from);
    clear_bit(colors[c], from);
    set_bit(pieces[p], to);
    set_bit(colors[c], to);
    hash_key ^= piece_keys[c * 6 + p][from];
    hash_key ^= piece_keys[c * 6 + p][to];
}

bool Board::is_square_attacked(Square sq, Color by_color) const {
    Bitboard by_occ = colors[by_color];
    Bitboard all_occ = colors[WHITE] | colors[BLACK];

    // Pawns
    if (by_color == WHITE) {
        if ((1ULL << sq) & ((pieces[PAWN] & colors[WHITE]) << 7 & 0x7F7F7F7F7F7F7F7FULL)) return true;
        if ((1ULL << sq) & ((pieces[PAWN] & colors[WHITE]) << 9 & 0xFEFEFEFEFEFEFEFEULL)) return true;
    } else {
        if ((1ULL << sq) & ((pieces[PAWN] & colors[BLACK]) >> 7 & 0xFEFEFEFEFEFEFEFEULL)) return true;
        if ((1ULL << sq) & ((pieces[PAWN] & colors[BLACK]) >> 9 & 0x7F7F7F7F7F7F7F7FULL)) return true;
    }

    // Knights
    if (knight_attacks[sq] & pieces[KNIGHT] & by_occ) return true;

    // Kings
    if (king_attacks[sq] & pieces[KING] & by_occ) return true;

    // Bishops / Queens (Diagonal)
    if (get_bishop_attacks(sq, all_occ) & (pieces[BISHOP] | pieces[QUEEN]) & by_occ) return true;

    // Rooks / Queens (Orthogonal)
    if (get_rook_attacks(sq, all_occ) & (pieces[ROOK] | pieces[QUEEN]) & by_occ) return true;

    return false;
}

// -----------------------------------------------------------------------------
// Make / Unmake Move
// -----------------------------------------------------------------------------

bool Board::make_move(Move move) {
    int from = get_move_source(move);
    int to = get_move_target(move);
    int piece = get_move_piece(move);
    int promoted = get_move_promoted(move);
    int capture = get_move_capture(move);
    int double_push = get_move_double(move);
    int enpassant = get_move_enpassant(move);
    int castling = get_move_castling(move);
    
    Color us = side_to_move;
    Color them = (Color)(1 - side_to_move);

    // Identify captured piece type
    PieceType cap_piece = PAWN; // Default/dummy
    if (capture) {
        if (enpassant) {
            cap_piece = PAWN;
        } else {
            for (int i = 0; i < PIECE_TYPE_COUNT; ++i) {
                if (get_bit(pieces[i], to) && get_bit(colors[them], to)) {
                    cap_piece = (PieceType)i;
                    break;
                }
            }
        }
    }

    // Push state to history
    history[ply].castling_rights = castling_rights;
    history[ply].en_passant_sq = en_passant_sq;
    history[ply].half_move_clock = half_move_clock;
    history[ply].captured_piece = cap_piece;
    history[ply].hash_key = hash_key;
    
    // Hash updates out
    if (en_passant_sq != SQUARE_COUNT) hash_key ^= en_passant_keys[en_passant_sq % 8];
    hash_key ^= castle_keys[castling_rights];

    // Update ply and half move clock
    ply++;
    half_move_clock++;
    if (piece == PAWN || capture) {
        half_move_clock = 0;
    }

    // Remove captured piece
    if (capture) {
        if (enpassant) {
            Square cap_sq = (us == WHITE) ? (Square)(to - 8) : (Square)(to + 8);
            remove_piece(cap_sq, them, PAWN);
        } else {
            remove_piece((Square)to, them, cap_piece);
        }
    }

    // Move the actual piece
    move_piece((Square)from, (Square)to, us, (PieceType)piece);

    // Handle Promotion
    if (promoted) {
        remove_piece((Square)to, us, PAWN);
        add_piece((Square)to, us, (PieceType)promoted);
    }

    // Handle En Passant square update
    if (double_push) {
        en_passant_sq = (us == WHITE) ? (Square)(from + 8) : (Square)(from - 8);
    } else {
        en_passant_sq = SQUARE_COUNT;
    }

    // Handle Castling moves
    if (castling) {
        if (to == G1) move_piece(H1, F1, us, ROOK);
        else if (to == C1) move_piece(A1, D1, us, ROOK);
        else if (to == G8) move_piece(H8, F8, us, ROOK);
        else if (to == C8) move_piece(A8, D8, us, ROOK);
    }

    // Update castling rights (bitwise AND with masks based on move's from/to)
    castling_rights &= castling_rights_mask[from];
    castling_rights &= castling_rights_mask[to];

    // Hash updates in
    if (en_passant_sq != SQUARE_COUNT) hash_key ^= en_passant_keys[en_passant_sq % 8];
    hash_key ^= castle_keys[castling_rights];

    // Swap sides
    side_to_move = them;
    hash_key ^= side_key;

    // Legality check: Is our king in check after this move?
    Bitboard our_king_bb = pieces[KING] & colors[us];
    if (our_king_bb == 0) return false; // Shouldn't happen unless king captured
    Square our_king_sq = (Square)lsb(our_king_bb);
    
    if (is_square_attacked(our_king_sq, them)) {
        unmake_move(move);
        return false;
    }

    // Castling legality includes checking current square and crossed square
    if (castling) {
        if (is_square_attacked((Square)from, them)) {
            unmake_move(move);
            return false;
        }
        Square crossed_sq = SQUARE_COUNT;
        if (to == G1) crossed_sq = F1;
        else if (to == C1) crossed_sq = D1;
        else if (to == G8) crossed_sq = F8;
        else if (to == C8) crossed_sq = D8;
        
        if (crossed_sq != SQUARE_COUNT && is_square_attacked(crossed_sq, them)) {
            unmake_move(move);
            return false;
        }
    }

    return true;
}

void Board::make_null_move() {
    // 1. Save state
    history[ply].castling_rights = castling_rights;
    history[ply].en_passant_sq = en_passant_sq;
    history[ply].half_move_clock = half_move_clock;
    history[ply].captured_piece = PIECE_TYPE_COUNT;
    history[ply].hash_key = hash_key;

    // 2. Clear en_passant
    if (en_passant_sq != SQUARE_COUNT) {
        hash_key ^= en_passant_keys[en_passant_sq % 8];
        en_passant_sq = SQUARE_COUNT;
    }

    // 3. Swap side
    side_to_move = (Color)(1 - side_to_move);
    hash_key ^= side_key;

    // 4. Update clocks
    half_move_clock++;
    ply++;
}

void Board::unmake_null_move() {
    ply--;

    // 1. Restore state
    castling_rights = history[ply].castling_rights;
    en_passant_sq = history[ply].en_passant_sq;
    half_move_clock = history[ply].half_move_clock;
    hash_key = history[ply].hash_key;

    // 2. Swap side back
    side_to_move = (Color)(1 - side_to_move);
}

void Board::unmake_move(Move move) {
    ply--;
    side_to_move = (Color)(1 - side_to_move);
    Color us = side_to_move;
    Color them = (Color)(1 - side_to_move);

    int from = get_move_source(move);
    int to = get_move_target(move);
    int piece = get_move_piece(move);
    int promoted = get_move_promoted(move);
    int capture = get_move_capture(move);
    int enpassant = get_move_enpassant(move);
    int castling = get_move_castling(move);

    // Restore state
    castling_rights = history[ply].castling_rights;
    en_passant_sq = history[ply].en_passant_sq;
    half_move_clock = history[ply].half_move_clock;
    PieceType cap_piece = history[ply].captured_piece;
    hash_key = history[ply].hash_key;

    // Handle Promotion Reverse
    if (promoted) {
        remove_piece((Square)to, us, (PieceType)promoted);
        add_piece((Square)to, us, PAWN);
    }

    // Move piece back
    move_piece((Square)to, (Square)from, us, (PieceType)piece);

    // Restore Captured piece
    if (capture) {
        if (enpassant) {
            Square cap_sq = (us == WHITE) ? (Square)(to - 8) : (Square)(to + 8);
            add_piece(cap_sq, them, PAWN);
        } else {
            add_piece((Square)to, them, cap_piece);
        }
    }

    // Handle Castling Reverse
    if (castling) {
        if (to == G1) move_piece(F1, H1, us, ROOK);
        else if (to == C1) move_piece(D1, A1, us, ROOK);
        else if (to == G8) move_piece(F8, H8, us, ROOK);
        else if (to == C8) move_piece(D8, A8, us, ROOK);
    }
}

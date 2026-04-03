#include "zobrist.h"
#include "bit_utils.h"

// Definition of global variable keys
U64 piece_keys[12][64];
U64 en_passant_keys[8];
U64 castle_keys[16];
U64 side_key;

// Simple 32-bit PRNG state
unsigned int state = 1804289383;

// Generate 32-bit pseudo-random number
unsigned int get_random_U32() {
    unsigned int number = state;
    number ^= number << 13;
    number ^= number >> 17;
    number ^= number << 5;
    state = number;
    return number;
}

// Generate 64-bit pseudo-random number
U64 get_random_U64() {
    U64 n1, n2, n3, n4;
    n1 = (U64)(get_random_U32()) & 0xFFFF;
    n2 = (U64)(get_random_U32()) & 0xFFFF;
    n3 = (U64)(get_random_U32()) & 0xFFFF;
    n4 = (U64)(get_random_U32()) & 0xFFFF;
    return n1 | (n2 << 16) | (n3 << 32) | (n4 << 48);
}

void init_zobrist() {
    for (int piece = 0; piece < 12; piece++) {
        for (int sq = 0; sq < 64; sq++) {
            piece_keys[piece][sq] = get_random_U64();
        }
    }
    for (int i = 0; i < 8; i++) {
        en_passant_keys[i] = get_random_U64();
    }
    for (int i = 0; i < 16; i++) {
        castle_keys[i] = get_random_U64();
    }
    side_key = get_random_U64();
}

U64 generate_hash(const Board& board) {
    U64 final_key = 0;
    
    // Pieces
    for (int color = 0; color < 2; color++) {
        for (int type = 0; type < 6; type++) {
            int piece_index = color * 6 + type;
            Bitboard bb = board.get_piece_bitboard((PieceType)type) & board.get_color_bitboard((Color)color);
            while (bb) {
                int sq = lsb(bb);
                final_key ^= piece_keys[piece_index][sq];
                bb &= ~(1ULL << sq);
            }
        }
    }
    
    // En Passant
    if (board.get_en_passant_sq() != SQUARE_COUNT) {
        int file = board.get_en_passant_sq() % 8;
        final_key ^= en_passant_keys[file];
    }
    
    // Castling
    final_key ^= castle_keys[board.get_castling_rights()];
    
    // Side to move
    if (board.get_side_to_move() == BLACK) {
        final_key ^= side_key;
    }
    
    return final_key;
}
